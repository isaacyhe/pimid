/**
 * @file reduction_workload.cpp
 * @brief Tree Reduction workload for LIBCom evaluation
 *
 * Performs hierarchical tree reduction across all subarrays.
 * Each level halves the number of active subarrays.
 * Tests worst-case scenario for H-tree vs LIBCom direct transfers.
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>

struct ReductionConfig {
    int num_subarrays;
    int elements_per_subarray;

    int copy_latency;
    int move_latency;
    int read_latency;
    int write_latency;
    int compute_latency;  // Per-element reduction operation
};

struct ReductionMetrics {
    uint64_t total_cycles = 0;
    uint64_t compute_cycles = 0;
    uint64_t transfer_cycles = 0;
    uint64_t intersubarray_transfers = 0;
    uint64_t htree_traversals = 0;
    uint64_t direct_transfers = 0;
    uint64_t elements_transferred = 0;
    double total_energy = 0.0;
};

class ReductionWorkload {
private:
    ReductionConfig config;
    ReductionMetrics metrics;

    std::vector<std::vector<double>> subarray_data;
    std::vector<bool> active_subarrays;

public:
    ReductionWorkload(const ReductionConfig& cfg) : config(cfg) {
        subarray_data.resize(config.num_subarrays);
        active_subarrays.resize(config.num_subarrays, true);

        // Initialize each subarray with local data
        for (int i = 0; i < config.num_subarrays; i++) {
            subarray_data[i].resize(config.elements_per_subarray, 1.0);
        }
    }

    void execute() {
        std::cout << "\n=== Tree Reduction Workload Execution ===" << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Elements per subarray: " << config.elements_per_subarray << std::endl;
        std::cout << "Total elements: " << (config.num_subarrays * config.elements_per_subarray) << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;

        // Perform tree reduction
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
            int stride = 1 << (level + 1);  // Distance between paired subarrays
            int half_stride = 1 << level;

            // Process pairs at this level
            for (int i = 0; i < config.num_subarrays; i += stride) {
                int dst_subarray = i;
                int src_subarray = i + half_stride;

                // Check if both subarrays are active
                if (src_subarray < config.num_subarrays &&
                    active_subarrays[dst_subarray] &&
                    active_subarrays[src_subarray]) {

                    // Transfer data from src to dst
                    transferAndReduce(src_subarray, dst_subarray);

                    // Mark source as inactive
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
        assert(src_subarray >= 0 && src_subarray < config.num_subarrays);
        assert(dst_subarray >= 0 && dst_subarray < config.num_subarrays);

        // Transfer elements from src to dst
        metrics.intersubarray_transfers++;
        metrics.elements_transferred += config.elements_per_subarray;

        // Transfer latency
        uint64_t transfer_cycles = config.copy_latency;
        metrics.transfer_cycles += transfer_cycles;
        metrics.total_cycles += transfer_cycles;

        // Track transfer type
        if (config.copy_latency == 1) {
            metrics.direct_transfers++;
            metrics.total_energy += 0.55 * config.elements_per_subarray;  // LIBCom: 55% of baseline
        } else {
            metrics.htree_traversals++;
            metrics.total_energy += 1.0 * config.elements_per_subarray;   // Baseline: H-tree
        }

        // Reduction computation (element-wise sum)
        uint64_t compute_cycles = config.elements_per_subarray * config.compute_latency;
        metrics.compute_cycles += compute_cycles;
        metrics.total_cycles += compute_cycles;

        // Simulate the actual reduction
        for (int i = 0; i < config.elements_per_subarray; i++) {
            subarray_data[dst_subarray][i] += subarray_data[src_subarray][i];
        }
    }

    void printMetrics() {
        std::cout << "\n=== Reduction Results ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Transfer cycles: " << metrics.transfer_cycles
                  << " (" << (100.0 * metrics.transfer_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nCommunication:" << std::endl;
        std::cout << "  Inter-subarray transfers: " << metrics.intersubarray_transfers << std::endl;
        std::cout << "  Elements transferred: " << metrics.elements_transferred << std::endl;

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

        // Expected reduction levels
        int expected_levels = static_cast<int>(std::ceil(std::log2(config.num_subarrays)));
        int expected_transfers = config.num_subarrays - 1;

        std::cout << "\nValidation:" << std::endl;
        std::cout << "  Expected tree levels: " << expected_levels << std::endl;
        std::cout << "  Expected transfers: " << expected_transfers << std::endl;
        std::cout << "  Actual transfers: " << metrics.intersubarray_transfers << std::endl;

        if (metrics.intersubarray_transfers == expected_transfers) {
            std::cout << "  ✓ Transfer count validated" << std::endl;
        } else {
            std::cout << "  ✗ Transfer count mismatch!" << std::endl;
        }

        // Final result validation
        double expected_sum = config.num_subarrays * config.elements_per_subarray * 1.0;
        double actual_sum = 0.0;
        for (int i = 0; i < config.elements_per_subarray; i++) {
            actual_sum += subarray_data[0][i];
        }

        std::cout << "  Expected final sum: " << expected_sum << std::endl;
        std::cout << "  Actual final sum: " << actual_sum << std::endl;

        if (std::abs(actual_sum - expected_sum) < 1e-6) {
            std::cout << "  ✓ Reduction result validated" << std::endl;
        } else {
            std::cout << "  ✗ Reduction result incorrect!" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <elements_per_subarray> <is_libcom>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 32 1024 0   # 32 subarrays, 1024 elements each, baseline" << std::endl;
        std::cerr << "Example: " << argv[0] << " 32 1024 1   # 32 subarrays, 1024 elements each, LIBCom" << std::endl;
        return 1;
    }

    ReductionConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.elements_per_subarray = std::atoi(argv[2]);
    bool is_libcom = (std::atoi(argv[3]) == 1);

    // Validate power-of-2 subarrays for clean tree reduction
    if ((config.num_subarrays & (config.num_subarrays - 1)) != 0) {
        std::cerr << "Warning: num_subarrays should be power of 2 for balanced tree" << std::endl;
    }

    config.read_latency = 1;
    config.write_latency = 1;
    config.compute_latency = 1;  // 1 cycle per element addition

    if (is_libcom) {
        config.copy_latency = 1;
        config.move_latency = 1;
    } else {
        // H-tree latency = log2(num_subarrays)
        int htree_latency = 0;
        int n = config.num_subarrays;
        while (n > 1) { htree_latency++; n >>= 1; }
        config.copy_latency = 2 + htree_latency;  // 2 × subarray + H-tree
        config.move_latency = 2 + htree_latency;
    }

    std::cout << "\n=== DAC'26 Tree Reduction Benchmark ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Subarrays: " << config.num_subarrays << std::endl;
    std::cout << "Elements per subarray: " << config.elements_per_subarray << std::endl;
    std::cout << "Transfer latency: " << config.copy_latency << " cycles" << std::endl;

    ReductionWorkload workload(config);
    workload.execute();

    return 0;
}
