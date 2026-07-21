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

#ifndef SIMPLE_CORE_H_
#define SIMPLE_CORE_H_

//A simple core model with IPC=1 except on memory accesses

#include "core.h"
#include "memory_hierarchy.h"
#include "pad.h"

class FilterCache;

class SimpleCore : public Core {
    protected:
        FilterCache* l1i;
        FilterCache* l1d;

        uint64_t instrs;
        uint64_t curCycle;
        uint64_t phaseEndCycle; //next stopping point
        uint64_t haltedCycles;

        // ROI baselines: snapshot at roi_begin so reported cycles/instrs reflect
        // ONLY the region of interest. roiBaseCycle snapshots the unhalted-cycle
        // metric (curCycle - haltedCycles). 0 until roi_begin fires.
        uint64_t roiBaseInstrs = 0;
        uint64_t roiBaseCycle  = 0;

    public:
        SimpleCore(FilterCache* _l1i, FilterCache* _l1d, g_string& _name);
        void initStats(AggregateStat* parentStat);

        uint64_t getInstrs() const {return instrs;}
        uint64_t getPhaseCycles() const;
        // 1.9.4 frozen-clock MPI waits: like the weave cores (in_order/ooo), the
        // rendezvous rewind is applied on the REPORTED side via pimidPhantomWait,
        // NOT by lowering curCycle. curCycle is re-pinned to the wall-pumped
        // globPhaseCycles by join() on every phase crossing, so a curCycle rewind
        // does not stick: it reappears at the next join and the wall-clock phase
        // floor leaks into the reported clock (thread-MPI cycles that tracked
        // globPhaseCycles instead of real work -- IPC << 1, size-independent).
        // Subtracting the accumulator at every read site (getCycles + the ROI
        // cycles stat) makes the rewind wall-free and join-immune, mirroring
        // in_order_core.h / ooo_core.h. 0 outside MPI frozen-clock waits, so
        // OMP and non-MPI paths are byte-identical.
        uint64_t pimidPhantomWait = 0;
        uint64_t getCycles() const {
            uint64_t c = curCycle - haltedCycles;
            return (c > pimidPhantomWait) ? (c - pimidPhantomWait) : 0;
        }

        void contextSwitch(int32_t gid);
        virtual void join();

        InstrFuncPtrs GetFuncPtrs();

        void addDelay(uint32_t cycles) override { curCycle += cycles; }
        void pimidRewindCycles(uint64_t delta) override { pimidPhantomWait += delta; }

        // Snapshot current counters as the ROI baseline (called on roi_begin).
        // getCycles() already nets out pimidPhantomWait (0 at roi_begin).
        void markRoiBegin() override { roiBaseInstrs = instrs; roiBaseCycle = getCycles(); }

    protected:
        //Simulation functions
        inline void load(Address addr);
        inline void store(Address addr);
        inline void bbl(Address bblAddr, BblInfo* bblInstrs);

        static void LoadFunc(THREADID tid, ADDRINT addr);
        static void StoreFunc(THREADID tid, ADDRINT addr);
        static void BblFunc(THREADID tid, ADDRINT bblAddr, BblInfo* bblInfo);
        static void PredLoadFunc(THREADID tid, ADDRINT addr, BOOL pred);
        static void PredStoreFunc(THREADID tid, ADDRINT addr, BOOL pred);

        static void BranchFunc(THREADID, ADDRINT, BOOL, ADDRINT, ADDRINT) {}
}  ATTR_LINE_ALIGNED; //This needs to take up a whole cache line, or false sharing will be extremely frequent

#endif  // SIMPLE_CORE_H_

