/*
 * Streaming K-Median Clustering -- Online clustering of streaming points
 * Facility-location-based k-median with streaming chunks (PARSEC streamcluster).
 *
 * Compile: g++ -std=c++11 -O2 -Wall -I<path-to-zsim/misc/hooks> -lpthread -lm streamcluster.c -o streamcluster
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
/*  Data structures                                                   */
/* ------------------------------------------------------------------ */
#define DIM 8  /* point dimensionality */

typedef struct {
    double coord[DIM];
    int    center_id;    /* index of assigned center, -1 = unassigned */
    double weight;       /* point weight (for merged centers) */
} Point;

/* ------------------------------------------------------------------ */
/*  Distance                                                          */
/* ------------------------------------------------------------------ */
static double dist2(const double* a, const double* b) {
    double d = 0.0;
    for (int i = 0; i < DIM; i++) {
        double diff = a[i] - b[i];
        d += diff * diff;
    }
    return d;
}

/* ------------------------------------------------------------------ */
/*  Thread context for parallel nearest-center assignment              */
/* ------------------------------------------------------------------ */
typedef struct {
    int     start, end;
    Point*  points;
    Point*  centers;
    int     num_centers;
} AssignCtx;

static void* assign_worker(void* arg) {
    AssignCtx* ctx = (AssignCtx*)arg;
    for (int i = ctx->start; i < ctx->end; i++) {
        double best_d = 1e30;
        int    best_c = 0;
        for (int c = 0; c < ctx->num_centers; c++) {
            double d = dist2(ctx->points[i].coord, ctx->centers[c].coord);
            if (d < best_d) {
                best_d = d;
                best_c = c;
            }
        }
        ctx->points[i].center_id = best_c;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Parallel assign all points to nearest center                      */
/* ------------------------------------------------------------------ */
static void parallel_assign(Point* points, int np, Point* centers, int nc,
                            int num_threads) {
    pthread_t*  threads = (pthread_t*)malloc((size_t)num_threads * sizeof(pthread_t));
    AssignCtx*  ctxs    = (AssignCtx*)malloc((size_t)num_threads * sizeof(AssignCtx));
    int chunk = np / num_threads;

    for (int t = 0; t < num_threads; t++) {
        ctxs[t].start       = t * chunk;
        ctxs[t].end         = (t == num_threads - 1) ? np : (t + 1) * chunk;
        ctxs[t].points      = points;
        ctxs[t].centers     = centers;
        ctxs[t].num_centers = nc;
        pthread_create(&threads[t], NULL, assign_worker, &ctxs[t]);
    }
    for (int t = 0; t < num_threads; t++)
        pthread_join(threads[t], NULL);

    free(ctxs);
    free(threads);
}

/* ------------------------------------------------------------------ */
/*  Facility-location open/close: greedy k-median                     */
/* ------------------------------------------------------------------ */
static int kmedian_greedy(Point* points, int np, Point* centers, int max_k,
                          int num_threads) {
    /* First center = first point */
    int nc = 1;
    memcpy(centers[0].coord, points[0].coord, sizeof(double) * DIM);
    centers[0].weight = 1.0;

    /* Greedy: open new centers far from existing ones */
    double* min_dists = (double*)malloc((size_t)np * sizeof(double));
    for (int i = 0; i < np; i++)
        min_dists[i] = dist2(points[i].coord, centers[0].coord);

    for (int c = 1; c < max_k; c++) {
        /* Find point with max min-distance to any center */
        int best_idx = 0;
        double best_val = min_dists[0];
        for (int i = 1; i < np; i++) {
            if (min_dists[i] > best_val) {
                best_val = min_dists[i];
                best_idx = i;
            }
        }
        if (best_val < 1e-6) break;  /* all points close to a center */

        memcpy(centers[c].coord, points[best_idx].coord, sizeof(double) * DIM);
        centers[c].weight = 1.0;
        nc++;

        /* Update min distances */
        for (int i = 0; i < np; i++) {
            double d = dist2(points[i].coord, centers[c].coord);
            if (d < min_dists[i]) min_dists[i] = d;
        }
    }
    free(min_dists);

    /* Assign points to centers */
    parallel_assign(points, np, centers, nc, num_threads);

    /* Recompute center positions as mean of assigned points */
    for (int c = 0; c < nc; c++) {
        for (int d = 0; d < DIM; d++) centers[c].coord[d] = 0.0;
        centers[c].weight = 0.0;
    }
    for (int i = 0; i < np; i++) {
        int cid = points[i].center_id;
        for (int d = 0; d < DIM; d++)
            centers[cid].coord[d] += points[i].coord[d] * points[i].weight;
        centers[cid].weight += points[i].weight;
    }
    for (int c = 0; c < nc; c++) {
        if (centers[c].weight > 0.0)
            for (int d = 0; d < DIM; d++)
                centers[c].coord[d] /= centers[c].weight;
    }

    return nc;
}

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */
int main(int argc, char** argv) {
    int num_points  = parse_int_arg(argc, argv, "--size", 4096);
    int k           = parse_int_arg(argc, argv, "--k", 10);
    int num_threads = parse_int_arg(argc, argv, "--threads", 1);
    int chunk_size  = num_points / 4;  /* 4 streaming chunks */
    if (chunk_size < 1) chunk_size = 1;

    printf("Streaming K-Median -- points=%d k=%d chunk=%d threads=%d\n",
           num_points, k, chunk_size, num_threads);

    /* Allocate all points */
    Point* points  = (Point*)calloc((size_t)num_points, sizeof(Point));
    Point* centers = (Point*)calloc((size_t)k, sizeof(Point));
    if (!points || !centers) { fprintf(stderr, "malloc failed\n"); return 1; }

    /* Generate points from LCG */
    uint32_t seed = 42;
    for (int i = 0; i < num_points; i++) {
        for (int d = 0; d < DIM; d++)
            points[i].coord[d] = (double)(bench_rand(&seed)) / 32768.0 * 100.0;
        points[i].weight = 1.0;
        points[i].center_id = -1;
    }

    /* ---- ROI begin ---- */
    zsim_roi_begin();

    /* Process points in streaming chunks */
    int nc = 0;
    int processed = 0;
    while (processed < num_points) {
        int cs = chunk_size;
        if (processed + cs > num_points) cs = num_points - processed;

        /* Run k-median on this chunk */
        Point* chunk_centers = (Point*)calloc((size_t)k, sizeof(Point));
        int chunk_nc = kmedian_greedy(&points[processed], cs, chunk_centers, k,
                                      num_threads);

        /* Merge chunk centers into global centers */
        if (nc == 0) {
            /* First chunk: adopt directly */
            memcpy(centers, chunk_centers, (size_t)chunk_nc * sizeof(Point));
            nc = chunk_nc;
        } else {
            /* Combine existing + chunk centers, re-cluster down to k */
            int total = nc + chunk_nc;
            Point* combined = (Point*)calloc((size_t)total, sizeof(Point));
            memcpy(combined, centers, (size_t)nc * sizeof(Point));
            memcpy(&combined[nc], chunk_centers, (size_t)chunk_nc * sizeof(Point));

            Point* new_centers = (Point*)calloc((size_t)k, sizeof(Point));
            nc = kmedian_greedy(combined, total, new_centers, k, num_threads);
            memcpy(centers, new_centers, (size_t)nc * sizeof(Point));

            free(new_centers);
            free(combined);
        }

        free(chunk_centers);
        processed += cs;
    }

    /* Final assignment */
    parallel_assign(points, num_points, centers, nc, num_threads);

    zsim_roi_end();
    /* ---- ROI end ---- */

    /* Compute total cost and checksum */
    double total_cost = 0.0;
    for (int i = 0; i < num_points; i++) {
        total_cost += sqrt(dist2(points[i].coord,
                                 centers[points[i].center_id].coord));
    }

    double checksum = total_cost;
    for (int c = 0; c < nc; c++)
        for (int d = 0; d < DIM; d++)
            checksum += centers[c].coord[d];

    printf("Centers: %d  Total cost: %.6f\n", nc, total_cost);
    printf("BENCH_CHECKSUM: %.6f\n", checksum);
    printf("BENCH_DONE\n");

    free(centers);
    free(points);
    return 0;
}
