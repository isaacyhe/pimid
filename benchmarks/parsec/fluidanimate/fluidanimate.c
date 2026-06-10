/*
 * SPH Fluid Simulation -- Smoothed Particle Hydrodynamics (PARSEC fluidanimate)
 * Spatial hash grid for neighbor finding. Leapfrog integration.
 * Density, pressure, viscosity forces computed per-particle.
 *
 * Compile: g++ -std=c++11 -O2 -Wall -I<path-to-zsim/misc/hooks> -lpthread -lm fluidanimate.c -o fluidanimate
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>
#include "zsim_hooks.h"

/* ------------------------------------------------------------------ */
/*  Arg parser                                                        */
/* ------------------------------------------------------------------ */
static int parse_int_arg(int argc, char** argv, const char* flag, int def) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return def;
}

/* ------------------------------------------------------------------ */
/*  LCG                                                               */
/* ------------------------------------------------------------------ */
static uint32_t bench_rand(uint32_t* s) {
    *s = *s * 1103515245 + 12345;
    return (*s >> 16) & 0x7FFF;
}

/* ------------------------------------------------------------------ */
/*  SPH constants                                                     */
/* ------------------------------------------------------------------ */
#define H           0.04
#define H2          (H * H)
#define REST_DENS   1000.0
#define GAS_K       2000.0
#define VISCOSITY   250.0
#define DT          0.0005
#define GRAVITY_Y  (-9.81)
#define DOMAIN_SIZE 1.0
#define DAMP        0.5
#define PI_VAL      3.14159265358979323846

#define GRID_SIDE   32
#define GRID_CELLS  (GRID_SIDE * GRID_SIDE * GRID_SIDE)
#define MAX_PER_CELL 64

typedef struct {
    double x, y, z;
    double vx, vy, vz;
    double fx, fy, fz;
    double density;
    double pressure;
} Particle;

static int g_grid[GRID_CELLS][MAX_PER_CELL];
static int g_grid_count[GRID_CELLS];

static double poly6_coeff;
static double spiky_coeff;
static double visc_coeff;

static void init_kernels(void) {
    poly6_coeff = 315.0 / (64.0 * PI_VAL * pow(H, 9));
    spiky_coeff = -45.0 / (PI_VAL * pow(H, 6));
    visc_coeff  =  45.0 / (PI_VAL * pow(H, 6));
}

static int grid_cell(double x, double y, double z) {
    int gx = (int)(x / DOMAIN_SIZE * GRID_SIDE);
    int gy = (int)(y / DOMAIN_SIZE * GRID_SIDE);
    int gz = (int)(z / DOMAIN_SIZE * GRID_SIDE);
    if (gx < 0) gx = 0;
    if (gx >= GRID_SIDE) gx = GRID_SIDE - 1;
    if (gy < 0) gy = 0;
    if (gy >= GRID_SIDE) gy = GRID_SIDE - 1;
    if (gz < 0) gz = 0;
    if (gz >= GRID_SIDE) gz = GRID_SIDE - 1;
    return gx * GRID_SIDE * GRID_SIDE + gy * GRID_SIDE + gz;
}

