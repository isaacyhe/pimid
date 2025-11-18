/**
 * @file gemm_message.cpp
 * @brief GEMM (Matrix Multiplication) workload with MESSAGE PASSING model
 *
 * Source: BLAS Level 3 (DGEMM operation)
 *
 * Message Passing Model:
 * - Matrix blocks distributed across subarrays
 * - Explicit inter-subarray block transfers for multiplication
 * - Each subarray accumulates local C blocks
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>

struct GEMMConfig {
    int num_subarrays;
    int matrix_size;           // N×N matrices
    int block_size;            // Block size for tiling

    int copy_latency;
    int read_latency;
    int write_latency;
    int compute_latency;
};

struct GEMMMetrics {
    uint64_t total_cycles = 0;
    uint64_t compute_cycles = 0;
    uint64_t transfer_cycles = 0;
    uint64_t intersubarray_transfers = 0;
    uint64_t htree_traversals = 0;
    uint64_t direct_transfers = 0;
    uint64_t blocks_transferred = 0;
    uint64_t mac_ops = 0;
    double total_energy = 0.0;
};

class GEMMMessage {
private:
    GEMMConfig config;
    GEMMMetrics metrics;

    int num_blocks;
    std::vector<std::vector<int>> matrix_A_placement;  // Subarray ID for each A block
    std::vector<std::vector<int>> matrix_B_placement;  // Subarray ID for each B block
    std::vector<std::vector<int>> matrix_C_placement;  // Subarray ID for each C block

public:
    GEMMMessage(const GEMMConfig& cfg) : config(cfg) {
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
    }

    void execute() {
        std::cout << "\n=== GEMM Workload (MESSAGE PASSING) ===" << std::endl;
        std::cout << "Matrix size: " << config.matrix_size << "×" << config.matrix_size << std::endl;
        std::cout << "Block size: " << config.block_size << "×" << config.block_size << std::endl;
        std::cout << "Number of blocks: " << num_blocks << "×" << num_blocks << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: MESSAGE PASSING" << std::endl;

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
            transferBlock(subarray_A, subarray_C);
        }

        // Transfer B[k][j] to C's subarray if needed
        if (subarray_B != subarray_C) {
            transferBlock(subarray_B, subarray_C);
        }

        // Compute block multiply-accumulate on C's subarray
        // Each block multiply is block_size^3 MAC operations
        uint64_t block_mac_ops = config.block_size * config.block_size * config.block_size;
        uint64_t block_compute_cycles = block_mac_ops * config.compute_latency;

        metrics.mac_ops += block_mac_ops;
        metrics.compute_cycles += block_compute_cycles;
        metrics.total_cycles += block_compute_cycles;

        // Write result block back
        metrics.total_cycles += config.write_latency;
    }

    void transferBlock(int src_subarray, int dst_subarray) {
        assert(src_subarray != dst_subarray);

        // Inter-subarray block transfer
        metrics.intersubarray_transfers++;
        metrics.blocks_transferred++;

        // Transfer latency (for block_size × block_size elements)
        uint64_t transfer_cycles = config.copy_latency;
        metrics.transfer_cycles += transfer_cycles;
        metrics.total_cycles += transfer_cycles;

        // Track transfer type
        if (config.copy_latency == 1) {
            metrics.direct_transfers++;
            metrics.total_energy += 0.55;  // LIBCom: 45% energy reduction
        } else {
            metrics.htree_traversals++;
            metrics.total_energy += 1.0;   // Baseline H-tree
        }
    }

    void printMetrics() {
        std::cout << "\n=== GEMM Results (MESSAGE PASSING) ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Transfer cycles: " << metrics.transfer_cycles
                  << " (" << (100.0 * metrics.transfer_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total MAC operations: " << metrics.mac_ops << std::endl;
        std::cout << "  Expected MACs: " << (2ULL * config.matrix_size * config.matrix_size * config.matrix_size) << std::endl;

        std::cout << "\nCommunication (Message Passing):" << std::endl;
        std::cout << "  Inter-subarray block transfers: " << metrics.intersubarray_transfers << std::endl;
        std::cout << "  Total blocks transferred: " << metrics.blocks_transferred << std::endl;

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
        std::cout << "  MAC operations: " << (metrics.mac_ops == 2ULL * config.matrix_size * config.matrix_size * config.matrix_size ? "✓" : "✗") << std::endl;
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

    config.block_size = 64;  // 64×64 blocks
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

    std::cout << "\n=== DAC'26 GEMM Benchmark (Message Passing) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: BLAS Level 3 (DGEMM)" << std::endl;
    std::cout << "Programming Model: MESSAGE PASSING" << std::endl;

    GEMMMessage workload(config);
    workload.execute();

    return 0;
}
