/**
 * @file reduction_tree_cosim.cpp
 * @brief TRUE Host/Device Co-Simulation: Hierarchical Reduction
 *
 * HOST (OOO core with cache):
 *   - Generates input array
 *   - Coordinates reduction phases
 *   - Performs final reduction of partial results
 *   - Verifies correctness
 *
 * DEVICE (ALU cores without cache):
 *   - Each PE performs local reduction on its chunk
 *   - Produces partial results
 *   - Parallel reduction operations
 *
 * Communication: Shared memory for data and partial results
 */

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cmath>
#include <pthread.h>
#include "zsim_hooks.h"

enum ReductionOp {
    SUM,
    MAX,
    MIN,
    PRODUCT
};

struct SharedReductionData {
    double* data;           // Input array
    int data_size;
    int num_device_pes;
    double* partial_results;  // One result per PE
    double final_result;      // Final reduced value
    ReductionOp operation;
};

// ============================================================================
// HOST CODE (runs on OOO/Simple core with cache)
// ============================================================================

class HostReductionCoordinator {
private:
    SharedReductionData& shared;

public:
    HostReductionCoordinator(SharedReductionData& data) : shared(data) {}

    // HOST: Generate input data
    void generateInputData() {
        std::cout << "[HOST] Generating input array (" << shared.data_size
                  << " elements)" << std::endl;

        shared.data = new double[shared.data_size];

        for (int i = 0; i < shared.data_size; i++) {
            shared.data[i] = (rand() % 100) / 10.0;
        }

        std::cout << "[HOST] Sample data: ";
        for (int i = 0; i < std::min(10, shared.data_size); i++) {
            std::cout << shared.data[i] << " ";
        }
        std::cout << "..." << std::endl;
    }

    // HOST: Allocate partial results array
    void allocatePartialResults() {
        std::cout << "[HOST] Allocating partial results for "
                  << shared.num_device_pes << " PEs" << std::endl;

        shared.partial_results = new double[shared.num_device_pes];
    }

    // HOST: Coordinate reduction phases
    void coordinateReduction() {
        int chunk_size = shared.data_size / shared.num_device_pes;

        std::cout << "[HOST] Phase 1: Device PEs perform local reductions"
                  << std::endl;
        std::cout << "[HOST] Each PE reduces ~" << chunk_size
                  << " elements" << std::endl;
        std::cout << "[HOST] Phase 2: Host performs final reduction of "
                  << shared.num_device_pes << " partial results" << std::endl;
    }

    // HOST: Perform final reduction
    void performFinalReduction() {
        std::cout << "[HOST] Performing final reduction..." << std::endl;

        switch (shared.operation) {
            case SUM:
                shared.final_result = 0.0;
                for (int i = 0; i < shared.num_device_pes; i++) {
                    shared.final_result += shared.partial_results[i];
                }
                break;

            case MAX:
                shared.final_result = shared.partial_results[0];
                for (int i = 1; i < shared.num_device_pes; i++) {
                    if (shared.partial_results[i] > shared.final_result) {
                        shared.final_result = shared.partial_results[i];
                    }
                }
                break;

            case MIN:
                shared.final_result = shared.partial_results[0];
                for (int i = 1; i < shared.num_device_pes; i++) {
                    if (shared.partial_results[i] < shared.final_result) {
                        shared.final_result = shared.partial_results[i];
                    }
                }
                break;

            case PRODUCT:
                shared.final_result = 1.0;
                for (int i = 0; i < shared.num_device_pes; i++) {
                    shared.final_result *= shared.partial_results[i];
                }
                break;
        }

        std::cout << "[HOST] ✓ Final reduction complete" << std::endl;
    }

    // HOST: Display partial results
    void displayPartialResults() {
        std::cout << "[HOST] Partial results from device:" << std::endl;
        for (int i = 0; i < shared.num_device_pes; i++) {
            std::cout << "[HOST]   PE-" << i << ": "
                      << shared.partial_results[i] << std::endl;
        }
    }

    // HOST: Display final result
    void displayFinalResult() {
        const char* op_names[] = {"SUM", "MAX", "MIN", "PRODUCT"};

        std::cout << "[HOST] Final result (" << op_names[shared.operation]
                  << "): " << shared.final_result << std::endl;
    }

    // HOST: Verify correctness (sequential reduction)
    void verifyCorrectness() {
        std::cout << "[HOST] Verifying result..." << std::endl;

        double expected = 0.0;
        switch (shared.operation) {
            case SUM:
                expected = 0.0;
                for (int i = 0; i < shared.data_size; i++) {
                    expected += shared.data[i];
                }
                break;

            case MAX:
                expected = shared.data[0];
                for (int i = 1; i < shared.data_size; i++) {
                    if (shared.data[i] > expected) expected = shared.data[i];
                }
                break;

            case MIN:
                expected = shared.data[0];
                for (int i = 1; i < shared.data_size; i++) {
                    if (shared.data[i] < expected) expected = shared.data[i];
                }
                break;

            case PRODUCT:
                expected = 1.0;
                for (int i = 0; i < shared.data_size; i++) {
                    expected *= shared.data[i];
                }
                break;
        }

        double error = std::abs(shared.final_result - expected);
        if (error < 0.001) {
            std::cout << "[HOST] ✓ Result verified!" << std::endl;
            std::cout << "[HOST]   Expected: " << expected << std::endl;
            std::cout << "[HOST]   Got: " << shared.final_result << std::endl;
        } else {
            std::cout << "[HOST] ✗ Verification failed!" << std::endl;
            std::cout << "[HOST]   Expected: " << expected << std::endl;
            std::cout << "[HOST]   Got: " << shared.final_result << std::endl;
            std::cout << "[HOST]   Error: " << error << std::endl;
        }
    }