static void build_grid(Particle* p, int np) {
    memset(g_grid_count, 0, sizeof(g_grid_count));
    for (int i = 0; i < np; i++) {
        int c = grid_cell(p[i].x, p[i].y, p[i].z);
        if (g_grid_count[c] < MAX_PER_CELL) {
            g_grid[c][g_grid_count[c]] = i;
            g_grid_count[c]++;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Compute density for particles in [start, end)                     */
/* ------------------------------------------------------------------ */
static void compute_density(Particle* p, int start, int end) {
    for (int i = start; i < end; i++) {
        double rho = 0.0;
        int ci = grid_cell(p[i].x, p[i].y, p[i].z);
        int gx = ci / (GRID_SIDE * GRID_SIDE);
        int gy = (ci / GRID_SIDE) % GRID_SIDE;
        int gz = ci % GRID_SIDE;

        for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
        for (int dz = -1; dz <= 1; dz++) {
            int nx = gx + dx, ny = gy + dy, nz = gz + dz;
            if (nx < 0 || nx >= GRID_SIDE || ny < 0 || ny >= GRID_SIDE ||
                nz < 0 || nz >= GRID_SIDE) continue;
            int nc = nx * GRID_SIDE * GRID_SIDE + ny * GRID_SIDE + nz;
            for (int k = 0; k < g_grid_count[nc]; k++) {
                int j = g_grid[nc][k];
                double ddx = p[i].x - p[j].x;
                double ddy = p[i].y - p[j].y;
                double ddz = p[i].z - p[j].z;
                double r2 = ddx * ddx + ddy * ddy + ddz * ddz;
                if (r2 < H2) {
                    double w = H2 - r2;
                    rho += poly6_coeff * w * w * w;
                }
            }
        }
        p[i].density = rho;
        p[i].pressure = GAS_K * (rho - REST_DENS);
    }
}

/* ------------------------------------------------------------------ */
/*  Compute pressure + viscosity forces for particles [start, end)    */
/* ------------------------------------------------------------------ */
static void compute_forces(Particle* p, int start, int end) {
    for (int i = start; i < end; i++) {
        double fx = 0.0, fy = 0.0, fz = 0.0;
        int ci = grid_cell(p[i].x, p[i].y, p[i].z);
        int gx = ci / (GRID_SIDE * GRID_SIDE);
        int gy = (ci / GRID_SIDE) % GRID_SIDE;
        int gz = ci % GRID_SIDE;

        for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
        for (int dz = -1; dz <= 1; dz++) {
            int nx = gx + dx, ny = gy + dy, nz = gz + dz;
            if (nx < 0 || nx >= GRID_SIDE || ny < 0 || ny >= GRID_SIDE ||
                nz < 0 || nz >= GRID_SIDE) continue;
            int nc = nx * GRID_SIDE * GRID_SIDE + ny * GRID_SIDE + nz;
            for (int k = 0; k < g_grid_count[nc]; k++) {
                int j = g_grid[nc][k];
                if (j == i) continue;
                double ddx = p[i].x - p[j].x;
                double ddy = p[i].y - p[j].y;
                double ddz = p[i].z - p[j].z;
                double r2 = ddx * ddx + ddy * ddy + ddz * ddz;
                if (r2 < H2 && r2 > 1e-12) {
                    double r = sqrt(r2);
                    double hr = H - r;

                    double pterm = spiky_coeff * hr * hr / r *
                                   (p[i].pressure + p[j].pressure) /
                                   (2.0 * p[j].density);
                    fx += pterm * ddx;
                    fy += pterm * ddy;
                    fz += pterm * ddz;

                    double vterm = visc_coeff * hr * VISCOSITY / p[j].density;
                    fx += vterm * (p[j].vx - p[i].vx);
                    fy += vterm * (p[j].vy - p[i].vy);
                    fz += vterm * (p[j].vz - p[i].vz);
                }
            }
        }
        p[i].fx = fx;
        p[i].fy = fy + GRAVITY_Y * p[i].density;
        p[i].fz = fz;
    }
}

/* ------------------------------------------------------------------ */
/*  Leapfrog integration + boundary clamping                          */
/* ------------------------------------------------------------------ */
static void integrate(Particle* p, int start, int end) {
    for (int i = start; i < end; i++) {
        double inv_rho = 1.0 / p[i].density;
        p[i].vx += DT * p[i].fx * inv_rho;
        p[i].vy += DT * p[i].fy * inv_rho;
        p[i].vz += DT * p[i].fz * inv_rho;
        p[i].x += DT * p[i].vx;
        p[i].y += DT * p[i].vy;
        p[i].z += DT * p[i].vz;

        if (p[i].x < 0.0) { p[i].x = 0.0; p[i].vx *= -DAMP; }
        if (p[i].x > DOMAIN_SIZE) { p[i].x = DOMAIN_SIZE; p[i].vx *= -DAMP; }
        if (p[i].y < 0.0) { p[i].y = 0.0; p[i].vy *= -DAMP; }
        if (p[i].y > DOMAIN_SIZE) { p[i].y = DOMAIN_SIZE; p[i].vy *= -DAMP; }
        if (p[i].z < 0.0) { p[i].z = 0.0; p[i].vz *= -DAMP; }
        if (p[i].z > DOMAIN_SIZE) { p[i].z = DOMAIN_SIZE; p[i].vz *= -DAMP; }
    }
}

/* ------------------------------------------------------------------ */
/*  Worker thread context                                             */
/* ------------------------------------------------------------------ */
typedef struct {
    int id;
    int start, end;
    int num_steps;
} WorkCtx;

static Particle* g_particles;
static int g_num_particles;
static pthread_barrier_t g_barrier;

static void* worker(void* arg) {
    WorkCtx* ctx = (WorkCtx*)arg;

    for (int step = 0; step < ctx->num_steps; step++) {
        /* Thread 0 builds the grid; others wait */
        if (ctx->id == 0)
            build_grid(g_particles, g_num_particles);
        pthread_barrier_wait(&g_barrier);

        /* Phase 1: density */
        compute_density(g_particles, ctx->start, ctx->end);
        pthread_barrier_wait(&g_barrier);

        /* Phase 2: forces */
        compute_forces(g_particles, ctx->start, ctx->end);
        pthread_barrier_wait(&g_barrier);

        /* Phase 3: integrate */
        integrate(g_particles, ctx->start, ctx->end);
        pthread_barrier_wait(&g_barrier);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */
int main(int argc, char** argv) {
    g_num_particles = parse_int_arg(argc, argv, "--size", 4096);
    int num_threads = parse_int_arg(argc, argv, "--threads", 1);
    int num_steps   = parse_int_arg(argc, argv, "--steps", 5);

    printf("SPH Fluid Simulation -- particles=%d steps=%d threads=%d\n",
           g_num_particles, num_steps, num_threads);

    init_kernels();

    g_particles = (Particle*)calloc((size_t)g_num_particles, sizeof(Particle));
    if (!g_particles) { fprintf(stderr, "malloc failed\n"); return 1; }

    /* Initialize particles in a block near center of domain */
    uint32_t seed = 42;
    for (int i = 0; i < g_num_particles; i++) {
        g_particles[i].x = 0.2 + (double)(bench_rand(&seed)) / 32768.0 * 0.6;
        g_particles[i].y = 0.4 + (double)(bench_rand(&seed)) / 32768.0 * 0.5;
        g_particles[i].z = 0.2 + (double)(bench_rand(&seed)) / 32768.0 * 0.6;
    }

    pthread_barrier_init(&g_barrier, NULL, (unsigned)num_threads);

    /* ---- ROI begin ---- */
    zsim_roi_begin();

    pthread_t* threads = (pthread_t*)malloc((size_t)num_threads * sizeof(pthread_t));
    WorkCtx*   ctxs    = (WorkCtx*)malloc((size_t)num_threads * sizeof(WorkCtx));

    int chunk = g_num_particles / num_threads;
    for (int t = 0; t < num_threads; t++) {
        ctxs[t].id        = t;
        ctxs[t].start     = t * chunk;
        ctxs[t].end       = (t == num_threads - 1) ? g_num_particles : (t + 1) * chunk;
        ctxs[t].num_steps = num_steps;
        pthread_create(&threads[t], NULL, worker, &ctxs[t]);
    }
    for (int t = 0; t < num_threads; t++)
        pthread_join(threads[t], NULL);

    zsim_roi_end();
    /* ---- ROI end ---- */

    /* Checksum: sum of all positions and velocities */
    double checksum = 0.0;
    for (int i = 0; i < g_num_particles; i++) {
        checksum += g_particles[i].x + g_particles[i].y + g_particles[i].z;
        checksum += g_particles[i].vx + g_particles[i].vy + g_particles[i].vz;
    }

    printf("BENCH_CHECKSUM: %.6f\n", checksum);
    printf("BENCH_DONE\n");

    pthread_barrier_destroy(&g_barrier);
    free(ctxs);
    free(threads);
    free(g_particles);
    return 0;
}
