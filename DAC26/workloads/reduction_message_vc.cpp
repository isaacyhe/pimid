/**
 * @file reduction_message_vc.cpp
 * @brief Tree Reduction with Virtual Channel support
 *
 * Extended version that models:
 * - Network contention in H-tree
 * - Virtual Channels (1, 2, 4 VCs)
 * - Head-of-line blocking effects
 */

#include "network_contention.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <iomanip>

using namespace dac26;

struct ReductionConfig {
    int num_subarrays;
    int elements_per_subarray;
    int num_vcs;  // NEW: Virtual channels per link

    int copy_latency;  // Base latency (without contention)
    int read_latency;
    int write_latency;
    int compute_latency;

    NetworkContentionModel::Topology topology;
};

struct ReductionMetrics {
    uint64_t total_cycles = 0;
    uint64_t compute_cycles = 0;
    uint64_t transfer_cycles = 0;
    uint64_t transfer_cycles_base = 0;  // Without contention
    uint64_t contention_cycles = 0;     // NEW: Cycles due to contention
    uint64_t blocked_cycles = 0;        // NEW: Cycles blocked by VCs
    uint64_t intersubarray_transfers = 0;
    uint64_t htree_traversals = 0;
    uint64_t direct_transfers = 0;
    uint64_t elements_transferred = 0;
    double total_energy = 0.0;
    double avg_contention = 0.0;
};

class ReductionMessageVC {
private:
    ReductionConfig config;
    ReductionMetrics metrics;
    NetworkContentionModel* network;  // Network simulator

    std::vector<std::vector<double>> subarray_data;
    std::vector<bool> active_subarrays;

public:
    ReductionMessageVC(const ReductionConfig& cfg) : config(cfg) {
        subarray_data.resize(config.num_subarrays);
        active_subarrays.resize(config.num_subarrays, true);

        // Initialize network model
        network = new NetworkContentionModel(
            config.num_subarrays,
            config.num_vcs,
            config.topology
        );

        // Initialize each subarray with local data
        for (int i = 0; i < config.num_subarrays; i++) {
            subarray_data[i].resize(config.elements_per_subarray, 1.0);
        }
    }

    ~ReductionMessageVC() {
        delete network;
    }

    void execute() {
        std::cout << "\n=== Tree Reduction with Virtual Channels ===" << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Elements per subarray: " << config.elements_per_subarray << std::endl;
        std::cout << "Total elements: " << (config.num_subarrays * config.elements_per_subarray) << std::endl;
        std::cout << "Virtual Channels: " << config.num_vcs << " VCs" << std::endl;
        std::cout << "Topology: " << (config.topology == NetworkContentionModel::Topology::H_TREE ? "H-tree" : "LIBCom") << std::endl;
        std::cout << "Base copy latency: " << config.copy_latency << " cycles" << std::endl;

        // H-tree switches info
        if (config.topology == NetworkContentionModel::Topology::H_TREE) {
            int num_switches = config.num_subarrays - 1;
            std::cout << "H-tree switches: " << num_switches
                      << " (" << config.num_subarrays << " - 1)" << std::endl;
            std::cout << "Control overhead: " << (num_switches * 2)
                      << " bits (" << num_switches << " switches × 2-bit control)" << std::endl;
        }

        performTreeReduction();
        printMetrics();
    }

private:
    void performTreeReduction() {
        int num_levels = static_cast<int>(std::ceil(std::log2(config.num_subarrays)));

        std::cout << "\n=== Reduction Tree ===" << std::endl;
        std::cout << "Number of levels: " << num_levels << std::endl;

        int active_count = config.num_subarrays;
        uint64_t current_cycle = 0;

        for (int level = 0; level < num_levels; level++) {
            std::cout << "\nLevel " << (level + 1) << ":" << std::endl;

            int transfers_this_level = 0;
            int stride = 1 << (level + 1);
            int half_stride = 1 << level;

            std::vector<int> completion_cycles;

            // Process all pairs at this level (potentially concurrent!)
            for (int i = 0; i < config.num_subarrays; i += stride) {
                int dst_subarray = i;
                int src_subarray = i + half_stride;

                if (src_subarray < config.num_subarrays &&
                    active_subarrays[dst_subarray] &&
                    active_subarrays[src_subarray]) {

                    // Submit transfer to network
                    int completion = transferAndReduce(src_subarray, dst_subarray, current_cycle);
                    completion_cycles.push_back(completion);

                    active_subarrays[src_subarray] = false;
                    transfers_this_level++;
                }
            }

            // All transfers at this level complete when the last one finishes
            if (!completion_cycles.empty()) {
                int max_completion = *std::max_element(
                    completion_cycles.begin(), completion_cycles.end());

                // Advance to completion
                current_cycle = max_completion;
            }

            active_count = (active_count + 1) / 2;

            std::cout << "  Concurrent transfers: " << transfers_this_level << std::endl;
            std::cout << "  Level completion cycle: " << current_cycle << std::endl;
            std::cout << "  Active subarrays remaining: " << active_count << std::endl;

            // Collect network stats for this level
            auto net_stats = network->getStats();
            if (net_stats.blocked_cycles > 0) {
                std::cout << "  Contention blocked cycles: " << net_stats.blocked_cycles << std::endl;
            }
        }

        metrics.total_cycles = current_cycle;

        std::cout << "\n✓ Reduction complete - final result in subarray 0" << std::endl;
    }

