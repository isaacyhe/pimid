/**
 * @file prefixsum_message.cpp
 * @brief Prefix Sum (Scan) workload with MESSAGE PASSING model
 *
 * Source: Parallel Research Kernels (PRK), CUDA SDK
 * Algorithm: Segmented scan with explicit boundary exchange
 *
 * Message Passing Model:
 * - Each subarray has local segment of array
 * - Compute local prefix sum independently
 * - Exchange boundary values between segments
 * - Adjust local results based on predecessors' sums
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
    uint64_t transfer_cycles = 0;
    uint64_t intersubarray_transfers = 0;
    uint64_t htree_traversals = 0;
    uint64_t direct_transfers = 0;
    uint64_t add_ops = 0;
    double total_energy = 0.0;
};

class PrefixSumMessage {
private:
    PrefixSumConfig config;
    PrefixSumMetrics metrics;

    std::vector<int> input_array;
    std::vector<int> output_array;
    std::vector<int> segment_sums;  // Sum of each segment
    std::vector<int> prefix_offsets;  // Offset to add to each segment

public:
    PrefixSumMessage(const PrefixSumConfig& cfg) : config(cfg) {
        input_array.resize(config.array_length);
        output_array.resize(config.array_length, 0);
        segment_sums.resize(config.num_subarrays, 0);
        prefix_offsets.resize(config.num_subarrays, 0);

        // Initialize input
        for (int i = 0; i < config.array_length; i++) {
            input_array[i] = 1;
        }
    }

    void execute() {
        std::cout << "\n=== Prefix Sum Workload (MESSAGE PASSING) ===" << std::endl;
        std::cout << "Array length: " << config.array_length << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: MESSAGE PASSING" << std::endl;
        std::cout << "Algorithm: Segmented scan with boundary exchange" << std::endl;

        computeLocalScans();
        computeSegmentOffsets();
        adjustWithOffsets();
        printMetrics();
    }

private:
    void computeLocalScans() {
        std::cout << "\nPhase 1: Local prefix sum (independent)" << std::endl;

        int segment_size = config.array_length / config.num_subarrays;

        for (int sa = 0; sa < config.num_subarrays; sa++) {
            int start = sa * segment_size;
            int end = start + segment_size;

            // Compute local exclusive prefix sum
            output_array[start] = 0;  // Exclusive scan
            int running_sum = 0;

            for (int i = start; i < end; i++) {
                output_array[i] = running_sum;
                running_sum += input_array[i];
                metrics.add_ops++;
                metrics.total_cycles += config.read_latency + config.write_latency + config.compute_latency;
            }

            segment_sums[sa] = running_sum;
            std::cout << "  Subarray " << sa << ": segment sum = " << segment_sums[sa] << std::endl;
            metrics.compute_cycles += (end - start) * config.compute_latency;
        }
    }

    void computeSegmentOffsets() {
        std::cout << "\nPhase 2: Compute offsets via message passing" << std::endl;

        // Sequential prefix sum of segment sums (with explicit transfers)
        prefix_offsets[0] = 0;

        for (int sa = 1; sa < config.num_subarrays; sa++) {
            // Transfer previous offset and segment sum to current subarray
            transferOffset(sa - 1, sa);

            // Compute new offset
            prefix_offsets[sa] = prefix_offsets[sa - 1] + segment_sums[sa - 1];
            metrics.total_cycles += config.compute_latency;
            metrics.add_ops++;

            std::cout << "  Subarray " << sa << ": offset = " << prefix_offsets[sa]
                      << " (from subarray " << (sa - 1) << ")" << std::endl;
        }
    }

    void transferOffset(int src_subarray, int dst_subarray) {
        assert(src_subarray != dst_subarray);

        // Transfer offset value from src to dst
        metrics.intersubarray_transfers++;

        // Transfer latency
        uint64_t transfer_cycles = config.copy_latency;
        metrics.transfer_cycles += transfer_cycles;
        metrics.total_cycles += transfer_cycles;

        // Track transfer type
        if (config.copy_latency == 1) {
            metrics.direct_transfers++;
            metrics.total_energy += 0.55;  // LIBCom
        } else {
            metrics.htree_traversals++;
            metrics.total_energy += 1.0;   // Baseline
        }
    }

    void adjustWithOffsets() {
        std::cout << "\nPhase 3: Adjust local results with offsets" << std::endl;

        int segment_size = config.array_length / config.num_subarrays;

        for (int sa = 0; sa < config.num_subarrays; sa++) {
            int start = sa * segment_size;
            int end = start + segment_size;
            int adjustments = 0;

            // Add offset to all elements in segment
            for (int i = start; i < end; i++) {
                output_array[i] += prefix_offsets[sa];
                metrics.total_cycles += config.read_latency + config.write_latency + config.compute_latency;
                metrics.add_ops++;
                adjustments++;
            }

            std::cout << "  Subarray " << sa << ": " << adjustments << " adjustments" << std::endl;
            metrics.compute_cycles += adjustments * config.compute_latency;
        }

        std::cout << "\n✓ Exclusive prefix sum complete" << std::endl;
    }

    void printMetrics() {
        std::cout << "\n=== Prefix Sum Results (MESSAGE PASSING) ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Transfer cycles: " << metrics.transfer_cycles
                  << " (" << (100.0 * metrics.transfer_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total add operations: " << metrics.add_ops << std::endl;

        std::cout << "\nCommunication (Message Passing):" << std::endl;
        std::cout << "  Offset transfers: " << metrics.intersubarray_transfers << std::endl;

        if (config.copy_latency == 1) {
            std::cout << "  Direct transfers (LIBCom): " << metrics.direct_transfers << std::endl;
        } else {
            std::cout << "  H-tree traversals (Baseline): " << metrics.htree_traversals << std::endl;
        }

        std::cout << "\nEnergy:" << std::endl;
        std::cout << "  Total energy (relative): " << metrics.total_energy << std::endl;
        if (metrics.intersubarray_transfers > 0) {
            std::cout << "  Avg energy per transfer: "
                      << (metrics.total_energy / metrics.intersubarray_transfers) << std::endl;
        }

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

        int expected_transfers = config.num_subarrays - 1;
        std::cout << "  Expected offset transfers: " << expected_transfers << std::endl;
        std::cout << "  Actual transfers: " << metrics.intersubarray_transfers << std::endl;

        if (correct && metrics.intersubarray_transfers == expected_transfers) {
            std::cout << "  ✓ Prefix sum validated" << std::endl;
            std::cout << "  Sample output: [";
            for (int i = 0; i < std::min(10, config.array_length); i++) {
                std::cout << output_array[i];
                if (i < std::min(10, config.array_length) - 1) std::cout << ", ";
            }
            std::cout << ", ...]" << std::endl;
        } else {
            std::cout << "  ✗ Prefix sum validation failed!" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <array_length> <is_libcom>" << std::endl;
        std::cerr << "Note: array_length should be divisible by num_subarrays" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 2048 0   # 8 subarrays, 2048 elements, baseline" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 2048 1   # 8 subarrays, 2048 elements, LIBCom" << std::endl;
        return 1;
    }

    PrefixSumConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.array_length = std::atoi(argv[2]);
    bool is_libcom = (std::atoi(argv[3]) == 1);

    // Check if evenly divisible
    if (config.array_length % config.num_subarrays != 0) {
        std::cerr << "Error: array_length must be evenly divisible by num_subarrays" << std::endl;
        return 1;
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

    std::cout << "\n=== DAC'26 Prefix Sum Benchmark (Message Passing) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: Parallel Research Kernels (PRK), CUDA SDK" << std::endl;
    std::cout << "Programming Model: MESSAGE PASSING" << std::endl;

    PrefixSumMessage workload(config);
    workload.execute();

    return 0;
}
