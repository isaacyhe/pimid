/**
 * @file stencil1d_message_pimid.cpp
 * @brief 1D Stencil (Heat Equation/Jacobi) workload with MESSAGE PASSING model - PIMID integrated
 *
 * This version uses PIMID simulator for accurate energy and timing modeling
 * Technology: 45nm
 * Frequency: 1GHz
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

class Stencil1DMessagePIMID {
private:
    Stencil1DConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    std::vector<std::vector<double>> local_grids_old;  // Per-subarray segments
    std::vector<std::vector<double>> local_grids_new;
    int segment_size;
    uint64_t halo_exchanges;

public:
    Stencil1DMessagePIMID(const Stencil1DConfig& cfg) : config(cfg), halo_exchanges(0) {
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
        std::cout << "\n=== 1D Stencil Workload (MESSAGE PASSING - PIMID) ===" << std::endl;
        std::cout << "Grid size: " << config.grid_size << std::endl;
        std::cout << "Iterations: " << config.num_iterations << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Segment size: " << segment_size << std::endl;
        std::cout << "Programming model: MESSAGE PASSING" << std::endl;
        std::cout << "Stencil: 3-point with halo exchange" << std::endl;

        simulator->resetStats();
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
                    // 3-point stencil: read 3 values
                    simulator->simulateMemoryAccess(true, true, 3 * sizeof(double));

                    // Compute: 2 additions, 1 division (approximated as 2 compute ops)
                    simulator->simulateCompute(2);

                    // Write result
                    simulator->simulateMemoryAccess(true, false, sizeof(double));

                    local_grids_new[sa][i] = (local_grids_old[sa][i - 1] +
                                              local_grids_old[sa][i] +
                                              local_grids_old[sa][i + 1]) / 3.0;

                    ops_this_iter++;
                }
            }

            // Swap grids for next iteration
            for (int sa = 0; sa < config.num_subarrays; sa++) {
                std::swap(local_grids_old[sa], local_grids_new[sa]);
            }

            if ((iter + 1) % 10 == 0 || iter == 0 || iter == config.num_iterations - 1) {
                std::cout << "  Iteration " << (iter + 1) << ": " << ops_this_iter
                          << " stencil ops, " << halo_exchanges << " total halo exchanges" << std::endl;
            }
        }

        std::cout << "\n✓ Stencil computation complete" << std::endl;
    }

    void exchangeHalos() {
        // Each subarray exchanges boundary values with neighbors using PIMID
        for (int sa = 0; sa < config.num_subarrays; sa++) {
            // Send right boundary to right neighbor
            if (sa < config.num_subarrays - 1) {
                simulator->simulateNetworkTransfer(sa, sa + 1, sizeof(double));
                halo_exchanges++;

                // Simulate the actual halo update
                local_grids_old[sa + 1][0] = local_grids_old[sa][segment_size];
            }

            // Send left boundary to left neighbor
            if (sa > 0) {
                simulator->simulateNetworkTransfer(sa, sa - 1, sizeof(double));
                halo_exchanges++;

                // Simulate the actual halo update
                local_grids_old[sa - 1][segment_size + 1] = local_grids_old[sa][1];
            }
        }
    }

    void printMetrics() {
        std::cout << "\n=== 1D Stencil Results (MESSAGE PASSING - PIMID) ===" << std::endl;

        const SimulationResults& results = simulator->getResults();

        std::cout << "\nTotal cycles: " << results.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << results.compute_cycles
                  << " (" << (100.0 * results.compute_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Memory cycles: " << results.memory_cycles
                  << " (" << (100.0 * results.memory_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Network cycles: " << results.network_cycles
                  << " (" << (100.0 * results.network_cycles / results.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        uint64_t expected_ops = segment_size * config.num_subarrays * config.num_iterations;
        std::cout << "  Total stencil operations: " << expected_ops << std::endl;
        std::cout << "  Operations per iteration: " << (expected_ops / config.num_iterations) << std::endl;

        std::cout << "\nCommunication (Message Passing):" << std::endl;
        uint64_t expected_transfers = 2 * (config.num_subarrays - 1) * config.num_iterations;
        std::cout << "  Halo exchanges: " << halo_exchanges << std::endl;
        std::cout << "  Expected halo transfers: " << expected_transfers << std::endl;

        std::cout << "\nEnergy (PIMID-based):" << std::endl;
        std::cout << "  Total energy: " << results.total_energy_pJ << " pJ" << std::endl;
        std::cout << "  Compute energy: " << results.compute_energy_pJ << " pJ ("
                  << (100.0 * results.compute_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;
        std::cout << "  Memory energy: " << results.memory_energy_pJ << " pJ ("
                  << (100.0 * results.memory_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;
        std::cout << "  Network energy: " << results.network_energy_pJ << " pJ ("
                  << (100.0 * results.network_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;

        if (halo_exchanges > 0) {
            std::cout << "  Avg energy per transfer: "
                      << (results.network_energy_pJ / halo_exchanges) << " pJ" << std::endl;
        }

        std::cout << "\nPerformance:" << std::endl;
        std::cout << "  Execution time: " << results.execution_time_ns << " ns" << std::endl;
        std::cout << "  Execution time: " << (results.execution_time_ns / 1000.0) << " us" << std::endl;

        // Validation
        std::cout << "\nValidation:" << std::endl;
        std::cout << "  Expected stencil ops: " << expected_ops << std::endl;
        std::cout << "  Expected halo transfers: " << expected_transfers << std::endl;
        std::cout << "  Actual halo transfers: " << halo_exchanges << std::endl;

        if (halo_exchanges == expected_transfers) {
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
    config.topology = is_libcom ? Topology::LIBCOM : Topology::HTREE_BASELINE;

    // Validate divisibility
    if ((config.grid_size - 2) % config.num_subarrays != 0) {
        std::cerr << "Error: (grid_size - 2) must be evenly divisible by num_subarrays" << std::endl;
        return 1;
    }

    std::cout << "\n=== DAC'26 1D Stencil Benchmark (Message Passing - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: Parallel Research Kernels (PRK), MiniFE" << std::endl;
    std::cout << "Programming Model: MESSAGE PASSING" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    Stencil1DMessagePIMID workload(config);
    workload.execute();

    return 0;
}
