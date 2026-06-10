/* reduction.c — Streaming sum reduction: result = sum(a[i])
 * Serial baseline. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zsim_hooks.h"

#define DEFAULT_SIZE 16384

static int parse_int_arg(int argc, char** argv, const char* flag, int def) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return def;
}

int main(int argc, char* argv[]) {
    int N = parse_int_arg(argc, argv, "--size", DEFAULT_SIZE);

    float* a = (float*)malloc(N * sizeof(float));
    if (!a) { fprintf(stderr, "malloc failed\n"); return 1; }

    for (int i = 0; i < N; i++)
        a[i] = (float)(i % 100) * 0.01f;

    zsim_roi_begin();
    double sum = 0.0;
    for (int i = 0; i < N; i++)
        sum += a[i];
    zsim_roi_end();

    printf("BENCH_CHECKSUM: %f\n", sum);
    printf("BENCH_DONE\n");

    free(a);
    return 0;
}
