/**
 * @file gemv_hostphase.cpp
 * @brief HOST-PHASE term of the composed co-simulation (GEMV).
 *
 * Host work for the gemv offload: dense N x N matrix + N input-vector
 * generation, and result handling (checksum over the returned output
 * vector). No device computation, no verification recompute.
 */
#include <iostream>
#include <cstdlib>
#include "zsim_hooks.h"

int main(int argc, char* argv[]) {
    int n = (argc > 1) ? std::atoi(argv[1]) : 512;
    double* mat = new double[(size_t)n * n];
    double* vec = new double[n];
    double* out = new double[n];

    zsim_roi_begin();
    for (long i = 0; i < (long)n * n; i++) mat[i] = (rand() % 100) / 10.0;
    for (int i = 0; i < n; i++) vec[i] = (rand() % 100) / 10.0;
    for (int i = 0; i < n; i++) out[i] = 0.0;   // device fills this
    double checksum = 0.0;
    for (int i = 0; i < n; i++) checksum += out[i];
    zsim_roi_end();

    std::cout << "[HOSTPHASE] gemv n=" << n << " checksum=" << checksum << std::endl;
    std::cout << "[HOSTPHASE] done" << std::endl;
    delete[] mat; delete[] vec; delete[] out;
    return 0;
}
