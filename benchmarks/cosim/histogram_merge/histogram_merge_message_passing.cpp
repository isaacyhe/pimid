/**
 * @file histogram_merge_message_passing.cpp
 * @brief Host/Device co-simulation: histogram with merging, MESSAGE-PASSING model.
 *
 * Serial host offloads per-chunk histogramming to N device PEs, then merges the
 * local histograms. Unlike the shared_memory variant (PEs read the data array
 * and bump shared local-histogram counters through host pointers), here each PE
 * runs FULL-DMA:
 *   (1) host->device  : DMA this PE's data chunk into a PRIVATE buffer via
 *                       pimid_pe_recv (charged on the host<->device link),
 *   (2) compute        : build a PRIVATE 256-bin histogram from the private chunk,
 *   (3) device->host  : DMA the 256-bin local histogram back via pimid_pe_send.
 * The PE never dereferences host data/histogram pointers in the compute loop.
 *
 * Result correctness (merged histogram) is identical to the shared_memory variant.
 */

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include "zsim_hooks.h"
#include "../cosim_pe_message.h"

const int NUM_BINS = 256;  // Histogram bins [0-255]

struct SharedHistogramData {
    unsigned char* data;      // Input data array
    int data_size;
    int num_device_pes;
    int** local_histograms;   // host-side landing buffers (filled via messages)
    int* final_histogram;     // Merged histogram (computed by host)
};

// ============================================================================
// HOST CODE (runs on OOO/Simple core with cache)
// ============================================================================

class HostHistogramCoordinator {
private:
    SharedHistogramData& shared;

public:
    HostHistogramCoordinator(SharedHistogramData& data) : shared(data) {}

    void generateInputData() {
        std::cout << "[HOST] Generating input data (" << shared.data_size
                  << " bytes)" << std::endl;

        shared.data = new unsigned char[shared.data_size];
        for (int i = 0; i < shared.data_size; i++) {
            shared.data[i] = rand() % 256;
        }

        std::cout << "[HOST] Sample data: ";
        for (int i = 0; i < std::min(10, shared.data_size); i++) {
            std::cout << (int)shared.data[i] << " ";
        }
        std::cout << "..." << std::endl;
    }

    void allocateHistograms() {
        std::cout << "[HOST] Allocating local histograms for "
                  << shared.num_device_pes << " PEs" << std::endl;

        shared.local_histograms = new int*[shared.num_device_pes];
        for (int pe = 0; pe < shared.num_device_pes; pe++) {
            shared.local_histograms[pe] = new int[NUM_BINS];
            memset(shared.local_histograms[pe], 0, NUM_BINS * sizeof(int));
        }

        shared.final_histogram = new int[NUM_BINS];
        memset(shared.final_histogram, 0, NUM_BINS * sizeof(int));
    }

    void coordinateDistribution() {
        int chunk_size = shared.data_size / shared.num_device_pes;
        std::cout << "[HOST] Each PE will process ~" << chunk_size
                  << " bytes" << std::endl;
    }

    void mergeHistograms() {
        std::cout << "[HOST] Merging " << shared.num_device_pes
                  << " local histograms..." << std::endl;

        for (int bin = 0; bin < NUM_BINS; bin++) {
            shared.final_histogram[bin] = 0;
            for (int pe = 0; pe < shared.num_device_pes; pe++) {
                shared.final_histogram[bin] += shared.local_histograms[pe][bin];
            }
        }

        std::cout << "[HOST] \xE2\x9C\x93 Histogram merge complete" << std::endl;
    }

    void displayStatistics() {
        std::cout << "[HOST] Computing histogram statistics..." << std::endl;

        int total_count = 0;
        int max_count = 0;
        int max_bin = 0;

        for (int bin = 0; bin < NUM_BINS; bin++) {
            total_count += shared.final_histogram[bin];
            if (shared.final_histogram[bin] > max_count) {
                max_count = shared.final_histogram[bin];
                max_bin = bin;
            }
        }

        std::cout << "[HOST] Total elements: " << total_count << std::endl;
        std::cout << "[HOST] Most frequent value: " << max_bin
                  << " (count: " << max_count << ")" << std::endl;

        std::cout << "[HOST] Sample bins:" << std::endl;
        for (int i = 0; i < 10; i++) {
            int bin = i * 25;
            std::cout << "[HOST]   Bin " << bin << ": "
                      << shared.final_histogram[bin] << std::endl;
        }
    }

