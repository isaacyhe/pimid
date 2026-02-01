/**
 * Simple OpenMP parallelized vector addition workload
 * For testing PIMID standalone/Pin mode with 16 PEs
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <omp.h>
#include <cstdlib>

// Default size: 16MB of data (4M floats), divisible by 16 PEs
constexpr size_t DEFAULT_SIZE = 4 * 1024 * 1024;

int main(int argc, char* argv[]) {
    size_t n = DEFAULT_SIZE;
    if (argc > 1) {
        n = std::atol(argv[1]);
    }

    int num_threads = 16;  // Match 16 PEs
    if (argc > 2) {
        num_threads = std::atoi(argv[2]);
    }
    omp_set_num_threads(num_threads);

    std::cout << "Vector Addition Workload" << std::endl;
    std::cout << "  Size: " << n << " elements (" << (n * sizeof(float) / (1024*1024)) << " MB per vector)" << std::endl;
    std::cout << "  Threads: " << num_threads << std::endl;

    // Allocate vectors
    std::vector<float> a(n);
    std::vector<float> b(n);
    std::vector<float> c(n);

    // Initialize with parallel threads
    #pragma omp parallel for
    for (size_t i = 0; i < n; i++) {
        a[i] = static_cast<float>(i % 1000) * 0.001f;
        b[i] = static_cast<float>((i + 500) % 1000) * 0.001f;
    }

    // Warmup
    #pragma omp parallel for
    for (size_t i = 0; i < n; i++) {
        c[i] = a[i] + b[i];
    }

    // Timed run
    auto start = std::chrono::high_resolution_clock::now();

    #pragma omp parallel for
    for (size_t i = 0; i < n; i++) {
        c[i] = a[i] + b[i];
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Verify result
    float sum = 0.0f;
    #pragma omp parallel for reduction(+:sum)
    for (size_t i = 0; i < n; i++) {
        sum += c[i];
    }

    double bandwidth = (3.0 * n * sizeof(float)) / (duration.count() * 1e-6) / (1024*1024*1024);

    std::cout << "Results:" << std::endl;
    std::cout << "  Time: " << duration.count() << " us" << std::endl;
    std::cout << "  Bandwidth: " << bandwidth << " GB/s" << std::endl;
    std::cout << "  Checksum: " << sum << std::endl;

    return 0;
}
