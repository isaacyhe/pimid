/**
 * @file gemm_shared_pimid.cpp
 * @brief GEMM (Matrix Multiplication) workload with SHARED MEMORY model - PIMID integrated
 *
 * This version uses PIMID simulator for accurate energy and timing modeling
 * Technology: 45nm
 * Frequency: 1GHz
 */

#include "../pimid_adapter/pim_simulator.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>

using namespace dac26;

struct GEMMConfig {
    int num_subarrays;
    int matrix_size;           // N×N matrices
    int block_size;            // Block size for tiling
    Topology topology;
};

class GEMMSharedPIMID {
private:
    GEMMConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    int num_blocks;
    std::vector<int> block_assignment;  // Which subarray computes each C block

public:
    GEMMSharedPIMID(const GEMMConfig& cfg) : config(cfg) {
        num_blocks = config.matrix_size / config.block_size;
        int total_c_blocks = num_blocks * num_blocks;
        block_assignment.resize(total_c_blocks);

        // Assign C blocks to subarrays in round-robin fashion
        for (int block_id = 0; block_id < total_c_blocks; block_id++) {
            block_assignment[block_id] = block_id % config.num_subarrays;
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
        std::cout << "\n=== GEMM Workload (SHARED MEMORY - PIMID) ===" << std::endl;
        std::cout << "Matrix size: " << config.matrix_size << "×" << config.matrix_size << std::endl;
        std::cout << "Block size: " << config.block_size << "×" << config.block_size << std::endl;
        std::cout << "Number of blocks: " << num_blocks << "×" << num_blocks << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;

        simulator->resetStats();
        computeGEMM();
        printMetrics();
    }

private:
    void computeGEMM() {
        std::cout << "\nBlocked matrix multiplication: C = A × B (shared memory)" << std::endl;

        // Each subarray processes its assigned C blocks
        for (int sa = 0; sa < config.num_subarrays; sa++) {
            uint64_t blocks_processed = 0;

            for (int i = 0; i < num_blocks; i++) {
                for (int j = 0; j < num_blocks; j++) {
                    int block_id = i * num_blocks + j;

                    if (block_assignment[block_id] == sa) {
                        // C[i][j] = Σ_k A[i][k] × B[k][j]
                        for (int k = 0; k < num_blocks; k++) {
                            computeBlockMultiply(i, j, k, sa);
                        }
                        blocks_processed++;
                    }
                }
            }

            std::cout << "  Subarray " << sa << ": processed " << blocks_processed << " C blocks" << std::endl;
        }

        // Synchronization barrier
        std::cout << "\nSynchronization barrier" << std::endl;
        simulator->simulateOperation(PIMOperation::BARRIER_SYNC, 1);

        std::cout << "✓ GEMM computation complete" << std::endl;
    }

    void computeBlockMultiply(int i, int j, int k, int subarray_id) {
        // Read A[i][k] block from shared memory
        // Assume A blocks are row-distributed: A[i][*] in subarray (i % num_subarrays)
        int a_owner = i % config.num_subarrays;
        if (a_owner == subarray_id) {
            simulator->simulateMemoryAccess(true, true, config.block_size * config.block_size * sizeof(double));
        } else {
            simulator->simulateMemoryAccess(false, true, config.block_size * config.block_size * sizeof(double));
        }

        // Read B[k][j] block from shared memory
        // Assume B blocks are row-distributed: B[k][*] in subarray (k % num_subarrays)
        int b_owner = k % config.num_subarrays;
        if (b_owner == subarray_id) {
            simulator->simulateMemoryAccess(true, true, config.block_size * config.block_size * sizeof(double));
        } else {
            simulator->simulateMemoryAccess(false, true, config.block_size * config.block_size * sizeof(double));
        }

        // Compute block multiply-accumulate
        // Each block multiply is block_size^3 MAC operations
        uint64_t block_mac_ops = config.block_size * config.block_size * config.block_size;
        simulator->simulateCompute(block_mac_ops);

        // Atomic update to shared C[i][j] block
        simulator->simulateOperation(PIMOperation::ATOMIC_OP, 1);
    }

    void printMetrics() {
        std::cout << "\n=== GEMM Results (SHARED MEMORY - PIMID) ===" << std::endl;

        const SimulationResults& results = simulator->getResults();

        std::cout << "\nTotal cycles: " << results.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << results.compute_cycles
                  << " (" << (100.0 * results.compute_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Memory cycles: " << results.memory_cycles
                  << " (" << (100.0 * results.memory_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Network cycles: " << results.network_cycles
                  << " (" << (100.0 * results.network_cycles / results.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total MAC operations: " << (2ULL * config.matrix_size * config.matrix_size * config.matrix_size) << std::endl;
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
        uint64_t expected_macs = 2ULL * config.matrix_size * config.matrix_size * config.matrix_size;
        std::cout << "\nValidation:" << std::endl;
        std::cout << "  MAC operations: " << (results.compute_ops == expected_macs ? "✓" : "✗") << std::endl;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <matrix_size> <is_libcom>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 512 0   # 8 subarrays, 512×512 matrix, baseline" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 512 1   # 8 subarrays, 512×512 matrix, LIBCom" << std::endl;
        return 1;
    }

    GEMMConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.matrix_size = std::atoi(argv[2]);
    bool is_libcom = (std::atoi(argv[3]) == 1);
    config.topology = is_libcom ? Topology::LIBCOM : Topology::HTREE_BASELINE;

    config.block_size = 64;  // 64×64 blocks

    std::cout << "\n=== DAC'26 GEMM Benchmark (Shared Memory - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    GEMMSharedPIMID workload(config);
    workload.execute();

    return 0;
}
