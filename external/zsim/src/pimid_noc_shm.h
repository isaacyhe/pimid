/* pimid_noc_shm.h -- shared NoC record log for detailed-MPI: ONE logical Garnet
 * driven by ALL ranks.
 *
 * MODEL (synchronous, deadlock-free by construction -- no barriers, no async
 * coordinator, no EWMA side-channel):
 *   - Every rank PUBLISHES its NoC accesses {src, dst, cycle} to its own
 *     single-producer ring in this segment, and advances its cycle WATERMARK.
 *   - At its own phase drain, every rank REPLAYS the identical merged stream
 *     (all ranks' rings, up to the min-watermark consistent cut) through its
 *     local Garnet replica. processBatch resets Garnet per drain, so a drain
 *     is a pure function of the record window: N replicas of one logical
 *     network, all seeing ALL ranks' packets. Cross-rank contention is real.
 *   - A rank blocked in guest MPI (mpi_comm_window) marks itself QUIESCENT --
 *     a blocked guest injects nothing, so it is exempt from the cut and nobody
 *     ever waits for it. Exited ranks likewise. NO cross-rank blocking, ever.
 *   - The MPI transport also injects message payloads as records (sender ->
 *     receiver, ceil(bytes/64) packets), so rank-to-rank messages contend on
 *     the same tree as memory traffic.
 *
 * LIFECYCLE (the shm-lifecycle lesson): the LAUNCHER creates + sizes + zero-
 * fills the segment BEFORE forking rank children and shm_unlink()s it only
 * after every child exited. Ranks only ever attach. Env: PIMID_NOC_SHM.
 *
 * Layout is fixed-size C structs only (no pointers in shm). Single-producer
 * rings: the owning rank appends; ALL ranks read (each reader keeps private
 * cursors). Producers NEVER block; a lagging reader detects overwrite via the
 * monotonic seq and resyncs, counting the loss loudly.
 */
#ifndef PIMID_NOC_SHM_H_
#define PIMID_NOC_SHM_H_

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define PIMID_NOC_SHM_MAGIC   0x504E6F43u  /* 'PNoC' */
#define PIMID_NOC_RING_SLOTS  (1u << 17)   /* 128k recs x 16B = 2MB per rank */
#define PIMID_NOC_MAX_MSG_RECS 4096u       /* cap per MPI message injection */

/* One NoC access: 16 bytes, cycle-monotonic per ring (1 guest thread/rank). */
struct PimidNocRec {
    uint32_t src, dst;   /* Garnet node ids (global; same topology in all ranks) */
    uint64_t cycle;      /* ZSim cycle at issue */
};

enum PimidNocRankState : uint32_t {
    PIMID_NOC_ACTIVE    = 0,
    PIMID_NOC_QUIESCENT = 1,   /* guest blocked in MPI (comm window open) */
    PIMID_NOC_EXITED    = 2
};

/* Per-rank control slot (one cache line). */
struct PimidNocRank {
    volatile uint64_t seq;        /* records published (monotonic; release) */
    volatile uint64_t watermark;  /* newest published sim cycle */
    volatile uint32_t state;      /* PimidNocRankState */
    volatile uint32_t dropped;    /* producer-side overwrites detected by readers */
    uint8_t pad[40];
};

struct PimidNocHdr {
    uint32_t magic;
    uint32_t nranks;
    uint32_t ringSlots;     /* power of two */
    uint32_t numNodes;      /* Garnet node count (PE+abstract endpoints) */
    uint32_t nodesPerRank;  /* rank r sources from node r*nodesPerRank */
    uint32_t pad[11];
    /* PimidNocRank ranks[nranks]; then PimidNocRec rings[nranks][ringSlots] */
};

static inline size_t pimid_noc_shm_bytes(uint32_t nranks, uint32_t ringSlots) {
    return sizeof(PimidNocHdr)
         + (size_t)nranks * sizeof(PimidNocRank)
         + (size_t)nranks * ringSlots * sizeof(PimidNocRec);
}
static inline PimidNocRank* pimid_noc_rank(PimidNocHdr* h, uint32_t r) {
    return (PimidNocRank*)((char*)h + sizeof(PimidNocHdr)) + r;
}
static inline PimidNocRec* pimid_noc_ring(PimidNocHdr* h, uint32_t r) {
    return (PimidNocRec*)((char*)h + sizeof(PimidNocHdr)
                          + (size_t)h->nranks * sizeof(PimidNocRank))
           + (size_t)r * h->ringSlots;
}

/* LAUNCHER: create + size + zero-fill BEFORE forking ranks. Returns 0 on ok.
 * (Zero-fill is implicit: ftruncate on a fresh POSIX shm yields zero pages.) */
