/**
 * @file stream_hostphase.cpp
 * @brief HOST-PHASE term of the composed co-simulation (STREAM triad).
 *
 * Host work for the triad offload (a = b + s*c): generation of the two input
 * arrays and result handling (checksum over the returned array). No device
 * computation, no verification recompute (see reduction_hostphase).
 */
#include <iostream>
#include <cstdlib>
#include "zsim_hooks.h"

int main(int argc, char* argv[]) {
    long n = (argc > 1) ? std::atol(argv[1]) : 65536;

    double* a = new double[n];
    double* b = new double[n];
    double* c = new double[n];

    zsim_roi_begin();

    // host_pre: input generation (B and C)
    for (long i = 0; i < n; i++) {
        b[i] = (rand() % 100) / 10.0;
        c[i] = (rand() % 100) / 10.0;
    }

    // (device computes A = B + s*C; A stands in for its returned result)
    for (long i = 0; i < n; i++) a[i] = 0.0;

    // host_post: result handling (checksum over the returned array)
    double checksum = 0.0;
    for (long i = 0; i < n; i++) checksum += a[i];

    zsim_roi_end();

    std::cout << "[HOSTPHASE] stream n=" << n << " checksum=" << checksum << std::endl;
    std::cout << "[HOSTPHASE] done" << std::endl;
    delete[] a;
    delete[] b;
    delete[] c;
    return 0;
}
