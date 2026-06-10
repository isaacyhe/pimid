/* cholesky.c -- Dense Cholesky Factorization (A = L * L^T)
 * SPLASH-3 style: pthreads parallel block-column method.
 * For each column k: factor diagonal, solve column panel below,
 * then parallel update of trailing submatrix. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include "zsim_hooks.h"

#define DEFAULT_SIZE    256
#define DEFAULT_THREADS 4
#define BLOCK_SIZE      32

static int parse_int_arg(int argc, char** argv, const char* flag, int def) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return def;
}

static uint32_t bench_rand(uint32_t* s) {
    *s = *s * 1103515245 + 12345;
    return (*s >> 16) & 0x7FFF;
}

/* Shared state */
static double* A;
static int N, nthreads;
static pthread_barrier_t barrier;

typedef struct {
    int tid;
} ThreadArg;

/* Factor the diagonal block: scalar Cholesky on A[k..k+bs-1, k..k+bs-1] */
static void factor_diagonal(int k, int bs) {
    for (int j = k; j < k + bs; j++) {
        double sum = A[j * N + j];
        for (int p = k; p < j; p++)
            sum -= A[j * N + p] * A[j * N + p];
        if (sum <= 0.0) sum = 1e-10;
        A[j * N + j] = sqrt(sum);
        double diag_inv = 1.0 / A[j * N + j];
        for (int i = j + 1; i < k + bs; i++) {
            double s = A[i * N + j];
            for (int p = k; p < j; p++)
                s -= A[i * N + p] * A[j * N + p];
            A[i * N + j] = s * diag_inv;
        }
    }
}

/* Solve column panel: A[k+bs..N-1, k..k+bs-1] using the diagonal block */
static void solve_panel(int k, int bs, int row_lo, int row_hi) {
    for (int j = k; j < k + bs; j++) {
        double diag_inv = 1.0 / A[j * N + j];
        for (int i = row_lo; i < row_hi; i++) {
            double s = A[i * N + j];
            for (int p = k; p < j; p++)
                s -= A[i * N + p] * A[j * N + p];
            A[i * N + j] = s * diag_inv;
        }
    }
}

/* Update trailing submatrix: A[i,j] -= sum_p A[i,k+p]*A[j,k+p] for p in block */
static void update_trailing(int k, int bs, int row_lo, int row_hi) {
    int trail_start = k + bs;
    for (int i = row_lo; i < row_hi; i++) {
        for (int j = trail_start; j <= i; j++) {
            double s = 0.0;
            for (int p = k; p < k + bs; p++)
                s += A[i * N + p] * A[j * N + p];
            A[i * N + j] -= s;
        }
    }
}

static void* worker(void* arg) {
    int tid = ((ThreadArg*)arg)->tid;

    for (int k = 0; k < N; k += BLOCK_SIZE) {
        int bs = (k + BLOCK_SIZE <= N) ? BLOCK_SIZE : (N - k);

        /* Phase 1: factor diagonal block (thread 0 only) */
        if (tid == 0)
            factor_diagonal(k, bs);
        pthread_barrier_wait(&barrier);

        /* Phase 2: solve column panel in parallel */
        int trail_start = k + bs;
        int trail_rows = N - trail_start;
        if (trail_rows > 0) {
            int lo = trail_start + (trail_rows * tid) / nthreads;
            int hi = trail_start + (trail_rows * (tid + 1)) / nthreads;
            solve_panel(k, bs, lo, hi);
        }
        pthread_barrier_wait(&barrier);

        /* Phase 3: update trailing submatrix in parallel */
        if (trail_rows > 0) {
            int lo = trail_start + (trail_rows * tid) / nthreads;
            int hi = trail_start + (trail_rows * (tid + 1)) / nthreads;
            update_trailing(k, bs, lo, hi);
        }
        pthread_barrier_wait(&barrier);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    N = parse_int_arg(argc, argv, "--size", DEFAULT_SIZE);
    nthreads = parse_int_arg(argc, argv, "--threads", DEFAULT_THREADS);

    A = (double*)malloc((size_t)N * N * sizeof(double));
    if (!A) { fprintf(stderr, "malloc failed\n"); return 1; }

    /* Build symmetric positive definite matrix: A = B * B^T + N*I
     * where B has random entries from LCG */
    uint32_t seed = 42;
    double* B = (double*)malloc((size_t)N * N * sizeof(double));
    if (!B) { fprintf(stderr, "malloc failed\n"); return 1; }
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            B[i * N + j] = 1.0 + 4.0 * bench_rand(&seed) / 32768.0;

    for (int i = 0; i < N; i++) {
        for (int j = 0; j <= i; j++) {
            double s = 0.0;
            for (int p = 0; p < N; p++)
                s += B[i * N + p] * B[j * N + p];
            if (i == j) s += (double)N;
            A[i * N + j] = s;
            A[j * N + i] = s;
        }
    }
    free(B);

    pthread_barrier_init(&barrier, NULL, (unsigned)nthreads);
    pthread_t* threads = (pthread_t*)malloc((size_t)nthreads * sizeof(pthread_t));
    ThreadArg* args = (ThreadArg*)malloc((size_t)nthreads * sizeof(ThreadArg));

    zsim_roi_begin();
    for (int t = 1; t < nthreads; t++) {
        args[t].tid = t;
        pthread_create(&threads[t], NULL, worker, &args[t]);
    }
    args[0].tid = 0;
    worker(&args[0]);
    for (int t = 1; t < nthreads; t++)
        pthread_join(threads[t], NULL);
    zsim_roi_end();

    /* Checksum: sum of diagonal of L (lower triangle stored in A) */
    double checksum = 0.0;
    for (int i = 0; i < N; i++)
        checksum += A[i * N + i];
    printf("BENCH_CHECKSUM: %f\n", checksum);
    printf("BENCH_DONE\n");

    pthread_barrier_destroy(&barrier);
    free(threads); free(args);
    free(A);
    return 0;
}