    // HOST: verify the merged histogram equals a sequential reference.
    void verifyResult() {
        std::cout << "[HOST] Verifying histogram..." << std::endl;
        std::vector<long> ref(NUM_BINS, 0);
        for (int i = 0; i < shared.data_size; i++) {
            ref[shared.data[i]]++;
        }
        long total = 0;
        bool ok = true;
        for (int bin = 0; bin < NUM_BINS; bin++) {
            if ((long)shared.final_histogram[bin] != ref[bin]) ok = false;
            total += ref[bin];
        }
        if (ok && total == (long)shared.data_size) {
            std::cout << "[HOST] \xE2\x9C\x93 Verification passed! ("
                      << total << " elements counted)" << std::endl;
        } else {
            std::cout << "[HOST] \xE2\x9C\x97 Verification failed!" << std::endl;
        }
    }

    void cleanup() {
        std::cout << "[HOST] Cleaning up memory" << std::endl;

        delete[] shared.data;
        delete[] shared.final_histogram;

        for (int pe = 0; pe < shared.num_device_pes; pe++) {
            delete[] shared.local_histograms[pe];
        }
        delete[] shared.local_histograms;
    }
};

// ============================================================================
// DEVICE CODE — full-DMA: recv data chunk, build private histogram, send it back
// ============================================================================

static void device_kernel(void* raw) {
    SharedHistogramData* shared = (SharedHistogramData*)raw;
    int n = shared->num_device_pes;
    pimid_parallel_pes_msg(n, [&](int pe_id) {
        int chunk = shared->data_size / n;
        int start = pe_id * chunk;
        int end   = (pe_id == n - 1) ? shared->data_size : (pe_id + 1) * chunk;
        int len   = end - start;
        unsigned in_bytes  = (unsigned)(len * sizeof(unsigned char));
        unsigned out_bytes = (unsigned)(NUM_BINS * sizeof(int));

        // FULL-DMA (1): host->device — copy this PE's data chunk into a PRIVATE
        // device buffer (charged on the link).
        std::vector<unsigned char> priv(len);
        pimid_pe_recv(priv.data(), &shared->data[start], in_bytes);

        // (2) build a PRIVATE local histogram from the private copy only.
        std::vector<int> local_hist(NUM_BINS, 0);
        for (int i = 0; i < len; i++) {
            local_hist[priv[i]]++;
        }

        // FULL-DMA (3): device->host — ship the 256-bin local histogram back.
        pimid_pe_send(shared->local_histograms[pe_id], local_hist.data(), out_bytes);

        if (pe_id == 0) {
            std::cout << "[DEVICE PE-0] DMA in " << in_bytes << " B (bytes "
                      << start << " to " << end << "), DMA out " << out_bytes
                      << " B (256-bin histogram)" << std::endl;
        }
    });
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <data_size> <num_device_pes>" << std::endl;
        return 1;
    }

    int data_size = std::atoi(argv[1]);
    int num_pes = std::atoi(argv[2]);

    std::cout << "=== HOST/DEVICE CO-SIM (message-passing): Histogram with Merging ===" << std::endl;
    std::cout << "Data size: " << data_size << " bytes" << std::endl;
    std::cout << "Histogram bins: " << NUM_BINS << std::endl;
    std::cout << "Device PEs: " << num_pes << std::endl;
    std::cout << std::endl;

    SharedHistogramData shared_data;
    shared_data.data_size = data_size;
    shared_data.num_device_pes = num_pes;

    HostHistogramCoordinator host(shared_data);
    host.generateInputData();
    host.allocateHistograms();
    host.coordinateDistribution();

    std::cout << "\n--- OFFLOADING TO DEVICE (PEs DMA chunk in, histogram out) ---\n" << std::endl;

    pimid_offload_sync(device_kernel, &shared_data);

    std::cout << "\n--- RETURNING TO HOST ---\n" << std::endl;

    host.mergeHistograms();
    host.displayStatistics();
    host.verifyResult();
    host.cleanup();

    std::cout << "\n=== Co-Simulation Complete ===" << std::endl;

    return 0;
}
