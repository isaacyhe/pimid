/**
 * @file spmv_csr_cosim.cpp
 * @brief COMPLEX Host/Device Co-Simulation: Sparse Matrix-Vector Multiplication (SpMV)
 *
 * HOST (OOO core with cache):
 *   - Generates sparse matrix in CSR (Compressed Sparse Row) format
 *   - Analyzes sparsity pattern
 *   - Distributes row ranges to PEs
 *   - Aggregates partial results
 *   - Verifies correctness
 *
 * DEVICE (ALU cores without cache):
 *   - Computes SpMV for assigned rows in parallel
 *   - Handles irregular memory access patterns
 *   - CSR traversal and multiplication
 *
 * Collaboration: Host builds CSR → Device computes SpMV → Host verifies
 * Complexity: Sparse data structure, irregular parallelism, CSR format
 */

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cmath>
#include "zsim_hooks.h"
#include "../cosim_pe_parallel.h"

// CSR (Compressed Sparse Row) format
struct CSRMatrix {
    int num_rows;
    int num_cols;
    int nnz;                // Number of non-zeros
    int* row_ptr;           // Row pointers (size: num_rows + 1)
    int* col_idx;           // Column indices (size: nnz)
    double* values;         // Non-zero values (size: nnz)
};

struct SpMVData {
    CSRMatrix matrix;
    double* input_vector;   // Dense input vector
    double* output_vector;  // Dense output vector
    int num_device_pes;
    int* pe_row_counts;     // Rows processed by each PE
};

// ============================================================================
// HOST CODE (runs on OOO/Simple core with cache)
// ============================================================================

class HostSparseManager {
private:
    SpMVData& data;

public:
    HostSparseManager(SpMVData& spmv_data) : data(spmv_data) {}

    // HOST: Generate sparse matrix (deterministic sparsity pattern)
    void generateSparseMatrix(double sparsity) {
        std::cout << "[HOST] Generating " << data.matrix.num_rows << "×"
                  << data.matrix.num_cols << " sparse matrix" << std::endl;
        std::cout << "[HOST] Target sparsity: " << (sparsity * 100) << "%" << std::endl;

        // Allocate CSR arrays
        data.matrix.row_ptr = new int[data.matrix.num_rows + 1];
        data.pe_row_counts = new int[data.num_device_pes];

        // Generate sparsity pattern deterministically
        std::vector<std::vector<int>> nonzero_cols(data.matrix.num_rows);

        int total_nnz = 0;
        for (int i = 0; i < data.matrix.num_rows; i++) {
            for (int j = 0; j < data.matrix.num_cols; j++) {
                // Deterministic pattern: hash(i,j) determines if non-zero
                int hash = (i * 7 + j * 11) % 100;
                if (hash >= (int)(sparsity * 100)) {
                    nonzero_cols[i].push_back(j);
                    total_nnz++;
                }
            }
        }

        data.matrix.nnz = total_nnz;
        std::cout << "[HOST] Non-zeros: " << total_nnz << " ("
                  << (100.0 * total_nnz / (data.matrix.num_rows * data.matrix.num_cols))
                  << "% density)" << std::endl;

        // Build row_ptr
        data.matrix.row_ptr[0] = 0;
        for (int i = 0; i < data.matrix.num_rows; i++) {
            data.matrix.row_ptr[i + 1] = data.matrix.row_ptr[i] + nonzero_cols[i].size();
        }

        // Allocate and fill col_idx and values
        data.matrix.col_idx = new int[total_nnz];
        data.matrix.values = new double[total_nnz];

        int nnz_count = 0;
        for (int i = 0; i < data.matrix.num_rows; i++) {
            for (size_t k = 0; k < nonzero_cols[i].size(); k++) {
                int j = nonzero_cols[i][k];
                data.matrix.col_idx[nnz_count] = j;
                // Pseudo-random value based on position
                data.matrix.values[nnz_count] = ((i * 13 + j * 17) % 100) / 10.0;
                nnz_count++;
            }
        }

        std::cout << "[HOST] CSR format constructed:" << std::endl;
        std::cout << "[HOST]   row_ptr size: " << (data.matrix.num_rows + 1) << std::endl;
        std::cout << "[HOST]   col_idx size: " << total_nnz << std::endl;
        std::cout << "[HOST]   values size: " << total_nnz << std::endl;
    }

    // HOST: Initialize vectors
    void initializeVectors() {
        std::cout << "[HOST] Initializing input/output vectors" << std::endl;

        data.input_vector = new double[data.matrix.num_cols];
        data.output_vector = new double[data.matrix.num_rows];

        // Initialize input vector
        for (int i = 0; i < data.matrix.num_cols; i++) {
            data.input_vector[i] = (double)rand() / RAND_MAX;
        }

        // Initialize output vector
        for (int i = 0; i < data.matrix.num_rows; i++) {
            data.output_vector[i] = 0.0;
        }

        for (int pe = 0; pe < data.num_device_pes; pe++) {
            data.pe_row_counts[pe] = 0;
        }
    }

    // HOST: Distribute rows to PEs
    void coordinateSpMV() {
        int rows_per_pe = (data.matrix.num_rows + data.num_device_pes - 1) /
                         data.num_device_pes;
        std::cout << "[HOST] Distributing rows to " << data.num_device_pes << " PEs" << std::endl;
        std::cout << "[HOST] Rows per PE: ~" << rows_per_pe << std::endl;
    }

