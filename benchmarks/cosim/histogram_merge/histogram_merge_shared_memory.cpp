/**
 * @file histogram_merge_cosim.cpp
 * @brief TRUE Host/Device Co-Simulation: Histogram with Merging
 *
 * HOST (OOO core with cache):
 *   - Generates input data array
 *   - Distributes data chunks to device PEs
 *   - Merges local histograms from all PEs
 *   - Displays final histogram
 *
 * DEVICE (ALU cores without cache):
 *   - Each PE computes local histogram for its data chunk
 *   - Parallel histogram computation
 *
 * Communication: Shared memory for data and histograms
 */

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include "zsim_hooks.h"
#include "../cosim_pe_parallel.h"

const int NUM_BINS = 256;  // Histogram bins [0-255]

struct SharedHistogramData {
    unsigned char* data;      // Input data array
    int data_size;
    int num_device_pes;
    int** local_histograms;   // One histogram per PE
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

    // HOST: Generate input data
    void generateInputData() {
        std::cout << "[HOST] Generating input data (" << shared.data_size
                  << " bytes)" << std::endl;

        shared.data = new unsigned char[shared.data_size];

        // Generate random data
        for (int i = 0; i < shared.data_size; i++) {
            shared.data[i] = rand() % 256;
        }

        std::cout << "[HOST] Sample data: ";
        for (int i = 0; i < std::min(10, shared.data_size); i++) {
            std::cout << (int)shared.data[i] << " ";
        }
        std::cout << "..." << std::endl;
    }

    // HOST: Allocate local histogram arrays
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

    // HOST: Distribute work to device
    void coordinateDistribution() {
        int chunk_size = shared.data_size / shared.num_device_pes;
        std::cout << "[HOST] Each PE will process ~" << chunk_size
                  << " bytes" << std::endl;
    }

    // HOST: Merge all local histograms
    void mergeHistograms() {
        std::cout << "[HOST] Merging " << shared.num_device_pes
                  << " local histograms..." << std::endl;

        // HOST: Aggregation work
        for (int bin = 0; bin < NUM_BINS; bin++) {
            shared.final_histogram[bin] = 0;
            for (int pe = 0; pe < shared.num_device_pes; pe++) {
                shared.final_histogram[bin] += shared.local_histograms[pe][bin];
            }
        }

        std::cout << "[HOST] ✓ Histogram merge complete" << std::endl;
    }

    // HOST: Display histogram statistics
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

        // Show a few bins
        std::cout << "[HOST] Sample bins:" << std::endl;
        for (int i = 0; i < 10; i++) {
            int bin = i * 25;  // Sample every 25th bin
            std::cout << "[HOST]   Bin " << bin << ": "
                      << shared.final_histogram[bin] << std::endl;
        }
    }

    // HOST: Cleanup
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
// DEVICE CODE (runs on ALU cores without cache)
// ============================================================================

class DeviceHistogramComputer {
private:
    int pe_id;
    SharedHistogramData& shared;

public:
    DeviceHistogramComputer(int id, SharedHistogramData& data)
        : pe_id(id), shared(data) {}

    // DEVICE: Compute local histogram for assigned chunk
    void computeLocalHistogram() {
        int chunk_size = shared.data_size / shared.num_device_pes;
        int start = pe_id * chunk_size;
        int end = (pe_id == shared.num_device_pes - 1) ?
                  shared.data_size : (pe_id + 1) * chunk_size;

        // DEVICE: Compute-intensive histogram calculation
        for (int i = start; i < end; i++) {
            unsigned char value = shared.data[i];
            shared.local_histograms[pe_id][value]++;
        }

        if (pe_id == 0) {
            std::cout << "[DEVICE PE-" << pe_id << "] Processed bytes "
                      << start << " to " << end << std::endl;

            // Show sample of local histogram
            std::cout << "[DEVICE PE-" << pe_id << "] Sample local bins: ";
            for (int i = 0; i < 5; i++) {
                std::cout << shared.local_histograms[pe_id][i * 50] << " ";
            }
            std::cout << std::endl;
        }
    }
};

// ============================================================================
// DEVICE KERNEL (offloaded via pimid_offload_sync)
// ============================================================================

static void device_kernel(void* raw) {
    SharedHistogramData* shared = (SharedHistogramData*)raw;
    pimid_parallel_pes(shared->num_device_pes, [&](int i) {
        DeviceHistogramComputer pe(i, *shared);
        pe.computeLocalHistogram();
    });
}

// ============================================================================
// MAIN: Orchestrates Host/Device Co-Simulation
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <data_size> <num_device_pes>" << std::endl;
        return 1;
    }

    int data_size = std::atoi(argv[1]);
    int num_pes = std::atoi(argv[2]);

    std::cout << "=== HOST/DEVICE CO-SIMULATION: Histogram with Merging ===" << std::endl;
    std::cout << "Data size: " << data_size << " bytes" << std::endl;
    std::cout << "Histogram bins: " << NUM_BINS << std::endl;
    std::cout << "Device PEs: " << num_pes << std::endl;
    std::cout << std::endl;

    SharedHistogramData shared_data;
    shared_data.data_size = data_size;
    shared_data.num_device_pes = num_pes;

    // HOST: Prepare data and allocate histograms
    HostHistogramCoordinator host(shared_data);
    host.generateInputData();
    host.allocateHistograms();
    host.coordinateDistribution();

    std::cout << std::endl;
    std::cout << "--- OFFLOADING TO DEVICE ---" << std::endl;
    std::cout << std::endl;

    // DEVICE: Offload histogram computation to ALU cores via ZSim hooks
    pimid_offload_sync(device_kernel, &shared_data);

    std::cout << std::endl;
    std::cout << "--- RETURNING TO HOST ---" << std::endl;
    std::cout << std::endl;

    // HOST: Merge and analyze
    host.mergeHistograms();
    host.displayStatistics();
    host.cleanup();

    std::cout << std::endl;
    std::cout << "=== Co-Simulation Complete ===" << std::endl;

    return 0;
}
