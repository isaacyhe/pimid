/**
 * @file stencil_hostphase.cpp
 * @brief HOST-PHASE term of the composed co-simulation (2D stencil).
 *
 * Host work for the stencil offload: grid generation and result handling
 * (checksum over the returned grid). No device computation, no verification
 * recompute (see reduction_hostphase).
 */
#include <iostream>
#include <cstdlib>
#include "zsim_hooks.h"

int main(int argc, char* argv[]) {
    int n = (argc > 1) ? std::atoi(argv[1]) : 128;

    double* grid = new double[(size_t)n * n];
    double* out = new double[(size_t)n * n];

    zsim_roi_begin();

    // host_pre: grid generation
    for (long i = 0; i < (long)n * n; i++)
        grid[i] = (rand() % 100) / 10.0;

    // (device runs the stencil iterations; out stands in for its result)
    for (long i = 0; i < (long)n * n; i++) out[i] = 0.0;

    // host_post: result handling (checksum over the returned grid)
    double checksum = 0.0;
    for (long i = 0; i < (long)n * n; i++) checksum += out[i];

    zsim_roi_end();

    std::cout << "[HOSTPHASE] stencil n=" << n << " checksum=" << checksum << std::endl;
    std::cout << "[HOSTPHASE] done" << std::endl;
    delete[] grid;
    delete[] out;
    return 0;
}
