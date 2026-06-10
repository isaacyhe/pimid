/**
 * @file reduction_tree_message_passing.cpp
 * @brief Host/Device co-simulation: hierarchical reduction, MESSAGE-PASSING model.
 *
 * Serial host offloads a parallel reduction to N device PEs. Unlike the
 * shared_memory variant (PEs write partials through a shared host pointer),
 * here each PE reduces its chunk into a PRIVATE local and then EXPLICITLY copies
 * the result back to the host via pimid_pe_send(), which charges the
 * host<->device link for the sized transfer. Models a non-coherent device.
 *
 * The input array is still read from host memory (an input DMA would be modeled
 * the same way; kept as a shared read here to isolate the result-transfer cost).
 */
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cmath>
#include "zsim_hooks.h"
#include "../cosim_pe_message.h"

enum ReductionOp { SUM, MAX, MIN, PRODUCT };

struct SharedReductionData {
    double* data;
    int data_size;
    int num_device_pes;
    double* partial_results;   // host-side landing buffer (filled via messages)
    double final_result;
    ReductionOp operation;
};

// ============================================================================
// HOST
// ============================================================================
class HostReductionCoordinator {
    SharedReductionData& shared;
public:
    HostReductionCoordinator(SharedReductionData& d) : shared(d) {}

    void generateInputData() {
        std::cout << "[HOST] Generating input array (" << shared.data_size
                  << " elements)" << std::endl;
        shared.data = new double[shared.data_size];
        for (int i = 0; i < shared.data_size; i++)
            shared.data[i] = (rand() % 100) / 10.0;
    }
    void allocatePartialResults() {
        std::cout << "[HOST] Allocating partial results for "
                  << shared.num_device_pes << " PEs" << std::endl;
        shared.partial_results = new double[shared.num_device_pes];
    }
    void performFinalReduction() {
        std::cout << "[HOST] Performing final reduction..." << std::endl;
        switch (shared.operation) {
            case SUM:
                shared.final_result = 0.0;
                for (int i = 0; i < shared.num_device_pes; i++)
                    shared.final_result += shared.partial_results[i];
                break;
            case MAX:
                shared.final_result = shared.partial_results[0];
                for (int i = 1; i < shared.num_device_pes; i++)
                    if (shared.partial_results[i] > shared.final_result)
                        shared.final_result = shared.partial_results[i];
                break;
            case MIN:
                shared.final_result = shared.partial_results[0];
                for (int i = 1; i < shared.num_device_pes; i++)
                    if (shared.partial_results[i] < shared.final_result)
                        shared.final_result = shared.partial_results[i];
                break;
            case PRODUCT:
                shared.final_result = 1.0;
                for (int i = 0; i < shared.num_device_pes; i++)
                    shared.final_result *= shared.partial_results[i];
                break;
        }
        std::cout << "[HOST] \xE2\x9C\x93 Final reduction complete" << std::endl;
    }
    void verifyCorrectness() {
        std::cout << "[HOST] Verifying result..." << std::endl;
        double expected = 0.0;
        switch (shared.operation) {
            case SUM:
                for (int i = 0; i < shared.data_size; i++) expected += shared.data[i];
                break;
            case MAX:
                expected = shared.data[0];
                for (int i = 1; i < shared.data_size; i++)
                    if (shared.data[i] > expected) expected = shared.data[i];
                break;
            case MIN:
                expected = shared.data[0];
                for (int i = 1; i < shared.data_size; i++)
                    if (shared.data[i] < expected) expected = shared.data[i];
                break;
            case PRODUCT:
                expected = 1.0;
                for (int i = 0; i < shared.data_size; i++) expected *= shared.data[i];
                break;
        }
        double error = std::abs(shared.final_result - expected);
        if (error < 0.001) {
            std::cout << "[HOST] \xE2\x9C\x93 Result verified!" << std::endl;
            std::cout << "[HOST]   Expected: " << expected << std::endl;
            std::cout << "[HOST]   Got: " << shared.final_result << std::endl;
        } else {
            std::cout << "[HOST] \xE2\x9C\x97 Verification failed!" << std::endl;
            std::cout << "[HOST]   Expected: " << expected
                      << "  Got: " << shared.final_result << std::endl;
        }
    }
    void cleanup() {
        std::cout << "[HOST] Cleaning up memory" << std::endl;
        delete[] shared.data;
        delete[] shared.partial_results;
    }
};

