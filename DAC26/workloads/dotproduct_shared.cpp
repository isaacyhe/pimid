/**
 * @file dotproduct_shared.cpp
 * @brief Dot Product workload with SHARED MEMORY model
 *
 * Source: BLAS Level 1 (DDOT operation)
 *
 * Shared Memory Model:
 * - Vectors A and B stored in shared memory
 * - Each subarray computes partial dot products for its portion
 * - Accumulate partial sums to shared result with synchronization
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>

struct DotProductConfig {
    int num_subarrays;
    int vector_length;

    int copy_latency;
    int read_latency;
    int write_latency;
    int compute_latency;  // MAC operation
};

struct DotProductMetrics {
    uint64_t total_cycles = 0;
    uint64_t compute_cycles = 0;
    uint64_t sync_cycles = 0;
    uint64_t mac_ops = 0;  // Multiply-accumulate operations
    uint64_t atomic_ops = 0;
    double total_energy = 0.0;
};

class DotProductShared {
private:
    DotProductConfig config;
    DotProductMetrics metrics;

    std::vector<double> vector_a;
    std::vector<double> vector_b;
    std::vector<int> element_assignment;  // Which subarray processes each element
    double shared_result;

public:
    DotProductShared(const DotProductConfig& cfg) : config(cfg), shared_result(0.0) {
        vector_a.resize(config.vector_length);
        vector_b.resize(config.vector_length);
        element_assignment.resize(config.vector_length);

        // Initialize vectors
        for (int i = 0; i < config.vector_length; i++) {
            vector_a[i] = 1.0 + (i % 100) / 100.0;
            vector_b[i] = 2.0 + (i % 50) / 50.0;
            element_assignment[i] = i % config.num_subarrays;
        }
    }

    void execute() {
        std::cout << "\n=== Dot Product Workload (SHARED MEMORY) ===" << std::endl;
        std::cout << "Vector length: " << config.vector_length << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;

        computeDotProduct();
        printMetrics();
    }

private:
    void computeDotProduct() {
        std::cout << "\nPhase 1: Parallel partial sum computation" << std::endl;

        std::vector<double> partial_sums(config.num_subarrays, 0.0);

        // Each subarray computes its partial sum
        for (int sa = 0; sa < config.num_subarrays; sa++) {
            int local_macs = 0;

            for (int i = 0; i < config.vector_length; i++) {
                if (element_assignment[i] == sa) {
                    // Read A[i] and B[i] from shared memory
                    metrics.total_cycles += 2 * config.read_latency;

                    // Multiply-accumulate
                    metrics.total_cycles += config.compute_latency;
                    partial_sums[sa] += vector_a[i] * vector_b[i];
                    local_macs++;
                    metrics.mac_ops++;
                }
            }

            std::cout << "  Subarray " << sa << ": " << local_macs << " MACs, partial sum = "
                      << partial_sums[sa] << std::endl;
            metrics.compute_cycles += local_macs * config.compute_latency;
        }

        // Phase 2: Accumulate to shared result with synchronization
        std::cout << "\nPhase 2: Atomic accumulation to shared result" << std::endl;

        for (int sa = 0; sa < config.num_subarrays; sa++) {
            // Atomic add to shared result
            metrics.total_cycles += 2;  // Atomic operation overhead
            metrics.atomic_ops++;
            shared_result += partial_sums[sa];
        }

        std::cout << "  " << metrics.atomic_ops << " atomic additions to shared result" << std::endl;

        // Phase 3: Barrier synchronization
        std::cout << "\nPhase 3: Synchronization barrier" << std::endl;
        metrics.sync_cycles += 10;
        metrics.total_cycles += 10;
        std::cout << "  Final result: " << shared_result << std::endl;
    }

    void printMetrics() {
        std::cout << "\n=== Dot Product Results (SHARED MEMORY) ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Sync cycles: " << metrics.sync_cycles
                  << " (" << (100.0 * metrics.sync_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total MAC operations: " << metrics.mac_ops << std::endl;
        std::cout << "  Atomic accumulations: " << metrics.atomic_ops << std::endl;

        std::cout << "\nCommunication (Shared Memory):" << std::endl;
        std::cout << "  Inter-subarray transfers: 0 (shared memory model)" << std::endl;

        // Validation
        std::cout << "\nValidation:" << std::endl;
        double expected_result = 0.0;
        for (int i = 0; i < config.vector_length; i++) {
            expected_result += vector_a[i] * vector_b[i];
        }

        std::cout << "  Expected result: " << expected_result << std::endl;
        std::cout << "  Actual result: " << shared_result << std::endl;
        std::cout << "  Difference: " << std::abs(expected_result - shared_result) << std::endl;

        if (std::abs(expected_result - shared_result) < 1e-6) {
            std::cout << "  ✓ Dot product validated" << std::endl;
        } else {
            std::cout << "  ✗ Dot product validation failed!" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <vector_length> <is_libcom>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 2048 0   # 8 subarrays, 2048 elements, baseline" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 2048 1   # 8 subarrays, 2048 elements, LIBCom" << std::endl;
        return 1;
    }

    DotProductConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.vector_length = std::atoi(argv[2]);
    bool is_libcom = (std::atoi(argv[3]) == 1);

    config.read_latency = 1;
    config.write_latency = 1;
    config.compute_latency = 1;  // 1 cycle for MAC

    if (is_libcom) {
        config.copy_latency = 1;
    } else {
        int htree_latency = 0;
        int n = config.num_subarrays;
        while (n > 1) { htree_latency++; n >>= 1; }
        config.copy_latency = 2 + htree_latency;
    }

    std::cout << "\n=== DAC'26 Dot Product Benchmark (Shared Memory) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: BLAS Level 1 (DDOT)" << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;

    DotProductShared workload(config);
    workload.execute();

    return 0;
}
