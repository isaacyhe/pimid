/**
 * @file histogram_shared.cpp
 * @brief Histogram workload with SHARED MEMORY model
 *
 * Source: Rodinia Benchmark Suite
 *
 * Shared Memory Model:
 * - All subarrays share a global histogram in memory
 * - Each subarray processes its portion of input data
 * - Updates are coordinated through synchronization
 * - Final histogram is already consolidated
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <algorithm>

struct HistogramConfig {
    int num_subarrays;
    int num_elements;
    int num_bins;

    int copy_latency;
    int read_latency;
    int write_latency;
    int atomic_latency;  // For atomic increment operations
};

struct HistogramMetrics {
    uint64_t total_cycles = 0;
    uint64_t compute_cycles = 0;
    uint64_t sync_cycles = 0;
    uint64_t atomic_ops = 0;
    uint64_t intersubarray_transfers = 0;
    uint64_t htree_traversals = 0;
    uint64_t direct_transfers = 0;
    uint64_t remote_accesses = 0;
    double total_energy = 0.0;
};

class HistogramShared {
private:
    HistogramConfig config;
    HistogramMetrics metrics;
    bool is_libcom;

    std::vector<int> input_data;
    std::vector<int> histogram;
    std::vector<int> subarray_assignment;  // Which subarray owns each element

public:
    HistogramShared(const HistogramConfig& cfg, bool use_libcom) : config(cfg), is_libcom(use_libcom) {
        histogram.resize(config.num_bins, 0);
        input_data.resize(config.num_elements);
        subarray_assignment.resize(config.num_elements);

        // Generate input data (values 0 to num_bins-1)
        for (int i = 0; i < config.num_elements; i++) {
            input_data[i] = (i * 7 + 13) % config.num_bins;  // Pseudo-random distribution
            subarray_assignment[i] = i % config.num_subarrays;
        }
    }

    void execute() {
        std::cout << "\n=== Histogram Workload (SHARED MEMORY) ===" << std::endl;
        std::cout << "Input elements: " << config.num_elements << std::endl;
        std::cout << "Histogram bins: " << config.num_bins << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;

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

                    // Read element (1 cycle)
                    metrics.total_cycles += config.read_latency;

                    // Atomic increment to shared histogram
                    // In shared memory model, this requires synchronization
                    metrics.total_cycles += config.atomic_latency;
                    metrics.atomic_ops++;
                    local_atomic_ops++;

                    // Energy tracking: check if bin is remote
                    int bin_owner = bin % config.num_subarrays;
                    if (bin_owner != sa) {
                        metrics.remote_accesses++;
                        if (is_libcom) {
                            metrics.total_energy += 0.55;
                        } else {
                            metrics.total_energy += 1.0;
                        }
                    }

                    histogram[bin]++;
                }
            }

            std::cout << "  Subarray " << sa << ": " << local_atomic_ops << " atomic updates" << std::endl;
            metrics.compute_cycles += local_atomic_ops * config.atomic_latency;
        }

        // Phase 2: Implicit synchronization barrier
        // All subarrays must finish before any can proceed
        std::cout << "\nPhase 2: Synchronization barrier" << std::endl;
        metrics.sync_cycles += 10;  // Conservative barrier overhead
        metrics.total_cycles += 10;

        std::cout << "  All subarrays synchronized" << std::endl;
        std::cout << "  Histogram ready in shared memory" << std::endl;
    }

    void printMetrics() {
        std::cout << "\n=== Histogram Results (SHARED MEMORY) ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Sync cycles: " << metrics.sync_cycles
                  << " (" << (100.0 * metrics.sync_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nAtomic Operations:" << std::endl;
        std::cout << "  Total atomic increments: " << metrics.atomic_ops << std::endl;
        std::cout << "  Avg per subarray: " << (metrics.atomic_ops / config.num_subarrays) << std::endl;

        std::cout << "\nCommunication (Shared Memory):" << std::endl;
        std::cout << "  Inter-subarray transfers: " << metrics.intersubarray_transfers << std::endl;
        std::cout << "  Remote accesses: " << metrics.remote_accesses << std::endl;
        std::cout << "  (Shared memory model - no explicit transfers)" << std::endl;

        std::cout << "\nEnergy:" << std::endl;
        std::cout << "  Total energy: " << metrics.total_energy << " pJ" << std::endl;
        if (metrics.remote_accesses > 0) {
            std::cout << "  Avg per remote access: " << (metrics.total_energy / metrics.remote_accesses) << " pJ" << std::endl;
        }

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

    config.num_bins = 256;  // Standard histogram size
    config.read_latency = 1;
    config.write_latency = 1;
    config.atomic_latency = 2;  // Atomic increment takes 2 cycles

    if (is_libcom) {
        config.copy_latency = 1;
    } else {
        int htree_latency = 0;
        int n = config.num_subarrays;
        while (n > 1) { htree_latency++; n >>= 1; }
        config.copy_latency = 2 + htree_latency;
    }

    std::cout << "\n=== DAC'26 Histogram Benchmark (Shared Memory) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: Rodinia Benchmark Suite" << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;

    HistogramShared workload(config, is_libcom);
    workload.execute();

    return 0;
}