    int transferAndReduce(int src_subarray, int dst_subarray, uint64_t start_cycle) {
        assert(src_subarray != dst_subarray);

        metrics.intersubarray_transfers++;
        metrics.elements_transferred += config.elements_per_subarray;

        // Submit transfer to network model (returns latency in cycles)
        int data_bytes = config.elements_per_subarray * 4;  // 4 bytes per element
        int transfer_latency = network->submitTransfer(
            src_subarray, dst_subarray, data_bytes);

        // Update metrics
        metrics.transfer_cycles_base += config.copy_latency;
        metrics.transfer_cycles += transfer_latency;

        // Contention penalty
        if (transfer_latency > config.copy_latency) {
            uint64_t contention = transfer_latency - config.copy_latency;
            metrics.contention_cycles += contention;
        }

        // Track transfer type
        if (config.topology == NetworkContentionModel::Topology::LIBCOM) {
            metrics.direct_transfers++;
            metrics.total_energy += 0.55 * config.elements_per_subarray;
        } else {
            metrics.htree_traversals++;
            metrics.total_energy += 1.0 * config.elements_per_subarray;
        }

        // Reduction computation (element-wise sum)
        // This happens after transfer completes
        uint64_t compute_cycles = config.elements_per_subarray * config.compute_latency;
        metrics.compute_cycles += compute_cycles;

        // Perform reduction
        for (int i = 0; i < config.elements_per_subarray; i++) {
            subarray_data[dst_subarray][i] += subarray_data[src_subarray][i];
        }

        // Return completion cycle (start + transfer + compute)
        return start_cycle + transfer_latency + compute_cycles;
    }

