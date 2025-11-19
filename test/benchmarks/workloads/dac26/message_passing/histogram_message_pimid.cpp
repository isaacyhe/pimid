/**
 * @file histogram_message_pimid.cpp
 * @brief Histogram workload with MESSAGE PASSING model - PIMID integrated
 *
 * This version uses PIMID simulator for accurate energy and timing modeling
 * Technology: 45nm
 * Frequency: 1GHz
 *
 * Source: Rodinia Benchmark Suite
 *
 * Message Passing Model:
 * - Each subarray maintains a LOCAL histogram
 * - Process assigned input elements independently
 * - Merge local histograms via explicit inter-subarray transfers
 * - Uses tree reduction pattern for merging
 */

#include "pim_simulator.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <cmath>

using namespace dac26;

struct HistogramConfig {
    int num_subarrays;
    int num_elements;
    int num_bins;
    Topology topology;
};

class HistogramMessagePIMID {
private:
    HistogramConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    std::vector<int> input_data;
    std::vector<std::vector<int>> local_histograms;  // Per-subarray histograms
    std::vector<int> final_histogram;
    std::vector<int> subarray_assignment;
    std::vector<bool> active_subarrays;

public:
    HistogramMessagePIMID(const HistogramConfig& cfg) : config(cfg) {
        local_histograms.resize(config.num_subarrays);
        for (int i = 0; i < config.num_subarrays; i++) {
            local_histograms[i].resize(config.num_bins, 0);
        }
        final_histogram.resize(config.num_bins, 0);
        input_data.resize(config.num_elements);
        subarray_assignment.resize(config.num_elements);
        active_subarrays.resize(config.num_subarrays, true);

        // Generate input data
        for (int i = 0; i < config.num_elements; i++) {
            input_data[i] = (i * 7 + 13) % config.num_bins;
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
        std::cout << "\n=== Histogram Workload (MESSAGE PASSING - PIMID) ===" << std::endl;
        std::cout << "Input elements: " << config.num_elements << std::endl;
        std::cout << "Histogram bins: " << config.num_bins << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Programming model: MESSAGE PASSING" << std::endl;

        simulator->resetStats();
        computeLocalHistograms();
        mergeHistograms();
        printMetrics();
    }

private:
    void computeLocalHistograms() {
        std::cout << "\nPhase 1: Local histogram computation (independent)" << std::endl;

        for (int sa = 0; sa < config.num_subarrays; sa++) {
            int local_ops = 0;

            // Each subarray builds its LOCAL histogram
            for (int i = 0; i < config.num_elements; i++) {
                if (subarray_assignment[i] == sa) {
                    int value = input_data[i];
                    int bin = value % config.num_bins;

                    // Local read and update (no synchronization needed)
                    simulator->simulateMemoryAccess(true, true, sizeof(int));   // read
                    simulator->simulateMemoryAccess(true, false, sizeof(int));  // write

                    local_histograms[sa][bin]++;
                    local_ops++;
                }
            }

            std::cout << "  Subarray " << sa << ": " << local_ops << " local updates" << std::endl;
        }
    }

    void mergeHistograms() {
        std::cout << "\nPhase 2: Tree reduction to merge local histograms" << std::endl;

        int num_levels = static_cast<int>(std::ceil(std::log2(config.num_subarrays)));
        std::cout << "Merge tree levels: " << num_levels << std::endl;

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

                    // Transfer histogram from src to dst
                    transferAndMergeHistogram(src_subarray, dst_subarray);

                    active_subarrays[src_subarray] = false;
                    transfers_this_level++;
                }
            }

            std::cout << "  Histogram transfers: " << transfers_this_level << std::endl;
        }

        // Copy final result to final_histogram
        for (int bin = 0; bin < config.num_bins; bin++) {
            final_histogram[bin] = local_histograms[0][bin];
        }

        std::cout << "\n✓ Final histogram in subarray 0" << std::endl;
    }

    void transferAndMergeHistogram(int src_subarray, int dst_subarray) {
        assert(src_subarray != dst_subarray);

        // Transfer entire histogram (num_bins values) using PIMID
        uint64_t transfer_bytes = config.num_bins * sizeof(int);
        simulator->simulateNetworkTransfer(src_subarray, dst_subarray, transfer_bytes);

        // Merge operation (element-wise addition)
        simulator->simulateCompute(config.num_bins);

        for (int bin = 0; bin < config.num_bins; bin++) {
            local_histograms[dst_subarray][bin] += local_histograms[src_subarray][bin];
        }
    }

    void printMetrics() {
        std::cout << "\n=== Histogram Results (MESSAGE PASSING - PIMID) ===" << std::endl;

        const SimulationResults& results = simulator->getResults();

        std::cout << "\nTotal cycles: " << results.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << results.compute_cycles
                  << " (" << (100.0 * results.compute_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Memory cycles: " << results.memory_cycles
                  << " (" << (100.0 * results.memory_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Network cycles: " << results.network_cycles
                  << " (" << (100.0 * results.network_cycles / results.total_cycles) << "%)" << std::endl;

        std::cout << "\nCommunication (Message Passing):" << std::endl;
        int expected_transfers = config.num_subarrays - 1;
        std::cout << "  Histogram transfers: " << expected_transfers << std::endl;
        std::cout << "  Bins transferred: " << (expected_transfers * config.num_bins) << std::endl;

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
        int total_count = 0;
        int non_zero_bins = 0;
        for (int bin = 0; bin < config.num_bins; bin++) {
            total_count += final_histogram[bin];
            if (final_histogram[bin] > 0) non_zero_bins++;
        }

        std::cout << "  Expected histogram transfers: " << expected_transfers << std::endl;
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

    config.num_bins = 256;

    std::cout << "\n=== DAC'26 Histogram Benchmark (Message Passing - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: Rodinia Benchmark Suite" << std::endl;
    std::cout << "Programming Model: MESSAGE PASSING" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    HistogramMessagePIMID workload(config);
    workload.execute();

    return 0;
}
