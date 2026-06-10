/**
 * @file spmv_csr_message_passing.cpp
 * @brief Host/Device co-simulation: sparse matrix-vector multiply, MESSAGE-PASSING.
 *
 * Serial host builds a CSR matrix and offloads row ranges to N device PEs.
 * Unlike the shared_memory variant (PEs traverse the host CSR arrays and the
 * input vector through shared pointers and write the output vector in place),
 * here each PE runs FULL-DMA:
 *   (1) host->device  : DMA into PRIVATE buffers — the whole dense input vector,
 *                       plus THIS PE's slice of the CSR structure (row_ptr slice,
 *                       col_idx slice, values slice) — all via pimid_pe_recv
 *                       (each charged on the host<->device link),
 *   (2) compute        : SpMV for the PE's rows using ONLY the private copies,
 *                       producing a private output sub-vector,
 *   (3) device->host  : DMA the output sub-vector back via pimid_pe_send.
 * The PE never dereferences host CSR/vector pointers in the compute loop.
 *
 * Result correctness is identical to the shared_memory variant.
 */

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cmath>
#include "zsim_hooks.h"
#include "../cosim_pe_message.h"

// CSR (Compressed Sparse Row) format
struct CSRMatrix {
    int num_rows;
    int num_cols;
    int nnz;
    int* row_ptr;
    int* col_idx;
    double* values;
};

struct SpMVData {
    CSRMatrix matrix;
    double* input_vector;
    double* output_vector;   // host-side landing buffer (filled via messages)
    int num_device_pes;
    int* pe_row_counts;
};

// ============================================================================
// HOST CODE (runs on OOO/Simple core with cache)
// ============================================================================

class HostSparseManager {
private:
    SpMVData& data;

public:
    HostSparseManager(SpMVData& spmv_data) : data(spmv_data) {}

