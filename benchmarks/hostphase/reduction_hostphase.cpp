/**
 * @file reduction_hostphase.cpp
 * @brief HOST-PHASE term of the composed co-simulation (reduction).
 *
 * Composed co-sim: T = host_phases + boundary(link) + device_exec(tech).
 * This binary measures ONLY the host's own work for the reduction offload:
 *   - input generation (host writes N doubles)
 *   - final merge of the PE partial results (host reads P partials)
 *   - result handling (host touches the final result)
 * It deliberately contains NO device computation and NO verification
 * recompute (verification is a test artifact, not offload workload; kernel
 * correctness is checked in the device-scope simulation of the kernel).
 *
 * Run under scope=system with a host node and an unused stub device; all
 * work executes on the host model (cores + caches + host memory).
 */
#include <iostream>
#include <cstdlib>
#include "zsim_hooks.h"

int main(int argc, char* argv[]) {
    int n = (argc > 1) ? std::atoi(argv[1]) : 4096;
    int pes = (argc > 2) ? std::atoi(argv[2]) : 8;

    double* data = new double[n];
    double* partials = new double[pes];

    zsim_roi_begin();

    // host_pre: input generation
    for (int i = 0; i < n; i++) data[i] = (rand() % 100) / 10.0;

    // (device would compute here; partials stand in for its returned results)
    for (int p = 0; p < pes; p++) partials[p] = 1.0;

    // host_post: final merge + result handling
    double final_result = 0.0;
    for (int p = 0; p < pes; p++) final_result += partials[p];

    zsim_roi_end();

    std::cout << "[HOSTPHASE] reduction n=" << n << " pes=" << pes
              << " result=" << final_result << std::endl;
    std::cout << "[HOSTPHASE] done" << std::endl;
    delete[] data;
    delete[] partials;
    return 0;
}
