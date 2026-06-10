/* bfs.c — Breadth-First Search on random graph (adjacency list via CSR)
 * Serial baseline. Generates random graph with given vertex count and degree. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zsim_hooks.h"

#define DEFAULT_VERTICES 512
#define DEFAULT_DEGREE   8

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
    int V = parse_int_arg(argc, argv, "--vertices", DEFAULT_VERTICES);
    int deg = parse_int_arg(argc, argv, "--degree", DEFAULT_DEGREE);
    int source = parse_int_arg(argc, argv, "--source", 0);
    uint32_t seed = 42;

    int total_edges = V * deg;
    int* row_ptr = (int*)malloc((V + 1) * sizeof(int));
    int* col_idx = (int*)malloc(total_edges * sizeof(int));
    int* dist = (int*)malloc(V * sizeof(int));
    int* frontier = (int*)malloc(V * sizeof(int));
    int* next_frontier = (int*)malloc(V * sizeof(int));
    if (!row_ptr || !col_idx || !dist || !frontier || !next_frontier) {
        fprintf(stderr, "malloc failed\n"); return 1;
    }

    /* Build random graph (CSR) */
    for (int i = 0; i <= V; i++) row_ptr[i] = i * deg;
    for (int i = 0; i < V; i++) {
        for (int d = 0; d < deg; d++)
            col_idx[i * deg + d] = bench_rand(&seed) % V;
    }

    /* Init distances */
    for (int i = 0; i < V; i++) dist[i] = -1;
    dist[source] = 0;
    frontier[0] = source;
    int fsize = 1;

    zsim_roi_begin();
    int level = 0;
    while (fsize > 0) {
        level++;
        int nsize = 0;
        for (int f = 0; f < fsize; f++) {
            int u = frontier[f];
            for (int e = row_ptr[u]; e < row_ptr[u + 1]; e++) {
                int v = col_idx[e];
                if (dist[v] == -1) {
                    dist[v] = level;
                    next_frontier[nsize++] = v;
                }
            }
        }
        /* Swap frontiers */
        int* tmp = frontier;
        frontier = next_frontier;
        next_frontier = tmp;
        fsize = nsize;
    }
    zsim_roi_end();

    long visited = 0;
    for (int i = 0; i < V; i++)
        if (dist[i] >= 0) visited++;

    printf("BENCH_LEVELS: %d\n", level);
    printf("BENCH_VISITED: %ld\n", visited);
    printf("BENCH_CHECKSUM: %ld\n", visited);
    printf("BENCH_DONE\n");

    free(row_ptr); free(col_idx); free(dist);
    free(frontier); free(next_frontier);
    return 0;
}
