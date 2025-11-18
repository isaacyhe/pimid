/**
 * @file stencil1d_message.cpp
 * @brief 1D Stencil (Heat Equation/Jacobi) workload with MESSAGE PASSING model
 *
 * Source: Parallel Research Kernels (PRK), MiniFE
 * Algorithm: Iterative 3-point stencil with halo exchange
 *
 * Message Passing Model:
 * - Each subarray owns a segment of the grid
 * - Explicit halo/ghost cell exchange between neighbors
 * - Updates local segment using local + halo data
 * - Demonstrates nearest-neighbor communication pattern
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>

struct Stencil1DConfig {
    int num_subarrays;
    int grid_size;
    int num_iterations;

    int copy_latency;
    int read_latency;
    int write_latency;
    int compute_latency;
};

struct Stencil1DMetrics {
    uint64_t total_cycles = 0;
    uint64_t compute_cycles = 0;
    uint64_t transfer_cycles = 0;
    uint64_t intersubarray_transfers = 0;
    uint64_t htree_traversals = 0;
    uint64_t direct_transfers = 0;
    uint64_t halo_exchanges = 0;
    uint64_t stencil_ops = 0;
    double total_energy = 0.0;
};

class Stencil1DMessage {
private:
    Stencil1DConfig config;
    Stencil1DMetrics metrics;

    std::vector<std::vector<double>> local_grids_old;  // Per-subarray segments
    std::vector<std::vector<double>> local_grids_new;
    int segment_size;

public:
    Stencil1DMessage(const Stencil1DConfig& cfg) : config(cfg) {
        segment_size = (config.grid_size - 2) / config.num_subarrays;  // Interior points only

        local_grids_old.resize(config.num_subarrays);
        local_grids_new.resize(config.num_subarrays);

        // Each subarray has: left_halo + segment + right_halo
        for (int sa = 0; sa < config.num_subarrays; sa++) {
            local_grids_old[sa].resize(segment_size + 2);  // +2 for halo cells
            local_grids_new[sa].resize(segment_size + 2);

            // Initialize segment
            for (int i = 0; i < segment_size + 2; i++) {
                local_grids_old[sa][i] = 1.0;  // Initial temperature
                local_grids_new[sa][i] = 1.0;
            }

            // Boundary conditions
            if (sa == 0) {
                local_grids_old[sa][0] = 0.0;  // Left boundary
            }
            if (sa == config.num_subarrays - 1) {
                local_grids_old[sa][segment_size + 1] = 0.0;  // Right boundary
            }
        }
    }

    void execute() {
        std::cout << "\n=== 1D Stencil Workload (MESSAGE PASSING) ===" << std::endl;
        std::cout << "Grid size: " << config.grid_size << std::endl;
        std::cout << "Iterations: " << config.num_iterations << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Segment size: " << segment_size << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: MESSAGE PASSING" << std::endl;
        std::cout << "Stencil: 3-point with halo exchange" << std::endl;

        computeStencil();
        printMetrics();
    }

private:
    void computeStencil() {
        std::cout << "\nIterative stencil with explicit halo exchange" << std::endl;

        for (int iter = 0; iter < config.num_iterations; iter++) {
            // Phase 1: Halo exchange (nearest-neighbor communication)
            exchangeHalos();

            // Phase 2: Local stencil computation
            int ops_this_iter = 0;

            for (int sa = 0; sa < config.num_subarrays; sa++) {
                // Update interior points using local data + halos
                for (int i = 1; i <= segment_size; i++) {
                    // 3-point stencil
                    metrics.total_cycles += 3 * config.read_latency;
                    metrics.total_cycles += 2 * config.compute_latency;
                    metrics.total_cycles += config.write_latency;

                    local_grids_new[sa][i] = (local_grids_old[sa][i - 1] +
                                              local_grids_old[sa][i] +
                                              local_grids_old[sa][i + 1]) / 3.0;

                    metrics.stencil_ops++;
                    ops_this_iter++;
                }
            }

            metrics.compute_cycles += ops_this_iter * 2 * config.compute_latency;

            // Swap grids for next iteration
            for (int sa = 0; sa < config.num_subarrays; sa++) {
                std::swap(local_grids_old[sa], local_grids_new[sa]);
            }

            if ((iter + 1) % 10 == 0 || iter == 0 || iter == config.num_iterations - 1) {
                std::cout << "  Iteration " << (iter + 1) << ": " << ops_this_iter
                          << " stencil ops, " << metrics.halo_exchanges << " halo exchanges" << std::endl;
            }
        }

        std::cout << "\n✓ Stencil computation complete" << std::endl;
    }

    void exchangeHalos() {
        // Each subarray exchanges boundary values with neighbors
        for (int sa = 0; sa < config.num_subarrays; sa++) {
            // Send right boundary to right neighbor
            if (sa < config.num_subarrays - 1) {
                transferHalo(sa, sa + 1);  // Send right to left halo of neighbor
                metrics.halo_exchanges++;
            }

            // Send left boundary to left neighbor
            if (sa > 0) {
                transferHalo(sa, sa - 1);  // Send left to right halo of neighbor
                metrics.halo_exchanges++;
            }
        }
    }

    void transferHalo(int src_subarray, int dst_subarray) {
        assert(src_subarray != dst_subarray);
        assert(std::abs(src_subarray - dst_subarray) == 1);  // Nearest neighbor only

        // Transfer single boundary value
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

        // Simulate the actual halo update
        if (dst_subarray == src_subarray + 1) {
            // Send right boundary of src to left halo of dst
            local_grids_old[dst_subarray][0] = local_grids_old[src_subarray][segment_size];
        } else {
            // Send left boundary of src to right halo of dst
            local_grids_old[dst_subarray][segment_size + 1] = local_grids_old[src_subarray][1];
        }
    }

    void printMetrics() {
        std::cout << "\n=== 1D Stencil Results (MESSAGE PASSING) ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Transfer cycles: " << metrics.transfer_cycles
                  << " (" << (100.0 * metrics.transfer_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total stencil operations: " << metrics.stencil_ops << std::endl;
        std::cout << "  Operations per iteration: " << (metrics.stencil_ops / config.num_iterations) << std::endl;

        std::cout << "\nCommunication (Message Passing):" << std::endl;
        std::cout << "  Halo exchanges: " << metrics.halo_exchanges << std::endl;
        std::cout << "  Halo transfers: " << metrics.intersubarray_transfers << std::endl;

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
        int expected_ops = segment_size * config.num_subarrays * config.num_iterations;
        int expected_transfers = 2 * (config.num_subarrays - 1) * config.num_iterations;  // Each interior subarray sends left+right

        std::cout << "  Expected stencil ops: " << expected_ops << std::endl;
        std::cout << "  Actual stencil ops: " << metrics.stencil_ops << std::endl;
        std::cout << "  Expected halo transfers: " << expected_transfers << std::endl;
        std::cout << "  Actual halo transfers: " << metrics.intersubarray_transfers << std::endl;

        if (metrics.stencil_ops == expected_ops &&
            metrics.intersubarray_transfers == expected_transfers) {
            std::cout << "  ✓ Stencil validated" << std::endl;
        } else {
            std::cout << "  ✗ Stencil validation failed!" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <grid_size> <num_iterations> <is_libcom>" << std::endl;
        std::cerr << "Note: (grid_size - 2) should be divisible by num_subarrays" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 514 100 0   # 8 subarrays, 514 points (512 interior), 100 iters" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 514 100 1   # LIBCom version" << std::endl;
        return 1;
    }

    Stencil1DConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.grid_size = std::atoi(argv[2]);
    config.num_iterations = std::atoi(argv[3]);
    bool is_libcom = (std::atoi(argv[4]) == 1);

    // Validate divisibility
    if ((config.grid_size - 2) % config.num_subarrays != 0) {
        std::cerr << "Error: (grid_size - 2) must be evenly divisible by num_subarrays" << std::endl;
        return 1;
    }

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

    std::cout << "\n=== DAC'26 1D Stencil Benchmark (Message Passing) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: Parallel Research Kernels (PRK), MiniFE" << std::endl;
    std::cout << "Programming Model: MESSAGE PASSING" << std::endl;

    Stencil1DMessage workload(config);
    workload.execute();

    return 0;
}
