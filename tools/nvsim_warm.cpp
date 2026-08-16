// 1.11.25: NVSim cache warmer.
//
// The pregenerated NVSim set is what every run actually reads -- characterization
// costs minutes, so a value absent from the cache does not exist for the corpus.
// When the cache schema gains a field (1.11.25 added the sub-bank ladder:
// subarray_latency_s, mat_latency_s), existing entries must be re-characterized
// or those tiers stay unsourceable forever.
//
// Reverse-engineering which simulation config produces a given cache entry is
// guesswork; this drives NVSimWrapper directly for an explicit (type, capacity,
// node, width) tuple, which is exactly what the cache key is.
//
// Usage: nvsim_warm <type> <capacity_bytes> <node_nm> <width_bits>
//        type: 0=STTRAM 1=PCRAM 2=RERAM
#include "memory/nvsim_wrapper.h"
#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
    if (argc != 5) {
        std::fprintf(stderr,
            "usage: %s <type 0|1|2> <capacity_bytes> <node_nm> <width_bits>\n", argv[0]);
        return 2;
    }
    const int type = std::atoi(argv[1]);
    pimid::NVSimWrapper::NVMConfig cfg;
    cfg.nvm_type = (type == 1) ? pimid::NVSimWrapper::NVMType::PCRAM
                 : (type == 2) ? pimid::NVSimWrapper::NVMType::RERAM
                               : pimid::NVSimWrapper::NVMType::STTRAM;
    cfg.capacity_bytes   = std::strtoull(argv[2], nullptr, 10);
    cfg.process_node_nm  = std::atoi(argv[3]);
    cfg.word_width_bits  = std::atoi(argv[4]);

    pimid::NVSimWrapper w(cfg);
    w.initialize();
    if (!w.isValid()) {
        std::fprintf(stderr, "[nvsim_warm] characterization FAILED for t%d c%s n%s w%s\n",
                     type, argv[2], argv[3], argv[4]);
        return 1;
    }
    std::printf("[nvsim_warm] t%d c%s n%s w%s: read=%.6e s  subarray=%.6e s  mat=%.6e s\n",
                type, argv[2], argv[3], argv[4],
                w.getReadLatency(), w.getSubarrayLatency(), w.getMatLatency());
    return 0;
}
