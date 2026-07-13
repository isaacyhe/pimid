/**
 * @file bfs_hostphase.cpp
 * @brief HOST-PHASE term of the composed co-simulation (BFS).
 *
 * Host work for the bfs offload: random-graph generation in CSR form
 * (row_ptr + col_idx) and the distance array, plus result handling
 * (checksum over the returned distances). No device computation, no
 * verification recompute. Mirrors the CSR layout built in
 * benchmarks/pim_kernels/bfs/bfs_mpi.c so the host-side data-prep cost is
 * measured at the same scale as the offloaded kernel.
 */
#include <iostream>
#include <cstdlib>
#include <cstring>
#include "zsim_hooks.h"

static int arg_int(int argc, char** argv, const char* key, int def) {
    for (int i = 1; i < argc - 1; i++)
        if (std::strcmp(argv[i], key) == 0) return std::atoi(argv[i + 1]);
    return def;
}

int main(int argc, char* argv[]) {
    int V   = arg_int(argc, argv, "--vertices", 5000000);
    int deg = arg_int(argc, argv, "--degree", 8);
    long total_edges = (long)V * deg;

    int* row_ptr = new int[(size_t)V + 1];
    int* col_idx = new int[(size_t)total_edges];
    int* dist    = new int[(size_t)V];

    zsim_roi_begin();

    // host_pre: build the CSR graph (same pattern as bfs_mpi.c) + init dist
    for (int i = 0; i <= V; i++) row_ptr[i] = i * deg;
    for (int i = 0; i < V; i++)
        for (int d = 0; d < deg; d++)
            col_idx[(long)i * deg + d] = (i + d + 1) % V;
    for (int i = 0; i < V; i++) dist[i] = -1;

    // (device runs the BFS traversal; dist stands in for its result)
    for (int i = 0; i < V; i++) dist[i] = 0;

    // host_post: result handling (checksum over the returned distances)
    long checksum = 0;
    for (int i = 0; i < V; i++) checksum += dist[i];

    zsim_roi_end();

    std::cout << "[HOSTPHASE] bfs V=" << V << " deg=" << deg
              << " checksum=" << checksum << std::endl;
    std::cout << "[HOSTPHASE] done" << std::endl;
    delete[] row_ptr; delete[] col_idx; delete[] dist;
    return 0;
}
