/** ZSim type definitions for use without Intel Pin.
 *
 * When building with ZSIM_USE_QEMU, this header provides the type aliases
 * that ZSim's simulation core (cores, caches, scheduler) expects from Pin.
 * These are simple integer typedefs — no Pin functionality is needed.
 */

#ifndef ZSIM_TYPES_H_
#define ZSIM_TYPES_H_

#include <stdint.h>

// Core Pin types used throughout ZSim simulation code
typedef uint32_t THREADID;
typedef uint64_t ADDRINT;
typedef uint32_t BOOL;

// Address type used by ZSim's memory hierarchy
typedef uint64_t Address;

// REG_LAST is used by decoder.h for temporary register numbering.
// The actual value doesn't matter for SimpleCore (no OOO decoding).
// We set it high enough to avoid collisions with temp register offsets.
#define REG_LAST 512

// Boolean constants
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* 1.11.8 (#84): active-phase tracker for power-gating residency. Touched on
 * an object's own event path (one compare+branch); CAS keeps shared trackers
 * race-safe (a lost race undercounts one phase, negligible). */
struct PhaseActivity {
    volatile uint64_t activePhases = 0;
    volatile uint64_t lastActivePhase = ~0ull;
    /* 1.11.36 (user ruling E14): MONOTONIC. lastActivePhase is the "have I
     * already counted this phase?" marker, and this used to CAS it to whatever
     * phase the calling thread held -- so a thread carrying a stale number
     * could walk it BACKWARDS:
     *   phase 5: A touches -> marker=5, count=1
     *            B (still holding 4) touches -> marker=4     rewound
     *            A touches again -> 4 != 5 -> count=2        counted TWICE
     * An inflated activePhases makes the component look busier than it was, so
     * its idle residency comes out too low and it is credited LESS gating
     * saving than it earned. The old comment claimed "a lost race undercounts
     * one phase, negligible" -- the real failure was over-counting, by an
     * amount that depends on thread interleaving.
     *
     * SCOPE, checked before changing it: cores hold their own tracker
     * (core.h pgAct) and cannot race; only the five SHARED trackers are
     * exposed -- anyCore, sharedCache, noc, hostMC, devMC[] -- which are
     * exactly the ones the reported residencies come from. numPhases advances
     * inside the scheduler's BARRIER callback, so threads agree on the phase
     * for essentially all of it; the stale window is a few instructions.
     *
     * TRADE-OFF: refusing to rewind DROPS a lagging thread's touch of an older
     * phase. The barrier has already left that phase, so another thread has
     * almost certainly counted it; where it has not, the result is a bounded
     * UNDER-count, which under-credits gating rather than over-crediting it.
     *
     * NOT BOUGHT: reproducibility. Which thread wins the CAS still depends on
     * interleaving, so the count is bounded and never inflated, not identical
     * run to run. Per-thread trackers would give that, at the cost of deciding
     * what "the phase was active" means when threads disagree. */
    inline void touch(uint64_t ph) {
        for (;;) {
            uint64_t last = lastActivePhase;
            if (last == ph) return;                     // already counted
            if (last != ~0ull && ph < last) return;     // never rewind
            if (__sync_bool_compare_and_swap(&lastActivePhase, last, ph)) {
                __sync_fetch_and_add(&activePhases, 1);
                return;
            }
            // lost the CAS: another thread moved it; re-read and retry
        }
    }
};

/* 1.11.40 (audit E17, measurement pass): INTER-EVENT GAP HISTOGRAM.
 *
 * PhaseActivity above answers "was this component busy in this phase?" at
 * 10000-cycle granularity -- 20 us at the 500 MHz device clock, roughly 2000x
 * coarser than the events power gating actually exploits (DRAM power-down exit
 * tXP ~10 ns; logic clock gating is cycle-scale). On our corpus that makes
 * every component read 100% busy and the gating model report nothing.
 *
 * Before redesigning the residency model we measure what is there. This
 * records, per component, the distribution of gaps between consecutive events
 * in log2-cycle buckets: how MANY gaps of each size, and how many CYCLES sit
 * in them. The second is the one that matters -- a gating scheme can only
 * recover time, and only from gaps longer than its wake penalty.
 *
 * This is instrumentation ONLY. Nothing reads it back into the model; it is
 * dumped at end of run and the numbers decide what the model should be.
 *
 * Concurrency follows the E14 lesson: lastCycle never rewinds, and a thread
 * that loses the CAS drops its sample rather than retrying. Dropping samples
 * biases the histogram toward FEWER recorded gaps, never toward larger ones,
 * so it cannot manufacture gating opportunity that is not there. */
struct GapHist {
    static const int kBuckets = 40;          // log2(cycles), 0 .. 2^39
    volatile uint64_t lastCycle = 0;
    volatile uint64_t events    = 0;         // every call, including dropped
    volatile uint64_t samples   = 0;         // calls that produced a gap
    volatile uint64_t armed     = 0;         // 0 until the ROI opens
    volatile uint64_t firstCycle = 0;        // first event cycle inside the ROI
    volatile uint64_t count[kBuckets]  = {0};
    volatile uint64_t cycles[kBuckets] = {0};

    /* ROI GATING, and why it is not optional. The first cut of this histogram
     * recorded across the WHOLE run and divided by globPhaseCycles. On the
     * detailed cell that span is 7086 phases, while the power model prices the
     * ROI window of 1016 -- so 86% of what was measured was setup and teardown,
     * where the device is idle BY CONSTRUCTION. The resulting "90.4% of the run
     * sits in gaps above tXP" was therefore dominated by idle no power model
     * ever prices, and was not usable for choosing a gating threshold.
     * Recording is armed at roi_begin and the denominator is the span the
     * samples actually cover. */
    inline void arm(uint64_t c) {
        lastCycle  = 0;      // discard any pre-ROI history
        firstCycle = c;
        armed      = 1;
    }

    /* The span the samples cover: last event minus first. This is the only
     * defensible denominator -- the recorded gaps telescope to exactly this, so
     * a percentage against it cannot exceed 100%. The globPhaseCycles version
     * produced 100.01-100.05%, which was the tell that it was the wrong
     * quantity rather than a rounding artefact. */
    inline uint64_t spanCycles() const {
        return (lastCycle > firstCycle) ? (lastCycle - firstCycle) : 0;
    }

    inline void event(uint64_t c) {
        if (!armed) return;                     // pre-ROI: not priced, not counted
        __sync_fetch_and_add(&events, 1);
        uint64_t last = lastCycle;
        if (c <= last) return;                  // out of order or repeat: no gap
        if (!__sync_bool_compare_and_swap(&lastCycle, last, c)) return;  // lost: drop
        if (last == 0) return;                  // first event only seeds the clock
        uint64_t gap = c - last;
        int b = 63 - __builtin_clzll(gap);      // gap >= 1 here, so clzll is defined
        if (b >= kBuckets) b = kBuckets - 1;
        __sync_fetch_and_add(&samples, 1);
        __sync_fetch_and_add(&count[b], 1);
        __sync_fetch_and_add(&cycles[b], gap);
    }

    /* Cycles sitting in gaps STRICTLY LONGER than a threshold, counted the way
     * a gating scheme would recover them: a gap of length g yields g - thresh
     * usable cycles, because the wake penalty is paid out of the gap itself.
     * Bucket b holds gaps in [2^b, 2^(b+1)), so a bucket straddling the
     * threshold is reported as its whole contents -- an OVER-estimate confined
     * to one bucket, and stated here rather than silently rounded away. */
    inline uint64_t cyclesInGapsOver(uint64_t thresh) const {
        uint64_t tot = 0;
        for (int b = 0; b < kBuckets; b++) {
            if ((1ull << b) <= thresh) continue;          // whole bucket below
            uint64_t c = cycles[b], n = count[b];
            tot += (c > n * thresh) ? (c - n * thresh) : 0;
        }
        return tot;
    }
};

#endif  // ZSIM_TYPES_H_
