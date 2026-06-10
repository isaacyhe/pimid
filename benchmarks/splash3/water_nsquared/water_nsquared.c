/* water_nsquared.c -- N^2 Molecular Dynamics (SPLASH-3 style)
 *
 * N-body molecular dynamics with Lennard-Jones potential. For each pair
 * of molecules, compute distance, force via LJ 12-6 potential, update
 * velocities. Multiple timesteps. N^2 all-pairs interaction pattern.
 * Pthreads: partition molecule pairs among threads.
 *
 * Compile: g++ -std=c++11 -O2 -Wall -I<hooks-path> -lpthread -lm water_nsquared.c -o water_nsquared
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>
#include "zsim_hooks.h"

#define DEFAULT_SIZE    512
#define DEFAULT_THREADS 4
#define TIMESTEPS       5
#define DT              0.001
#define BOX_SIZE        10.0
#define CUTOFF          2.5
#define CUTOFF2         (CUTOFF * CUTOFF)
#define EPSILON         1.0
#define SIGMA           1.0

/* ------------------------------------------------------------------ */
/*  Arg parser                                                         */
/* ------------------------------------------------------------------ */
static int parse_int_arg(int argc, char** argv, const char* flag, int def) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return def;
}

/* ------------------------------------------------------------------ */
/*  Deterministic LCG                                                  */
/* ------------------------------------------------------------------ */
static uint32_t bench_rand(uint32_t* s) {
    *s = *s * 1103515245 + 12345;
    return (*s >> 16) & 0x7FFF;
}

/* ------------------------------------------------------------------ */
/*  Molecule data                                                      */
/* ------------------------------------------------------------------ */
typedef struct {
    double x, y, z;
    double vx, vy, vz;
    double fx, fy, fz;
} molecule_t;

static molecule_t* mols;
static int N;
static int num_threads;
static pthread_barrier_t barrier;
/* Per-thread force accumulation (to avoid races on mol.fx/fy/fz) */
static double* thread_forces;  /* num_threads * N * 3 */

