/**
 * @file spmv_shared.cpp
 * @brief SpMV (Sparse Matrix-Vector Multiply) workload with SHARED MEMORY model
 *
 * Source: Sparse BLAS (SpBLAS)
 *
 * Shared Memory Model:
 * - Sparse matrix and vectors stored in shared memory
 * - Each subarray processes assigned rows independently
 * - Atomic updates to result vector for concurrent writes
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>

struct SpMVConfig {
    int num_subarrays;
    int num_rows;
    int num_cols;
    double sparsity;

    int copy_latency;
    int read_latency;
    int write_latency;
    int compute_latency;
};

struct SpMVMetrics {
    uint64_t total_cycles = 0;
    uint64_t compute_cycles = 0;
    uint64_t sync_cycles = 0;
    uint64_t atomic_ops = 0;
    uint64_t mac_ops = 0;
    uint64_t nnz = 0;
    double total_energy = 0.0;
};

struct SparseRow {
    std::vector<int> col_indices;
    std::vector<double> values;
};

class SpMVShared {
private:
    SpMVConfig config;
    SpMVMetrics metrics;

    std::vector<SparseRow> sparse_matrix;
    std::vector<int> row_assignment;
    std::vector<double> vector_x;
    std::vector<double> result_y;

public:
    SpMVShared(const SpMVConfig& cfg) : config(cfg) {
        sparse_matrix.resize(config.num_rows);
        row_assignment.resize(config.num_rows);
        vector_x.resize(config.num_cols, 1.0);
        result_y.resize(config.num_rows, 0.0);

        // Distribute rows across subarrays for processing
        for (int i = 0; i < config.num_rows; i++) {
            row_assignment[i] = i % config.num_subarrays;
        }

        generateSparseMatrix();
    }

    void execute() {
        std::cout << "\n=== SpMV Workload (SHARED MEMORY) ===" << std::endl;
        std::cout << "Matrix size: " << config.num_rows << "×" << config.num_cols << std::endl;
        std::cout << "Sparsity: " << (config.sparsity * 100) << "%" << std::endl;
        std::cout << "Non-zeros: " << metrics.nnz << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;

        computeSpMV();
        printMetrics();
    }

private:
    void generateSparseMatrix() {
        std::cout << "\nGenerating sparse matrix..." << std::endl;

        for (int i = 0; i < config.num_rows; i++) {
            int nnz_per_row = static_cast<int>(config.num_cols * config.sparsity);

            for (int k = 0; k < nnz_per_row; k++) {
                int j = (i + k * 7) % config.num_cols;
                sparse_matrix[i].col_indices.push_back(j);
                sparse_matrix[i].values.push_back(1.0 + (k % 10) / 10.0);
                metrics.nnz++;
            }
        }

        std::cout << "  Generated " << metrics.nnz << " non-zero elements" << std::endl;
    }

    void computeSpMV() {
        std::cout << "\nComputing y = A × x (shared memory)..." << std::endl;

        // Each subarray processes its assigned rows
        for (int sa = 0; sa < config.num_subarrays; sa++) {
            uint64_t rows_processed = 0;

            for (int i = 0; i < config.num_rows; i++) {
                if (row_assignment[i] == sa) {
                    computeRowDotProduct(i);
                    rows_processed++;
                }
            }

            std::cout << "  Subarray " << sa << ": processed " << rows_processed << " rows" << std::endl;
        }

        // Synchronization barrier
        std::cout << "\nSynchronization barrier" << std::endl;
        metrics.sync_cycles += 10;
        metrics.total_cycles += 10;

        std::cout << "✓ SpMV computation complete" << std::endl;
    }

    void computeRowDotProduct(int row_id) {
        double partial_sum = 0.0;

        // Process all non-zero elements in this row
        for (size_t k = 0; k < sparse_matrix[row_id].col_indices.size(); k++) {
            int col_idx = sparse_matrix[row_id].col_indices[k];
            double matrix_value = sparse_matrix[row_id].values[k];

            // Read matrix value from shared memory
            metrics.total_cycles += config.read_latency;

            // Read x[col_idx] from shared memory
            metrics.total_cycles += config.read_latency;

            // Multiply-accumulate
            metrics.total_cycles += config.compute_latency;
            metrics.compute_cycles += config.compute_latency;
            metrics.mac_ops++;

            partial_sum += matrix_value * vector_x[col_idx];
        }

        // Atomic write to shared result vector
        metrics.atomic_ops++;
        metrics.total_cycles += 2;  // Atomic operation overhead

        result_y[row_id] = partial_sum;
    }

    void printMetrics() {
        std::cout << "\n=== SpMV Results (SHARED MEMORY) ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Sync cycles: " << metrics.sync_cycles
                  << " (" << (100.0 * metrics.sync_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total MAC operations: " << metrics.mac_ops << std::endl;
        std::cout << "  Expected MACs: " << metrics.nnz << std::endl;
        std::cout << "  Atomic writes: " << metrics.atomic_ops << std::endl;

        std::cout << "\nCommunication (Shared Memory):" << std::endl;
        std::cout << "  Inter-subarray transfers: 0 (shared memory model)" << std::endl;

        // Validation
        std::cout << "\nValidation:" << std::endl;
        std::cout << "  MAC operations: " << (metrics.mac_ops == metrics.nnz ? "✓" : "✗") << std::endl;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <matrix_size> <is_libcom>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 128 0   # 8 subarrays, 128×128 matrix, baseline" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 128 1   # 8 subarrays, 128×128 matrix, LIBCom" << std::endl;
        return 1;
    }

    SpMVConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.num_rows = std::atoi(argv[2]);
    config.num_cols = config.num_rows;
    bool is_libcom = (std::atoi(argv[3]) == 1);

    config.sparsity = 0.05;  // 5% non-zero
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

    std::cout << "\n=== DAC'26 SpMV Benchmark (Shared Memory) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: Sparse BLAS (SpBLAS)" << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;

    SpMVShared workload(config);
    workload.execute();

    return 0;
}