    void generateSparseMatrix(double sparsity) {
        std::cout << "[HOST] Generating " << data.matrix.num_rows << "\xC3\x97"
                  << data.matrix.num_cols << " sparse matrix" << std::endl;
        std::cout << "[HOST] Target sparsity: " << (sparsity * 100) << "%" << std::endl;

        data.matrix.row_ptr = new int[data.matrix.num_rows + 1];
        data.pe_row_counts = new int[data.num_device_pes];

        std::vector<std::vector<int> > nonzero_cols(data.matrix.num_rows);

        int total_nnz = 0;
        for (int i = 0; i < data.matrix.num_rows; i++) {
            for (int j = 0; j < data.matrix.num_cols; j++) {
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

        data.matrix.row_ptr[0] = 0;
        for (int i = 0; i < data.matrix.num_rows; i++) {
            data.matrix.row_ptr[i + 1] = data.matrix.row_ptr[i] + nonzero_cols[i].size();
        }

        data.matrix.col_idx = new int[total_nnz];
        data.matrix.values = new double[total_nnz];

        int nnz_count = 0;
        for (int i = 0; i < data.matrix.num_rows; i++) {
            for (size_t k = 0; k < nonzero_cols[i].size(); k++) {
                int j = nonzero_cols[i][k];
                data.matrix.col_idx[nnz_count] = j;
                data.matrix.values[nnz_count] = ((i * 13 + j * 17) % 100) / 10.0;
                nnz_count++;
            }
        }

        std::cout << "[HOST] CSR format constructed:" << std::endl;
        std::cout << "[HOST]   row_ptr size: " << (data.matrix.num_rows + 1) << std::endl;
        std::cout << "[HOST]   col_idx size: " << total_nnz << std::endl;
        std::cout << "[HOST]   values size: " << total_nnz << std::endl;
    }

    void initializeVectors() {
        std::cout << "[HOST] Initializing input/output vectors" << std::endl;

        data.input_vector = new double[data.matrix.num_cols];
        data.output_vector = new double[data.matrix.num_rows];

        for (int i = 0; i < data.matrix.num_cols; i++) {
            data.input_vector[i] = (double)rand() / RAND_MAX;
        }
        for (int i = 0; i < data.matrix.num_rows; i++) {
            data.output_vector[i] = 0.0;
        }
        for (int pe = 0; pe < data.num_device_pes; pe++) {
            data.pe_row_counts[pe] = 0;
        }
    }

    void coordinateSpMV() {
        int rows_per_pe = (data.matrix.num_rows + data.num_device_pes - 1) /
                         data.num_device_pes;
        std::cout << "[HOST] Distributing rows to " << data.num_device_pes << " PEs" << std::endl;
        std::cout << "[HOST] Rows per PE: ~" << rows_per_pe << std::endl;
    }

    void verifyResult() {
        std::cout << "[HOST] Verifying SpMV result..." << std::endl;

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
            std::cout << "[HOST] \xE2\x9C\x93 Verification passed!" << std::endl;
        } else {
            std::cout << "[HOST] \xE2\x9C\x97 Verification failed!" << std::endl;
        }

        int total_rows = 0;
        for (int pe = 0; pe < data.num_device_pes; pe++) {
            total_rows += data.pe_row_counts[pe];
        }
        std::cout << "[HOST] Total rows processed: " << total_rows << std::endl;
    }

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
// DEVICE CODE — full-DMA: recv input vector + this PE's CSR slice, compute
// privately, send the output sub-vector back.
// ============================================================================

static void device_kernel(void* raw) {
    SpMVData* data = (SpMVData*)raw;
    int n = data->num_device_pes;
    pimid_parallel_pes_msg(n, [&](int pe_id) {
        int rows_per_pe = (data->matrix.num_rows + n - 1) / n;
        int start_row = pe_id * rows_per_pe;
        int end_row = std::min(start_row + rows_per_pe, data->matrix.num_rows);
        if (start_row >= end_row) { return; }  // no rows for this PE
        int num_rows = end_row - start_row;

        // Nonzero range covered by this PE's rows.
        int nz_start = data->matrix.row_ptr[start_row];
        int nz_end   = data->matrix.row_ptr[end_row];
        int nz_len   = nz_end - nz_start;

        unsigned vec_bytes  = (unsigned)(data->matrix.num_cols * sizeof(double));
        unsigned rptr_bytes = (unsigned)((num_rows + 1) * sizeof(int));
        unsigned col_bytes  = (unsigned)(nz_len * sizeof(int));
        unsigned val_bytes  = (unsigned)(nz_len * sizeof(double));

        // FULL-DMA (1): host->device — copy into PRIVATE device buffers:
        //   - the whole dense input vector,
        //   - this PE's row_ptr slice (rows start_row..end_row inclusive),
        //   - this PE's col_idx and values slices.
        std::vector<double> vec_priv(data->matrix.num_cols);
        pimid_pe_recv(vec_priv.data(), data->input_vector, vec_bytes);

        std::vector<int> rptr_priv(num_rows + 1);
        pimid_pe_recv(rptr_priv.data(), &data->matrix.row_ptr[start_row], rptr_bytes);

        std::vector<int> col_priv(nz_len > 0 ? nz_len : 1);
        std::vector<double> val_priv(nz_len > 0 ? nz_len : 1);
        if (nz_len > 0) {
            pimid_pe_recv(col_priv.data(), &data->matrix.col_idx[nz_start], col_bytes);
            pimid_pe_recv(val_priv.data(), &data->matrix.values[nz_start], val_bytes);
        }

        // (2) compute SpMV for this PE's rows using ONLY private copies.
        // Rebase row_ptr/nonzero indices to the private slices.
        std::vector<double> out_priv(num_rows);
        for (int r = 0; r < num_rows; r++) {
            double sum = 0.0;
            int rs = rptr_priv[r]     - nz_start;
            int re = rptr_priv[r + 1] - nz_start;
            for (int j = rs; j < re; j++) {
                int col = col_priv[j];
                double val = val_priv[j];
                sum += val * vec_priv[col];
            }
            out_priv[r] = sum;
        }
        data->pe_row_counts[pe_id] = num_rows;

        // FULL-DMA (3): device->host — ship the output sub-vector back.
        pimid_pe_send(&data->output_vector[start_row], out_priv.data(),
                      (unsigned)(num_rows * sizeof(double)));

        if (pe_id == 0) {
            unsigned in_bytes = vec_bytes + rptr_bytes + col_bytes + val_bytes;
            std::cout << "[DEVICE PE-0] DMA in " << in_bytes << " B (vec+row_ptr+col+val), "
                      << "computed SpMV rows " << start_row << " to " << end_row
                      << ", DMA out " << (num_rows * sizeof(double)) << " B" << std::endl;
        }
    });
}

// ============================================================================
// MAIN
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

    std::cout << "=== HOST/DEVICE CO-SIM (message-passing): Sparse Matrix-Vector Multiply ===\n";
    std::cout << "Matrix size: " << matrix_size << "\xC3\x97" << matrix_size << std::endl;
    std::cout << "Sparsity: " << sparsity_pct << "%" << std::endl;
    std::cout << "Device PEs: " << num_pes << std::endl;
    std::cout << std::endl;

    SpMVData spmv_data;
    spmv_data.matrix.num_rows = matrix_size;
    spmv_data.matrix.num_cols = matrix_size;
    spmv_data.num_device_pes = num_pes;

    HostSparseManager host(spmv_data);
    host.generateSparseMatrix(sparsity);
    host.initializeVectors();
    host.coordinateSpMV();

    std::cout << "--- OFFLOADING TO DEVICE (PEs DMA CSR slice in, sub-vector out) ---\n";

    pimid_offload_sync(device_kernel, &spmv_data);

    std::cout << "--- RETURNING TO HOST ---\n";

    host.verifyResult();
    host.cleanup();

    std::cout << "\n=== Co-Simulation Complete ===\n";
    return 0;
}
