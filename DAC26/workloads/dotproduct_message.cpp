/**
 * @file dotproduct_message.cpp
 * @brief Dot Product workload with MESSAGE PASSING model
 *
 * Source: BLAS Level 1 (DDOT operation)
 *
 * Message Passing Model:
 * - Each subarray has local portions of vectors A and B
 * - Compute local partial sums independently
 * - Reduce partial sums via tree reduction with explicit transfers
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>

struct DotProductConfig {
    int num_subarrays;
    int vector_length;

    int copy_latency;
    int read_latency;
    int write_latency;
    int compute_latency;
};

struct DotProductMetrics {
    uint64_t total_cycles = 0;
    uint64_t compute_cycles = 0;
    uint64_t transfer_cycles = 0;
    uint64_t intersubarray_transfers = 0;
    uint64_t htree_traversals = 0;
    uint64_t direct_transfers = 0;
    uint64_t mac_ops = 0;
    double total_energy = 0.0;
};

class DotProductMessage {
private:
    DotProductConfig config;
    DotProductMetrics metrics;

    std::vector<double> vector_a;
    std::vector<double> vector_b;
    std::vector<double> partial_sums;  // Per-subarray partial sums
    std::vector<bool> active_subarrays;
    std::vector<int> element_assignment;

public:
    DotProductMessage(const DotProductConfig& cfg) : config(cfg) {
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
    }

    void execute() {
        std::cout << "\n=== Dot Product Workload (MESSAGE PASSING) ===" << std::endl;
        std::cout << "Vector length: " << config.vector_length << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: MESSAGE PASSING" << std::endl;

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
                    metrics.total_cycles += 2 * config.read_latency;

                    // Multiply-accumulate
                    metrics.total_cycles += config.compute_latency;
                    partial_sums[sa] += vector_a[i] * vector_b[i];
                    local_macs++;
                    metrics.mac_ops++;
                }
            }

            std::cout << "  Subarray " << sa << ": " << local_macs << " MACs, partial sum = "
                      << partial_sums[sa] << std::endl;
            metrics.compute_cycles += local_macs * config.compute_latency;
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

        // Transfer partial sum from src to dst
        metrics.intersubarray_transfers++;

        // Transfer latency
        uint64_t transfer_cycles = config.copy_latency;
        metrics.transfer_cycles += transfer_cycles;
        metrics.total_cycles += transfer_cycles;

        // Track transfer type
        if (config.copy_latency == 1) {
            metrics.direct_transfers++;
            metrics.total_energy += 0.55;  // LIBCom
        } else {
            metrics.htree_traversals++;
            metrics.total_energy += 1.0;   // Baseline
        }

        // Reduction (addition)
        metrics.total_cycles += config.compute_latency;
        metrics.compute_cycles += config.compute_latency;

        partial_sums[dst_subarray] += partial_sums[src_subarray];
    }

    void printMetrics() {
        std::cout << "\n=== Dot Product Results (MESSAGE PASSING) ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Transfer cycles: " << metrics.transfer_cycles
                  << " (" << (100.0 * metrics.transfer_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total MAC operations: " << metrics.mac_ops << std::endl;

        std::cout << "\nCommunication (Message Passing):" << std::endl;
        std::cout << "  Partial sum transfers: " << metrics.intersubarray_transfers << std::endl;

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
        double expected_result = 0.0;
        for (int i = 0; i < config.vector_length; i++) {
            expected_result += vector_a[i] * vector_b[i];
        }

        int expected_transfers = config.num_subarrays - 1;
        std::cout << "  Expected transfers: " << expected_transfers << std::endl;
        std::cout << "  Actual transfers: " << metrics.intersubarray_transfers << std::endl;

        std::cout << "  Expected result: " << expected_result << std::endl;
        std::cout << "  Actual result: " << partial_sums[0] << std::endl;
        std::cout << "  Difference: " << std::abs(expected_result - partial_sums[0]) << std::endl;

        if (std::abs(expected_result - partial_sums[0]) < 1e-6 &&
            metrics.intersubarray_transfers == expected_transfers) {
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

    config.read_latency = 1;
    config.write_latency = 1;
    config.compute_latency = 1;

    if (is_libcom) {
        config.copy_latency = 1;
    } else {
        int htree_latency = 0;
        int n = config.num_subarrays;
        while (n > 1) { htree_latency++; n >>= 1; }
        config.copy_latency = 2 + htree_latency;
    }

    std::cout << "\n=== DAC'26 Dot Product Benchmark (Message Passing) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: BLAS Level 1 (DDOT)" << std::endl;
    std::cout << "Programming Model: MESSAGE PASSING" << std::endl;

    DotProductMessage workload(config);
    workload.execute();

    return 0;
}
