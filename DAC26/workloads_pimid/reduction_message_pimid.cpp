/**
 * @file reduction_message_pimid.cpp
 * @brief Tree Reduction workload with MESSAGE PASSING model - PIMID integrated
 *
 * This version uses PIMID simulator for accurate energy and timing modeling
 * Technology: 45nm
 * Frequency: 1GHz
 *
 * Message Passing Model:
 * - Data distributed across subarrays
 * - Hierarchical tree reduction with explicit inter-subarray transfers
 * - Each level halves the number of active subarrays
 */

#include "../pimid_adapter/pim_simulator.h"
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

class ReductionMessagePIMID {
private:
    ReductionConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    std::vector<std::vector<double>> subarray_data;
    std::vector<bool> active_subarrays;

public:
    ReductionMessagePIMID(const ReductionConfig& cfg) : config(cfg) {
        subarray_data.resize(config.num_subarrays);
        active_subarrays.resize(config.num_subarrays, true);

        // Initialize each subarray with local data
        for (int i = 0; i < config.num_subarrays; i++) {
            subarray_data[i].resize(config.elements_per_subarray, 1.0);
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
        std::cout << "\n=== Tree Reduction Workload (MESSAGE PASSING - PIMID) ===" << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Elements per subarray: " << config.elements_per_subarray << std::endl;
        std::cout << "Total elements: " << (config.num_subarrays * config.elements_per_subarray) << std::endl;
        std::cout << "Programming model: MESSAGE PASSING" << std::endl;

        simulator->resetStats();
        performTreeReduction();
        printMetrics();
    }

private:
    void performTreeReduction() {
        int num_levels = static_cast<int>(std::ceil(std::log2(config.num_subarrays)));

        std::cout << "\n=== Reduction Tree ===" << std::endl;
        std::cout << "Number of levels: " << num_levels << std::endl;

        int active_count = config.num_subarrays;

        for (int level = 0; level < num_levels; level++) {
            std::cout << "\nLevel " << (level + 1) << ":" << std::endl;
            int transfers_this_level = 0;
            int stride = 1 << (level + 1);
            int half_stride = 1 << level;

            // Process pairs at this level
            for (int i = 0; i < config.num_subarrays; i += stride) {
                int dst_subarray = i;
                int src_subarray = i + half_stride;

                if (src_subarray < config.num_subarrays &&
                    active_subarrays[dst_subarray] &&
                    active_subarrays[src_subarray]) {

                    // Transfer and reduce
                    transferAndReduce(src_subarray, dst_subarray);

                    active_subarrays[src_subarray] = false;
                    transfers_this_level++;
                }
            }

            active_count = (active_count + 1) / 2;
            std::cout << "  Transfers: " << transfers_this_level << std::endl;
            std::cout << "  Active subarrays remaining: " << active_count << std::endl;
        }

        std::cout << "\n✓ Reduction complete - final result in subarray 0" << std::endl;
    }

    void transferAndReduce(int src_subarray, int dst_subarray) {
        assert(src_subarray != dst_subarray);

        // Transfer elements from src to dst using PIMID
        uint64_t transfer_bytes = config.elements_per_subarray * sizeof(double);
        simulator->simulateNetworkTransfer(src_subarray, dst_subarray, transfer_bytes);

        // Reduction computation (element-wise sum)
        simulator->simulateCompute(config.elements_per_subarray);

        // Perform reduction
        for (int i = 0; i < config.elements_per_subarray; i++) {
            subarray_data[dst_subarray][i] += subarray_data[src_subarray][i];
        }
    }

    void printMetrics() {
        std::cout << "\n=== Reduction Results (MESSAGE PASSING - PIMID) ===" << std::endl;

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

        std::cout << "\nCommunication (Message Passing):" << std::endl;
        int expected_transfers = config.num_subarrays - 1;
        std::cout << "  Inter-subarray transfers: " << expected_transfers << std::endl;
        std::cout << "  Elements transferred: " << (expected_transfers * config.elements_per_subarray) << std::endl;

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
        int num_levels = static_cast<int>(std::ceil(std::log2(config.num_subarrays)));
        double expected_sum = config.num_subarrays * config.elements_per_subarray * 1.0;
        double actual_sum = 0.0;
        for (int i = 0; i < config.elements_per_subarray; i++) {
            actual_sum += subarray_data[0][i];
        }

        std::cout << "\nValidation:" << std::endl;
        std::cout << "  Expected tree levels: " << num_levels << std::endl;
        std::cout << "  Expected transfers: " << expected_transfers << std::endl;
        std::cout << "  Expected final sum: " << expected_sum << std::endl;
        std::cout << "  Actual final sum: " << actual_sum << std::endl;

        if (std::abs(actual_sum - expected_sum) < 1e-6) {
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

    if ((config.num_subarrays & (config.num_subarrays - 1)) != 0) {
        std::cerr << "Warning: num_subarrays should be power of 2 for balanced tree" << std::endl;
    }

    std::cout << "\n=== DAC'26 Tree Reduction Benchmark (Message Passing - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Programming Model: MESSAGE PASSING" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    ReductionMessagePIMID workload(config);
    workload.execute();

    return 0;
}
