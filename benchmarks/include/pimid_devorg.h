/* pimid_devorg.h -- EXPLICIT device-organization awareness for PIM benchmarks.
 *
 * The PIMID simulator prices a device memory access purely by LOCATION: data in
 * the accessing PE's own placement-level unit is CLOSE (fast local path); data in
 * another unit is FAR (crosses the network). It does NOT move, cache, translate,
 * or auto-prep anything -- that is the BENCHMARK's job.
 *
 * So a benchmark that wants near-data speed must EXPLICITLY relocate each PE's
 * working set into that PE's own device unit. To do that it must know the device
 * organization -- placement level, PE count, unit count, and pages-per-unit --
 * which the harness passes on the command line. This header parses that org and
 * gives the address math the simulator uses:
 *
 *     unit(addr) = (page(addr) / pagesPerUnit) % totalUnits,   page = addr >> 12
 *     PE i owns the contiguous unit range [i*unitsPerPe, (i+1)*unitsPerPe)
 *
 * A buffer aligned to (totalUnits * pagesPerUnit * 4096) has its byte 0 at unit 0;
 * PE i's slot at offset i*unitsPerPe*pagesPerUnit*4096 therefore maps to unit
 * i*unitsPerPe = PE i's first owned unit -> served LOCAL.
 *
 * COARSE / host-shared placement (a PE already sees the whole memory in host
 * layout) needs no relocation: needs_prep() returns 0 and the kernel runs on the
 * host-laid-out data directly.
 *
 * The relocate is ordinary setup code -- run it BEFORE zsim_roi_begin(), just like
 * the data init. In device-only mode there is no host, but the benchmark must
 * still DO the relocate so the ROI's accesses land local; the relocate itself is
 * NOT part of the measured ROI (only the compute ROI is measured).
 */
#ifndef PIMID_DEVORG_H_
#define PIMID_DEVORG_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PIMID_PAGE_BYTES 4096u

/* placement levels, matching the simulator's pe_hierarchy_level */
enum {
    PIMID_LVL_SUBARRAY = 0,
    PIMID_LVL_BANK = 1,
    PIMID_LVL_BANK_GROUP = 2,
    PIMID_LVL_CHIP = 3,
    PIMID_LVL_RANK = 4,
    PIMID_LVL_CHANNEL = 5,
    PIMID_LVL_LOGIC_DIE = 6
};

typedef struct {
    int level;          /* PIMID_LVL_* */
    int num_pes;        /* PEs sharing the device */
    uint64_t total_units;   /* placement-level units across the whole device */
    uint64_t pages_per_unit;/* 4KB pages per unit */
    uint64_t units_per_pe;  /* total_units / num_pes (>=1) */
} pimid_devorg_t;

static inline int pimid__arg_int(int argc, char** argv, const char* flag, int def) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return def;
}

static inline int pimid__level_from_name(const char* s, int def) {
    if (!s) return def;
    if (!strcmp(s, "SUBARRAY"))   return PIMID_LVL_SUBARRAY;
    if (!strcmp(s, "BANK"))       return PIMID_LVL_BANK;
    if (!strcmp(s, "BANK_GROUP")) return PIMID_LVL_BANK_GROUP;
    if (!strcmp(s, "CHIP"))       return PIMID_LVL_CHIP;
    if (!strcmp(s, "RANK"))       return PIMID_LVL_RANK;
    if (!strcmp(s, "CHANNEL"))    return PIMID_LVL_CHANNEL;
    if (!strcmp(s, "LOGIC_DIE"))  return PIMID_LVL_LOGIC_DIE;
    return def;
}

/* Parse device org from argv. The harness passes:
 *   --placement <LEVEL> --pes <N> --total-units <U> --pages-per-unit <P>
 * Missing values fall back to a single host-shared unit (no prep). */
static inline pimid_devorg_t pimid_devorg_from_args(int argc, char** argv) {
    pimid_devorg_t d;
    const char* lvl = NULL;
    for (int i = 1; i < argc - 1; i++)
        if (!strcmp(argv[i], "--placement")) { lvl = argv[i + 1]; break; }
    d.level = pimid__level_from_name(lvl, PIMID_LVL_CHANNEL);
    d.num_pes = pimid__arg_int(argc, argv, "--pes", 1);
    if (d.num_pes < 1) d.num_pes = 1;
    d.total_units = (uint64_t)pimid__arg_int(argc, argv, "--total-units", d.num_pes);
    if (d.total_units < 1) d.total_units = d.num_pes;
    d.pages_per_unit = (uint64_t)pimid__arg_int(argc, argv, "--pages-per-unit", 32);
    if (d.pages_per_unit < 1) d.pages_per_unit = 1;
    d.units_per_pe = d.total_units / (uint64_t)d.num_pes;
    if (d.units_per_pe < 1) d.units_per_pe = 1;
    return d;
}

/* CHANNEL / LOGIC_DIE are the host-shared aggregation levels: a PE sees the whole
 * device in host layout, so no relocation is needed (the simulator already serves
 * these local). The finer, device-private levels (subarray..rank) need the
 * benchmark to relocate each PE's slice into its own unit. */
static inline int pimid_devorg_needs_prep(const pimid_devorg_t* d) {
    return (d->num_pes > 1)
        && (d->level < PIMID_LVL_CHANNEL)
        && (d->units_per_pe < d->total_units);
}

/* Bytes per PE slot: the address span of a PE's owned units. */
static inline size_t pimid_devorg_slot_bytes(const pimid_devorg_t* d) {
    return (size_t)d->units_per_pe * (size_t)d->pages_per_unit * PIMID_PAGE_BYTES;
}

/* Allocate a device buffer of num_pes PE-local slots whose byte 0 maps to unit 0,
 * so slot i maps to PE i's units. Over-allocates one org "period" and rounds the
 * usable base up to a period boundary (period = totalUnits*pagesPerUnit*4096); the
 * guest virtual address the simulator sees is that aligned base, so
 * unit(base) = (base/4096 / pagesPerUnit) % totalUnits == 0. Works for any
 * totalUnits (no power-of-two requirement). The raw allocation is intentionally
 * not returned/freed -- benchmark processes are short-lived. Returns NULL on OOM.
 * *slot_bytes_out gets the per-slot span. */
static inline void* pimid_devorg_alloc(const pimid_devorg_t* d, size_t* slot_bytes_out) {
    size_t slot = pimid_devorg_slot_bytes(d);
    size_t period = (size_t)d->total_units * (size_t)d->pages_per_unit * PIMID_PAGE_BYTES;
    size_t total = slot * (size_t)d->num_pes;
    char* raw = (char*)malloc(total + period + PIMID_PAGE_BYTES);
    if (slot_bytes_out) *slot_bytes_out = slot;
    if (!raw) return NULL;
    uintptr_t addr = (uintptr_t)raw;
    uintptr_t aligned = ((addr + period - 1) / period) * period;  /* -> page maps to unit 0 */
    return (void*)aligned;
}

/* Byte pointer to PE i's slot within a pimid_devorg_alloc buffer. */
static inline void* pimid_devorg_pe_slot(void* base, int pe, size_t slot_bytes) {
    return (void*)((char*)base + (size_t)pe * slot_bytes);
}

#endif /* PIMID_DEVORG_H_ */
