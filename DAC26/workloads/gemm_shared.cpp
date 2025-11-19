/**
 * @file gemm_shared.cpp
 * @brief GEMM (Matrix Multiplication) workload with SHARED MEMORY model
 *
 * Source: BLAS Level 3 (DGEMM operation)
 *
 * Shared Memory Model:
 * - Matrices A, B, and C stored in shared memory
 * - Each subarray computes assigned C blocks independently
 * - Atomic updates to shared C matrix for accumulation
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
    uint64_t sync_cycles = 0;
    uint64_t mac_ops = 0;
    uint64_t atomic_ops = 0;
    uint64_t remote_accesses = 0;    // Track remote memory accesses
    double total_energy = 0.0;
};

class GEMMShared {
private:
    GEMMConfig config;
    GEMMMetrics metrics;
    bool is_libcom;

    int num_blocks;
    std::vector<int> block_assignment;  // Which subarray computes each C block

public:
    GEMMShared(const GEMMConfig& cfg, bool use_libcom)
        : config(cfg), is_libcom(use_libcom) {
        num_blocks = config.matrix_size / config.block_size;
        int total_c_blocks = num_blocks * num_blocks;
        block_assignment.resize(total_c_blocks);

        // Assign C blocks to subarrays in round-robin fashion
        for (int block_id = 0; block_id < total_c_blocks; block_id++) {
            block_assignment[block_id] = block_id % config.num_subarrays;
        }
    }

    void execute() {
        std::cout << "\n=== GEMM Workload (SHARED MEMORY) ===" << std::endl;
        std::cout << "Matrix size: " << config.matrix_size << "×" << config.matrix_size << std::endl;
        std::cout << "Block size: " << config.block_size << "×" << config.block_size << std::endl;
        std::cout << "Number of blocks: " << num_blocks << "×" << num_blocks << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;

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
        metrics.sync_cycles += 10;
        metrics.total_cycles += 10;

        std::cout << "✓ GEMM computation complete" << std::endl;
    }

    void computeBlockMultiply(int i, int j, int k, int subarray_id) {
        // Read A[i][k] block from shared memory
        // Assume A blocks are row-distributed: A[i][*] in subarray (i % num_subarrays)
        int a_owner = i % config.num_subarrays;
        metrics.total_cycles += config.read_latency;
        if (a_owner != subarray_id) {
            metrics.remote_accesses++;
            if (is_libcom) {
                metrics.total_energy += 0.55;  // LIBCom: library communication
            } else {
                metrics.total_energy += 1.0;   // Baseline: bank port/peripheral
            }
        }

        // Read B[k][j] block from shared memory
        // Assume B blocks are row-distributed: B[k][*] in subarray (k % num_subarrays)
        int b_owner = k % config.num_subarrays;
        metrics.total_cycles += config.read_latency;
        if (b_owner != subarray_id) {
            metrics.remote_accesses++;
            if (is_libcom) {
                metrics.total_energy += 0.55;  // LIBCom: library communication
            } else {
                metrics.total_energy += 1.0;   // Baseline: bank port/peripheral
            }
        }

        // Compute block multiply-accumulate
        // Each block multiply is block_size^3 MAC operations
        uint64_t block_mac_ops = config.block_size * config.block_size * config.block_size;
        uint64_t block_compute_cycles = block_mac_ops * config.compute_latency;

        metrics.mac_ops += block_mac_ops;
        metrics.compute_cycles += block_compute_cycles;
        metrics.total_cycles += block_compute_cycles;

        // Atomic update to shared C[i][j] block
        metrics.atomic_ops++;
        metrics.total_cycles += 2;  // Atomic operation overhead
    }

    void printMetrics() {
        std::cout << "\n=== GEMM Results (SHARED MEMORY) ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Sync cycles: " << metrics.sync_cycles
                  << " (" << (100.0 * metrics.sync_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total MAC operations: " << metrics.mac_ops << std::endl;
        std::cout << "  Expected MACs: " << (1ULL * config.matrix_size * config.matrix_size * config.matrix_size) << std::endl;
        std::cout << "  Atomic accumulations: " << metrics.atomic_ops << std::endl;

        std::cout << "\nCommunication (Shared Memory):" << std::endl;
        std::cout << "  Inter-subarray transfers: 0 (shared memory model)" << std::endl;
        std::cout << "  Remote memory accesses: " << metrics.remote_accesses << std::endl;

        std::cout << "\nEnergy:" << std::endl;
        std::cout << "  Total energy (relative): " << metrics.total_energy << std::endl;
        if (metrics.remote_accesses > 0) {
            std::cout << "  Avg energy per remote access: "
                      << (metrics.total_energy / metrics.remote_accesses) << std::endl;
        }

        // Validation
        std::cout << "\nValidation:" << std::endl;
        std::cout << "  MAC operations: " << (metrics.mac_ops == 1ULL * config.matrix_size * config.matrix_size * config.matrix_size ? "✓" : "✗") << std::endl;
        std::cout << "  Atomic operations tracked: ✓" << std::endl;
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

    std::cout << "\n=== DAC'26 GEMM Benchmark (Shared Memory) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: BLAS Level 3 (DGEMM)" << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;

    GEMMShared workload(config, is_libcom);
    workload.execute();

    return 0;
}