static inline int pimid_noc_shm_create(const char* name, uint32_t nranks,
                                       uint32_t numNodes) {
    shm_unlink(name);  /* stale segment from a crashed launch */
    int fd = shm_open(name, O_CREAT | O_RDWR | O_EXCL, 0600);
    if (fd < 0) {
        fprintf(stderr, "[pimid_noc] shm_open(%s) failed: %s\n", name, strerror(errno));
        return -1;
    }
    size_t bytes = pimid_noc_shm_bytes(nranks, PIMID_NOC_RING_SLOTS);
    if (ftruncate(fd, (off_t)bytes) < 0) {
        fprintf(stderr, "[pimid_noc] ftruncate(%s, %zu) failed: %s\n",
                name, bytes, strerror(errno));
        close(fd); shm_unlink(name);
        return -1;
    }
    void* p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) {
        fprintf(stderr, "[pimid_noc] mmap(%s) failed: %s\n", name, strerror(errno));
        shm_unlink(name);
        return -1;
    }
    PimidNocHdr* h = (PimidNocHdr*)p;
    h->nranks = nranks;
    h->ringSlots = PIMID_NOC_RING_SLOTS;
    h->numNodes = numNodes;
    h->nodesPerRank = (nranks > 0 && numNodes >= nranks) ? numNodes / nranks : 1;
    __atomic_store_n(&h->magic, PIMID_NOC_SHM_MAGIC, __ATOMIC_RELEASE);
    munmap(p, bytes);
    return 0;
}

/* RANK / TRANSPORT: attach an existing segment. Returns mapped header or NULL. */
static inline PimidNocHdr* pimid_noc_shm_attach(const char* name) {
    int fd = shm_open(name, O_RDWR, 0600);
    if (fd < 0) return NULL;
    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size < (off_t)sizeof(PimidNocHdr)) {
        close(fd);
        return NULL;
    }
    void* p = mmap(NULL, (size_t)st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) return NULL;
    PimidNocHdr* h = (PimidNocHdr*)p;
    if (__atomic_load_n(&h->magic, __ATOMIC_ACQUIRE) != PIMID_NOC_SHM_MAGIC) {
        munmap(p, (size_t)st.st_size);
        return NULL;
    }
    return h;
}

/* PRODUCER (only rank r's own process): append one record + bump watermark.
 * Never blocks; laggards lose oldest entries (readers detect via seq). */
static inline void pimid_noc_publish(PimidNocHdr* h, uint32_t r,
                                     uint32_t src, uint32_t dst, uint64_t cycle) {
    PimidNocRank* rk = pimid_noc_rank(h, r);
    PimidNocRec* ring = pimid_noc_ring(h, r);
    uint64_t s = rk->seq;  /* single producer: plain read of own seq */
    PimidNocRec* rec = &ring[s & (h->ringSlots - 1)];
    rec->src = src; rec->dst = dst; rec->cycle = cycle;
    __atomic_store_n(&rk->seq, s + 1, __ATOMIC_RELEASE);
    if (cycle > rk->watermark)
        __atomic_store_n(&rk->watermark, cycle, __ATOMIC_RELEASE);
}

/* Advance the watermark without publishing (any access, local or remote). */
static inline void pimid_noc_touch(PimidNocHdr* h, uint32_t r, uint64_t cycle) {
    PimidNocRank* rk = pimid_noc_rank(h, r);
    if (cycle > rk->watermark)
        __atomic_store_n(&rk->watermark, cycle, __ATOMIC_RELEASE);
}

static inline void pimid_noc_set_state(PimidNocHdr* h, uint32_t r, uint32_t st) {
    __atomic_store_n(&pimid_noc_rank(h, r)->state, st, __ATOMIC_RELEASE);
}

/* Consistent cut: min watermark over ACTIVE, PUBLISHING ranks. Exempt:
 *  - quiescent (guest blocked in MPI -- injects nothing),
 *  - exited,
 *  - watermark==0 (not yet past its ROI baseline -- has published nothing).
 * Nobody ever waits for an exempt rank. If no rank qualifies, returns ~0
 * (drain everything available). */
static inline uint64_t pimid_noc_cut(PimidNocHdr* h) {
    uint64_t cut = ~0ull;
    for (uint32_t r = 0; r < h->nranks; r++) {
        PimidNocRank* rk = pimid_noc_rank(h, r);
        if (__atomic_load_n(&rk->state, __ATOMIC_ACQUIRE) != PIMID_NOC_ACTIVE) continue;
        uint64_t w = __atomic_load_n(&rk->watermark, __ATOMIC_ACQUIRE);
        if (w == 0) continue;   /* pre-baseline: nothing published yet */
        if (w < cut) cut = w;
    }
    return cut;
}

#endif /* PIMID_NOC_SHM_H_ */
