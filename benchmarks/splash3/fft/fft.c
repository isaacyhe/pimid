/* fft.c -- 1D Cooley-Tukey Radix-2 DIT FFT (SPLASH-3 style)
 *
 * Butterfly FFT with power-of-2 strided access. Bit-reversal permutation,
 * then log2(N) butterfly stages. Pthreads: each thread handles a portion
 * of the butterfly operations per stage, with a barrier between stages.
 *
 * Compile: g++ -std=c++11 -O2 -Wall -I<hooks-path> -lpthread -lm fft.c -o fft
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>
#include "zsim_hooks.h"

#define DEFAULT_SIZE    65536
#define DEFAULT_THREADS 4

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/*  Complex number type                                                */
/* ------------------------------------------------------------------ */
typedef struct { double re, im; } complex_t;

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
static complex_t* data;
static int N;
static int num_threads;
static int log2N;
static pthread_barrier_t barrier;

/* ------------------------------------------------------------------ */
/*  Bit-reversal permutation (serial -- small fraction of work)        */
/* ------------------------------------------------------------------ */
static void bit_reverse_permute(complex_t* x, int n, int logn) {
    for (int i = 0; i < n; i++) {
        int rev = 0;
        int tmp = i;
        for (int b = 0; b < logn; b++) {
            rev = (rev << 1) | (tmp & 1);
            tmp >>= 1;
        }
        if (rev > i) {
            complex_t t = x[i];
            x[i] = x[rev];
            x[rev] = t;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Worker thread: butterfly stages                                    */
/* ------------------------------------------------------------------ */
static void* fft_worker(void* arg) {
    int tid = (int)(intptr_t)arg;

    for (int stage = 0; stage < log2N; stage++) {
        int half = 1 << stage;          /* butterfly distance */
        int full = half << 1;           /* group size = 2^(stage+1) */
        int num_groups = N / full;      /* total number of butterfly groups */

        /* Distribute groups among threads */
        int groups_per = (num_groups + num_threads - 1) / num_threads;
        int g_start = tid * groups_per;
        int g_end = g_start + groups_per;
        if (g_end > num_groups) g_end = num_groups;

        for (int g = g_start; g < g_end; g++) {
            int base = g * full;
            for (int k = 0; k < half; k++) {
                double angle = -M_PI * k / half;
                double wr = cos(angle);
                double wi = sin(angle);

                int i0 = base + k;
                int i1 = base + k + half;

                double tre = wr * data[i1].re - wi * data[i1].im;
                double tim = wr * data[i1].im + wi * data[i1].re;

                data[i1].re = data[i0].re - tre;
                data[i1].im = data[i0].im - tim;
                data[i0].re = data[i0].re + tre;
                data[i0].im = data[i0].im + tim;
            }
        }

        pthread_barrier_wait(&barrier);
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */
int main(int argc, char** argv) {
    N = parse_int_arg(argc, argv, "--size", DEFAULT_SIZE);
    num_threads = parse_int_arg(argc, argv, "--threads", DEFAULT_THREADS);

    /* Verify power of 2 */
    if (N <= 0 || (N & (N - 1)) != 0) {
        fprintf(stderr, "Error: --size must be a power of 2 (got %d)\n", N);
        return 1;
    }

    log2N = 0;
    { int tmp = N; while (tmp > 1) { log2N++; tmp >>= 1; } }

    printf("FFT (Cooley-Tukey radix-2 DIT) -- N=%d, threads=%d\n", N, num_threads);

    /* Allocate and initialize with deterministic data */
    data = (complex_t*)malloc((size_t)N * sizeof(complex_t));
    if (!data) { fprintf(stderr, "malloc failed\n"); return 1; }

    uint32_t seed = 42;
    for (int i = 0; i < N; i++) {
        data[i].re = (double)bench_rand(&seed) / 32768.0 - 0.5;
        data[i].im = (double)bench_rand(&seed) / 32768.0 - 0.5;
    }

    /* Bit-reversal (serial, outside ROI is fine -- small work) */
    bit_reverse_permute(data, N, log2N);

    /* Setup barrier */
    pthread_barrier_init(&barrier, NULL, (unsigned)num_threads);

    /* Create threads */
    pthread_t* threads = (pthread_t*)malloc((size_t)num_threads * sizeof(pthread_t));

    /* ---- ROI begin ---- */
    zsim_roi_begin();

    for (int t = 1; t < num_threads; t++)
        pthread_create(&threads[t], NULL, fft_worker, (void*)(intptr_t)t);
    fft_worker((void*)(intptr_t)0);

    for (int t = 1; t < num_threads; t++)
        pthread_join(threads[t], NULL);

    zsim_roi_end();
    /* ---- ROI end ---- */

    /* Checksum: sum of magnitudes */
    double checksum = 0.0;
    for (int i = 0; i < N; i++)
        checksum += sqrt(data[i].re * data[i].re + data[i].im * data[i].im);

    printf("BENCH_CHECKSUM: %f\n", checksum);
    printf("BENCH_DONE\n");

    pthread_barrier_destroy(&barrier);
    free(threads);
    free(data);
    return 0;
}
