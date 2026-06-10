/* lu.c -- Block LU Decomposition (SPLASH-3 style)
 *
 * Block LU factorization on dense NxN matrix. Process blocks of size B:
 *   1. Factor diagonal block (serial)
 *   2. Update row panel below diagonal block
 *   3. Update column panel right of diagonal block
 *   4. Update trailing submatrix (DGEMM-like, parallel)
 * Block-strided access pattern (regular within block, stride between blocks).
 *
 * Compile: g++ -std=c++11 -O2 -Wall -I<hooks-path> -lpthread -lm lu.c -o lu
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>
#include "zsim_hooks.h"

#define DEFAULT_SIZE    256
#define DEFAULT_THREADS 4
#define BLOCK_SIZE      16

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
/*  Shared state                                                       */
/* ------------------------------------------------------------------ */
static double* A;       /* NxN matrix, row-major */
static int N;
static int num_threads;
static int B;           /* effective block size */

/* Per-stage parallel update info */
static int cur_diag;        /* current diagonal block index (in block coords) */
static int num_trail_blocks; /* number of trailing block updates to do */
static pthread_barrier_t barrier;

/* ------------------------------------------------------------------ */
/*  Factor diagonal block in-place (no pivoting, Doolittle)            */
/* ------------------------------------------------------------------ */
static void factor_diag_block(int bi) {
    int r0 = bi * B;
    for (int k = 0; k < B && (r0 + k) < N; k++) {
        int gk = r0 + k;
        double pivot = A[gk * N + gk];
        if (fabs(pivot) < 1e-12) continue; /* skip near-zero pivots */
        for (int i = k + 1; i < B && (r0 + i) < N; i++) {
            int gi = r0 + i;
            A[gi * N + gk] /= pivot;
            for (int j = k + 1; j < B && (r0 + j) < N; j++) {
                int gj = r0 + j;
                A[gi * N + gj] -= A[gi * N + gk] * A[gk * N + gj];
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Update row panel (blocks below diagonal in the same column)        */
/* ------------------------------------------------------------------ */
static void update_row_panel(int bi, int bj) {
    /* bj is the block row below diagonal block bi (same block column) */
    int r0 = bj * B;
    int c0 = bi * B;
    for (int k = 0; k < B && (c0 + k) < N; k++) {
        int gk = c0 + k;
        double pivot = A[gk * N + gk];
        if (fabs(pivot) < 1e-12) continue;
        for (int i = 0; i < B && (r0 + i) < N; i++) {
            int gi = r0 + i;
            A[gi * N + gk] /= pivot;
            for (int j = k + 1; j < B && (c0 + j) < N; j++) {
                int gj = c0 + j;
                A[gi * N + gj] -= A[gi * N + gk] * A[gk * N + gj];
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Update column panel (blocks right of diagonal in the same row)     */
/* ------------------------------------------------------------------ */
static void update_col_panel(int bi, int bk) {
    /* bk is the block column right of diagonal block bi (same block row) */
    int r0 = bi * B;
    int c0 = bk * B;
    for (int k = 0; k < B && (r0 + k) < N; k++) {
        int gk = r0 + k;
        for (int j = 0; j < B && (c0 + j) < N; j++) {
            int gj = c0 + j;
            for (int m = 0; m < k; m++) {
                int gm = r0 + m;
                A[gk * N + gj] -= A[gk * N + gm] * A[gm * N + gj];
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Trailing submatrix update: A[bi][bj] -= A[bi][diag] * A[diag][bj] */
/* ------------------------------------------------------------------ */
static void update_trailing(int diag, int bi, int bj) {
    int r0 = bi * B;
    int c0 = bj * B;
    int d0 = diag * B;
    for (int i = 0; i < B && (r0 + i) < N; i++) {
        int gi = r0 + i;
        for (int j = 0; j < B && (c0 + j) < N; j++) {
            int gj = c0 + j;
            double sum = 0.0;
            for (int k = 0; k < B && (d0 + k) < N; k++) {
                int gk = d0 + k;
                sum += A[gi * N + gk] * A[gk * N + gj];
            }
            A[gi * N + gj] -= sum;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Parallel trailing update worker                                    */
/* ------------------------------------------------------------------ */
typedef struct {
    int tid;
} trail_arg_t;

static int nb;  /* number of blocks */

/* Linearized (bi,bj) pairs for trailing updates */
static int* trail_bi;
static int* trail_bj;
static int trail_count;

static void* trail_worker(void* arg) {
    int tid = ((trail_arg_t*)arg)->tid;
    int chunk = (trail_count + num_threads - 1) / num_threads;
    int lo = tid * chunk;
    int hi = lo + chunk;
    if (hi > trail_count) hi = trail_count;

    for (int idx = lo; idx < hi; idx++)
        update_trailing(cur_diag, trail_bi[idx], trail_bj[idx]);

    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */
int main(int argc, char** argv) {
    N = parse_int_arg(argc, argv, "--size", DEFAULT_SIZE);
    num_threads = parse_int_arg(argc, argv, "--threads", DEFAULT_THREADS);
    B = BLOCK_SIZE;
    if (B > N) B = N;

    nb = (N + B - 1) / B;

    printf("LU Decomposition (block, no pivot) -- N=%d, B=%d, threads=%d\n", N, B, num_threads);

    A = (double*)malloc((size_t)N * N * sizeof(double));
    if (!A) { fprintf(stderr, "malloc failed\n"); return 1; }

    /* Initialize with deterministic data, add diagonal dominance for stability */
    uint32_t seed = 42;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            A[i * N + j] = (double)bench_rand(&seed) / 32768.0 - 0.5;
        }
        A[i * N + i] += (double)N; /* diagonal dominance */
    }

    /* Allocate trailing update index arrays */
    trail_bi = (int*)malloc((size_t)nb * nb * sizeof(int));
    trail_bj = (int*)malloc((size_t)nb * nb * sizeof(int));

    pthread_t* threads = (pthread_t*)malloc((size_t)num_threads * sizeof(pthread_t));
    trail_arg_t* targs = (trail_arg_t*)malloc((size_t)num_threads * sizeof(trail_arg_t));
    for (int t = 0; t < num_threads; t++) targs[t].tid = t;

    /* ---- ROI begin ---- */
    zsim_roi_begin();

    for (int d = 0; d < nb; d++) {
        /* 1. Factor diagonal block (serial) */
        factor_diag_block(d);

        /* 2. Update row panel (serial, below diagonal) */
        for (int bi = d + 1; bi < nb; bi++)
            update_row_panel(d, bi);

        /* 3. Update column panel (serial, right of diagonal) */
        for (int bj = d + 1; bj < nb; bj++)
            update_col_panel(d, bj);

        /* 4. Update trailing submatrix (parallel) */
        trail_count = 0;
        for (int bi = d + 1; bi < nb; bi++) {
            for (int bj = d + 1; bj < nb; bj++) {
                trail_bi[trail_count] = bi;
                trail_bj[trail_count] = bj;
                trail_count++;
            }
        }
        cur_diag = d;

        if (trail_count > 0) {
            for (int t = 1; t < num_threads; t++)
                pthread_create(&threads[t], NULL, trail_worker, &targs[t]);
            trail_worker(&targs[0]);
            for (int t = 1; t < num_threads; t++)
                pthread_join(threads[t], NULL);
        }
    }

    zsim_roi_end();
    /* ---- ROI end ---- */

    /* Checksum: sum of diagonal elements (should approximate original diagonal) */
    double checksum = 0.0;
    for (int i = 0; i < N; i++)
        checksum += A[i * N + i];

    printf("BENCH_CHECKSUM: %f\n", checksum);
    printf("BENCH_DONE\n");

    free(targs);
    free(threads);
    free(trail_bi);
    free(trail_bj);
    free(A);
    return 0;
}
