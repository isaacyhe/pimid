/*
 * Monte Carlo Swaption Pricing -- Simplified HJM model (PARSEC swaptions)
 * For each swaption: generate MC paths of forward rates, compute payoff, average.
 * Box-Muller for Gaussian RNG from LCG.
 *
 * Compile: g++ -std=c++11 -O2 -Wall -I<path-to-zsim/misc/hooks> -lpthread -lm swaptions.c -o swaptions
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
/*  Box-Muller: two uniform [0,1) -> one standard normal              */
/* ------------------------------------------------------------------ */
static double gauss_rand(uint32_t* s) {
    double u1, u2;
    do {
        u1 = (double)(bench_rand(s) + 1) / 32769.0;
        u2 = (double)(bench_rand(s)) / 32768.0;
    } while (u1 <= 0.0);
    return sqrt(-2.0 * log(u1)) * cos(2.0 * 3.14159265358979323846 * u2);
}

/* ------------------------------------------------------------------ */
/*  Swaption parameters                                               */
/* ------------------------------------------------------------------ */
#define NUM_FACTORS 3   /* number of yield-curve factors */
#define NUM_TENORS  10  /* number of forward rate tenors */

typedef struct {
    double strike;                          /* swaption strike rate */
    double expiry;                          /* years to expiry */
    double tenor_delta;                     /* time between tenor points */
    double init_rates[NUM_TENORS];          /* initial forward rates */
    double vols[NUM_FACTORS][NUM_TENORS];   /* factor volatilities */
    double price;                           /* output: computed price */
} Swaption;

/* ------------------------------------------------------------------ */
/*  HJM single-path simulation                                        */
/* ------------------------------------------------------------------ */
static double simulate_path(const Swaption* sw, int num_steps, uint32_t* seed) {
    double dt = sw->expiry / (double)num_steps;
    double rates[NUM_TENORS];
    memcpy(rates, sw->init_rates, sizeof(rates));

    /* Evolve forward rates through time steps */
    for (int t = 0; t < num_steps; t++) {
        double new_rates[NUM_TENORS];
        for (int j = 0; j < NUM_TENORS; j++) {
            /* Drift: HJM no-arbitrage drift = sum_k vol_k * (sum_{l<=j} vol_k * delta_t) */
            double drift = 0.0;
            for (int k = 0; k < NUM_FACTORS; k++) {
                double vol_sum = 0.0;
                for (int l = 0; l <= j; l++)
                    vol_sum += sw->vols[k][l] * sw->tenor_delta;
                drift += sw->vols[k][j] * vol_sum;
            }
            drift *= dt;

            /* Diffusion */
            double diffusion = 0.0;
            for (int k = 0; k < NUM_FACTORS; k++) {
                diffusion += sw->vols[k][j] * gauss_rand(seed) * sqrt(dt);
            }

            new_rates[j] = rates[j] + drift + diffusion;
            if (new_rates[j] < 0.0) new_rates[j] = 0.0;  /* floor at zero */
        }
        memcpy(rates, new_rates, sizeof(rates));
    }

    /* Compute swap value at expiry: sum of discounted (rate - strike) cash flows */
    double swap_val = 0.0;
    double disc = 1.0;
    for (int j = 0; j < NUM_TENORS; j++) {
        disc /= (1.0 + rates[j] * sw->tenor_delta);
        swap_val += disc * (rates[j] - sw->strike) * sw->tenor_delta;
    }

    /* Swaption payoff = max(swap_val, 0) */
    return (swap_val > 0.0) ? swap_val : 0.0;
}

/* ------------------------------------------------------------------ */
/*  Thread context                                                    */
/* ------------------------------------------------------------------ */
typedef struct {
    int        id;
    int        start;          /* first swaption index */
    int        end;            /* one past last swaption index */
    Swaption*  swaptions;
    int        num_sims;       /* MC paths per swaption */
} ThreadCtx;

static void* worker(void* arg) {
    ThreadCtx* ctx = (ThreadCtx*)arg;
    int num_steps = 10;  /* time discretization steps */

    for (int i = ctx->start; i < ctx->end; i++) {
        double sum = 0.0;
        uint32_t seed = (uint32_t)(42 + i * 7919);

        for (int s = 0; s < ctx->num_sims; s++) {
            sum += simulate_path(&ctx->swaptions[i], num_steps, &seed);
        }
        ctx->swaptions[i].price = sum / (double)ctx->num_sims;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */
int main(int argc, char** argv) {
    int num_swaptions = parse_int_arg(argc, argv, "--size", 16);
    int num_sims      = parse_int_arg(argc, argv, "--sims", 10000);
    int num_threads   = parse_int_arg(argc, argv, "--threads", 1);

    printf("Monte Carlo Swaptions (HJM) -- swaptions=%d sims=%d threads=%d\n",
           num_swaptions, num_sims, num_threads);

    Swaption* swaptions = (Swaption*)calloc((size_t)num_swaptions, sizeof(Swaption));
    if (!swaptions) { fprintf(stderr, "malloc failed\n"); return 1; }

    /* Initialize swaptions from LCG */
    uint32_t seed = 42;
    for (int i = 0; i < num_swaptions; i++) {
        swaptions[i].strike = 0.04 + (double)(bench_rand(&seed) % 60) / 1000.0;
        swaptions[i].expiry = 1.0 + (double)(bench_rand(&seed) % 90) / 10.0;
        swaptions[i].tenor_delta = 0.5;

        /* Initial forward rate curve */
        for (int j = 0; j < NUM_TENORS; j++)
            swaptions[i].init_rates[j] = 0.03 + (double)(bench_rand(&seed) % 40) / 1000.0;

        /* Factor volatilities */
        for (int k = 0; k < NUM_FACTORS; k++)
            for (int j = 0; j < NUM_TENORS; j++)
                swaptions[i].vols[k][j] = 0.005 + (double)(bench_rand(&seed) % 30) / 1000.0;

        swaptions[i].price = 0.0;
    }

    /* ---- ROI begin ---- */
    zsim_roi_begin();

    pthread_t*  threads = (pthread_t*)malloc((size_t)num_threads * sizeof(pthread_t));
    ThreadCtx*  ctxs    = (ThreadCtx*)malloc((size_t)num_threads * sizeof(ThreadCtx));

    int chunk = num_swaptions / num_threads;
    for (int t = 0; t < num_threads; t++) {
        ctxs[t].id        = t;
        ctxs[t].start     = t * chunk;
        ctxs[t].end       = (t == num_threads - 1) ? num_swaptions : (t + 1) * chunk;
        ctxs[t].swaptions = swaptions;
        ctxs[t].num_sims  = num_sims;
        pthread_create(&threads[t], NULL, worker, &ctxs[t]);
    }
    for (int t = 0; t < num_threads; t++)
        pthread_join(threads[t], NULL);

    zsim_roi_end();
    /* ---- ROI end ---- */

    /* Checksum: sum of all prices */
    double checksum = 0.0;
    for (int i = 0; i < num_swaptions; i++) {
        checksum += swaptions[i].price;
    }

    printf("BENCH_CHECKSUM: %.6f\n", checksum);
    printf("BENCH_DONE\n");

    free(ctxs);
    free(threads);
    free(swaptions);
    return 0;
}
