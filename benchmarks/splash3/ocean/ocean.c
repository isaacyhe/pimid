/* ocean.c -- SOR relaxation on 2D grid (SPLASH-3 style)
 *
 * Red-black Successive Over-Relaxation (SOR) for Laplace equation on
 * an NxN grid. Iterate until convergence (max residual < tolerance)
 * or max iterations. Uses 5-point stencil (center + 4 neighbors).
 * Pthreads: rows partitioned among threads, barrier between red/black.
 *
 * Compile: g++ -std=c++11 -O2 -Wall -I<hooks-path> -lpthread -lm ocean.c -o ocean
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>
#include "zsim_hooks.h"

#define DEFAULT_SIZE    128
#define DEFAULT_THREADS 4
#define MAX_ITERS       100
#define OMEGA           1.4     /* SOR relaxation factor */
#define TOLERANCE       1e-6

/* ------------------------------------------------------------------ */
/*  Arg parser                                                         */
/* ------------------------------------------------------------------ */
static int parse_int_arg(int argc, char** argv, const char* flag, int def) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return def;
}

/* ------------------------------------------------------------------ */
/*  Shared state                                                       */
/* ------------------------------------------------------------------ */
static double* grid;
static int N;
static int num_threads;
static int converged;
static double global_max_diff;
static pthread_barrier_t barrier;
static pthread_mutex_t diff_lock = PTHREAD_MUTEX_INITIALIZER;

/* ------------------------------------------------------------------ */
/*  Worker thread                                                      */
/* ------------------------------------------------------------------ */
static void* sor_worker(void* arg) {
    int tid = (int)(intptr_t)arg;

    /* Partition interior rows [1, N-2] among threads */
    int interior = N - 2;
    int chunk = (interior + num_threads - 1) / num_threads;
    int row_lo = 1 + tid * chunk;
    int row_hi = row_lo + chunk;
    if (row_hi > N - 1) row_hi = N - 1;

    for (int iter = 0; iter < MAX_ITERS; iter++) {
        double local_max = 0.0;

        /* Red phase: (i+j) even */
        for (int i = row_lo; i < row_hi; i++) {
            for (int j = 1; j < N - 1; j++) {
                if (((i + j) & 1) != 0) continue;
                double gs = 0.25 * (grid[(i - 1) * N + j] + grid[(i + 1) * N + j] +
                                    grid[i * N + (j - 1)] + grid[i * N + (j + 1)]);
                double old_val = grid[i * N + j];
                double new_val = old_val + OMEGA * (gs - old_val);
                double diff = fabs(new_val - old_val);
                if (diff > local_max) local_max = diff;
                grid[i * N + j] = new_val;
            }
        }

        pthread_barrier_wait(&barrier);

        /* Black phase: (i+j) odd */
        for (int i = row_lo; i < row_hi; i++) {
            for (int j = 1; j < N - 1; j++) {
                if (((i + j) & 1) != 1) continue;
                double gs = 0.25 * (grid[(i - 1) * N + j] + grid[(i + 1) * N + j] +
                                    grid[i * N + (j - 1)] + grid[i * N + (j + 1)]);
                double old_val = grid[i * N + j];
                double new_val = old_val + OMEGA * (gs - old_val);
                double diff = fabs(new_val - old_val);
                if (diff > local_max) local_max = diff;
                grid[i * N + j] = new_val;
            }
        }

        /* Reduce max diff across threads */
        pthread_mutex_lock(&diff_lock);
        if (local_max > global_max_diff) global_max_diff = local_max;
        pthread_mutex_unlock(&diff_lock);

        pthread_barrier_wait(&barrier);

        /* Thread 0 checks convergence */
        if (tid == 0) {
            if (global_max_diff < TOLERANCE) {
                converged = iter + 1;
            }
            global_max_diff = 0.0;
        }

        pthread_barrier_wait(&barrier);

        if (converged > 0) break;
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */
int main(int argc, char** argv) {
    N = parse_int_arg(argc, argv, "--size", DEFAULT_SIZE);
    num_threads = parse_int_arg(argc, argv, "--threads", DEFAULT_THREADS);

    printf("Ocean SOR (red-black) -- grid=%dx%d, threads=%d, omega=%.2f\n",
           N, N, num_threads, OMEGA);

    grid = (double*)calloc((size_t)N * N, sizeof(double));
    if (!grid) { fprintf(stderr, "malloc failed\n"); return 1; }

    /* Boundary conditions: top row = 100, bottom row = 0,
       left col = linear gradient, right col = linear gradient */
    for (int j = 0; j < N; j++) grid[j] = 100.0;                       /* top */
    for (int i = 0; i < N; i++) {
        double frac = (double)i / (double)(N - 1);
        grid[i * N + 0] = 100.0 * (1.0 - frac);       /* left */
        grid[i * N + (N - 1)] = 100.0 * (1.0 - frac);  /* right */
    }

    converged = 0;
    global_max_diff = 0.0;

    pthread_barrier_init(&barrier, NULL, (unsigned)num_threads);
    pthread_t* threads = (pthread_t*)malloc((size_t)num_threads * sizeof(pthread_t));

    /* ---- ROI begin ---- */
    zsim_roi_begin();

    for (int t = 1; t < num_threads; t++)
        pthread_create(&threads[t], NULL, sor_worker, (void*)(intptr_t)t);
    sor_worker((void*)(intptr_t)0);

    for (int t = 1; t < num_threads; t++)
        pthread_join(threads[t], NULL);

    zsim_roi_end();
    /* ---- ROI end ---- */

    if (converged > 0)
        printf("Converged after %d iterations\n", converged);
    else
        printf("Did not converge within %d iterations\n", MAX_ITERS);

    /* Checksum: sum of all grid values */
    double checksum = 0.0;
    for (int i = 0; i < N * N; i++) checksum += grid[i];

    printf("BENCH_CHECKSUM: %f\n", checksum);
    printf("BENCH_DONE\n");

    pthread_barrier_destroy(&barrier);
    free(threads);
    free(grid);
    return 0;
}
