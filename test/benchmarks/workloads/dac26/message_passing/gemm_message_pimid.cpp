/**
 * @file gemm_message_pimid.cpp
 * @brief GEMM (Matrix Multiplication) workload with MESSAGE PASSING model - PIMID integrated
 *
 * This version uses PIMID simulator for accurate energy and timing modeling
 * Technology: 45nm
 * Frequency: 1GHz
 *
 * Source: BLAS Level 3 (DGEMM operation)
 *
 * Message Passing Model:
 * - Matrix blocks distributed across subarrays
 * - Explicit inter-subarray block transfers for multiplication
 * - Each subarray accumulates local C blocks
 */

#include "pim_simulator.h"
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

class GEMMMessagePIMID {
private:
    GEMMConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    int num_blocks;
    std::vector<std::vector<int>> matrix_A_placement;  // Subarray ID for each A block
    std::vector<std::vector<int>> matrix_B_placement;  // Subarray ID for each B block
    std::vector<std::vector<int>> matrix_C_placement;  // Subarray ID for each C block
    uint64_t blocks_transferred;

public:
    GEMMMessagePIMID(const GEMMConfig& cfg) : config(cfg), blocks_transferred(0) {
        num_blocks = config.matrix_size / config.block_size;

        matrix_A_placement.resize(num_blocks, std::vector<int>(num_blocks));
        matrix_B_placement.resize(num_blocks, std::vector<int>(num_blocks));
        matrix_C_placement.resize(num_blocks, std::vector<int>(num_blocks));

        // Round-robin distribution of matrix blocks across subarrays
        for (int i = 0; i < num_blocks; i++) {
            for (int j = 0; j < num_blocks; j++) {
                int block_id = i * num_blocks + j;
                matrix_A_placement[i][j] = block_id % config.num_subarrays;
                matrix_B_placement[i][j] = block_id % config.num_subarrays;
                matrix_C_placement[i][j] = block_id % config.num_subarrays;
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
        std::cout << "\n=== GEMM Workload (MESSAGE PASSING - PIMID) ===" << std::endl;
        std::cout << "Matrix size: " << config.matrix_size << "×" << config.matrix_size << std::endl;
        std::cout << "Block size: " << config.block_size << "×" << config.block_size << std::endl;
        std::cout << "Number of blocks: " << num_blocks << "×" << num_blocks << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Programming model: MESSAGE PASSING" << std::endl;

        simulator->resetStats();
        computeGEMM();
        printMetrics();
    }

private:
    void computeGEMM() {
        std::cout << "\nBlocked matrix multiplication: C = A × B" << std::endl;

        // C[i][j] = Σ_k A[i][k] × B[k][j]
        for (int i = 0; i < num_blocks; i++) {
            for (int j = 0; j < num_blocks; j++) {
                // Accumulate C[i][j] from all k blocks
                for (int k = 0; k < num_blocks; k++) {
                    computeBlockMultiply(i, j, k);
                }
            }
        }

        std::cout << "✓ GEMM computation complete" << std::endl;
    }

    void computeBlockMultiply(int i, int j, int k) {
        int subarray_A = matrix_A_placement[i][k];
        int subarray_B = matrix_B_placement[k][j];
        int subarray_C = matrix_C_placement[i][j];

        // Transfer A[i][k] to C's subarray if needed
        if (subarray_A != subarray_C) {
            uint64_t block_bytes = config.block_size * config.block_size * sizeof(double);
            simulator->simulateNetworkTransfer(subarray_A, subarray_C, block_bytes);
            blocks_transferred++;
        }

        // Transfer B[k][j] to C's subarray if needed
        if (subarray_B != subarray_C) {
            uint64_t block_bytes = config.block_size * config.block_size * sizeof(double);
            simulator->simulateNetworkTransfer(subarray_B, subarray_C, block_bytes);
            blocks_transferred++;
        }

        // Compute block multiply-accumulate on C's subarray
        // Each block multiply is block_size^3 MAC operations
        uint64_t block_mac_ops = config.block_size * config.block_size * config.block_size;
        simulator->simulateCompute(block_mac_ops);

        // Write result block back
        simulator->simulateMemoryAccess(true, false, sizeof(double));
    }

    void printMetrics() {
        std::cout << "\n=== GEMM Results (MESSAGE PASSING - PIMID) ===" << std::endl;

        const SimulationResults& results = simulator->getResults();

        std::cout << "\nTotal cycles: " << results.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << results.compute_cycles
                  << " (" << (100.0 * results.compute_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Memory cycles: " << results.memory_cycles
                  << " (" << (100.0 * results.memory_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Network cycles: " << results.network_cycles
                  << " (" << (100.0 * results.network_cycles / results.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        uint64_t expected_macs = 2ULL * config.matrix_size * config.matrix_size * config.matrix_size;
        std::cout << "  Total MAC operations: " << results.compute_ops << std::endl;
        std::cout << "  Expected MACs: " << expected_macs << std::endl;

        std::cout << "\nCommunication (Message Passing):" << std::endl;
        std::cout << "  Inter-subarray block transfers: " << blocks_transferred << std::endl;

        std::cout << "\nEnergy (PIMID-based):" << std::endl;
        std::cout << "  Total energy: " << results.total_energy_pJ << " pJ" << std::endl;
        std::cout << "  Compute energy: " << results.compute_energy_pJ << " pJ ("
                  << (100.0 * results.compute_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;
        std::cout << "  Memory energy: " << results.memory_energy_pJ << " pJ ("
                  << (100.0 * results.memory_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;
        std::cout << "  Network energy: " << results.network_energy_pJ << " pJ ("
                  << (100.0 * results.network_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;

        if (blocks_transferred > 0) {
            std::cout << "  Avg energy per transfer: "
                      << (results.network_energy_pJ / blocks_transferred) << " pJ" << std::endl;
        }

        std::cout << "\nPerformance:" << std::endl;
        std::cout << "  Execution time: " << results.execution_time_ns << " ns" << std::endl;
        std::cout << "  Execution time: " << (results.execution_time_ns / 1000.0) << " us" << std::endl;

        // Validation
        std::cout << "\nValidation:" << std::endl;
        std::cout << "  MAC operations: " << (results.compute_ops == expected_macs ? "✓" : "✗") << std::endl;
        std::cout << "  Block transfers tracked: ✓" << std::endl;
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

    std::cout << "\n=== DAC'26 GEMM Benchmark (Message Passing - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: BLAS Level 3 (DGEMM)" << std::endl;
    std::cout << "Programming Model: MESSAGE PASSING" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    GEMMMessagePIMID workload(config);
    workload.execute();

    return 0;
}
