/**
 * @file vector_add_message_passing.cpp
 * @brief Host/Device co-simulation: vector addition, MESSAGE-PASSING model.
 *
 * Serial host offloads C = A + B to N device PEs. Unlike the shared_memory
 * variant (PEs read A/B and write C directly through shared host pointers), here
 * each PE runs FULL-DMA:
 *   (1) host->device  : DMA this PE's A-chunk and B-chunk into PRIVATE buffers
 *                       via pimid_pe_recv (charged on the host<->device link),
 *   (2) compute        : C_priv = A_priv + B_priv on the PE's own copies,
 *   (3) device->host  : DMA the C-chunk back via pimid_pe_send (charged).
 * The PE never dereferences host A/B/C in the compute loop — that is the whole
 * point of the message-passing (non-coherent) data model.
 *
 * Result correctness is identical to the shared_memory variant.
 */

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cassert>
#include "zsim_hooks.h"
#include "../cosim_pe_message.h"

// Host-owned arrays (the device copies in/out by value, not by shared pointer).
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

    void prepareData() {
        std::cout << "[HOST] Allocating vectors (size=" << shared.size << ")" << std::endl;

        shared.A = new float[shared.size];
        shared.B = new float[shared.size];
        shared.C = new float[shared.size];

        std::cout << "[HOST] Initializing input vectors" << std::endl;
        for (int i = 0; i < shared.size; i++) {
            shared.A[i] = static_cast<float>(i);
            shared.B[i] = static_cast<float>(i * 2);
            shared.C[i] = 0.0f;
        }
    }

    void coordinateDeviceExecution() {
        std::cout << "[HOST] Coordinating " << shared.num_device_pes
                  << " device PEs" << std::endl;

        int chunk_size = shared.size / shared.num_device_pes;
        std::cout << "[HOST] Each PE will process " << chunk_size
                  << " elements" << std::endl;
    }

    void verifyAndAggregate() {
        std::cout << "[HOST] Verifying results..." << std::endl;

        int errors = 0;
        double checksum = 0.0;

        for (int i = 0; i < shared.size; i++) {
            float expected = shared.A[i] + shared.B[i];
            if (shared.C[i] != expected) {
                errors++;
                if (errors < 5) {
                    std::cout << "[HOST] Error at " << i << ": got "
                              << shared.C[i] << ", expected " << expected << std::endl;
                }
            }
            checksum += shared.C[i];
        }

        if (errors == 0) {
            std::cout << "[HOST] \xE2\x9C\x93 All results correct!" << std::endl;
        } else {
            std::cout << "[HOST] \xE2\x9C\x97 Found " << errors << " errors" << std::endl;
        }

        std::cout << "[HOST] Result checksum: " << checksum << std::endl;
    }

    void cleanup() {
        std::cout << "[HOST] Cleaning up memory" << std::endl;
        delete[] shared.A;
        delete[] shared.B;
        delete[] shared.C;
    }
};

// ============================================================================
// DEVICE CODE — full-DMA: recv A/B chunk, compute privately, send C chunk back
// ============================================================================

static void device_kernel(void* raw) {
    SharedData* shared = (SharedData*)raw;
    int n = shared->num_device_pes;
    pimid_parallel_pes_msg(n, [&](int pe_id) {
        int chunk = shared->size / n;
        int start = pe_id * chunk;
        int end   = (pe_id == n - 1) ? shared->size : (pe_id + 1) * chunk;
        int len   = end - start;
        unsigned bytes = (unsigned)(len * sizeof(float));

        // FULL-DMA (1): host->device — copy this PE's A and B chunks into
        // PRIVATE device buffers (each charged on the link).
        std::vector<float> a_priv(len), b_priv(len), c_priv(len);
        pimid_pe_recv(a_priv.data(), &shared->A[start], bytes);
        pimid_pe_recv(b_priv.data(), &shared->B[start], bytes);

        // (2) compute on private copies only.
        for (int i = 0; i < len; i++) {
            c_priv[i] = a_priv[i] + b_priv[i];
        }

        // FULL-DMA (3): device->host — ship the C chunk back (charged).
        pimid_pe_send(&shared->C[start], c_priv.data(), bytes);

        if (pe_id == 0) {
            std::cout << "[DEVICE PE-0] DMA in " << (2 * bytes) << " B (A+B chunk), computed "
                      << start << " to " << end << ", DMA out " << bytes << " B" << std::endl;
        }
    });
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <vector_size> <num_device_pes>" << std::endl;
        return 1;
    }

    int size = std::atoi(argv[1]);
    int num_pes = std::atoi(argv[2]);

    std::cout << "=== HOST/DEVICE CO-SIM (message-passing): Vector Addition ===" << std::endl;
    std::cout << "Vector size: " << size << std::endl;
    std::cout << "Device PEs: " << num_pes << std::endl;
    std::cout << std::endl;

    SharedData shared_data;
    shared_data.size = size;
    shared_data.num_device_pes = num_pes;

    HostCoordinator host(shared_data);
    host.prepareData();
    host.coordinateDeviceExecution();

    std::cout << "\n--- OFFLOADING TO DEVICE (PEs DMA chunks in/out) ---\n" << std::endl;

    pimid_offload_sync(device_kernel, &shared_data);

    std::cout << "\n--- RETURNING TO HOST ---\n" << std::endl;

    host.verifyAndAggregate();
    host.cleanup();

    std::cout << "\n=== Co-Simulation Complete ===" << std::endl;

    return 0;
}
