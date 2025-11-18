/**
 * @file stencil1d_shared.cpp
 * @brief 1D Stencil (Heat Equation/Jacobi) workload with SHARED MEMORY model
 *
 * Source: Parallel Research Kernels (PRK), MiniFE
 * Algorithm: Iterative 3-point stencil (left, center, right)
 *
 * Shared Memory Model:
 * - Grid stored in shared memory
 * - Each subarray updates its assigned points
 * - Synchronization barriers between iterations
 * - All subarrays see consistent boundary values
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
    uint64_t sync_cycles = 0;
    uint64_t barrier_count = 0;
    uint64_t stencil_ops = 0;
    double total_energy = 0.0;
};

class Stencil1DShared {
private:
    Stencil1DConfig config;
    Stencil1DMetrics metrics;

    std::vector<double> grid_old;
    std::vector<double> grid_new;
    std::vector<int> point_assignment;

public:
    Stencil1DShared(const Stencil1DConfig& cfg) : config(cfg) {
        grid_old.resize(config.grid_size);
        grid_new.resize(config.grid_size);
        point_assignment.resize(config.grid_size);

        // Initialize grid (boundary conditions + initial values)
        for (int i = 0; i < config.grid_size; i++) {
            if (i == 0 || i == config.grid_size - 1) {
                grid_old[i] = 0.0;  // Boundary conditions
            } else {
                grid_old[i] = 1.0;  // Initial temperature
            }
            grid_new[i] = grid_old[i];
            point_assignment[i] = i % config.num_subarrays;
        }
    }

    void execute() {
        std::cout << "\n=== 1D Stencil Workload (SHARED MEMORY) ===" << std::endl;
        std::cout << "Grid size: " << config.grid_size << std::endl;
        std::cout << "Iterations: " << config.num_iterations << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;
        std::cout << "Stencil: 3-point (left, center, right)" << std::endl;

        computeStencil();
        printMetrics();
    }

private:
    void computeStencil() {
        std::cout << "\nIterative stencil computation (synchronized)" << std::endl;

        for (int iter = 0; iter < config.num_iterations; iter++) {
            int ops_this_iter = 0;

            // Each subarray updates its assigned interior points
            for (int sa = 0; sa < config.num_subarrays; sa++) {
                int local_ops = 0;

                for (int i = 1; i < config.grid_size - 1; i++) {
                    if (point_assignment[i] == sa) {
                        // 3-point stencil: average of left, center, right
                        // Read from shared memory (3 reads)
                        metrics.total_cycles += 3 * config.read_latency;

                        // Compute average
                        metrics.total_cycles += 2 * config.compute_latency;  // 2 adds + divide
                        grid_new[i] = (grid_old[i - 1] + grid_old[i] + grid_old[i + 1]) / 3.0;

                        // Write back
                        metrics.total_cycles += config.write_latency;

                        metrics.stencil_ops++;
                        local_ops++;
                        ops_this_iter++;
                    }
                }
            }

            metrics.compute_cycles += ops_this_iter * 2 * config.compute_latency;

            // Barrier: all subarrays must complete before next iteration
            barrier();

            // Swap grids for next iteration
            std::swap(grid_old, grid_new);

            if ((iter + 1) % 10 == 0 || iter == 0 || iter == config.num_iterations - 1) {
                std::cout << "  Iteration " << (iter + 1) << ": " << ops_this_iter << " stencil operations" << std::endl;
            }
        }

        std::cout << "\n✓ Stencil computation complete" << std::endl;
    }

    void barrier() {
        // Synchronization barrier for shared memory model
        metrics.barrier_count++;
        metrics.sync_cycles += 10;
        metrics.total_cycles += 10;
    }

    void printMetrics() {
        std::cout << "\n=== 1D Stencil Results (SHARED MEMORY) ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Sync cycles: " << metrics.sync_cycles
                  << " (" << (100.0 * metrics.sync_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total stencil operations: " << metrics.stencil_ops << std::endl;
        std::cout << "  Operations per iteration: " << (metrics.stencil_ops / config.num_iterations) << std::endl;
        std::cout << "  Synchronization barriers: " << metrics.barrier_count << std::endl;

        std::cout << "\nCommunication (Shared Memory):" << std::endl;
        std::cout << "  Inter-subarray transfers: 0 (shared memory model)" << std::endl;
        std::cout << "  Boundary access: implicit via shared memory" << std::endl;

        // Validation
        std::cout << "\nValidation:" << std::endl;
        std::cout << "  Interior points computed: " << (config.grid_size - 2) << std::endl;
        std::cout << "  Expected stencil ops: " << ((config.grid_size - 2) * config.num_iterations) << std::endl;
        std::cout << "  Actual stencil ops: " << metrics.stencil_ops << std::endl;

        // Check boundaries remain zero
        bool boundaries_correct = (grid_old[0] == 0.0 && grid_old[config.grid_size - 1] == 0.0);

        if (metrics.stencil_ops == (config.grid_size - 2) * config.num_iterations && boundaries_correct) {
            std::cout << "  ✓ Stencil validated" << std::endl;
            std::cout << "  Boundary values: [" << grid_old[0] << ", ..., "
                      << grid_old[config.grid_size - 1] << "]" << std::endl;
        } else {
            std::cout << "  ✗ Stencil validation failed!" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <grid_size> <num_iterations> <is_libcom>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 512 100 0   # 8 subarrays, 512 points, 100 iters, baseline" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 512 100 1   # 8 subarrays, 512 points, 100 iters, LIBCom" << std::endl;
        return 1;
    }

    Stencil1DConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.grid_size = std::atoi(argv[2]);
    config.num_iterations = std::atoi(argv[3]);
    bool is_libcom = (std::atoi(argv[4]) == 1);

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

    std::cout << "\n=== DAC'26 1D Stencil Benchmark (Shared Memory) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: Parallel Research Kernels (PRK), MiniFE" << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;

    Stencil1DShared workload(config);
    workload.execute();

    return 0;
}
