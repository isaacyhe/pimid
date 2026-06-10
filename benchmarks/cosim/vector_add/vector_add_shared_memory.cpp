/**
 * @file vector_add_cosim.cpp
 * @brief TRUE Host/Device Co-Simulation: Vector Addition
 *
 * HOST (OOO core with cache):
 *   - Allocates and initializes vectors A and B
 *   - Distributes work to device cores
 *   - Verifies results and computes checksum
 *
 * DEVICE (ALU cores without cache):
 *   - Performs parallel vector addition C = A + B
 *   - Each PE handles a chunk of the vectors
 *
 * Communication: Shared memory arrays
 */

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cassert>
#include "zsim_hooks.h"
#include "../cosim_pe_parallel.h"

// Shared memory arrays (accessible by both host and device)
struct SharedData {
    float* A;
    float* B;
    float* C;
    int size;
    int num_device_pes;
};

// ============================================================================
// HOST CODE (runs on OOO/Simple core with cache)
// ============================================================================

class HostCoordinator {
private:
    SharedData& shared;

public:
    HostCoordinator(SharedData& data) : shared(data) {}

    // HOST: Allocate and initialize vectors
    void prepareData() {
        std::cout << "[HOST] Allocating vectors (size=" << shared.size << ")" << std::endl;

        shared.A = new float[shared.size];
        shared.B = new float[shared.size];
        shared.C = new float[shared.size];

        std::cout << "[HOST] Initializing input vectors" << std::endl;
        for (int i = 0; i < shared.size; i++) {
            shared.A[i] = static_cast<float>(i);
            shared.B[i] = static_cast<float>(i * 2);
            shared.C[i] = 0.0f;  // Result array
        }
    }

    // HOST: Coordinate device execution
    void coordinateDeviceExecution() {
        std::cout << "[HOST] Coordinating " << shared.num_device_pes
                  << " device PEs" << std::endl;

        int chunk_size = shared.size / shared.num_device_pes;
        std::cout << "[HOST] Each PE will process " << chunk_size
                  << " elements" << std::endl;

        // HOST: Send work distribution info to device
        // (In real system: write to control registers or shared queue)
    }

    // HOST: Verify results and compute checksum
    void verifyAndAggregate() {
        std::cout << "[HOST] Verifying results..." << std::endl;

        int errors = 0;
        double checksum = 0.0;

        for (int i = 0; i < shared.size; i++) {
            float expected = shared.A[i] + shared.B[i];
            if (shared.C[i] != expected) {
                errors++;
                if (errors < 5) {  // Show first few errors
                    std::cout << "[HOST] Error at " << i << ": got "
                              << shared.C[i] << ", expected " << expected << std::endl;
                }
            }
            checksum += shared.C[i];
        }

        if (errors == 0) {
            std::cout << "[HOST] ✓ All results correct!" << std::endl;
        } else {
            std::cout << "[HOST] ✗ Found " << errors << " errors" << std::endl;
        }

        std::cout << "[HOST] Result checksum: " << checksum << std::endl;
    }

    // HOST: Cleanup
    void cleanup() {
        std::cout << "[HOST] Cleaning up memory" << std::endl;
        delete[] shared.A;
        delete[] shared.B;
        delete[] shared.C;
    }
};

// ============================================================================
// DEVICE CODE (runs on ALU cores without cache)
// ============================================================================

class DevicePE {
private:
    int pe_id;
    SharedData& shared;

public:
    DevicePE(int id, SharedData& data) : pe_id(id), shared(data) {}

    // DEVICE: Parallel vector addition
    void computeChunk() {
        int chunk_size = shared.size / shared.num_device_pes;
        int start = pe_id * chunk_size;
        int end = (pe_id == shared.num_device_pes - 1) ?
                  shared.size : (pe_id + 1) * chunk_size;

        // DEVICE: Compute-intensive loop (cacheless ALU operations)
        for (int i = start; i < end; i++) {
            shared.C[i] = shared.A[i] + shared.B[i];
        }

        if (pe_id == 0) {
            std::cout << "[DEVICE PE-" << pe_id << "] Computed elements "
                      << start << " to " << end << std::endl;
        }
    }
};

// ============================================================================
// DEVICE KERNEL (offloaded via pimid_offload_sync)
// ============================================================================

static void device_kernel(void* raw) {
    SharedData* shared = (SharedData*)raw;
    pimid_parallel_pes(shared->num_device_pes, [&](int i) {
        DevicePE pe(i, *shared);
        pe.computeChunk();
    });
}

// ============================================================================
// MAIN: Orchestrates Host/Device Co-Simulation
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <vector_size> <num_device_pes>" << std::endl;
        return 1;
    }

    int size = std::atoi(argv[1]);
    int num_pes = std::atoi(argv[2]);

    std::cout << "=== HOST/DEVICE CO-SIMULATION: Vector Addition ===" << std::endl;
    std::cout << "Vector size: " << size << std::endl;
    std::cout << "Device PEs: " << num_pes << std::endl;
    std::cout << std::endl;

    // Shared memory
    SharedData shared_data;
    shared_data.size = size;
    shared_data.num_device_pes = num_pes;

    // HOST: Prepare data
    HostCoordinator host(shared_data);
    host.prepareData();
    host.coordinateDeviceExecution();

    std::cout << std::endl;
    std::cout << "--- OFFLOADING TO DEVICE ---" << std::endl;
    std::cout << std::endl;

    // DEVICE: Offload computation to ALU cores via ZSim hooks
    pimid_offload_sync(device_kernel, &shared_data);

    std::cout << std::endl;
    std::cout << "--- RETURNING TO HOST ---" << std::endl;
    std::cout << std::endl;

    // HOST: Verify and aggregate
    host.verifyAndAggregate();
    host.cleanup();

    std::cout << std::endl;
    std::cout << "=== Co-Simulation Complete ===" << std::endl;

    return 0;
}