    void printMetrics() {
        auto net_stats = network->getStats();

        std::cout << "\n=== Reduction Results (with Virtual Channels) ===" << std::endl;

        std::cout << "\nTiming:" << std::endl;
        std::cout << "  Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << std::fixed << std::setprecision(1)
                  << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Transfer cycles (actual): " << metrics.transfer_cycles
                  << " (" << (100.0 * metrics.transfer_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Transfer cycles (base): " << metrics.transfer_cycles_base << std::endl;
        std::cout << "  Contention overhead: " << metrics.contention_cycles
                  << " cycles (" << (100.0 * metrics.contention_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nVirtual Channel Impact:" << std::endl;
        std::cout << "  VCs configured: " << config.num_vcs << std::endl;
        std::cout << "  Blocked cycles (VC): " << net_stats.blocked_cycles << std::endl;
        std::cout << "  Avg contention level: " << std::fixed << std::setprecision(2)
                  << net_stats.avg_contention << std::endl;

        // Show VC benefit
        if (config.num_vcs > 1) {
            double speedup = (double)metrics.transfer_cycles_base / metrics.transfer_cycles;
            std::cout << "  VC speedup factor: " << std::setprecision(2) << speedup << "×" << std::endl;
        }

        std::cout << "\nCommunication:" << std::endl;
        std::cout << "  Inter-subarray transfers: " << metrics.intersubarray_transfers << std::endl;
        std::cout << "  Elements transferred: " << metrics.elements_transferred << std::endl;

        if (config.topology == NetworkContentionModel::Topology::LIBCOM) {
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
        int expected_levels = static_cast<int>(std::ceil(std::log2(config.num_subarrays)));
        int expected_transfers = config.num_subarrays - 1;

        std::cout << "\nValidation:" << std::endl;
        std::cout << "  Expected tree levels: " << expected_levels << std::endl;
        std::cout << "  Expected transfers: " << expected_transfers << std::endl;
        std::cout << "  Actual transfers: " << metrics.intersubarray_transfers << std::endl;

        double expected_sum = config.num_subarrays * config.elements_per_subarray * 1.0;
        double actual_sum = 0.0;
        for (int i = 0; i < config.elements_per_subarray; i++) {
            actual_sum += subarray_data[0][i];
        }

        std::cout << "  Expected final sum: " << expected_sum << std::endl;
        std::cout << "  Actual final sum: " << actual_sum << std::endl;

        if (metrics.intersubarray_transfers == expected_transfers &&
            std::abs(actual_sum - expected_sum) < 1e-6) {
            std::cout << "  ✓ Reduction validated" << std::endl;
        } else {
            std::cout << "  ✗ Validation failed!" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <elements_per_subarray> <is_libcom> <num_vcs>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 32 1024 0 1   # 32 SA, baseline H-tree, 1 VC" << std::endl;
        std::cerr << "Example: " << argv[0] << " 32 1024 0 2   # 32 SA, baseline H-tree, 2 VCs" << std::endl;
        std::cerr << "Example: " << argv[0] << " 32 1024 0 4   # 32 SA, baseline H-tree, 4 VCs" << std::endl;
        std::cerr << "Example: " << argv[0] << " 32 1024 1 1   # 32 SA, LIBCom, 1 VC" << std::endl;
        return 1;
    }

    ReductionConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.elements_per_subarray = std::atoi(argv[2]);
    bool is_libcom = (std::atoi(argv[3]) == 1);
    config.num_vcs = std::atoi(argv[4]);

    if ((config.num_subarrays & (config.num_subarrays - 1)) != 0) {
        std::cerr << "Warning: num_subarrays should be power of 2 for balanced tree" << std::endl;
    }

    if (config.num_vcs != 1 && config.num_vcs != 2 && config.num_vcs != 4) {
        std::cerr << "Error: num_vcs must be 1, 2, or 4" << std::endl;
        return 1;
    }

    config.read_latency = 1;
    config.write_latency = 1;
    config.compute_latency = 1;

    if (is_libcom) {
        config.copy_latency = 1;
        config.topology = NetworkContentionModel::Topology::LIBCOM;
    } else {
        int htree_latency = 0;
        int n = config.num_subarrays;
        while (n > 1) { htree_latency++; n >>= 1; }
        config.copy_latency = 2 + htree_latency;
        config.topology = NetworkContentionModel::Topology::H_TREE;
    }

    std::cout << "\n=== DAC'26 Tree Reduction Benchmark (VC-Aware) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Virtual Channels: " << config.num_vcs << std::endl;

    ReductionMessageVC workload(config);
    workload.execute();

    return 0;
}
