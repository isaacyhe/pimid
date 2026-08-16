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

#endif  // ZSIM_TYPES_H_
