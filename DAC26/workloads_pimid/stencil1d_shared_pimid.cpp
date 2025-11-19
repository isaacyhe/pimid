/**
 * @file stencil1d_shared_pimid.cpp
 * @brief 1D Stencil (Heat Equation/Jacobi) workload with SHARED MEMORY model - PIMID integrated
 *
 * This version uses PIMID simulator for accurate energy and timing modeling
 * Technology: 45nm
 * Frequency: 1GHz
 * Algorithm: Iterative 3-point stencil (left, center, right)
 */

#include "../pimid_adapter/pim_simulator.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>

using namespace dac26;

struct Stencil1DConfig {
    int num_subarrays;
    int grid_size;
    int num_iterations;
    Topology topology;
};

class Stencil1DSharedPIMID {
private:
    Stencil1DConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    std::vector<double> grid_old;
    std::vector<double> grid_new;
    std::vector<int> point_assignment;

    uint64_t barrier_count;
    uint64_t stencil_ops;

public:
    Stencil1DSharedPIMID(const Stencil1DConfig& cfg) : config(cfg), barrier_count(0), stencil_ops(0) {
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
        std::cout << "\n=== 1D Stencil Workload (SHARED MEMORY - PIMID) ===" << std::endl;
        std::cout << "Grid size: " << config.grid_size << std::endl;
        std::cout << "Iterations: " << config.num_iterations << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;
        std::cout << "Stencil: 3-point (left, center, right)" << std::endl;

        simulator->resetStats();
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

                        // Read left neighbor
                        if (point_assignment[i - 1] == sa) {
                            simulator->simulateMemoryAccess(true, true, sizeof(double));
                        } else {
                            simulator->simulateMemoryAccess(false, true, sizeof(double));
                        }

                        // Read center (local)
                        simulator->simulateMemoryAccess(true, true, sizeof(double));

                        // Read right neighbor
                        if (point_assignment[i + 1] == sa) {
                            simulator->simulateMemoryAccess(true, true, sizeof(double));
                        } else {
                            simulator->simulateMemoryAccess(false, true, sizeof(double));
                        }

                        // Compute average (2 adds + divide = 2 compute ops)
                        simulator->simulateCompute(2);
                        grid_new[i] = (grid_old[i - 1] + grid_old[i] + grid_old[i + 1]) / 3.0;

                        // Write back
                        simulator->simulateMemoryAccess(true, false, sizeof(double));

                        stencil_ops++;
                        local_ops++;
                        ops_this_iter++;
                    }
                }
            }

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
        barrier_count++;
        simulator->simulateOperation(PIMOperation::BARRIER_SYNC, 1);
    }

    void printMetrics() {
        std::cout << "\n=== 1D Stencil Results (SHARED MEMORY - PIMID) ===" << std::endl;

        const SimulationResults& results = simulator->getResults();

        std::cout << "\nTotal cycles: " << results.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << results.compute_cycles
                  << " (" << (100.0 * results.compute_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Memory cycles: " << results.memory_cycles
                  << " (" << (100.0 * results.memory_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Network cycles: " << results.network_cycles
                  << " (" << (100.0 * results.network_cycles / results.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total stencil operations: " << stencil_ops << std::endl;
        std::cout << "  Operations per iteration: " << (stencil_ops / config.num_iterations) << std::endl;
        std::cout << "  Synchronization barriers: " << barrier_count << std::endl;
        std::cout << "  Compute operations: " << results.compute_ops << std::endl;

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
        std::cout << "  Interior points computed: " << (config.grid_size - 2) << std::endl;
        std::cout << "  Expected stencil ops: " << ((config.grid_size - 2) * config.num_iterations) << std::endl;
        std::cout << "  Actual stencil ops: " << stencil_ops << std::endl;

        // Check boundaries remain zero
        bool boundaries_correct = (grid_old[0] == 0.0 && grid_old[config.grid_size - 1] == 0.0);

        if (stencil_ops == (config.grid_size - 2) * config.num_iterations && boundaries_correct) {
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
    config.topology = is_libcom ? Topology::LIBCOM : Topology::HTREE_BASELINE;

    std::cout << "\n=== DAC'26 1D Stencil Benchmark (Shared Memory - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    Stencil1DSharedPIMID workload(config);
    workload.execute();

    return 0;
}