// ============================================================================
// DEVICE — compute into a PRIVATE local, then message the result to the host
// ============================================================================
static void device_kernel(void* raw) {
    SharedReductionData* shared = (SharedReductionData*)raw;
    int n = shared->num_device_pes;
    pimid_parallel_pes_msg(n, [&](int pe_id) {
        int chunk = shared->data_size / n;
        int start = pe_id * chunk;
        int end   = (pe_id == n - 1) ? shared->data_size : (pe_id + 1) * chunk;
        int len   = end - start;

        // FULL-DMA MESSAGE PASSING (1): host->device — copy this PE's input
        // chunk into a PRIVATE device buffer (charged on the link), so the PE
        // computes on its own copy, not host pointers.
        std::vector<double> priv(len);
        pimid_pe_recv(priv.data(), &shared->data[start],
                      (unsigned)(len * sizeof(double)));

        double local;
        switch (shared->operation) {
            case SUM:     local = 0.0; break;
            case MAX:
            case MIN:     local = priv[0]; break;
            default:      local = 1.0; break;
        }
        for (int i = 0; i < len; i++) {
            switch (shared->operation) {
                case SUM:     local += priv[i]; break;
                case MAX:     if (priv[i] > local) local = priv[i]; break;
                case MIN:     if (priv[i] < local) local = priv[i]; break;
                case PRODUCT: local *= priv[i]; break;
            }
        }
        // FULL-DMA MESSAGE PASSING (2): device->host — ship the result back.
        pimid_pe_send(&shared->partial_results[pe_id], &local, sizeof(double));
        if (pe_id == 0)
            std::cout << "[DEVICE PE-0] DMA in " << (len*sizeof(double)) << " B, reduced "
                      << start << " to " << end << " \xE2\x86\x92 " << local
                      << ", DMA out " << sizeof(double) << " B" << std::endl;
    });
}

// ============================================================================
// MAIN
// ============================================================================
int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <array_size> <num_device_pes> <operation>" << std::endl;
        std::cerr << "  operation: 0=SUM, 1=MAX, 2=MIN, 3=PRODUCT" << std::endl;
        return 1;
    }
    int data_size = std::atoi(argv[1]);
    int num_pes   = std::atoi(argv[2]);
    int op        = std::atoi(argv[3]);
    if (op < 0 || op > 3) op = 0;

    std::cout << "=== HOST/DEVICE CO-SIM (message-passing): Hierarchical Reduction ==="
              << std::endl;
    std::cout << "Array size: " << data_size << "  Device PEs: " << num_pes
              << "  Operation: " << (op==0?"SUM":op==1?"MAX":op==2?"MIN":"PRODUCT")
              << std::endl << std::endl;

    SharedReductionData shared;
    shared.data_size = data_size;
    shared.num_device_pes = num_pes;
    shared.operation = static_cast<ReductionOp>(op);

    HostReductionCoordinator host(shared);
    host.generateInputData();
    host.allocatePartialResults();

    std::cout << "\n--- OFFLOADING TO DEVICE (PEs message results back) ---\n" << std::endl;
    pimid_offload_sync(device_kernel, &shared);

    std::cout << "\n--- RETURNING TO HOST (Final Reduction) ---\n" << std::endl;
    host.performFinalReduction();
    host.verifyCorrectness();
    host.cleanup();

    std::cout << "\n=== Co-Simulation Complete ===" << std::endl;
    return 0;
}
