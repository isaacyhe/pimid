/**
 * @file spmv_workload.cpp
 * @brief SpMV (Sparse Matrix-Vector Multiply) workload for LIBCom evaluation
 *
 * Sparse matrix rows are distributed across subarrays.
 * Tests partial sum accumulation across subarrays.
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>

struct SpMVConfig {
    int num_subarrays;
    int num_rows;
    int num_cols;
    double sparsity;           // Fraction of non-zero elements
    int subarray_size_words;

    int copy_latency;
    int move_latency;
    int read_latency;
    int write_latency;
};

struct SpMVMetrics {
    uint64_t total_cycles = 0;
    uint64_t intersubarray_transfers = 0;
    uint64_t htree_traversals = 0;
    uint64_t direct_transfers = 0;
    uint64_t partial_sum_transfers = 0;
    uint64_t nnz = 0;  // Number of non-zeros
    double total_energy = 0.0;
};

struct SparseRow {
    std::vector<int> col_indices;
    std::vector<double> values;
};

class SpMVWorkload {
private:
    SpMVConfig config;
    SpMVMetrics metrics;

    std::vector<SparseRow> sparse_matrix;
    std::vector<int> row_to_subarray;
    std::vector<double> vector_x;
    std::vector<double> result_y;

public:
    SpMVWorkload(const SpMVConfig& cfg) : config(cfg) {
        sparse_matrix.resize(config.num_rows);
        row_to_subarray.resize(config.num_rows);
        vector_x.resize(config.num_cols, 1.0);
        result_y.resize(config.num_rows, 0.0);

        // Distribute rows across subarrays
        for (int i = 0; i < config.num_rows; i++) {
            row_to_subarray[i] = i % config.num_subarrays;
        }

        // Generate sparse matrix
        generateSparseMatrix();
    }

    void execute() {
        std::cout << "\n=== SpMV Workload Execution ===" << std::endl;
        std::cout << "Matrix size: " << config.num_rows << "×" << config.num_cols << std::endl;
        std::cout << "Sparsity: " << (config.sparsity * 100) << "%" << std::endl;
        std::cout << "Non-zeros: " << metrics.nnz << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;

        // Compute y = A * x
        computeSpMV();

        printMetrics();
    }

private:
    void generateSparseMatrix() {
        for (int i = 0; i < config.num_rows; i++) {
            int nnz_per_row = static_cast<int>(config.num_cols * config.sparsity);

            for (int k = 0; k < nnz_per_row; k++) {
                int j = (i + k * 7) % config.num_cols;  // Pseudo-random pattern
                sparse_matrix[i].col_indices.push_back(j);
                sparse_matrix[i].values.push_back(1.0);
                metrics.nnz++;
            }
        }
    }

    void computeSpMV() {
        // y = A * x
        for (int i = 0; i < config.num_rows; i++) {
            double partial_sum = 0.0;
            int row_subarray = row_to_subarray[i];

            // Compute partial sum for this row
            for (size_t k = 0; k < sparse_matrix[i].col_indices.size(); k++) {
                int j = sparse_matrix[i].col_indices[k];
                double a_ij = sparse_matrix[i].values[k];

                // Access x[j] - may require inter-subarray transfer
                int x_subarray = j % config.num_subarrays;

                if (x_subarray != row_subarray) {
                    transferElement(x_subarray, row_subarray);
                } else {
                    metrics.total_cycles += config.read_latency;
                }

                // Multiply-accumulate (1 cycle)
                metrics.total_cycles += 1;
                partial_sum += a_ij * vector_x[j];
            }

            // Write result
            result_y[i] = partial_sum;
            metrics.total_cycles += config.write_latency;

            // If row result needs to go to different subarray for reduction
            // (simulating multi-stage SpMV)
            if (i % 4 == 0 && sparse_matrix[i].col_indices.size() > 0) {
                int target_subarray = (row_subarray + 1) % config.num_subarrays;
                if (target_subarray != row_subarray) {
                    transferPartialSum(row_subarray, target_subarray);
                }
            }
        }
    }

    void transferElement(int src_subarray, int dst_subarray) {
        assert(src_subarray != dst_subarray);

        metrics.intersubarray_transfers++;
        metrics.total_cycles += config.copy_latency;

        if (config.copy_latency == 1) {
            metrics.direct_transfers++;
            metrics.total_energy += 0.55;
        } else {
            metrics.htree_traversals++;
            metrics.total_energy += 1.0;
        }
    }

    void transferPartialSum(int src_subarray, int dst_subarray) {
        assert(src_subarray != dst_subarray);

        metrics.partial_sum_transfers++;
        metrics.intersubarray_transfers++;
        metrics.total_cycles += config.copy_latency;

        if (config.copy_latency == 1) {
            metrics.direct_transfers++;
            metrics.total_energy += 0.55;
        } else {
            metrics.htree_traversals++;
            metrics.total_energy += 1.0;
        }
    }

    void printMetrics() {
        std::cout << "\n=== SpMV Results ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "Inter-subarray transfers: " << metrics.intersubarray_transfers << std::endl;
        std::cout << "Partial sum transfers: " << metrics.partial_sum_transfers << std::endl;

        if (config.copy_latency == 1) {
            std::cout << "Direct transfers (LIBCom): " << metrics.direct_transfers << std::endl;
        } else {
            std::cout << "H-tree traversals (Baseline): " << metrics.htree_traversals << std::endl;
        }

        std::cout << "Total energy (relative): " << metrics.total_energy << std::endl;
        if (metrics.intersubarray_transfers > 0) {
            std::cout << "Avg energy per transfer: "
                      << (metrics.total_energy / metrics.intersubarray_transfers) << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <matrix_size> <is_libcom>" << std::endl;
        return 1;
    }

    SpMVConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.num_rows = std::atoi(argv[2]);
    config.num_cols = config.num_rows;
    bool is_libcom = (std::atoi(argv[3]) == 1);

    config.sparsity = 0.05;  // 5% non-zero elements
    config.subarray_size_words = 1024;

    config.read_latency = 1;
    config.write_latency = 1;

    if (is_libcom) {
        config.copy_latency = 1;
        config.move_latency = 1;
    } else {
        int htree_latency = 0;
        int n = config.num_subarrays;
        while (n > 1) { htree_latency++; n >>= 1; }
        config.copy_latency = 2 + htree_latency;
        config.move_latency = 2 + htree_latency;
    }

    SpMVWorkload workload(config);
    workload.execute();

    return 0;
}