    // HOST: Cleanup
    void cleanup() {
        std::cout << "[HOST] Cleaning up memory" << std::endl;
        delete[] shared.data;
        delete[] shared.partial_results;
    }
};

// ============================================================================
// DEVICE CODE (runs on ALU cores without cache)
// ============================================================================

class DeviceReducer {
private:
    int pe_id;
    SharedReductionData& shared;

public:
    DeviceReducer(int id, SharedReductionData& data)
        : pe_id(id), shared(data) {}

    // DEVICE: Perform local reduction
    void performLocalReduction() {
        int chunk_size = shared.data_size / shared.num_device_pes;
        int start = pe_id * chunk_size;
        int end = (pe_id == shared.num_device_pes - 1) ?
                  shared.data_size : (pe_id + 1) * chunk_size;

        // DEVICE: Initialize partial result
        double local_result;
        switch (shared.operation) {
            case SUM:
                local_result = 0.0;
                break;
            case MAX:
            case MIN:
                local_result = shared.data[start];
                break;
            case PRODUCT:
                local_result = 1.0;
                break;
        }

        // DEVICE: Compute-intensive reduction loop
        for (int i = start; i < end; i++) {
            switch (shared.operation) {
                case SUM:
                    local_result += shared.data[i];
                    break;
                case MAX:
                    if (shared.data[i] > local_result) {
                        local_result = shared.data[i];
                    }
                    break;
                case MIN:
                    if (shared.data[i] < local_result) {
                        local_result = shared.data[i];
                    }
                    break;
                case PRODUCT:
                    local_result *= shared.data[i];
                    break;
            }
        }

        shared.partial_results[pe_id] = local_result;

        if (pe_id == 0) {
            std::cout << "[DEVICE PE-" << pe_id << "] Reduced elements "
                      << start << " to " << end
                      << " → " << local_result << std::endl;
        }
    }
};

// ============================================================================
// DEVICE KERNEL (offloaded via pimid_offload_sync)
// ============================================================================

// SERIAL variant: no device parallelism. A single device worker performs every
// PE's chunk inside the offload region (plain loop on one thread), so all work
// is attributed to one device context rather than N PEs.
static void device_kernel(void* raw) {
    SharedReductionData* shared = (SharedReductionData*)raw;
    int n = shared->num_device_pes;
    for (int pe_id = 0; pe_id < n; pe_id++) {
        DeviceReducer pe(pe_id, *shared);
        pe.performLocalReduction();
    }
}

// ============================================================================
// MAIN: Orchestrates Host/Device Co-Simulation
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <array_size> <num_device_pes> <operation>" << std::endl;
        std::cerr << "  operation: 0=SUM, 1=MAX, 2=MIN, 3=PRODUCT" << std::endl;
        return 1;
    }

    int data_size = std::atoi(argv[1]);
    int num_pes = std::atoi(argv[2]);
    int op = std::atoi(argv[3]);

    if (op < 0 || op > 3) op = 0;  // Default to SUM

    std::cout << "=== HOST/DEVICE CO-SIMULATION: Hierarchical Reduction ===" << std::endl;
    std::cout << "Array size: " << data_size << " elements" << std::endl;
    std::cout << "Device PEs: " << num_pes << std::endl;
    std::cout << "Operation: " << (op == 0 ? "SUM" : op == 1 ? "MAX" : op == 2 ? "MIN" : "PRODUCT")
              << std::endl;
    std::cout << std::endl;

    SharedReductionData shared_data;
    shared_data.data_size = data_size;
    shared_data.num_device_pes = num_pes;
    shared_data.operation = static_cast<ReductionOp>(op);

    // HOST: Prepare data
    HostReductionCoordinator host(shared_data);
    host.generateInputData();
    host.allocatePartialResults();
    host.coordinateReduction();

    std::cout << std::endl;
    std::cout << "--- OFFLOADING TO DEVICE (Phase 1: Local Reductions) ---" << std::endl;
    std::cout << std::endl;

    // DEVICE: Offload local reductions to ALU cores via ZSim hooks
    pimid_offload_sync(device_kernel, &shared_data);

    std::cout << std::endl;
    std::cout << "--- RETURNING TO HOST (Phase 2: Final Reduction) ---" << std::endl;
    std::cout << std::endl;

    // HOST: Final reduction and verification
    host.displayPartialResults();
    host.performFinalReduction();
    host.displayFinalResult();
    host.verifyCorrectness();
    host.cleanup();

    std::cout << std::endl;
    std::cout << "=== Co-Simulation Complete ===" << std::endl;

    return 0;
}
