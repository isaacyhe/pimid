/**
 * @file reduction_shared_pimid.cpp
 * @brief Tree Reduction workload with SHARED MEMORY model - PIMID integrated
 *
 * This version uses PIMID simulator for accurate energy and timing modeling
 * Technology: 45nm
 * Frequency: 1GHz
 */

#include "../../../DAC26/pimid_adapter/pim_simulator.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>

using namespace dac26;

struct ReductionConfig {
    int num_subarrays;
    int elements_per_subarray;
    Topology topology;
};

class ReductionSharedPIMID {
private:
    ReductionConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    std::vector<double> shared_data;
    std::vector<double> partial_sums;
    double final_result;

public:
    ReductionSharedPIMID(const ReductionConfig& cfg) : config(cfg), final_result(0.0) {
        int total_elements = config.num_subarrays * config.elements_per_subarray;
        shared_data.resize(total_elements, 1.0);
        partial_sums.resize(config.num_subarrays, 0.0);

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
        std::cout << "\n=== Tree Reduction Workload (SHARED MEMORY - PIMID) ===" << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Elements per subarray: " << config.elements_per_subarray << std::endl;
        std::cout << "Total elements: " << (config.num_subarrays * config.elements_per_subarray) << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;

        simulator->resetStats();
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
        simulator->simulateOperation(PIMOperation::BARRIER_SYNC, 1);

        // Phase 2: Atomic accumulation
        std::cout << "\nPhase 2: Atomic accumulation to shared result" << std::endl;
        accumulatePartialSums();

        // Barrier synchronization
        std::cout << "\nFinal barrier synchronization" << std::endl;
        simulator->simulateOperation(PIMOperation::BARRIER_SYNC, 1);

        std::cout << "\n✓ Reduction complete - result: " << final_result << std::endl;
    }

    void computeLocalPartialSums() {
        // Each subarray computes partial sum of its assigned elements
        for (int sa = 0; sa < config.num_subarrays; sa++) {
            int start_idx = sa * config.elements_per_subarray;
            int end_idx = start_idx + config.elements_per_subarray;

            for (int i = start_idx; i < end_idx; i++) {
                // Read from shared memory (local to this subarray)
                simulator->simulateMemoryAccess(true, true, sizeof(double));

                // Accumulate (compute operation)
                simulator->simulateCompute(1);

                partial_sums[sa] += shared_data[i];
            }

            std::cout << "  Subarray " << sa << ": partial sum = " << partial_sums[sa]
                      << " (" << config.elements_per_subarray << " elements)" << std::endl;
        }
    }

    void accumulatePartialSums() {
        // Each subarray atomically adds its partial sum to final result
        // Assume final_result resides in subarray 0
        uint64_t remote_accesses = 0;

        for (int sa = 0; sa < config.num_subarrays; sa++) {
            // Atomic add to shared result
            if (sa == 0) {
                // Local access for subarray 0
                simulator->simulateMemoryAccess(true, false, sizeof(double));
            } else {
                // Remote access for other subarrays
                simulator->simulateMemoryAccess(false, false, sizeof(double));
                remote_accesses++;
            }

            simulator->simulateOperation(PIMOperation::ATOMIC_OP, 1);
            final_result += partial_sums[sa];
        }

        std::cout << "  " << config.num_subarrays << " atomic additions" << std::endl;
        std::cout << "  " << remote_accesses << " remote memory accesses" << std::endl;
    }

    void printMetrics() {
        std::cout << "\n=== Reduction Results (SHARED MEMORY - PIMID) ===" << std::endl;

        const SimulationResults& results = simulator->getResults();

        std::cout << "\nTotal cycles: " << results.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << results.compute_cycles
                  << " (" << (100.0 * results.compute_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Memory cycles: " << results.memory_cycles
                  << " (" << (100.0 * results.memory_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Network cycles: " << results.network_cycles
                  << " (" << (100.0 * results.network_cycles / results.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Elements processed: " << (config.num_subarrays * config.elements_per_subarray) << std::endl;
        std::cout << "  Compute operations: " << results.compute_ops << std::endl;

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

        uint64_t total_remote = results.remote_reads + results.remote_writes;
        if (total_remote > 0) {
            std::cout << "  Avg energy per remote access: "
                      << (results.network_energy_pJ / total_remote) << " pJ" << std::endl;
        }

        std::cout << "\nPerformance:" << std::endl;
        std::cout << "  Execution time: " << results.execution_time_ns << " ns" << std::endl;
        std::cout << "  Execution time: " << (results.execution_time_ns / 1000.0) << " us" << std::endl;

        // Validation
        double expected_sum = config.num_subarrays * config.elements_per_subarray * 1.0;

        std::cout << "\nValidation:" << std::endl;
        std::cout << "  Expected final sum: " << expected_sum << std::endl;
        std::cout << "  Actual final sum: " << final_result << std::endl;

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
    config.topology = is_libcom ? Topology::LIBCOM : Topology::HTREE_BASELINE;

    std::cout << "\n=== DAC'26 Tree Reduction Benchmark (Shared Memory - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    ReductionSharedPIMID workload(config);
    workload.execute();

    return 0;
}
