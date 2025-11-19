/**
 * @file prefixsum_shared_pimid.cpp
 * @brief Prefix Sum (Scan) workload with SHARED MEMORY model - PIMID integrated
 *
 * This version uses PIMID simulator for accurate energy and timing modeling
 * Technology: 45nm
 * Frequency: 1GHz
 * Algorithm: Blelloch's work-efficient parallel scan
 */

#include "../../../DAC26/pimid_adapter/pim_simulator.h"
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

class PrefixSumSharedPIMID {
private:
    PrefixSumConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    std::vector<int> input_array;
    std::vector<int> output_array;
    std::vector<int> element_assignment;

    uint64_t barrier_count;

public:
    PrefixSumSharedPIMID(const PrefixSumConfig& cfg) : config(cfg), barrier_count(0) {
        input_array.resize(config.array_length);
        output_array.resize(config.array_length, 0);
        element_assignment.resize(config.array_length);

        // Initialize input (1, 1, 1, ...) for easy validation
        for (int i = 0; i < config.array_length; i++) {
            input_array[i] = 1;
            element_assignment[i] = i % config.num_subarrays;
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
        std::cout << "\n=== Prefix Sum Workload (SHARED MEMORY - PIMID) ===" << std::endl;
        std::cout << "Array length: " << config.array_length << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;
        std::cout << "Algorithm: Blelloch work-efficient scan" << std::endl;

        simulator->resetStats();
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
                    if (left_subarray == idx_subarray) {
                        simulator->simulateMemoryAccess(true, true, sizeof(int));
                    } else {
                        simulator->simulateMemoryAccess(false, true, sizeof(int));
                    }

                    // Reading idx element (local)
                    simulator->simulateMemoryAccess(true, true, sizeof(int));

                    // Compute addition
                    simulator->simulateCompute(1);

                    // Write back
                    simulator->simulateMemoryAccess(true, false, sizeof(int));

                    output_array[idx] += output_array[left];
                    ops_this_level++;
                }
            }

            // Barrier: all subarrays must complete this level
            barrier();
            std::cout << "  Level " << (d + 1) << ": " << ops_this_level << " additions" << std::endl;
        }

        // Set last element to 0 for exclusive scan
        simulator->simulateMemoryAccess(true, false, sizeof(int));
        output_array[config.array_length - 1] = 0;

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
                    if (left_subarray == idx_subarray) {
                        simulator->simulateMemoryAccess(true, true, sizeof(int));
                    } else {
                        simulator->simulateMemoryAccess(false, true, sizeof(int));
                    }

                    // Reading idx element (local)
                    simulator->simulateMemoryAccess(true, true, sizeof(int));

                    // Compute addition
                    simulator->simulateCompute(1);

                    // Write to left element
                    if (left_subarray == idx_subarray) {
                        simulator->simulateMemoryAccess(true, false, sizeof(int));
                    } else {
                        simulator->simulateMemoryAccess(false, false, sizeof(int));
                    }

                    // Write to idx element
                    simulator->simulateMemoryAccess(true, false, sizeof(int));

                    // Swap and add
                    int temp = output_array[left];
                    output_array[left] = output_array[idx];
                    output_array[idx] += temp;
                    ops_this_level++;
                }
            }

            // Barrier
            barrier();
            std::cout << "  Level " << (depth - d) << ": " << ops_this_level << " operations" << std::endl;
        }

        std::cout << "\n✓ Exclusive prefix sum complete" << std::endl;
    }

    void barrier() {
        // Synchronization barrier for shared memory model
        barrier_count++;
        simulator->simulateOperation(PIMOperation::BARRIER_SYNC, 1);
    }

    void printMetrics() {
        std::cout << "\n=== Prefix Sum Results (SHARED MEMORY - PIMID) ===" << std::endl;

        const SimulationResults& results = simulator->getResults();

        std::cout << "\nTotal cycles: " << results.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << results.compute_cycles
                  << " (" << (100.0 * results.compute_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Memory cycles: " << results.memory_cycles
                  << " (" << (100.0 * results.memory_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Network cycles: " << results.network_cycles
                  << " (" << (100.0 * results.network_cycles / results.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Compute operations: " << results.compute_ops << std::endl;
        std::cout << "  Synchronization barriers: " << barrier_count << std::endl;

        std::cout << "\nCommunication (Shared Memory):" << std::endl;
        std::cout << "  Inter-subarray transfers: 0 (shared memory model)" << std::endl;
        std::cout << "  Local reads: " << results.local_reads << std::endl;
        std::cout << "  Local writes: " << results.local_writes << std::endl;
        std::cout << "  Remote accesses: " << results.remote_reads + results.remote_writes << std::endl;

        std::cout << "\nEnergy (PIMID-based):" << std::endl;
        std::cout << "  Total energy: " << results.total_energy_pJ << " pJ" << std::endl;
        std::cout << "  Compute energy: " << results.compute_energy_pJ << " pJ ("
                  << (100.0 * results.compute_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;
        std::cout << "  Memory energy: " << results.memory_energy_pJ << " pJ ("
                  << (100.0 * results.memory_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;
        std::cout << "  Network energy: " << results.network_energy_pJ << " pJ ("
                  << (100.0 * results.network_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;

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
    config.topology = is_libcom ? Topology::LIBCOM : Topology::HTREE_BASELINE;

    // Check if array length is power of 2
    if ((config.array_length & (config.array_length - 1)) != 0) {
        std::cerr << "Warning: array_length should be power of 2 for optimal Blelloch scan" << std::endl;
    }

    std::cout << "\n=== DAC'26 Prefix Sum Benchmark (Shared Memory - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    PrefixSumSharedPIMID workload(config);
    workload.execute();

    return 0;
}
