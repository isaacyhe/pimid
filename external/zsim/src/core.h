/** $lic$
 * Copyright (C) 2012-2015 by Massachusetts Institute of Technology
 * Copyright (C) 2010-2013 by The Board of Trustees of Stanford University
 *
 * This file is part of zsim.
 *
 * zsim is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * If you use this software in your research, we request that you reference
 * the zsim paper ("ZSim: Fast and Accurate Microarchitectural Simulation of
 * Thousand-Core Systems", Sanchez and Kozyrakis, ISCA-40, June 2013) as the
 * source of the simulator in any publications that use this software, and that
 * you send us a citation of your work.
 *
 * zsim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CORE_H_
#define CORE_H_

#include <stdint.h>
#include "zsim_types.h"
#include "decoder_simple.h"
#include "g_std/g_string.h"
#include "stats.h"

struct BblInfo {
    uint32_t instrs;
    uint32_t bytes;
    DynBbl oooBbl[0]; //0 bytes, but will be 1-sized when we have an element (and that element has variable size as well)
};

/* Analysis function pointer struct
 * As an artifact of having a shared code cache, we need these to be the same for different core types.
 */
struct InstrFuncPtrs {  // NOLINT(whitespace)
    void (*loadPtr)(THREADID, ADDRINT);
    void (*storePtr)(THREADID, ADDRINT);
    void (*bblPtr)(THREADID, ADDRINT, BblInfo*);
    void (*branchPtr)(THREADID, ADDRINT, BOOL, ADDRINT, ADDRINT);
    // Same as load/store functions, but last arg indicated whether op is executing
    void (*predLoadPtr)(THREADID, ADDRINT, BOOL);
    void (*predStorePtr)(THREADID, ADDRINT, BOOL);
    uint64_t type;
    uint64_t pad[1];
    //NOTE: By having the struct be a power of 2 bytes, indirect calls are simpler (w/ gcc 4.4 -O3, 6->5 instructions, and those instructions are simpler)
};


//TODO: Switch type to an enum by using sizeof macros...
#define FPTR_ANALYSIS (0L)
#define FPTR_JOIN (1L)
#define FPTR_NOP (2L)
#define FPTR_RETRY (3L)

// Forward declarations for type checking without RTTI
class InOrderCore;
class OOOCore;
class EventRecorder;

//Generic core class

class Core : public GlobAlloc {
    private:
        uint64_t lastUpdateCycles;
        uint64_t lastUpdateInstrs;

    protected:
        g_string name;

    public:
        explicit Core(g_string& _name) : lastUpdateCycles(0), lastUpdateInstrs(0), name(_name) {}

        virtual uint64_t getInstrs() const = 0; // typically used to find out termination conditions or dumps
        virtual uint64_t getPhaseCycles() const = 0; // used by RDTSC faking --- we need to know how far along we are in the phase, but not the total number of phases
        virtual uint64_t getCycles() const = 0;

        /* 1.6 thread-MPI frozen-clock waits: rewind the cycle counter by the
         * raw growth a transport wait injected (park-rejoin fast-forward is
         * wall-dependent; the rendezvous ADVANCE re-places the clock from
         * deterministic arithmetic right after). No-op for cores with weave
         * event state (rewinding under scheduled events is unsafe) -- those
         * get frozen-clock support in 1.6.1. */
        virtual void pimidRewindCycles(uint64_t delta) { (void)delta; }

        /* 1.6.1 thread-MPI: set while the guest is parked in a transport wait
         * (COMM window). OOOCore::join consults it: a parked core's pipeline
         * is empty, so the post-wake join JUMPS to the phase clock instead of
         * simulating the pipeline across a wall-dependent gap (advance() there
         * converted wake luck into unhalted cycles -- the +-1-phase ooo
         * nondeterminism signature). */
        volatile bool pimidCommParked = false;

        virtual void initStats(AggregateStat* parentStat) = 0;
        virtual void contextSwitch(int32_t gid) = 0; //gid == -1 means descheduled, otherwise this is the new gid

        //Called by scheduler on every leave and join action, before barrier methods are called
        virtual void leave() {}
        virtual void join() {}

        virtual InstrFuncPtrs GetFuncPtrs() = 0;

        // Virtual type checks for use without RTTI (Pin 4.x requires -fno-rtti)
        virtual InOrderCore* asInOrderCore() { return nullptr; }
        virtual OOOCore* asOOOCore() { return nullptr; }

        // Virtual contention simulation interface (returns false if not supported)
        virtual bool hasContentionSim() const { return false; }
        virtual EventRecorder* getEventRecorder() { return nullptr; }
        virtual void cSimStart() {}
        virtual void cSimEnd() {}

        // Inject a delay (in cycles) into the core's cycle counter.
        // Used by MPI timing handlers and PE-MC remote access modeling.
        // Default: no-op (overridden by SimpleCore, ALUCore, etc.)
        virtual void addDelay(uint32_t /*cycles*/) {}

        // Snapshot the ROI baseline so per-core cycle/instr stats report only
        // the region of interest (roi_begin..roi_end). Default no-op.
        virtual void markRoiBegin() {}

        // Address-routed pricing DMA window (v1.1.1): while paused, loads and
        // stores are NOT priced by the serving memory -- used to bracket an
        // explicit staging memcpy whose transfer cost is already charged as a
        // sized M/D/1 link transfer (otherwise the copy loop would pay the
        // attach toll per element AND the link charge: double counting).
        // Default no-op (only ALUCore consults address-routed pricing).
        virtual void setMemPricingPaused(bool /*paused*/) {}
};

#endif  // CORE_H_

