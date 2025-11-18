/**
 * @file prefixsum_shared.cpp
 * @brief Prefix Sum (Scan) workload with SHARED MEMORY model
 *
 * Source: Parallel Research Kernels (PRK), CUDA SDK
 * Algorithm: Blelloch's work-efficient parallel scan
 *
 * Shared Memory Model:
 * - Array stored in shared memory
 * - Up-sweep phase: build sum tree with synchronization
 * - Down-sweep phase: propagate sums with synchronization
 * - Barriers ensure all subarrays see consistent state
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>

struct PrefixSumConfig {
    int num_subarrays;
    int array_length;

    int copy_latency;
    int read_latency;
    int write_latency;
    int compute_latency;
};

struct PrefixSumMetrics {
    uint64_t total_cycles = 0;
    uint64_t compute_cycles = 0;
    uint64_t sync_cycles = 0;
    uint64_t barrier_count = 0;
    uint64_t add_ops = 0;
    uint64_t remote_accesses = 0;
    double total_energy = 0.0;
};

class PrefixSumShared {
private:
    PrefixSumConfig config;
    PrefixSumMetrics metrics;
    bool is_libcom;

    std::vector<int> input_array;
    std::vector<int> output_array;
    std::vector<int> element_assignment;

public:
    PrefixSumShared(const PrefixSumConfig& cfg, bool use_libcom) : config(cfg), is_libcom(use_libcom) {
        input_array.resize(config.array_length);
        output_array.resize(config.array_length, 0);
        element_assignment.resize(config.array_length);

        // Initialize input (1, 1, 1, ...) for easy validation
        for (int i = 0; i < config.array_length; i++) {
            input_array[i] = 1;
            element_assignment[i] = i % config.num_subarrays;
        }
    }

    void execute() {
        std::cout << "\n=== Prefix Sum Workload (SHARED MEMORY) ===" << std::endl;
        std::cout << "Array length: " << config.array_length << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;
        std::cout << "Algorithm: Blelloch work-efficient scan" << std::endl;

        computePrefixSum();
        printMetrics();
    }

private:
    void computePrefixSum() {
        // Copy input to output (working array in shared memory)
        output_array = input_array;

        // Phase 1: Up-sweep (reduce phase)
        std::cout << "\nPhase 1: Up-sweep (build sum tree)" << std::endl;
        int depth = static_cast<int>(std::log2(config.array_length));

        for (int d = 0; d < depth; d++) {
            int stride = 1 << (d + 1);
            int offset = (1 << d) - 1;

            int ops_this_level = 0;

            for (int i = 0; i < config.array_length; i += stride) {
                int idx = i + stride - 1;
                if (idx < config.array_length) {
                    int left = i + offset;

                    // Track remote accesses (cross-subarray accesses)
                    int idx_subarray = element_assignment[idx];
                    int left_subarray = element_assignment[left];

                    // Reading left element
                    if (left_subarray != idx_subarray) {
                        metrics.remote_accesses++;
                    }
                    // Reading idx element
                    if (idx_subarray != idx_subarray) {  // Always local for idx
                        metrics.remote_accesses++;
                    }

                    // Read-modify-write in shared memory
                    metrics.total_cycles += 2 * config.read_latency + config.write_latency + config.compute_latency;
                    output_array[idx] += output_array[left];
                    metrics.add_ops++;
                    ops_this_level++;
                }
            }

            // Barrier: all subarrays must complete this level
            barrier();
            std::cout << "  Level " << (d + 1) << ": " << ops_this_level << " additions" << std::endl;
            metrics.compute_cycles += ops_this_level * config.compute_latency;
        }

        // Set last element to 0 for exclusive scan
        output_array[config.array_length - 1] = 0;
        metrics.total_cycles += config.write_latency;

        barrier();

        // Phase 2: Down-sweep (propagate sums)
        std::cout << "\nPhase 2: Down-sweep (distribute sums)" << std::endl;

        for (int d = depth - 1; d >= 0; d--) {
            int stride = 1 << (d + 1);
            int offset = (1 << d) - 1;

            int ops_this_level = 0;

            for (int i = 0; i < config.array_length; i += stride) {
                int idx = i + stride - 1;
                if (idx < config.array_length) {
                    int left = i + offset;

                    // Track remote accesses (cross-subarray accesses)
                    int idx_subarray = element_assignment[idx];
                    int left_subarray = element_assignment[left];

                    // Reading left element
                    if (left_subarray != idx_subarray) {
                        metrics.remote_accesses++;
                    }
                    // Reading idx element
                    if (idx_subarray != idx_subarray) {  // Always local for idx
                        metrics.remote_accesses++;
                    }
                    // Writing to left element
                    if (left_subarray != idx_subarray) {
                        metrics.remote_accesses++;
                    }

                    // Swap and add
                    metrics.total_cycles += 2 * config.read_latency + 2 * config.write_latency + config.compute_latency;
                    int temp = output_array[left];
                    output_array[left] = output_array[idx];
                    output_array[idx] += temp;
                    metrics.add_ops++;
                    ops_this_level++;
                }
            }

            // Barrier
            barrier();
            std::cout << "  Level " << (depth - d) << ": " << ops_this_level << " operations" << std::endl;
            metrics.compute_cycles += ops_this_level * config.compute_latency;
        }

        // Calculate total energy based on remote accesses
        double energy_per_access = is_libcom ? 0.55 : 1.0;
        metrics.total_energy = metrics.remote_accesses * energy_per_access;

        std::cout << "\n✓ Exclusive prefix sum complete" << std::endl;
    }

    void barrier() {
        // Synchronization barrier for shared memory model
        metrics.barrier_count++;
        metrics.sync_cycles += 10;  // Conservative barrier overhead
        metrics.total_cycles += 10;
    }

    void printMetrics() {
        std::cout << "\n=== Prefix Sum Results (SHARED MEMORY) ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Sync cycles: " << metrics.sync_cycles
                  << " (" << (100.0 * metrics.sync_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total add operations: " << metrics.add_ops << std::endl;
        std::cout << "  Synchronization barriers: " << metrics.barrier_count << std::endl;

        std::cout << "\nCommunication (Shared Memory):" << std::endl;
        std::cout << "  Inter-subarray transfers: 0 (shared memory model)" << std::endl;
        std::cout << "  Remote accesses: " << metrics.remote_accesses << std::endl;

        std::cout << "\nEnergy:" << std::endl;
        std::cout << "  Remote accesses: " << metrics.remote_accesses << std::endl;
        std::cout << "  Energy per access: " << (is_libcom ? 0.55 : 1.0) << " pJ" << std::endl;
        std::cout << "  Total energy: " << metrics.total_energy << " pJ" << std::endl;

        // Validation
        std::cout << "\nValidation:" << std::endl;
        bool correct = true;
        for (int i = 0; i < std::min(10, config.array_length); i++) {
            int expected = i;  // Exclusive scan of all 1s
            if (output_array[i] != expected) {
                correct = false;
                std::cout << "  ✗ Mismatch at index " << i << ": expected " << expected
                          << ", got " << output_array[i] << std::endl;
                break;
            }
        }

        if (correct) {
            std::cout << "  ✓ Prefix sum validated (first 10 elements correct)" << std::endl;
            std::cout << "  Sample output: [";
            for (int i = 0; i < std::min(10, config.array_length); i++) {
                std::cout << output_array[i];
                if (i < std::min(10, config.array_length) - 1) std::cout << ", ";
            }
            std::cout << ", ...]" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <array_length> <is_libcom>" << std::endl;
        std::cerr << "Note: array_length should be power of 2 for Blelloch scan" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 2048 0   # 8 subarrays, 2048 elements, baseline" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 2048 1   # 8 subarrays, 2048 elements, LIBCom" << std::endl;
        return 1;
    }

    PrefixSumConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.array_length = std::atoi(argv[2]);
    bool is_libcom = (std::atoi(argv[3]) == 1);

    // Check if array length is power of 2
    if ((config.array_length & (config.array_length - 1)) != 0) {
        std::cerr << "Warning: array_length should be power of 2 for optimal Blelloch scan" << std::endl;
    }

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

    std::cout << "\n=== DAC'26 Prefix Sum Benchmark (Shared Memory) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: Parallel Research Kernels (PRK), CUDA SDK" << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;

    PrefixSumShared workload(config, is_libcom);
    workload.execute();

    return 0;
}
