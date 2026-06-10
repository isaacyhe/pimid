/* babelstream.cpp — Self-contained OpenMP BabelStream implementation.
 *
 * Performs the 5 standard BabelStream kernels:
 *   Copy:  c[i] = a[i]
 *   Mul:   b[i] = scalar * c[i]
 *   Add:   c[i] = a[i] + b[i]
 *   Triad: a[i] = b[i] + scalar * c[i]
 *   Dot:   sum  = SUM(a[i] * b[i])
 *
 * All arrays use double precision (standard BabelStream).
 * Reports bandwidth in MB/s for each kernel.
 *
 * Usage: babelstream [--arraysize N] [--numtimes T]
 *   Default: --arraysize 33554432 (2^25), --numtimes 100
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <omp.h>
#include "zsim_hooks.h"

static const int DEFAULT_ARRAYSIZE = 33554432;  /* 2^25 */
static const int DEFAULT_NUMTIMES  = 100;

static int parse_int_arg(int argc, char** argv, const char* flag, int def) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return def;
}

int main(int argc, char* argv[]) {
    int arraysize = parse_int_arg(argc, argv, "--arraysize", DEFAULT_ARRAYSIZE);
    int numtimes  = parse_int_arg(argc, argv, "--numtimes",  DEFAULT_NUMTIMES);

    if (arraysize <= 0 || numtimes <= 0) {
        fprintf(stderr, "Error: arraysize and numtimes must be positive\n");
        return 1;
    }

    printf("BabelStream (PIMID OpenMP)\n");
    printf("Array size: %d (%.1f MB per array)\n",
           arraysize, (double)arraysize * sizeof(double) / 1.0e6);
    printf("Num times:  %d\n", numtimes);
    printf("Precision:  double\n");
    printf("Num threads: %d\n", omp_get_max_threads());

    /* Allocate arrays */
    double* a = (double*)malloc(arraysize * sizeof(double));
    double* b = (double*)malloc(arraysize * sizeof(double));
    double* c = (double*)malloc(arraysize * sizeof(double));
    if (!a || !b || !c) {
        fprintf(stderr, "Error: malloc failed for arraysize=%d\n", arraysize);
        return 1;
    }

    /* Initialize: a=0.1, b=0.2, c=0.0 */
    const double startA = 0.1;
    const double startB = 0.2;
    const double startC = 0.0;
    const double scalar = 0.4;

    #pragma omp parallel for
    for (int i = 0; i < arraysize; i++) {
        a[i] = startA;
        b[i] = startB;
        c[i] = startC;
    }

    /* Timing arrays: min time for each kernel */
    double copy_min  = DBL_MAX;
    double mul_min   = DBL_MAX;
    double add_min   = DBL_MAX;
    double triad_min = DBL_MAX;
    double dot_min   = DBL_MAX;

    double copy_max  = 0.0, mul_max  = 0.0, add_max  = 0.0;
    double triad_max = 0.0, dot_max  = 0.0;
    double copy_avg  = 0.0, mul_avg  = 0.0, add_avg  = 0.0;
    double triad_avg = 0.0, dot_avg  = 0.0;

    double dot_sum = 0.0;

    /* Main benchmark loop */
    zsim_roi_begin();

    for (int t = 0; t < numtimes; t++) {
        double t0, t1, elapsed;

        /* Copy: c[i] = a[i] */
        t0 = omp_get_wtime();
        #pragma omp parallel for
        for (int i = 0; i < arraysize; i++)
            c[i] = a[i];
        t1 = omp_get_wtime();
        elapsed = t1 - t0;
        if (elapsed < copy_min) copy_min = elapsed;
        if (elapsed > copy_max) copy_max = elapsed;
        copy_avg += elapsed;

        /* Mul: b[i] = scalar * c[i] */
        t0 = omp_get_wtime();
        #pragma omp parallel for
        for (int i = 0; i < arraysize; i++)
            b[i] = scalar * c[i];
        t1 = omp_get_wtime();
        elapsed = t1 - t0;
        if (elapsed < mul_min) mul_min = elapsed;
        if (elapsed > mul_max) mul_max = elapsed;
        mul_avg += elapsed;

        /* Add: c[i] = a[i] + b[i] */
        t0 = omp_get_wtime();
        #pragma omp parallel for
        for (int i = 0; i < arraysize; i++)
            c[i] = a[i] + b[i];
        t1 = omp_get_wtime();
        elapsed = t1 - t0;
        if (elapsed < add_min) add_min = elapsed;
        if (elapsed > add_max) add_max = elapsed;
        add_avg += elapsed;

        /* Triad: a[i] = b[i] + scalar * c[i] */
        t0 = omp_get_wtime();
        #pragma omp parallel for
        for (int i = 0; i < arraysize; i++)
            a[i] = b[i] + scalar * c[i];
        t1 = omp_get_wtime();
        elapsed = t1 - t0;
        if (elapsed < triad_min) triad_min = elapsed;
        if (elapsed > triad_max) triad_max = elapsed;
        triad_avg += elapsed;

        /* Dot: sum = SUM(a[i] * b[i]) */
        dot_sum = 0.0;
        t0 = omp_get_wtime();
        #pragma omp parallel for reduction(+:dot_sum)
        for (int i = 0; i < arraysize; i++)
            dot_sum += a[i] * b[i];
        t1 = omp_get_wtime();
        elapsed = t1 - t0;
        if (elapsed < dot_min) dot_min = elapsed;
        if (elapsed > dot_max) dot_max = elapsed;
        dot_avg += elapsed;
    }

    zsim_roi_end();

    /* Compute averages */
    copy_avg  /= numtimes;
    mul_avg   /= numtimes;
    add_avg   /= numtimes;
    triad_avg /= numtimes;
    dot_avg   /= numtimes;

    /* Bandwidth calculations:
     * Copy:  reads + writes = 2 * sizeof(double) * N bytes
     * Mul:   reads + writes = 2 * sizeof(double) * N bytes
     * Add:   reads + writes = 3 * sizeof(double) * N bytes
     * Triad: reads + writes = 3 * sizeof(double) * N bytes
     * Dot:   reads          = 2 * sizeof(double) * N bytes */
    double copy_bytes  = 2.0 * sizeof(double) * arraysize;
    double mul_bytes   = 2.0 * sizeof(double) * arraysize;
    double add_bytes   = 3.0 * sizeof(double) * arraysize;
    double triad_bytes = 3.0 * sizeof(double) * arraysize;
    double dot_bytes   = 2.0 * sizeof(double) * arraysize;

    printf("\n");
    printf("Function    MBytes/s    Min (s)       Max (s)       Avg (s)\n");
    printf("Copy      %12.1f %12.6f  %12.6f  %12.6f\n",
           1.0e-6 * copy_bytes / copy_min, copy_min, copy_max, copy_avg);
    printf("Mul       %12.1f %12.6f  %12.6f  %12.6f\n",
           1.0e-6 * mul_bytes / mul_min, mul_min, mul_max, mul_avg);
    printf("Add       %12.1f %12.6f  %12.6f  %12.6f\n",
           1.0e-6 * add_bytes / add_min, add_min, add_max, add_avg);
    printf("Triad     %12.1f %12.6f  %12.6f  %12.6f\n",
           1.0e-6 * triad_bytes / triad_min, triad_min, triad_max, triad_avg);
    printf("Dot       %12.1f %12.6f  %12.6f  %12.6f\n",
           1.0e-6 * dot_bytes / dot_min, dot_min, dot_max, dot_avg);

    /* Validation: compute expected values after numtimes iterations.
     * Initial: a=0.1, b=0.2, c=0.0, scalar=0.4
     * Each iteration:
     *   c = a               → c = a_prev
     *   b = scalar * c      → b = scalar * a_prev
     *   c = a + b           → c = a_prev + scalar * a_prev = a_prev * (1 + scalar)
     *   a = b + scalar * c  → a = scalar*a_prev + scalar*a_prev*(1+scalar)
     *                        = a_prev * scalar * (2 + scalar)
     *   dot = a * b (not cumulative, just last iteration) */
    double gold_a = startA;
    double gold_b = startB;
    double gold_c = startC;
    for (int t = 0; t < numtimes; t++) {
        gold_c = gold_a;
        gold_b = scalar * gold_c;
        gold_c = gold_a + gold_b;
        gold_a = gold_b + scalar * gold_c;
    }
    double gold_dot = gold_a * gold_b * arraysize;

    /* Check results (allow 1% tolerance for floating point accumulation) */
    double err_a = fabs((a[0] - gold_a) / gold_a);
    double err_b = fabs((b[0] - gold_b) / gold_b);
    double err_c = fabs((c[0] - gold_c) / gold_c);
    double err_dot = (gold_dot != 0.0) ? fabs((dot_sum - gold_dot) / gold_dot) : fabs(dot_sum);

    int pass = 1;
    if (err_a > 0.01) { printf("VALIDATION FAILED: a[0]=%.6e expected=%.6e err=%.2e\n", a[0], gold_a, err_a); pass = 0; }
    if (err_b > 0.01) { printf("VALIDATION FAILED: b[0]=%.6e expected=%.6e err=%.2e\n", b[0], gold_b, err_b); pass = 0; }
    if (err_c > 0.01) { printf("VALIDATION FAILED: c[0]=%.6e expected=%.6e err=%.2e\n", c[0], gold_c, err_c); pass = 0; }
    if (err_dot > 0.01) { printf("VALIDATION FAILED: dot=%.6e expected=%.6e err=%.2e\n", dot_sum, gold_dot, err_dot); pass = 0; }
    if (pass) printf("VALIDATION PASSED\n");

    printf("BENCH_DONE\n");

    free(a);
    free(b);
    free(c);
    return 0;
}
