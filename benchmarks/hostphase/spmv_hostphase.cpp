/**
 * @file spmv_hostphase.cpp
 * @brief HOST-PHASE term of the composed co-simulation (SpMV CSR).
 *
 * Host work for the SpMV offload: CSR matrix + input-vector generation,
 * gathering of the output sub-vectors returned by the PEs, result touch.
 * No device computation, no verification recompute (see reduction_hostphase).
 */
#include <iostream>
#include <cstdlib>
#include <vector>
#include "zsim_hooks.h"

int main(int argc, char* argv[]) {
    int m = (argc > 1) ? std::atoi(argv[1]) : 2048;        // matrix dim
    int sparsity = (argc > 2) ? std::atoi(argv[2]) : 90;   // percent zeros
    int pes = (argc > 3) ? std::atoi(argv[3]) : 8;

    zsim_roi_begin();

    // host_pre: CSR generation (same construction as the cosim host code)
    std::vector<int> row_ptr(m + 1, 0);
    std::vector<int> col_idx;
    std::vector<double> values;
    for (int r = 0; r < m; r++) {
        for (int c = 0; c < m; c++) {
            if (rand() % 100 >= sparsity) {
                col_idx.push_back(c);
                values.push_back((rand() % 100) / 10.0);
            }
        }
        row_ptr[r + 1] = (int)col_idx.size();
    }
    std::vector<double> x(m);
    for (int i = 0; i < m; i++) x[i] = (rand() % 100) / 10.0;

    // (device computes y; host gathers the returned sub-vectors)
    std::vector<double> y(m, 0.0);

    // host_post: touch the gathered output (row-count bookkeeping + checksum)
    int rows_per_pe = m / pes;
    long covered = 0;
    for (int p = 0; p < pes; p++)
        covered += (p == pes - 1) ? (m - p * rows_per_pe) : rows_per_pe;
    double checksum = 0.0;
    for (int i = 0; i < m; i++) checksum += y[i];

    zsim_roi_end();

    std::cout << "[HOSTPHASE] spmv m=" << m << " sparsity=" << sparsity
              << " nnz=" << values.size() << " covered=" << covered
              << " checksum=" << checksum << std::endl;
    std::cout << "[HOSTPHASE] done" << std::endl;
    return 0;
}
