/**
 * @file prefixsum_message_pimid.cpp
 * @brief Prefix Sum (Scan) workload with MESSAGE PASSING model - PIMID integrated
 *
 * This version uses PIMID simulator for accurate energy and timing modeling
 * Technology: 45nm
 * Frequency: 1GHz
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

#include "pim_simulator.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>

using namespace dac26;

struct PrefixSumConfig {
    int num_subarrays;
    int array_length;
    Topology topology;
};

class PrefixSumMessagePIMID {
private:
    PrefixSumConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    std::vector<int> input_array;
    std::vector<int> output_array;
    std::vector<int> segment_sums;  // Sum of each segment
    std::vector<int> prefix_offsets;  // Offset to add to each segment

public:
    PrefixSumMessagePIMID(const PrefixSumConfig& cfg) : config(cfg) {
        input_array.resize(config.array_length);
        output_array.resize(config.array_length, 0);
        segment_sums.resize(config.num_subarrays, 0);
        prefix_offsets.resize(config.num_subarrays, 0);

        // Initialize input
        for (int i = 0; i < config.array_length; i++) {
            input_array[i] = 1;
        }

        // Initialize PIMID simulator
        PIMConfig pim_config;
        pim_config.tech_node_nm = 45;
        pim_config.frequency_ghz = 1.0;
        pim_config.num_subarrays = config.num_subarrays;
        pim_config.topology = config.topology;

        simulator = std::make_shared<PIMSimulator>(pim_config);
        simulator->initialize();
    }

    void execute() {
        std::cout << "\n=== Prefix Sum Workload (MESSAGE PASSING - PIMID) ===" << std::endl;
        std::cout << "Array length: " << config.array_length << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Programming model: MESSAGE PASSING" << std::endl;
        std::cout << "Algorithm: Segmented scan with boundary exchange" << std::endl;

        simulator->resetStats();
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

                // Local memory accesses
                simulator->simulateMemoryAccess(true, true, sizeof(int));   // read
                simulator->simulateMemoryAccess(true, false, sizeof(int));  // write
                simulator->simulateCompute(1);  // add
            }

            segment_sums[sa] = running_sum;
            std::cout << "  Subarray " << sa << ": segment sum = " << segment_sums[sa] << std::endl;
        }
    }

    void computeSegmentOffsets() {
        std::cout << "\nPhase 2: Compute offsets via message passing" << std::endl;

        // Sequential prefix sum of segment sums (with explicit transfers)
        prefix_offsets[0] = 0;

        for (int sa = 1; sa < config.num_subarrays; sa++) {
            // Transfer previous offset and segment sum to current subarray using PIMID
            simulator->simulateNetworkTransfer(sa - 1, sa, 2 * sizeof(int));

            // Compute new offset
            prefix_offsets[sa] = prefix_offsets[sa - 1] + segment_sums[sa - 1];
            simulator->simulateCompute(1);  // add

            std::cout << "  Subarray " << sa << ": offset = " << prefix_offsets[sa]
                      << " (from subarray " << (sa - 1) << ")" << std::endl;
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

                // Local memory accesses
                simulator->simulateMemoryAccess(true, true, sizeof(int));   // read
                simulator->simulateMemoryAccess(true, false, sizeof(int));  // write
                simulator->simulateCompute(1);  // add

                adjustments++;
            }

            std::cout << "  Subarray " << sa << ": " << adjustments << " adjustments" << std::endl;
        }

        std::cout << "\n✓ Exclusive prefix sum complete" << std::endl;
    }

    void printMetrics() {
        std::cout << "\n=== Prefix Sum Results (MESSAGE PASSING - PIMID) ===" << std::endl;

        const SimulationResults& results = simulator->getResults();

        std::cout << "\nTotal cycles: " << results.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << results.compute_cycles
                  << " (" << (100.0 * results.compute_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Memory cycles: " << results.memory_cycles
                  << " (" << (100.0 * results.memory_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Network cycles: " << results.network_cycles
                  << " (" << (100.0 * results.network_cycles / results.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total add operations: " << results.compute_ops << std::endl;

        std::cout << "\nCommunication (Message Passing):" << std::endl;
        int expected_transfers = config.num_subarrays - 1;
        std::cout << "  Offset transfers: " << expected_transfers << std::endl;

        std::cout << "\nEnergy (PIMID-based):" << std::endl;
        std::cout << "  Total energy: " << results.total_energy_pJ << " pJ" << std::endl;
        std::cout << "  Compute energy: " << results.compute_energy_pJ << " pJ ("
                  << (100.0 * results.compute_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;
        std::cout << "  Memory energy: " << results.memory_energy_pJ << " pJ ("
                  << (100.0 * results.memory_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;
        std::cout << "  Network energy: " << results.network_energy_pJ << " pJ ("
                  << (100.0 * results.network_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;

        if (expected_transfers > 0) {
            std::cout << "  Avg energy per transfer: "
                      << (results.network_energy_pJ / expected_transfers) << " pJ" << std::endl;
        }

        std::cout << "\nPerformance:" << std::endl;
        std::cout << "  Execution time: " << results.execution_time_ns << " ns" << std::endl;
        std::cout << "  Execution time: " << (results.execution_time_ns / 1000.0) << " us" << std::endl;

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

        std::cout << "  Expected offset transfers: " << expected_transfers << std::endl;

        if (correct) {
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
    config.topology = is_libcom ? Topology::LIBCOM : Topology::HTREE_BASELINE;

    // Check if evenly divisible
    if (config.array_length % config.num_subarrays != 0) {
        std::cerr << "Error: array_length must be evenly divisible by num_subarrays" << std::endl;
        return 1;
    }

    std::cout << "\n=== DAC'26 Prefix Sum Benchmark (Message Passing - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: Parallel Research Kernels (PRK), CUDA SDK" << std::endl;
    std::cout << "Programming Model: MESSAGE PASSING" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    PrefixSumMessagePIMID workload(config);
    workload.execute();

    return 0;
}
