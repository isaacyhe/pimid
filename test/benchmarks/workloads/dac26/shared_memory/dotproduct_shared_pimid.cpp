/**
 * @file dotproduct_shared_pimid.cpp
 * @brief Dot Product workload with SHARED MEMORY model - PIMID integrated
 *
 * This version uses PIMID simulator for accurate energy and timing modeling
 * Technology: 45nm
 * Frequency: 1GHz
 */

#include "pim_simulator.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>

using namespace dac26;

struct DotProductConfig {
    int num_subarrays;
    int vector_length;
    Topology topology;
};

class DotProductSharedPIMID {
private:
    DotProductConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    std::vector<double> vector_a;
    std::vector<double> vector_b;
    std::vector<int> element_assignment;  // Which subarray processes each element
    std::vector<double> partial_sums;
    double shared_result;

public:
    DotProductSharedPIMID(const DotProductConfig& cfg) : config(cfg), shared_result(0.0) {
        vector_a.resize(config.vector_length);
        vector_b.resize(config.vector_length);
        element_assignment.resize(config.vector_length);
        partial_sums.resize(config.num_subarrays, 0.0);

        // Initialize vectors
        for (int i = 0; i < config.vector_length; i++) {
            vector_a[i] = 1.0 + (i % 100) / 100.0;
            vector_b[i] = 2.0 + (i % 50) / 50.0;
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
        std::cout << "\n=== Dot Product Workload (SHARED MEMORY - PIMID) ===" << std::endl;
        std::cout << "Vector length: " << config.vector_length << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;

        simulator->resetStats();
        computeDotProduct();
        printMetrics();
    }

private:
    void computeDotProduct() {
        std::cout << "\nPhase 1: Parallel partial sum computation" << std::endl;

        // Each subarray computes its partial sum
        for (int sa = 0; sa < config.num_subarrays; sa++) {
            int local_macs = 0;

            for (int i = 0; i < config.vector_length; i++) {
                if (element_assignment[i] == sa) {
                    // Read A[i] and B[i] from shared memory (local)
                    simulator->simulateMemoryAccess(true, true, sizeof(double));
                    simulator->simulateMemoryAccess(true, true, sizeof(double));

                    // Multiply-accumulate
                    simulator->simulateCompute(1);
                    partial_sums[sa] += vector_a[i] * vector_b[i];
                    local_macs++;
                }
            }

            std::cout << "  Subarray " << sa << ": " << local_macs << " MACs, partial sum = "
                      << partial_sums[sa] << std::endl;
        }

        // Barrier synchronization
        std::cout << "\nBarrier synchronization" << std::endl;
        simulator->simulateOperation(PIMOperation::BARRIER_SYNC, 1);

        // Phase 2: Accumulate to shared result with synchronization
        std::cout << "\nPhase 2: Atomic accumulation to shared result" << std::endl;

        uint64_t remote_accesses = 0;

        for (int sa = 0; sa < config.num_subarrays; sa++) {
            // Atomic add to shared result (assume result in subarray 0)
            if (sa == 0) {
                simulator->simulateMemoryAccess(true, false, sizeof(double));
            } else {
                simulator->simulateMemoryAccess(false, false, sizeof(double));
                remote_accesses++;
            }

            simulator->simulateOperation(PIMOperation::ATOMIC_OP, 1);

            shared_result += partial_sums[sa];
        }

        std::cout << "  " << config.num_subarrays << " atomic additions to shared result" << std::endl;
        std::cout << "  " << remote_accesses << " remote memory accesses" << std::endl;

        // Final barrier synchronization
        std::cout << "\nFinal barrier synchronization" << std::endl;
        simulator->simulateOperation(PIMOperation::BARRIER_SYNC, 1);
        std::cout << "  Final result: " << shared_result << std::endl;
    }

    void printMetrics() {
        std::cout << "\n=== Dot Product Results (SHARED MEMORY - PIMID) ===" << std::endl;

        const SimulationResults& results = simulator->getResults();

        std::cout << "\nTotal cycles: " << results.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << results.compute_cycles
                  << " (" << (100.0 * results.compute_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Memory cycles: " << results.memory_cycles
                  << " (" << (100.0 * results.memory_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Network cycles: " << results.network_cycles
                  << " (" << (100.0 * results.network_cycles / results.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total MAC operations: " << config.vector_length << std::endl;
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

        std::cout << "\nPerformance:" << std::endl;
        std::cout << "  Execution time: " << results.execution_time_ns << " ns" << std::endl;
        std::cout << "  Execution time: " << (results.execution_time_ns / 1000.0) << " us" << std::endl;

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
    config.topology = is_libcom ? Topology::LIBCOM : Topology::HTREE_BASELINE;

    std::cout << "\n=== DAC'26 Dot Product Benchmark (Shared Memory - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    DotProductSharedPIMID workload(config);
    workload.execute();

    return 0;
}
