/* radiosity.c -- Progressive Radiosity with form factor computation
 * SPLASH-3 style: pthreads parallel. Generate rectangular patches (room),
 * compute form factors between pairs, iteratively shoot energy from
 * brightest unshot patch to all others. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include "zsim_hooks.h"

#define DEFAULT_SIZE    256
#define DEFAULT_THREADS 4
#define NUM_ITERATIONS  20
#define PI              3.14159265358979323846

static int parse_int_arg(int argc, char** argv, const char* flag, int def) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return def;
}

static uint32_t bench_rand(uint32_t* s) {
    *s = *s * 1103515245 + 12345;
    return (*s >> 16) & 0x7FFF;
}

typedef struct {
    double cx, cy, cz;   /* center position */
    double nx, ny, nz;   /* normal */
    double area;
    double emission[3];  /* emitted RGB energy */
    double radiosity[3]; /* total radiosity */
    double unshot[3];    /* unshot radiosity (to distribute) */
    double reflectance;
} Patch;

/* Shared state */
static Patch* patches;
static double* form_factors; /* N x N matrix */
static int npatches, nthreads;
static pthread_barrier_t barrier;

typedef struct { int tid; } ThreadArg;

/* Simplified form factor: cosine-weighted area / (pi * distance^2 + epsilon) */
static double compute_ff(int i, int j) {
    if (i == j) return 0.0;
    double dx = patches[j].cx - patches[i].cx;
    double dy = patches[j].cy - patches[i].cy;
    double dz = patches[j].cz - patches[i].cz;
    double r2 = dx * dx + dy * dy + dz * dz;
    if (r2 < 1e-10) return 0.0;
    double r = sqrt(r2);
    double inv_r = 1.0 / r;
    /* Cosine of angle between normal_i and direction i->j */
    double cos_i = (patches[i].nx * dx + patches[i].ny * dy + patches[i].nz * dz) * inv_r;
    /* Cosine of angle between normal_j and direction j->i */
    double cos_j = -(patches[j].nx * dx + patches[j].ny * dy + patches[j].nz * dz) * inv_r;
    if (cos_i <= 0.0 || cos_j <= 0.0) return 0.0;
    return cos_i * cos_j * patches[j].area / (PI * r2 + 1e-6);
}

/* Phase 1: compute form factors in parallel */
static void* compute_ff_worker(void* arg) {
    int tid = ((ThreadArg*)arg)->tid;
    int lo = (npatches * tid) / nthreads;
    int hi = (npatches * (tid + 1)) / nthreads;
    for (int i = lo; i < hi; i++) {
        for (int j = 0; j < npatches; j++) {
            form_factors[i * npatches + j] = compute_ff(i, j);
        }
    }
    return NULL;
}

/* Shared: index of brightest unshot patch (set by thread 0) */
static int brightest_idx;
static double brightest_energy;

