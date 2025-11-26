/**
 * @file dotproduct_message_pimid.cpp
 * @brief Dot Product workload with MESSAGE PASSING model - PIMID integrated
 *
 * This version uses PIMID simulator for accurate energy and timing modeling
 * Technology: 45nm
 * Frequency: 1GHz
 *
 * Source: BLAS Level 1 (DDOT operation)
 *
 * Message Passing Model:
 * - Each subarray has local portions of vectors A and B
 * - Compute local partial sums independently
 * - Reduce partial sums via tree reduction with explicit transfers
 */

#include "../../../DAC26/pimid_adapter/pim_simulator.h"
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

class DotProductMessagePIMID {
private:
    DotProductConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    std::vector<double> vector_a;
    std::vector<double> vector_b;
    std::vector<double> partial_sums;  // Per-subarray partial sums
    std::vector<bool> active_subarrays;
    std::vector<int> element_assignment;

public:
    DotProductMessagePIMID(const DotProductConfig& cfg) : config(cfg) {
        vector_a.resize(config.vector_length);
        vector_b.resize(config.vector_length);
        partial_sums.resize(config.num_subarrays, 0.0);
        active_subarrays.resize(config.num_subarrays, true);
        element_assignment.resize(config.vector_length);

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
        std::cout << "\n=== Dot Product Workload (MESSAGE PASSING - PIMID) ===" << std::endl;
        std::cout << "Vector length: " << config.vector_length << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Programming model: MESSAGE PASSING" << std::endl;

        simulator->resetStats();
        computeLocalPartialSums();
        reducePartialSums();
        printMetrics();
    }

private:
    void computeLocalPartialSums() {
        std::cout << "\nPhase 1: Local partial sum computation (independent)" << std::endl;

        for (int sa = 0; sa < config.num_subarrays; sa++) {
            int local_macs = 0;

            // Each subarray computes LOCAL partial sum
            for (int i = 0; i < config.vector_length; i++) {
                if (element_assignment[i] == sa) {
                    // Local reads (no inter-subarray communication)
                    simulator->simulateMemoryAccess(true, true, 2 * sizeof(double));

                    // Multiply-accumulate
                    simulator->simulateCompute(1);
                    partial_sums[sa] += vector_a[i] * vector_b[i];
                    local_macs++;
                }
            }

            std::cout << "  Subarray " << sa << ": " << local_macs << " MACs, partial sum = "
                      << partial_sums[sa] << std::endl;
        }
    }

    void reducePartialSums() {
        std::cout << "\nPhase 2: Tree reduction of partial sums" << std::endl;

        int num_levels = static_cast<int>(std::ceil(std::log2(config.num_subarrays)));
        std::cout << "Reduction tree levels: " << num_levels << std::endl;

        for (int level = 0; level < num_levels; level++) {
            std::cout << "\nLevel " << (level + 1) << ":" << std::endl;
            int transfers_this_level = 0;
            int stride = 1 << (level + 1);
            int half_stride = 1 << level;

            for (int i = 0; i < config.num_subarrays; i += stride) {
                int dst_subarray = i;
                int src_subarray = i + half_stride;

                if (src_subarray < config.num_subarrays &&
                    active_subarrays[dst_subarray] &&
                    active_subarrays[src_subarray]) {

                    // Transfer and reduce partial sum
                    transferAndReduce(src_subarray, dst_subarray);

                    active_subarrays[src_subarray] = false;
                    transfers_this_level++;
                }
            }

            std::cout << "  Partial sum transfers: " << transfers_this_level << std::endl;
        }

        std::cout << "\n✓ Final result in subarray 0: " << partial_sums[0] << std::endl;
    }

    void transferAndReduce(int src_subarray, int dst_subarray) {
        assert(src_subarray != dst_subarray);

        // Transfer partial sum from src to dst using PIMID
        simulator->simulateNetworkTransfer(src_subarray, dst_subarray, sizeof(double));

        // Reduction (addition)
        simulator->simulateCompute(1);

        partial_sums[dst_subarray] += partial_sums[src_subarray];
    }

    void printMetrics() {
        std::cout << "\n=== Dot Product Results (MESSAGE PASSING - PIMID) ===" << std::endl;

        const SimulationResults& results = simulator->getResults();

        std::cout << "\nTotal cycles: " << results.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << results.compute_cycles
                  << " (" << (100.0 * results.compute_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Memory cycles: " << results.memory_cycles
                  << " (" << (100.0 * results.memory_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Network cycles: " << results.network_cycles
                  << " (" << (100.0 * results.network_cycles / results.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total MAC operations: " << results.compute_ops << std::endl;

        std::cout << "\nCommunication (Message Passing):" << std::endl;
        int expected_transfers = config.num_subarrays - 1;
        std::cout << "  Partial sum transfers: " << expected_transfers << std::endl;

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
        double expected_result = 0.0;
        for (int i = 0; i < config.vector_length; i++) {
            expected_result += vector_a[i] * vector_b[i];
        }

        std::cout << "  Expected transfers: " << expected_transfers << std::endl;
        std::cout << "  Expected result: " << expected_result << std::endl;
        std::cout << "  Actual result: " << partial_sums[0] << std::endl;
        std::cout << "  Difference: " << std::abs(expected_result - partial_sums[0]) << std::endl;

        if (std::abs(expected_result - partial_sums[0]) < 1e-6) {
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

    std::cout << "\n=== DAC'26 Dot Product Benchmark (Message Passing - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: BLAS Level 1 (DDOT)" << std::endl;
    std::cout << "Programming Model: MESSAGE PASSING" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    DotProductMessagePIMID workload(config);
    workload.execute();

    return 0;
}
