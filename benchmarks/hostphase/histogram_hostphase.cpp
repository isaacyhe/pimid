/**
 * @file histogram_hostphase.cpp
 * @brief HOST-PHASE term of the composed co-simulation (histogram merge).
 *
 * Host work for the histogram offload: input generation (N bytes), merge of
 * P per-PE 256-bin local histograms into the final histogram, result touch.
 * No device computation, no verification recompute (see reduction_hostphase).
 */
#include <iostream>
#include <cstdlib>
#include <cstring>
#include "zsim_hooks.h"

static const int NUM_BINS = 256;

int main(int argc, char* argv[]) {
    int n = (argc > 1) ? std::atoi(argv[1]) : 4096;
    int pes = (argc > 2) ? std::atoi(argv[2]) : 8;

    unsigned char* data = new unsigned char[n];
    int** local_hists = new int*[pes];
    for (int p = 0; p < pes; p++) {
        local_hists[p] = new int[NUM_BINS];
    }
    long* final_hist = new long[NUM_BINS];

    zsim_roi_begin();

    // host_pre: input generation
    for (int i = 0; i < n; i++) data[i] = (unsigned char)(rand() % 256);

    // (device computes per-PE local histograms; stand-ins here)
    for (int p = 0; p < pes; p++)
        memset(local_hists[p], 0, NUM_BINS * sizeof(int));

    // host_post: merge P local histograms into the final histogram
    memset(final_hist, 0, NUM_BINS * sizeof(long));
    for (int p = 0; p < pes; p++)
        for (int b = 0; b < NUM_BINS; b++)
            final_hist[b] += local_hists[p][b];

    long total = 0;
    for (int b = 0; b < NUM_BINS; b++) total += final_hist[b];

    zsim_roi_end();

    std::cout << "[HOSTPHASE] histogram n=" << n << " pes=" << pes
              << " total=" << total << std::endl;
    std::cout << "[HOSTPHASE] done" << std::endl;
    for (int p = 0; p < pes; p++) delete[] local_hists[p];
    delete[] local_hists;
    delete[] final_hist;
    delete[] data;
    return 0;
}