/* ------------------------------------------------------------------ */
/*  Worker thread: compute pairwise forces                             */
/* ------------------------------------------------------------------ */
static void* force_worker(void* arg) {
    int tid = (int)(intptr_t)arg;
    double* my_f = &thread_forces[(size_t)tid * N * 3];

    /* Zero local force accumulators */
    memset(my_f, 0, (size_t)N * 3 * sizeof(double));

    /* Partition upper triangle pairs among threads.
       Total pairs = N*(N-1)/2. We partition by row ownership. */
    int chunk = (N + num_threads - 1) / num_threads;
    int lo = tid * chunk;
    int hi = lo + chunk;
    if (hi > N) hi = N;

    for (int i = lo; i < hi; i++) {
        for (int j = i + 1; j < N; j++) {
            double dx = mols[i].x - mols[j].x;
            double dy = mols[i].y - mols[j].y;
            double dz = mols[i].z - mols[j].z;

            /* Minimum image convention (periodic box) */
            if (dx > BOX_SIZE * 0.5) dx -= BOX_SIZE;
            else if (dx < -BOX_SIZE * 0.5) dx += BOX_SIZE;
            if (dy > BOX_SIZE * 0.5) dy -= BOX_SIZE;
            else if (dy < -BOX_SIZE * 0.5) dy += BOX_SIZE;
            if (dz > BOX_SIZE * 0.5) dz -= BOX_SIZE;
            else if (dz < -BOX_SIZE * 0.5) dz += BOX_SIZE;

            double r2 = dx * dx + dy * dy + dz * dz;

            if (r2 < CUTOFF2 && r2 > 1e-12) {
                /* Lennard-Jones 12-6 force */
                double r2_inv = SIGMA * SIGMA / r2;
                double r6_inv = r2_inv * r2_inv * r2_inv;
                double force_mag = 24.0 * EPSILON * r6_inv * (2.0 * r6_inv - 1.0) / r2;

                double fx = force_mag * dx;
                double fy = force_mag * dy;
                double fz = force_mag * dz;

                my_f[i * 3 + 0] += fx;
                my_f[i * 3 + 1] += fy;
                my_f[i * 3 + 2] += fz;
                my_f[j * 3 + 0] -= fx;
                my_f[j * 3 + 1] -= fy;
                my_f[j * 3 + 2] -= fz;
            }
        }
    }

    pthread_barrier_wait(&barrier);

    /* Thread 0 reduces forces from all threads into mol structs */
    if (tid == 0) {
        for (int i = 0; i < N; i++) {
            mols[i].fx = 0; mols[i].fy = 0; mols[i].fz = 0;
            for (int t = 0; t < num_threads; t++) {
                double* tf = &thread_forces[(size_t)t * N * 3];
                mols[i].fx += tf[i * 3 + 0];
                mols[i].fy += tf[i * 3 + 1];
                mols[i].fz += tf[i * 3 + 2];
            }
        }
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Wrap position into periodic box [0, BOX_SIZE)                      */
/* ------------------------------------------------------------------ */
static double wrap(double x) {
    while (x < 0) x += BOX_SIZE;
    while (x >= BOX_SIZE) x -= BOX_SIZE;
    return x;
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */
int main(int argc, char** argv) {
    N = parse_int_arg(argc, argv, "--size", DEFAULT_SIZE);
    num_threads = parse_int_arg(argc, argv, "--threads", DEFAULT_THREADS);

    printf("Water N-squared MD -- N=%d, threads=%d, timesteps=%d\n", N, num_threads, TIMESTEPS);

    mols = (molecule_t*)calloc((size_t)N, sizeof(molecule_t));
    thread_forces = (double*)malloc((size_t)num_threads * N * 3 * sizeof(double));
    if (!mols || !thread_forces) { fprintf(stderr, "malloc failed\n"); return 1; }

    /* Initialize molecules on a grid-like pattern with small random offsets */
    uint32_t seed = 42;
    int per_dim = 1;
    while (per_dim * per_dim * per_dim < N) per_dim++;
    double spacing = BOX_SIZE / per_dim;

    int idx = 0;
    for (int ix = 0; ix < per_dim && idx < N; ix++) {
        for (int iy = 0; iy < per_dim && idx < N; iy++) {
            for (int iz = 0; iz < per_dim && idx < N; iz++) {
                mols[idx].x = (ix + 0.5) * spacing +
                              0.1 * spacing * ((double)bench_rand(&seed) / 32768.0 - 0.5);
                mols[idx].y = (iy + 0.5) * spacing +
                              0.1 * spacing * ((double)bench_rand(&seed) / 32768.0 - 0.5);
                mols[idx].z = (iz + 0.5) * spacing +
                              0.1 * spacing * ((double)bench_rand(&seed) / 32768.0 - 0.5);
                mols[idx].vx = 0.01 * ((double)bench_rand(&seed) / 32768.0 - 0.5);
                mols[idx].vy = 0.01 * ((double)bench_rand(&seed) / 32768.0 - 0.5);
                mols[idx].vz = 0.01 * ((double)bench_rand(&seed) / 32768.0 - 0.5);
                idx++;
            }
        }
    }

    pthread_barrier_init(&barrier, NULL, (unsigned)num_threads);
    pthread_t* threads = (pthread_t*)malloc((size_t)num_threads * sizeof(pthread_t));

    /* ---- ROI begin ---- */
    zsim_roi_begin();

    for (int step = 0; step < TIMESTEPS; step++) {
        /* Compute forces (parallel) */
        for (int t = 1; t < num_threads; t++)
            pthread_create(&threads[t], NULL, force_worker, (void*)(intptr_t)t);
        force_worker((void*)(intptr_t)0);
        for (int t = 1; t < num_threads; t++)
            pthread_join(threads[t], NULL);

        /* Velocity Verlet integration (serial, lightweight) */
        for (int i = 0; i < N; i++) {
            /* mass = 1.0 so acceleration = force */
            mols[i].vx += mols[i].fx * DT;
            mols[i].vy += mols[i].fy * DT;
            mols[i].vz += mols[i].fz * DT;
            mols[i].x = wrap(mols[i].x + mols[i].vx * DT);
            mols[i].y = wrap(mols[i].y + mols[i].vy * DT);
            mols[i].z = wrap(mols[i].z + mols[i].vz * DT);
        }
    }

    zsim_roi_end();
    /* ---- ROI end ---- */

    /* Checksum: total kinetic energy */
    double checksum = 0.0;
    for (int i = 0; i < N; i++) {
        checksum += 0.5 * (mols[i].vx * mols[i].vx +
                           mols[i].vy * mols[i].vy +
                           mols[i].vz * mols[i].vz);
    }

    printf("BENCH_CHECKSUM: %f\n", checksum);
    printf("BENCH_DONE\n");

    pthread_barrier_destroy(&barrier);
    free(threads);
    free(thread_forces);
    free(mols);
    return 0;
}
