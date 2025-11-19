/**
 * @file spmv_shared_pimid.cpp
 * @brief SpMV (Sparse Matrix-Vector Multiply) workload with SHARED MEMORY model - PIMID integrated
 *
 * This version uses PIMID simulator for accurate energy and timing modeling
 * Technology: 45nm
 * Frequency: 1GHz
 */

#include "../../../DAC26/pimid_adapter/pim_simulator.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>

using namespace dac26;

struct SpMVConfig {
    int num_subarrays;
    int num_rows;
    int num_cols;
    double sparsity;
    Topology topology;
};

struct SparseRow {
    std::vector<int> col_indices;
    std::vector<double> values;
};

class SpMVSharedPIMID {
private:
    SpMVConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    std::vector<SparseRow> sparse_matrix;
    std::vector<int> row_assignment;
    std::vector<double> vector_x;
    std::vector<double> result_y;
    uint64_t nnz;

public:
    SpMVSharedPIMID(const SpMVConfig& cfg) : config(cfg), nnz(0) {
        sparse_matrix.resize(config.num_rows);
        row_assignment.resize(config.num_rows);
        vector_x.resize(config.num_cols, 1.0);
        result_y.resize(config.num_rows, 0.0);

        // Distribute rows across subarrays for processing
        for (int i = 0; i < config.num_rows; i++) {
            row_assignment[i] = i % config.num_subarrays;
        }

        generateSparseMatrix();

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
        std::cout << "\n=== SpMV Workload (SHARED MEMORY - PIMID) ===" << std::endl;
        std::cout << "Matrix size: " << config.num_rows << "×" << config.num_cols << std::endl;
        std::cout << "Sparsity: " << (config.sparsity * 100) << "%" << std::endl;
        std::cout << "Non-zeros: " << nnz << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;

        simulator->resetStats();
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
                nnz++;
            }
        }

        std::cout << "  Generated " << nnz << " non-zero elements" << std::endl;
    }

    void computeSpMV() {
        std::cout << "\nComputing y = A × x (shared memory)..." << std::endl;

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

        // Synchronization barrier
        std::cout << "\nSynchronization barrier" << std::endl;
        simulator->simulateOperation(PIMOperation::BARRIER_SYNC, 1);

        std::cout << "✓ SpMV computation complete" << std::endl;
    }

    void computeRowDotProduct(int row_id, int subarray_id) {
        double partial_sum = 0.0;

        // Process all non-zero elements in this row
        for (size_t k = 0; k < sparse_matrix[row_id].col_indices.size(); k++) {
            int col_idx = sparse_matrix[row_id].col_indices[k];
            double matrix_value = sparse_matrix[row_id].values[k];

            // Read matrix value from shared memory (local)
            simulator->simulateMemoryAccess(true, true, sizeof(double));

            // Read x[col_idx] from shared memory (potentially remote)
            int col_owner = col_idx % config.num_subarrays;
            if (col_owner == subarray_id) {
                simulator->simulateMemoryAccess(true, true, sizeof(double));
            } else {
                simulator->simulateMemoryAccess(false, true, sizeof(double));
            }

            // Multiply-accumulate
            simulator->simulateCompute(1);

            partial_sum += matrix_value * vector_x[col_idx];
        }

        // Atomic write to shared result vector
        simulator->simulateOperation(PIMOperation::ATOMIC_OP, 1);

        result_y[row_id] = partial_sum;
    }

    void printMetrics() {
        std::cout << "\n=== SpMV Results (SHARED MEMORY - PIMID) ===" << std::endl;

        const SimulationResults& results = simulator->getResults();

        std::cout << "\nTotal cycles: " << results.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << results.compute_cycles
                  << " (" << (100.0 * results.compute_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Memory cycles: " << results.memory_cycles
                  << " (" << (100.0 * results.memory_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Network cycles: " << results.network_cycles
                  << " (" << (100.0 * results.network_cycles / results.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Total MAC operations: " << nnz << std::endl;
        std::cout << "  Compute operations: " << results.compute_ops << std::endl;

        std::cout << "\nCommunication (Shared Memory):" << std::endl;
        std::cout << "  Inter-subarray transfers: 0 (shared memory model)" << std::endl;
        std::cout << "  Local reads: " << results.local_reads << std::endl;
        std::cout << "  Local writes: " << results.local_writes << std::endl;
        std::cout << "  Remote accesses: " << results.remote_reads + results.remote_writes << std::endl;

        std::cout << "\nEnergy (PIMID-based):" << std::endl;
        std::cout << "  Total energy: " << results.total_energy_pJ << " pJ" << std::endl;
        std::cout << "  Compute energy: " << results.compute_energy_pJ << " pJ ("
                  << (100.0 * results.compute_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;
        std::cout << "  Memory energy: " << results.memory_energy_pJ << " pJ ("
                  << (100.0 * results.memory_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;
        std::cout << "  Network energy: " << results.network_energy_pJ << " pJ ("
                  << (100.0 * results.network_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;

        std::cout << "\nPerformance:" << std::endl;
        std::cout << "  Execution time: " << results.execution_time_ns << " ns" << std::endl;
        std::cout << "  Execution time: " << (results.execution_time_ns / 1000.0) << " us" << std::endl;

        // Validation
        std::cout << "\nValidation:" << std::endl;
        std::cout << "  MAC operations: " << (results.compute_ops == nnz ? "✓" : "✗") << std::endl;
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
    config.topology = is_libcom ? Topology::LIBCOM : Topology::HTREE_BASELINE;

    config.sparsity = 0.05;  // 5% non-zero

    std::cout << "\n=== DAC'26 SpMV Benchmark (Shared Memory - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    SpMVSharedPIMID workload(config);
    workload.execute();

    return 0;
}
