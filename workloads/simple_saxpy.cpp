/**
 * SAXPY (Single-precision A*X Plus Y) workload
 * Classic memory-bound kernel for PIM evaluation
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <omp.h>
#include <cstdlib>
#include <cmath>

constexpr size_t DEFAULT_SIZE = 4 * 1024 * 1024;  // 4M elements
constexpr int DEFAULT_ITERATIONS = 100;

int main(int argc, char* argv[]) {
    size_t n = DEFAULT_SIZE;
    int iterations = DEFAULT_ITERATIONS;
    int num_threads = 16;

    if (argc > 1) n = std::atol(argv[1]);
    if (argc > 2) iterations = std::atoi(argv[2]);
    if (argc > 3) num_threads = std::atoi(argv[3]);

    omp_set_num_threads(num_threads);

    std::cout << "SAXPY Workload (y = a*x + y)" << std::endl;
    std::cout << "  Size: " << n << " elements" << std::endl;
    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "  Threads: " << num_threads << std::endl;

    std::vector<float> x(n);
    std::vector<float> y(n);
    float a = 2.5f;

    // Initialize
    #pragma omp parallel for
    for (size_t i = 0; i < n; i++) {
        x[i] = static_cast<float>(i % 1000) * 0.001f;
        y[i] = static_cast<float>((i + 500) % 1000) * 0.001f;
    }

    // Warmup
    #pragma omp parallel for
    for (size_t i = 0; i < n; i++) {
        y[i] = a * x[i] + y[i];
    }

    // Timed iterations
    auto start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < iterations; iter++) {
        #pragma omp parallel for
        for (size_t i = 0; i < n; i++) {
            y[i] = a * x[i] + y[i];
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Compute stats
    double total_bytes = 3.0 * n * sizeof(float) * iterations;  // 2 reads + 1 write
    double total_flops = 2.0 * n * iterations;  // 1 multiply + 1 add
    double bandwidth = total_bytes / (duration.count() * 1e-6) / (1024*1024*1024);
    double gflops = total_flops / (duration.count() * 1e-6) / 1e9;

    // Checksum
    double sum = 0.0;
    #pragma omp parallel for reduction(+:sum)
    for (size_t i = 0; i < n; i++) {
        sum += y[i];
    }

    std::cout << "Results:" << std::endl;
    std::cout << "  Time: " << duration.count() / 1000.0 << " ms" << std::endl;
    std::cout << "  Bandwidth: " << bandwidth << " GB/s" << std::endl;
    std::cout << "  GFLOPS: " << gflops << std::endl;
    std::cout << "  Checksum: " << sum << std::endl;

    return 0;
}
