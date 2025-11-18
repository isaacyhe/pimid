/**
 * @file reduction_shared.cpp
 * @brief Tree Reduction workload with SHARED MEMORY model
 *
 * Source: Common parallel computing pattern
 *
 * Shared Memory Model:
 * - Data stored in shared memory
 * - Each subarray processes assigned elements
 * - Atomic accumulation to shared result
 * - Barrier synchronization between phases
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>

struct ReductionConfig {
    int num_subarrays;
    int elements_per_subarray;

    int copy_latency;
    int read_latency;
    int write_latency;
    int compute_latency;
};

struct ReductionMetrics {
    uint64_t total_cycles = 0;
    uint64_t compute_cycles = 0;
    uint64_t sync_cycles = 0;
    uint64_t atomic_ops = 0;
    uint64_t elements_processed = 0;
    double total_energy = 0.0;
};

class ReductionShared {
private:
    ReductionConfig config;
    ReductionMetrics metrics;

    std::vector<double> shared_data;
    std::vector<double> partial_sums;
    double final_result;

public:
    ReductionShared(const ReductionConfig& cfg) : config(cfg), final_result(0.0) {
        int total_elements = config.num_subarrays * config.elements_per_subarray;
        shared_data.resize(total_elements, 1.0);
        partial_sums.resize(config.num_subarrays, 0.0);
    }

    void execute() {
        std::cout << "\n=== Tree Reduction Workload (SHARED MEMORY) ===" << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Elements per subarray: " << config.elements_per_subarray << std::endl;
        std::cout << "Total elements: " << (config.num_subarrays * config.elements_per_subarray) << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;

        performReduction();
        printMetrics();
    }

private:
    void performReduction() {
        std::cout << "\n=== Shared Memory Reduction ===" << std::endl;

        // Phase 1: Local reduction
        std::cout << "\nPhase 1: Local partial sum computation" << std::endl;
        computeLocalPartialSums();

        // Barrier synchronization
        std::cout << "\nBarrier synchronization" << std::endl;
        metrics.sync_cycles += 10;
        metrics.total_cycles += 10;

        // Phase 2: Atomic accumulation
        std::cout << "\nPhase 2: Atomic accumulation to shared result" << std::endl;
        accumulatePartialSums();

        // Barrier synchronization
        std::cout << "\nFinal barrier synchronization" << std::endl;
        metrics.sync_cycles += 10;
        metrics.total_cycles += 10;

        std::cout << "\n✓ Reduction complete - result: " << final_result << std::endl;
    }

    void computeLocalPartialSums() {
        // Each subarray computes partial sum of its assigned elements
        for (int sa = 0; sa < config.num_subarrays; sa++) {
            int start_idx = sa * config.elements_per_subarray;
            int end_idx = start_idx + config.elements_per_subarray;

            for (int i = start_idx; i < end_idx; i++) {
                // Read from shared memory
                metrics.total_cycles += config.read_latency;

                // Accumulate
                metrics.total_cycles += config.compute_latency;
                metrics.compute_cycles += config.compute_latency;
                metrics.elements_processed++;

                partial_sums[sa] += shared_data[i];
            }

            std::cout << "  Subarray " << sa << ": partial sum = " << partial_sums[sa]
                      << " (" << config.elements_per_subarray << " elements)" << std::endl;
        }
    }

    void accumulatePartialSums() {
        // Each subarray atomically adds its partial sum to final result
        for (int sa = 0; sa < config.num_subarrays; sa++) {
            // Atomic add to shared result
            metrics.total_cycles += 2;  // Atomic operation overhead
            metrics.atomic_ops++;

            final_result += partial_sums[sa];
        }

        std::cout << "  " << metrics.atomic_ops << " atomic additions" << std::endl;
    }

    void printMetrics() {
        std::cout << "\n=== Reduction Results (SHARED MEMORY) ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Sync cycles: " << metrics.sync_cycles
                  << " (" << (100.0 * metrics.sync_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Elements processed: " << metrics.elements_processed << std::endl;
        std::cout << "  Atomic operations: " << metrics.atomic_ops << std::endl;

        std::cout << "\nCommunication (Shared Memory):" << std::endl;
        std::cout << "  Inter-subarray transfers: 0 (shared memory model)" << std::endl;

        // Validation
        double expected_sum = config.num_subarrays * config.elements_per_subarray * 1.0;

        std::cout << "\nValidation:" << std::endl;
        std::cout << "  Expected final sum: " << expected_sum << std::endl;
        std::cout << "  Actual final sum: " << final_result << std::endl;
        std::cout << "  Elements processed: " << (metrics.elements_processed == config.num_subarrays * config.elements_per_subarray ? "✓" : "✗") << std::endl;

        if (std::abs(final_result - expected_sum) < 1e-6) {
            std::cout << "  ✓ Reduction validated" << std::endl;
        } else {
            std::cout << "  ✗ Validation failed!" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <elements_per_subarray> <is_libcom>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 32 1024 0   # 32 subarrays, 1024 elements, baseline" << std::endl;
        std::cerr << "Example: " << argv[0] << " 32 1024 1   # 32 subarrays, 1024 elements, LIBCom" << std::endl;
        return 1;
    }

    ReductionConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.elements_per_subarray = std::atoi(argv[2]);
    bool is_libcom = (std::atoi(argv[3]) == 1);

    config.read_latency = 1;
    config.write_latency = 1;
    config.compute_latency = 1;

    if (is_libcom) {
        config.copy_latency = 1;
    } else {
        int htree_latency = 0;
        int n = config.num_subarrays;
        while (n > 1) { htree_latency++; n >>= 1; }
        config.copy_latency = 2 + htree_latency;
    }

    std::cout << "\n=== DAC'26 Tree Reduction Benchmark (Shared Memory) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;

    ReductionShared workload(config);
    workload.execute();

    return 0;
}
