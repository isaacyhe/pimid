/*
 * Black-Scholes Option Pricing -- Closed-form European option pricing
 * Self-contained PARSEC-style benchmark with pthreads parallelism.
 *
 * Compile: g++ -std=c++11 -O2 -Wall -I<path-to-zsim/misc/hooks> -lpthread -lm blackscholes.c -o blackscholes
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
/*  Option parameters                                                 */
/* ------------------------------------------------------------------ */
typedef struct {
    double spot;       /* current stock price */
    double strike;     /* strike price */
    double rate;       /* risk-free interest rate */
    double vol;        /* volatility */
    double time;       /* time to expiration (years) */
    int    is_put;     /* 0 = call, 1 = put */
} OptionData;

/* ------------------------------------------------------------------ */
/*  Cumulative Normal Distribution -- Abramowitz & Stegun 26.2.17     */
/* ------------------------------------------------------------------ */
static double cnd(double x) {
    static const double a1 =  0.31938153;
    static const double a2 = -0.356563782;
    static const double a3 =  1.781477937;
    static const double a4 = -1.821255978;
    static const double a5 =  1.330274429;
    static const double rsqrt2pi = 0.39894228040143267793994605993438;

    double L = fabs(x);
    double K = 1.0 / (1.0 + 0.2316419 * L);
    double K2 = K * K;
    double K3 = K2 * K;
    double K4 = K3 * K;
    double K5 = K4 * K;

    double w = rsqrt2pi * exp(-0.5 * L * L);
    double cnd_val = 1.0 - w * (a1 * K + a2 * K2 + a3 * K3 + a4 * K4 + a5 * K5);

    if (x < 0.0)
        cnd_val = 1.0 - cnd_val;
    return cnd_val;
}

/* ------------------------------------------------------------------ */
/*  Black-Scholes formula                                             */
/* ------------------------------------------------------------------ */
static double black_scholes(const OptionData* opt) {
    double S = opt->spot;
    double X = opt->strike;
    double r = opt->rate;
    double v = opt->vol;
    double T = opt->time;

    double sqrt_T = sqrt(T);
    double d1 = (log(S / X) + (r + 0.5 * v * v) * T) / (v * sqrt_T);
    double d2 = d1 - v * sqrt_T;

    double Nd1 = cnd(d1);
    double Nd2 = cnd(d2);
    double exp_rT = exp(-r * T);

    if (opt->is_put) {
        return X * exp_rT * (1.0 - Nd2) - S * (1.0 - Nd1);
    } else {
        return S * Nd1 - X * exp_rT * Nd2;
    }
}

/* ------------------------------------------------------------------ */
/*  Thread context                                                    */
/* ------------------------------------------------------------------ */
typedef struct {
    int            id;
    int            start;
    int            end;
    OptionData*    options;
    double*        prices;
} ThreadCtx;

static void* worker(void* arg) {
    ThreadCtx* ctx = (ThreadCtx*)arg;
    for (int i = ctx->start; i < ctx->end; i++) {
        ctx->prices[i] = black_scholes(&ctx->options[i]);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */
int main(int argc, char** argv) {
    int num_options = parse_int_arg(argc, argv, "--size", 4096);
    int num_threads = parse_int_arg(argc, argv, "--threads", 1);

    printf("Black-Scholes Option Pricing -- options=%d threads=%d\n",
           num_options, num_threads);

    /* Allocate */
    OptionData* options = (OptionData*)malloc((size_t)num_options * sizeof(OptionData));
    double*     prices  = (double*)malloc((size_t)num_options * sizeof(double));
    if (!options || !prices) { fprintf(stderr, "malloc failed\n"); return 1; }

    /* Initialize options from LCG */
    uint32_t seed = 42;
    for (int i = 0; i < num_options; i++) {
        options[i].spot   = 30.0 + (double)(bench_rand(&seed) % 100);
        options[i].strike = 30.0 + (double)(bench_rand(&seed) % 100);
        options[i].rate   = 0.02 + (double)(bench_rand(&seed) % 80) / 1000.0;
        options[i].vol    = 0.10 + (double)(bench_rand(&seed) % 40) / 100.0;
        options[i].time   = 0.25 + (double)(bench_rand(&seed) % 40) / 10.0;
        options[i].is_put = (int)(bench_rand(&seed) & 1);
        prices[i] = 0.0;
    }

    /* ---- ROI begin ---- */
    zsim_roi_begin();

    pthread_t* threads = (pthread_t*)malloc((size_t)num_threads * sizeof(pthread_t));
    ThreadCtx* ctxs    = (ThreadCtx*)malloc((size_t)num_threads * sizeof(ThreadCtx));

    int chunk = num_options / num_threads;
    for (int t = 0; t < num_threads; t++) {
        ctxs[t].id      = t;
        ctxs[t].start   = t * chunk;
        ctxs[t].end     = (t == num_threads - 1) ? num_options : (t + 1) * chunk;
        ctxs[t].options = options;
        ctxs[t].prices  = prices;
        pthread_create(&threads[t], NULL, worker, &ctxs[t]);
    }
    for (int t = 0; t < num_threads; t++) {
        pthread_join(threads[t], NULL);
    }

    zsim_roi_end();
    /* ---- ROI end ---- */

    /* Checksum: sum of all prices */
    double checksum = 0.0;
    for (int i = 0; i < num_options; i++)
        checksum += prices[i];

    printf("BENCH_CHECKSUM: %.6f\n", checksum);
    printf("BENCH_DONE\n");

    free(ctxs);
    free(threads);
    free(prices);
    free(options);
    return 0;
}
