/**
 * @file gemm_workload.cpp
 * @brief GEMM (Matrix Multiplication) workload for LIBCom evaluation
 *
 * Matrix blocks are distributed across subarrays.
 * Tests inter-subarray block transfers during multiplication.
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>

struct GEMMConfig {
    int num_subarrays;
    int matrix_size;           // N×N matrices
    int block_size;            // Block size per subarray
    int subarray_size_words;   // 1024 words per subarray

    // Latencies (from config)
    int copy_latency;          // 5-7 cycles (baseline) or 1 cycle (LIBCom)
    int move_latency;          // 5-7 cycles (baseline) or 1 cycle (LIBCom)
    int read_latency;          // 1 cycle
    int write_latency;         // 1 cycle
};

struct GEMMMetrics {
    uint64_t total_cycles = 0;
    uint64_t intersubarray_transfers = 0;
    uint64_t htree_traversals = 0;      // Baseline only
    uint64_t direct_transfers = 0;       // LIBCom only
    double total_energy = 0.0;           // Relative to baseline
};

class GEMMWorkload {
private:
    GEMMConfig config;
    GEMMMetrics metrics;

    // Matrix data distribution
    std::vector<std::vector<int>> matrix_A_placement;  // Subarray ID for each block
    std::vector<std::vector<int>> matrix_B_placement;
    std::vector<std::vector<int>> matrix_C_placement;

public:
    GEMMWorkload(const GEMMConfig& cfg) : config(cfg) {
        // Distribute matrix blocks across subarrays
        int num_blocks = config.matrix_size / config.block_size;

        matrix_A_placement.resize(num_blocks, std::vector<int>(num_blocks));
        matrix_B_placement.resize(num_blocks, std::vector<int>(num_blocks));
        matrix_C_placement.resize(num_blocks, std::vector<int>(num_blocks));

        // Round-robin distribution
        for (int i = 0; i < num_blocks; i++) {
            for (int j = 0; j < num_blocks; j++) {
                int subarray_id = (i * num_blocks + j) % config.num_subarrays;
                matrix_A_placement[i][j] = subarray_id;
                matrix_B_placement[i][j] = subarray_id;
                matrix_C_placement[i][j] = subarray_id;
            }
        }
    }

    void execute() {
        std::cout << "\n=== GEMM Workload Execution ===" << std::endl;
        std::cout << "Matrix size: " << config.matrix_size << "×" << config.matrix_size << std::endl;
        std::cout << "Block size: " << config.block_size << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;

        int num_blocks = config.matrix_size / config.block_size;

        // C = A × B using blocked matrix multiplication
        for (int i = 0; i < num_blocks; i++) {
            for (int j = 0; j < num_blocks; j++) {
                for (int k = 0; k < num_blocks; k++) {
                    // C[i][j] += A[i][k] × B[k][j]
                    computeBlockMultiply(i, j, k);
                }
            }
        }

        printMetrics();
    }

private:
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

        // Compute multiply-accumulate on C's subarray
        uint64_t compute_cycles = config.block_size * config.block_size * config.block_size;
        metrics.total_cycles += compute_cycles;

        // Write result back
        metrics.total_cycles += config.write_latency;
    }

    void transferBlock(int src_subarray, int dst_subarray) {
        assert(src_subarray != dst_subarray);

        // Inter-subarray transfer
        metrics.intersubarray_transfers++;
        metrics.total_cycles += config.copy_latency;

        // Track transfer type
        if (config.copy_latency == 1) {
            // LIBCom: direct transfer
            metrics.direct_transfers++;
            metrics.total_energy += 0.55;  // 45% energy reduction
        } else {
            // Baseline: H-tree traversal
            metrics.htree_traversals++;
            metrics.total_energy += 1.0;   // Baseline energy
        }
    }

    void printMetrics() {
        std::cout << "\n=== GEMM Results ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "Inter-subarray transfers: " << metrics.intersubarray_transfers << std::endl;

        if (config.copy_latency == 1) {
            std::cout << "Direct transfers (LIBCom): " << metrics.direct_transfers << std::endl;
        } else {
            std::cout << "H-tree traversals (Baseline): " << metrics.htree_traversals << std::endl;
        }

        std::cout << "Total energy (relative): " << metrics.total_energy << std::endl;
        std::cout << "Avg energy per transfer: "
                  << (metrics.total_energy / metrics.intersubarray_transfers) << std::endl;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <matrix_size> <is_libcom>" << std::endl;
        return 1;
    }

    GEMMConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.matrix_size = std::atoi(argv[2]);
    bool is_libcom = (std::atoi(argv[3]) == 1);

    config.block_size = 64;  // 64×64 blocks
    config.subarray_size_words = 1024;

    // Set latencies based on interconnect type
    config.read_latency = 1;
    config.write_latency = 1;

    if (is_libcom) {
        config.copy_latency = 1;  // LIBCom
        config.move_latency = 1;
    } else {
        // Baseline H-tree: 2 + log2(num_subarrays)
        int htree_latency = 0;
        int n = config.num_subarrays;
        while (n > 1) { htree_latency++; n >>= 1; }
        config.copy_latency = 2 + htree_latency;
        config.move_latency = 2 + htree_latency;
    }

    GEMMWorkload workload(config);
    workload.execute();

    return 0;
}
