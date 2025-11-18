/**
 * @file histogram_message.cpp
 * @brief Histogram workload with MESSAGE PASSING model
 *
 * Source: Rodinia Benchmark Suite
 *
 * Message Passing Model:
 * - Each subarray maintains a LOCAL histogram
 * - Process assigned input elements independently
 * - Merge local histograms via explicit inter-subarray transfers
 * - Uses tree reduction pattern for merging
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <cmath>

struct HistogramConfig {
    int num_subarrays;
    int num_elements;
    int num_bins;

    int copy_latency;
    int read_latency;
    int write_latency;
    int atomic_latency;
};

struct HistogramMetrics {
    uint64_t total_cycles = 0;
    uint64_t compute_cycles = 0;
    uint64_t transfer_cycles = 0;
    uint64_t intersubarray_transfers = 0;
    uint64_t htree_traversals = 0;
    uint64_t direct_transfers = 0;
    uint64_t bins_transferred = 0;
    double total_energy = 0.0;
};

class HistogramMessage {
private:
    HistogramConfig config;
    HistogramMetrics metrics;

    std::vector<int> input_data;
    std::vector<std::vector<int>> local_histograms;  // Per-subarray histograms
    std::vector<int> final_histogram;
    std::vector<int> subarray_assignment;
    std::vector<bool> active_subarrays;

public:
    HistogramMessage(const HistogramConfig& cfg) : config(cfg) {
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
    }

    void execute() {
        std::cout << "\n=== Histogram Workload (MESSAGE PASSING) ===" << std::endl;
        std::cout << "Input elements: " << config.num_elements << std::endl;
        std::cout << "Histogram bins: " << config.num_bins << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: MESSAGE PASSING" << std::endl;

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
                    metrics.total_cycles += config.read_latency + config.write_latency;
                    local_histograms[sa][bin]++;
                    local_ops++;
                }
            }

            std::cout << "  Subarray " << sa << ": " << local_ops << " local updates" << std::endl;
            metrics.compute_cycles += local_ops * (config.read_latency + config.write_latency);
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

        // Transfer entire histogram (num_bins values)
        metrics.intersubarray_transfers++;
        metrics.bins_transferred += config.num_bins;

        // Transfer latency
        uint64_t transfer_cycles = config.copy_latency;
        metrics.transfer_cycles += transfer_cycles;
        metrics.total_cycles += transfer_cycles;

        // Track transfer type
        if (config.copy_latency == 1) {
            metrics.direct_transfers++;
            metrics.total_energy += 0.55 * config.num_bins;  // LIBCom
        } else {
            metrics.htree_traversals++;
            metrics.total_energy += 1.0 * config.num_bins;   // Baseline
        }

        // Merge operation (element-wise addition)
        uint64_t merge_cycles = config.num_bins * config.write_latency;
        metrics.compute_cycles += merge_cycles;
        metrics.total_cycles += merge_cycles;

        for (int bin = 0; bin < config.num_bins; bin++) {
            local_histograms[dst_subarray][bin] += local_histograms[src_subarray][bin];
        }
    }

    void printMetrics() {
        std::cout << "\n=== Histogram Results (MESSAGE PASSING) ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Transfer cycles: " << metrics.transfer_cycles
                  << " (" << (100.0 * metrics.transfer_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nCommunication (Message Passing):" << std::endl;
        std::cout << "  Histogram transfers: " << metrics.intersubarray_transfers << std::endl;
        std::cout << "  Bins transferred: " << metrics.bins_transferred << std::endl;

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
        int total_count = 0;
        int non_zero_bins = 0;
        for (int bin = 0; bin < config.num_bins; bin++) {
            total_count += final_histogram[bin];
            if (final_histogram[bin] > 0) non_zero_bins++;
        }

        int expected_transfers = config.num_subarrays - 1;
        std::cout << "  Expected histogram transfers: " << expected_transfers << std::endl;
        std::cout << "  Actual histogram transfers: " << metrics.intersubarray_transfers << std::endl;

        std::cout << "  Expected total count: " << config.num_elements << std::endl;
        std::cout << "  Actual total count: " << total_count << std::endl;
        std::cout << "  Non-zero bins: " << non_zero_bins << " / " << config.num_bins << std::endl;

        if (total_count == config.num_elements &&
            metrics.intersubarray_transfers == expected_transfers) {
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

    config.num_bins = 256;
    config.read_latency = 1;
    config.write_latency = 1;
    config.atomic_latency = 2;

    if (is_libcom) {
        config.copy_latency = 1;
    } else {
        int htree_latency = 0;
        int n = config.num_subarrays;
        while (n > 1) { htree_latency++; n >>= 1; }
        config.copy_latency = 2 + htree_latency;
    }

    std::cout << "\n=== DAC'26 Histogram Benchmark (Message Passing) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: Rodinia Benchmark Suite" << std::endl;
    std::cout << "Programming Model: MESSAGE PASSING" << std::endl;

    HistogramMessage workload(config);
    workload.execute();

    return 0;
}
