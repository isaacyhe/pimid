/* histogram_omp.c — Histogram construction: random scatter into bins
 * OpenMP parallel version. Private histograms per thread, merged at end.
 *
 * Device-organization aware (PIMID): the simulator prices each PE access by
 * LOCATION (own unit = fast, elsewhere = far). Near-data speed requires the
 * benchmark to EXPLICITLY relocate each PE's slice into that PE's own unit --
 * the simulator never moves data. Prep here (before the ROI, untimed):
 * PE p's slot holds [its input chunk][its private histogram]; the ROI scan is
 * then PE-local, and only the final merge into the shared host histogram
 * crosses. Coarse/host-shared placement runs the original path unchanged. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "zsim_hooks.h"
#include "../../include/pimid_devorg.h"

#define DEFAULT_SIZE 16384
#define DEFAULT_BINS 256

static uint32_t bench_rand(uint32_t* s) {
    *s = *s * 1103515245 + 12345;
    return (*s >> 16) & 0x7FFF;
}

static int parse_int_arg(int argc, char** argv, const char* flag, int def) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return def;
}

int main(int argc, char* argv[]) {
    int N = parse_int_arg(argc, argv, "--size", DEFAULT_SIZE);
    int nbins = parse_int_arg(argc, argv, "--bins", DEFAULT_BINS);
    uint32_t seed = 42;
    pimid_devorg_t dev = pimid_devorg_from_args(argc, argv);
    int P = dev.num_pes;

    int* data = (int*)malloc((size_t)N * sizeof(int));
    int* hist = (int*)calloc(nbins, sizeof(int));
    if (!data || !hist) { fprintf(stderr, "malloc failed\n"); return 1; }

    for (int i = 0; i < N; i++)
        data[i] = bench_rand(&seed) % nbins;

    int prep = pimid_devorg_needs_prep(&dev);
    int chunk = (N + P - 1) / P;
    size_t need = ((size_t)chunk + (size_t)nbins) * sizeof(int);

    size_t slot_bytes = 0;
    char* base = NULL;
    if (prep) {
        base = (char*)pimid_devorg_alloc(&dev, &slot_bytes);
        if (!base || need > slot_bytes) {
            fprintf(stderr, "devorg: per-PE need %zu > slot %zu (or alloc fail); "
                            "running host-layout (no prep)\n", need, slot_bytes);
            prep = 0;
        }
    }

    if (prep) {
        /* ---- DATA PREP (before ROI, not timed): PE p's slot = [chunk][hist] */
        omp_set_num_threads(P);
        for (int p = 0; p < P; p++) {
            int i0 = p * chunk, i1 = i0 + chunk; if (i1 > N) i1 = N;
            if (i0 >= N) continue;
            int* dpe = (int*)pimid_devorg_pe_slot(base, p, slot_bytes);
            memcpy(dpe, data + i0, (size_t)(i1 - i0) * sizeof(int));
            memset(dpe + chunk, 0, (size_t)nbins * sizeof(int));  /* local hist */
        }

        zsim_roi_begin();
        #pragma omp parallel for schedule(static, 1)
        for (int p = 0; p < P; p++) {
            int i0 = p * chunk, i1 = i0 + chunk; if (i1 > N) i1 = N;
            if (i0 >= N) continue;
            int* dpe = (int*)pimid_devorg_pe_slot(base, p, slot_bytes);
            int* local_hist = dpe + chunk;
            int n = i1 - i0;
            for (int i = 0; i < n; i++)
                local_hist[dpe[i]]++;
            #pragma omp critical
            {
                for (int b = 0; b < nbins; b++)
                    hist[b] += local_hist[b];
            }
        }
        zsim_roi_end();
    } else {
        zsim_roi_begin();
        #pragma omp parallel
        {
            int* local_hist = (int*)calloc(nbins, sizeof(int));

            #pragma omp for
            for (int i = 0; i < N; i++)
                local_hist[data[i]]++;

            #pragma omp critical
            {
                for (int b = 0; b < nbins; b++)
                    hist[b] += local_hist[b];
            }

            free(local_hist);
        }
        zsim_roi_end();
    }

    long checksum = 0;
    for (int i = 0; i < nbins; i++) checksum += hist[i];
    printf("BENCH_CHECKSUM: %ld\n", checksum);
    printf("BENCH_DONE\n");

    free(data); free(hist);
    return 0;
}
