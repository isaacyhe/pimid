/**
 * @file spmv_message.cpp
 * @brief SpMV (Sparse Matrix-Vector Multiply) workload with MESSAGE PASSING model
 *
 * Source: Sparse BLAS (SpBLAS)
 *
 * Message Passing Model:
 * - Sparse matrix rows distributed across subarrays
 * - Input vector x partitioned across subarrays
 * - Required x elements transferred explicitly for local computation
 * - Partial sums may be transferred for reduction
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
    uint64_t transfer_cycles = 0;
    uint64_t intersubarray_transfers = 0;
    uint64_t htree_traversals = 0;
    uint64_t direct_transfers = 0;
    uint64_t vector_element_transfers = 0;
    uint64_t partial_sum_transfers = 0;
    uint64_t mac_ops = 0;
    uint64_t nnz = 0;
    double total_energy = 0.0;
};

struct SparseRow {
    std::vector<int> col_indices;
    std::vector<double> values;
};

class SpMVMessage {
private:
    SpMVConfig config;
    SpMVMetrics metrics;

    std::vector<SparseRow> sparse_matrix;
    std::vector<int> row_assignment;
    std::vector<int> vector_x_assignment;
    std::vector<double> vector_x;
    std::vector<double> result_y;

public:
    SpMVMessage(const SpMVConfig& cfg) : config(cfg) {
        sparse_matrix.resize(config.num_rows);
        row_assignment.resize(config.num_rows);
        vector_x_assignment.resize(config.num_cols);
        vector_x.resize(config.num_cols, 1.0);
        result_y.resize(config.num_rows, 0.0);

        // Distribute matrix rows across subarrays
        for (int i = 0; i < config.num_rows; i++) {
            row_assignment[i] = i % config.num_subarrays;
        }

        // Distribute vector x across subarrays
        for (int j = 0; j < config.num_cols; j++) {
            vector_x_assignment[j] = j % config.num_subarrays;
        }

        generateSparseMatrix();
    }

    void execute() {
        std::cout << "\n=== SpMV Workload (MESSAGE PASSING) ===" << std::endl;
        std::cout << "Matrix size: " << config.num_rows << "×" << config.num_cols << std::endl;
        std::cout << "Sparsity: " << (config.sparsity * 100) << "%" << std::endl;
        std::cout << "Non-zeros: " << metrics.nnz << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: MESSAGE PASSING" << std::endl;

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
        std::cout << "\nComputing y = A × x (message passing)..." << std::endl;

        // Each subarray processes its assigned rows
        for (int sa = 0; sa < config.num_subarrays; sa++) {
            uint64_t rows_processed = 0;

            for (int i = 0; i < config.num_rows; i++) {
                if (row_assignment[i] == sa) {
                    computeRowDotProduct(i, sa);
                    rows_processed++;
                }
            }

            std::cout << "  Subarray " << sa << ": processed " << rows_processed << " rows" << std::endl;
        }

        std::cout << "✓ SpMV computation complete" << std::endl;
    }

    void computeRowDotProduct(int row_id, int subarray_id) {
        double partial_sum = 0.0;

        // Process all non-zero elements in this row
        for (size_t k = 0; k < sparse_matrix[row_id].col_indices.size(); k++) {
            int col_idx = sparse_matrix[row_id].col_indices[k];
            double matrix_value = sparse_matrix[row_id].values[k];

            // Check if x[col_idx] is local or remote
            int x_subarray = vector_x_assignment[col_idx];

            if (x_subarray != subarray_id) {
                // Transfer x element from remote subarray
                transferVectorElement(x_subarray, subarray_id);
            } else {
                // Local read
                metrics.total_cycles += config.read_latency;
            }

            // Multiply-accumulate
            metrics.total_cycles += config.compute_latency;
            metrics.compute_cycles += config.compute_latency;
            metrics.mac_ops++;

            partial_sum += matrix_value * vector_x[col_idx];
        }

        // Write result locally
        result_y[row_id] = partial_sum;
        metrics.total_cycles += config.write_latency;

        // Simulate partial sum transfer for reduction (every 4th row)
        if (row_id % 4 == 0 && sparse_matrix[row_id].col_indices.size() > 0) {
            int target_subarray = (subarray_id + 1) % config.num_subarrays;
            if (target_subarray != subarray_id) {
                transferPartialSum(subarray_id, target_subarray);
            }
        }
    }

    void transferVectorElement(int src_subarray, int dst_subarray) {
        assert(src_subarray != dst_subarray);

        metrics.intersubarray_transfers++;
        metrics.vector_element_transfers++;

        uint64_t transfer_cycles = config.copy_latency;
        metrics.transfer_cycles += transfer_cycles;
        metrics.total_cycles += transfer_cycles;

        if (config.copy_latency == 1) {
            metrics.direct_transfers++;
            metrics.total_energy += 0.55;  // LIBCom
        } else {
            metrics.htree_traversals++;
            metrics.total_energy += 1.0;   // Baseline
        }
    }

    void transferPartialSum(int src_subarray, int dst_subarray) {
        assert(src_subarray != dst_subarray);

        metrics.intersubarray_transfers++;
        metrics.partial_sum_transfers++;

        uint64_t transfer_cycles = config.copy_latency;
        metrics.transfer_cycles += transfer_cycles;
        metrics.total_cycles += transfer_cycles;

        if (config.copy_latency == 1) {
            metrics.direct_transfers++;
            metrics.total_energy += 0.55;  // LIBCom
        } else {
            metrics.htree_traversals++;
            metrics.total_energy += 1.0;   // Baseline
        }
    }

    void printMetrics() {
        std::cout << "\n=== SpMV Results (MESSAGE PASSING) ===" << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Transfer cycles: " << metrics.transfer_cycles
                  << " (" << (100.0 * metrics.transfer_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total MAC operations: " << metrics.mac_ops << std::endl;
        std::cout << "  Expected MACs: " << metrics.nnz << std::endl;

        std::cout << "\nCommunication (Message Passing):" << std::endl;
        std::cout << "  Total inter-subarray transfers: " << metrics.intersubarray_transfers << std::endl;
        std::cout << "  Vector element transfers: " << metrics.vector_element_transfers << std::endl;
        std::cout << "  Partial sum transfers: " << metrics.partial_sum_transfers << std::endl;

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

    std::cout << "\n=== DAC'26 SpMV Benchmark (Message Passing) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: Sparse BLAS (SpBLAS)" << std::endl;
    std::cout << "Programming Model: MESSAGE PASSING" << std::endl;

    SpMVMessage workload(config);
    workload.execute();

    return 0;
}