/* Phase 2: distribute energy from brightest in parallel */
static void* distribute_worker(void* arg) {
    int tid = ((ThreadArg*)arg)->tid;
    int lo = (npatches * tid) / nthreads;
    int hi = (npatches * (tid + 1)) / nthreads;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        /* Thread 0 finds brightest */
        if (tid == 0) {
            brightest_idx = -1;
            brightest_energy = 0.0;
            for (int i = 0; i < npatches; i++) {
                double e = patches[i].unshot[0] + patches[i].unshot[1] + patches[i].unshot[2];
                e *= patches[i].area;
                if (e > brightest_energy) {
                    brightest_energy = e;
                    brightest_idx = i;
                }
            }
        }
        pthread_barrier_wait(&barrier);
        if (brightest_idx < 0) break;

        int src = brightest_idx;
        /* Distribute energy to this thread's patches */
        for (int j = lo; j < hi; j++) {
            if (j == src) continue;
            double ff = form_factors[j * npatches + src];
            if (ff <= 0.0) continue;
            double rho = patches[j].reflectance;
            for (int c = 0; c < 3; c++) {
                double delta = rho * patches[src].unshot[c] * ff;
                patches[j].radiosity[c] += delta;
                patches[j].unshot[c] += delta;
            }
        }
        pthread_barrier_wait(&barrier);

        /* Thread 0 zeros the unshot of the source */
        if (tid == 0) {
            patches[src].unshot[0] = 0;
            patches[src].unshot[1] = 0;
            patches[src].unshot[2] = 0;
        }
        pthread_barrier_wait(&barrier);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    npatches = parse_int_arg(argc, argv, "--size", DEFAULT_SIZE);
    nthreads = parse_int_arg(argc, argv, "--threads", DEFAULT_THREADS);

    patches = (Patch*)malloc((size_t)npatches * sizeof(Patch));
    form_factors = (double*)malloc((size_t)npatches * npatches * sizeof(double));
    if (!patches || !form_factors) { fprintf(stderr, "malloc failed\n"); return 1; }

    /* Generate patches: room-like scene
     * First 1/6 on each of 6 walls (floor, ceiling, 4 side walls) */
    uint32_t seed = 42;
    int per_wall = npatches / 6;
    if (per_wall < 1) per_wall = 1;
    double room = 10.0;
    int idx = 0;

    /* Helper: normals for 6 walls */
    double normals[6][3] = {
        { 0,  1, 0}, /* floor   (y=0, normal up) */
        { 0, -1, 0}, /* ceiling (y=room, normal down) */
        { 1,  0, 0}, /* left wall  (x=0, normal +x) */
        {-1,  0, 0}, /* right wall (x=room, normal -x) */
        { 0,  0, 1}, /* back wall  (z=0, normal +z) */
        { 0,  0,-1}, /* front wall (z=room, normal -z) */
    };
    for (int w = 0; w < 6 && idx < npatches; w++) {
        int cnt = (w < 5) ? per_wall : (npatches - idx);
        for (int p = 0; p < cnt && idx < npatches; p++, idx++) {
            double u = room * bench_rand(&seed) / 32768.0;
            double v = room * bench_rand(&seed) / 32768.0;
            switch (w) {
                case 0: patches[idx].cx = u; patches[idx].cy = 0;    patches[idx].cz = v; break;
                case 1: patches[idx].cx = u; patches[idx].cy = room; patches[idx].cz = v; break;
                case 2: patches[idx].cx = 0;    patches[idx].cy = u; patches[idx].cz = v; break;
                case 3: patches[idx].cx = room; patches[idx].cy = u; patches[idx].cz = v; break;
                case 4: patches[idx].cx = u; patches[idx].cy = v; patches[idx].cz = 0;    break;
                case 5: patches[idx].cx = u; patches[idx].cy = v; patches[idx].cz = room; break;
            }
            patches[idx].nx = normals[w][0];
            patches[idx].ny = normals[w][1];
            patches[idx].nz = normals[w][2];
            patches[idx].area = (room * room) / (double)per_wall;
            patches[idx].reflectance = 0.3 + 0.5 * bench_rand(&seed) / 32768.0;
            for (int c = 0; c < 3; c++) {
                patches[idx].radiosity[c] = 0;
                patches[idx].unshot[c] = 0;
                patches[idx].emission[c] = 0;
            }
        }
    }
    /* A few light source patches on the ceiling */
    for (int i = per_wall; i < per_wall + 4 && i < npatches; i++) {
        patches[i].emission[0] = 50.0;
        patches[i].emission[1] = 50.0;
        patches[i].emission[2] = 40.0;
        for (int c = 0; c < 3; c++) {
            patches[i].radiosity[c] = patches[i].emission[c];
            patches[i].unshot[c] = patches[i].emission[c];
        }
    }

    pthread_barrier_init(&barrier, NULL, (unsigned)nthreads);
    pthread_t* threads = (pthread_t*)malloc((size_t)nthreads * sizeof(pthread_t));
    ThreadArg* args = (ThreadArg*)malloc((size_t)nthreads * sizeof(ThreadArg));

    zsim_roi_begin();

    /* Phase 1: compute form factors in parallel */
    for (int t = 1; t < nthreads; t++) {
        args[t].tid = t;
        pthread_create(&threads[t], NULL, compute_ff_worker, &args[t]);
    }
    args[0].tid = 0;
    compute_ff_worker(&args[0]);
    for (int t = 1; t < nthreads; t++)
        pthread_join(threads[t], NULL);

    /* Phase 2: progressive radiosity (iterative distribution) */
    for (int t = 1; t < nthreads; t++) {
        args[t].tid = t;
        pthread_create(&threads[t], NULL, distribute_worker, &args[t]);
    }
    args[0].tid = 0;
    distribute_worker(&args[0]);
    for (int t = 1; t < nthreads; t++)
        pthread_join(threads[t], NULL);

    zsim_roi_end();

    /* Checksum: sum of all patch radiosities */
    double checksum = 0.0;
    for (int i = 0; i < npatches; i++)
        checksum += patches[i].radiosity[0] + patches[i].radiosity[1] + patches[i].radiosity[2];
    printf("BENCH_CHECKSUM: %f\n", checksum);
    printf("BENCH_DONE\n");

    pthread_barrier_destroy(&barrier);
    free(threads); free(args);
    free(patches); free(form_factors);
    return 0;
}