    // HOST: Verify result
    void verifyResult() {
        std::cout << "[HOST] Verifying SpMV result..." << std::endl;

        // Verify by checking a sample row
        int sample_row = data.matrix.num_rows / 2;
        double expected = 0.0;

        int row_start = data.matrix.row_ptr[sample_row];
        int row_end = data.matrix.row_ptr[sample_row + 1];

        for (int j = row_start; j < row_end; j++) {
            int col = data.matrix.col_idx[j];
            double val = data.matrix.values[j];
            expected += val * data.input_vector[col];
        }

        double actual = data.output_vector[sample_row];
        double error = std::abs(expected - actual);

        std::cout << "[HOST] Sample row " << sample_row << ":" << std::endl;
        std::cout << "[HOST]   Expected: " << expected << std::endl;
        std::cout << "[HOST]   Actual: " << actual << std::endl;
        std::cout << "[HOST]   Error: " << error << std::endl;

        if (error < 1e-6) {
            std::cout << "[HOST] ✓ Verification passed!" << std::endl;
        } else {
            std::cout << "[HOST] ✗ Verification failed!" << std::endl;
        }

        // Statistics
        int total_rows = 0;
        for (int pe = 0; pe < data.num_device_pes; pe++) {
            total_rows += data.pe_row_counts[pe];
        }
        std::cout << "[HOST] Total rows processed: " << total_rows << std::endl;
    }

    // HOST: Cleanup
    void cleanup() {
        std::cout << "[HOST] Cleaning up sparse matrix data" << std::endl;
        delete[] data.matrix.row_ptr;
        delete[] data.matrix.col_idx;
        delete[] data.matrix.values;
        delete[] data.input_vector;
        delete[] data.output_vector;
        delete[] data.pe_row_counts;
    }
};

// ============================================================================
// DEVICE CODE (runs on ALU cores without cache)
// ============================================================================

class DeviceSparseComputer {
private:
    int pe_id;
    SpMVData& data;

public:
    DeviceSparseComputer(int id, SpMVData& spmv_data)
        : pe_id(id), data(spmv_data) {}

    // DEVICE: Compute SpMV for assigned rows
    void computeSpMV() {
        int rows_per_pe = (data.matrix.num_rows + data.num_device_pes - 1) /
                         data.num_device_pes;
        int start_row = pe_id * rows_per_pe;
        int end_row = std::min(start_row + rows_per_pe, data.matrix.num_rows);

        // DEVICE: Sparse matrix computation (irregular memory access)
        for (int i = start_row; i < end_row; i++) {
            double sum = 0.0;
            int row_start = data.matrix.row_ptr[i];
            int row_end = data.matrix.row_ptr[i + 1];

            // Traverse sparse row
            for (int j = row_start; j < row_end; j++) {
                int col = data.matrix.col_idx[j];
                double val = data.matrix.values[j];
                sum += val * data.input_vector[col];
            }

            data.output_vector[i] = sum;
            data.pe_row_counts[pe_id]++;
        }

        if (pe_id == 0) {
            std::cout << "[DEVICE PE-" << pe_id << "] Computed SpMV for rows "
                      << start_row << " to " << end_row << std::endl;
        }
    }
};

// ============================================================================
// DEVICE KERNEL (offloaded via pimid_offload_sync)
// ============================================================================

static void device_kernel(void* raw) {
    SpMVData* data = (SpMVData*)raw;
    pimid_parallel_pes(data->num_device_pes, [&](int i) {
        DeviceSparseComputer pe(i, *data);
        pe.computeSpMV();
    });
}

// ============================================================================
// MAIN: Orchestrates Host/Device Co-Simulation
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <matrix_size> <sparsity_percent> <num_device_pes>" << std::endl;
        std::cerr << "  sparsity_percent: 70-99 (higher = more sparse)" << std::endl;
        return 1;
    }

    int matrix_size = std::atoi(argv[1]);
    int sparsity_pct = std::atoi(argv[2]);
    int num_pes = std::atoi(argv[3]);

    double sparsity = sparsity_pct / 100.0;

    std::cout << "=== HOST/DEVICE CO-SIMULATION: Sparse Matrix-Vector Multiply ===\n";
    std::cout << "Matrix size: " << matrix_size << "×" << matrix_size << std::endl;
    std::cout << "Sparsity: " << sparsity_pct << "%" << std::endl;
    std::cout << "Device PEs: " << num_pes << std::endl;
    std::cout << std::endl;

    SpMVData spmv_data;
    spmv_data.matrix.num_rows = matrix_size;
    spmv_data.matrix.num_cols = matrix_size;
    spmv_data.num_device_pes = num_pes;

    // HOST: Setup
    HostSparseManager host(spmv_data);
    host.generateSparseMatrix(sparsity);
    host.initializeVectors();
    host.coordinateSpMV();

    std::cout << "--- OFFLOADING TO DEVICE ---\n";

    // DEVICE: Offload SpMV to ALU cores via ZSim hooks
    pimid_offload_sync(device_kernel, &spmv_data);

    std::cout << "--- RETURNING TO HOST ---\n";

    // HOST: Verify
    host.verifyResult();
    host.cleanup();

    std::cout << "\n=== Co-Simulation Complete ===\n";
    return 0;
}
