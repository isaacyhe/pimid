/* histogram_omp.c — Histogram construction: random scatter into bins
 * OpenMP parallel version. Private histograms per thread, merged at end. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "zsim_hooks.h"

#define DEFAULT_SIZE 16384
#define DEFAULT_BINS 256

static uint32_t bench_rand(uint32_t* s) {
    *s = *s * 1103515245 + 12345;
    return (*s >> 16) & 0x7FFF;
}

static int parse_int_arg(int argc, char** argv, const char* flag, int def) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return def;
}

int main(int argc, char* argv[]) {
    int N = parse_int_arg(argc, argv, "--size", DEFAULT_SIZE);
    int nbins = parse_int_arg(argc, argv, "--bins", DEFAULT_BINS);
    uint32_t seed = 42;

    int* data = (int*)malloc(N * sizeof(int));
    int* hist = (int*)calloc(nbins, sizeof(int));
    if (!data || !hist) { fprintf(stderr, "malloc failed\n"); return 1; }

    for (int i = 0; i < N; i++)
        data[i] = bench_rand(&seed) % nbins;

    zsim_roi_begin();
    #pragma omp parallel
    {
        int* local_hist = (int*)calloc(nbins, sizeof(int));

        #pragma omp for
        for (int i = 0; i < N; i++)
            local_hist[data[i]]++;

        #pragma omp critical
        {
            for (int b = 0; b < nbins; b++)
                hist[b] += local_hist[b];
        }

        free(local_hist);
    }
    zsim_roi_end();

    long checksum = 0;
    for (int i = 0; i < nbins; i++) checksum += hist[i];
    printf("BENCH_CHECKSUM: %ld\n", checksum);
    printf("BENCH_DONE\n");

    free(data); free(hist);
    return 0;
}
