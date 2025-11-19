/**
 * @file histogram_shared_pimid.cpp
 * @brief Histogram workload with SHARED MEMORY model - PIMID integrated
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
#include <algorithm>

using namespace dac26;

struct HistogramConfig {
    int num_subarrays;
    int num_elements;
    int num_bins;
    Topology topology;
};

class HistogramSharedPIMID {
private:
    HistogramConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    std::vector<int> input_data;
    std::vector<int> histogram;
    std::vector<int> subarray_assignment;  // Which subarray owns each element

public:
    HistogramSharedPIMID(const HistogramConfig& cfg) : config(cfg) {
        histogram.resize(config.num_bins, 0);
        input_data.resize(config.num_elements);
        subarray_assignment.resize(config.num_elements);

        // Generate input data (values 0 to num_bins-1)
        for (int i = 0; i < config.num_elements; i++) {
            input_data[i] = (i * 7 + 13) % config.num_bins;  // Pseudo-random distribution
            subarray_assignment[i] = i % config.num_subarrays;
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
        std::cout << "\n=== Histogram Workload (SHARED MEMORY - PIMID) ===" << std::endl;
        std::cout << "Input elements: " << config.num_elements << std::endl;
        std::cout << "Histogram bins: " << config.num_bins << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;

        simulator->resetStats();
        computeHistogram();
        printMetrics();
    }

private:
    void computeHistogram() {
        // Phase 1: Each subarray processes its assigned elements
        // In shared memory model, all subarrays update the SAME histogram
        std::cout << "\nPhase 1: Parallel histogram computation (shared memory)" << std::endl;

        for (int sa = 0; sa < config.num_subarrays; sa++) {
            uint64_t local_atomic_ops = 0;

            // Process elements assigned to this subarray
            for (int i = 0; i < config.num_elements; i++) {
                if (subarray_assignment[i] == sa) {
                    int value = input_data[i];
                    int bin = value % config.num_bins;

                    // Read element from local memory
                    simulator->simulateMemoryAccess(true, true, sizeof(int));

                    // Atomic increment to shared histogram
                    // Check if bin is remote
                    int bin_owner = bin % config.num_subarrays;
                    if (bin_owner == sa) {
                        // Local atomic operation
                        simulator->simulateMemoryAccess(true, false, sizeof(int));
                    } else {
                        // Remote atomic operation
                        simulator->simulateMemoryAccess(false, false, sizeof(int));
                    }

                    simulator->simulateOperation(PIMOperation::ATOMIC_OP, 1);
                    local_atomic_ops++;

                    histogram[bin]++;
                }
            }

            std::cout << "  Subarray " << sa << ": " << local_atomic_ops << " atomic updates" << std::endl;
        }

        // Phase 2: Implicit synchronization barrier
        // All subarrays must finish before any can proceed
        std::cout << "\nPhase 2: Synchronization barrier" << std::endl;
        simulator->simulateOperation(PIMOperation::BARRIER_SYNC, 1);

        std::cout << "  All subarrays synchronized" << std::endl;
        std::cout << "  Histogram ready in shared memory" << std::endl;
    }

    void printMetrics() {
        std::cout << "\n=== Histogram Results (SHARED MEMORY - PIMID) ===" << std::endl;

        const SimulationResults& results = simulator->getResults();

        std::cout << "\nTotal cycles: " << results.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << results.compute_cycles
                  << " (" << (100.0 * results.compute_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Memory cycles: " << results.memory_cycles
                  << " (" << (100.0 * results.memory_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Network cycles: " << results.network_cycles
                  << " (" << (100.0 * results.network_cycles / results.total_cycles) << "%)" << std::endl;

        std::cout << "\nAtomic Operations:" << std::endl;
        std::cout << "  Total elements processed: " << config.num_elements << std::endl;
        std::cout << "  Avg per subarray: " << (config.num_elements / config.num_subarrays) << std::endl;

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
        int total_count = 0;
        int non_zero_bins = 0;
        for (int bin = 0; bin < config.num_bins; bin++) {
            total_count += histogram[bin];
            if (histogram[bin] > 0) non_zero_bins++;
        }

        std::cout << "  Expected total count: " << config.num_elements << std::endl;
        std::cout << "  Actual total count: " << total_count << std::endl;
        std::cout << "  Non-zero bins: " << non_zero_bins << " / " << config.num_bins << std::endl;

        if (total_count == config.num_elements) {
            std::cout << "  ✓ Histogram validated" << std::endl;
        } else {
            std::cout << "  ✗ Histogram validation failed!" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <num_elements> <is_libcom>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 2048 0   # 8 subarrays, 2048 elements, baseline" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 2048 1   # 8 subarrays, 2048 elements, LIBCom" << std::endl;
        return 1;
    }

    HistogramConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.num_elements = std::atoi(argv[2]);
    bool is_libcom = (std::atoi(argv[3]) == 1);
    config.topology = is_libcom ? Topology::LIBCOM : Topology::HTREE_BASELINE;

    config.num_bins = 256;  // Standard histogram size

    std::cout << "\n=== DAC'26 Histogram Benchmark (Shared Memory - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    HistogramSharedPIMID workload(config);
    workload.execute();

    return 0;
}
