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

#ifndef IN_ORDER_CORE_H_
#define IN_ORDER_CORE_H_

#include "core.h"
#include "core_recorder.h"
#include "event_recorder.h"
#include "memory_hierarchy.h"
#include "ooo_core.h"  /* BranchPredictorPAg -- same predictor structure as OOOCore */
#include "pad.h"

class FilterCache;

/* Genuine in-order pipeline PE core.
 *
 * Historically this core modeled IPC=1 (curCycle += bblInstrs) plus blocking
 * memory latency and a CoreRecorder weave for cross-PE contention -- which made
 * it numerically identical to SimpleCore whenever there was no contention
 * ("in-order in name only"). This version consumes the same x86-decoded DynUop
 * stream the OOO core uses (BblInfo->oooBbl, populated by the plugin's
 * x86_decoder.h when an in-order OR out-of-order core is present) and runs a
 * real in-order scoreboard:
 *
 *   - uops issue in STRICT PROGRAM ORDER (the defining in-order property);
 *   - a per-physical-register ready-cycle scoreboard stalls issue on RAW
 *     hazards (a uop cannot issue until its source registers are ready);
 *   - up to issueWidth INDEPENDENT uops may issue in one cycle, but the
 *     front-end STOPS at the first uop that cannot issue (no reordering);
 *   - functional-unit port contention is modeled from the decoder port masks;
 *   - loads/stores go through the l1d cache path; a load's destination-register
 *     readiness is gated on the returned latency (load-use stall), and memory
 *     accesses are serialized (blocking L1) which also preserves the
 *     CoreRecorder weave invariant.
 *
 * The CoreRecorder cross-PE contention weave is layered on top unchanged.
 *
 * Escape hatch: PIMID_INORDER_NODECODE=1 restores the exact legacy IPC=1
 * immediate-processing path (byte-identical A/B baseline). A BBL with no decoded
 * uops falls back to a per-BBL synthetic 1-CPI path that still drains buffered
 * loads/stores through the cache so memory/NoC traffic never desyncs.
 *
 * Branch mispredictions: the plugin resolves each TB-terminating conditional
 * branch's real direction (from the next TB's address) and feeds it via
 * BranchFunc, exactly as for the OOO core. The in-order core runs the SAME
 * BranchPredictorPAg<11,18,14> predictor as OOOCore; on a mispredict it charges
 * a fixed front-end flush/refill bubble (default 7 cycles ~= the OOO model's
 * fetch-to-issue depth, ISSUE_STAGE; typical of short in-order pipelines such
 * as Cortex-A53's ~8-cycle mispredict penalty). Overridable via
 * PIMID_INORDER_MISPRED_PENALTY; disable the whole feed with
 * PIMID_INORDER_NOBRANCH=1 (mirrors PIMID_OOO_NOBRANCH).
 */
class InOrderCore : public Core {
    private:
        FilterCache* l1i;
        FilterCache* l1d;

        uint64_t instrs;
        uint64_t uops;
        uint64_t bbls;

        uint64_t curCycle; //phase 1 clock (in-order issue/commit cursor)
        uint64_t phaseEndCycle; //phase 1 end clock

        CoreRecorder cRec;

        // ---- In-order scoreboard state (decoded path) ----
        // decodeMode: false when PIMID_INORDER_NODECODE is set -> legacy path.
        bool decodeMode;
        uint32_t issueWidth; // independent uops issuable per cycle (default 2)

        BblInfo* prevBbl;    // deferred: simulate prev BBL once its mem addrs arrive

        // Buffered memory addresses for the BBL currently executing (decode path)
        Address loadAddrs[256];
        Address storeAddrs[256];
        uint32_t loads;
        uint32_t stores;

        // Per-physical-register ready cycle (timestamp a reg's value is available)
        uint64_t regScoreboard[MAX_REGISTERS];
        // Per functional-unit port: earliest cycle the port is free (1 uop/cyc)
        static const uint32_t NUM_PORTS = 6;
        uint64_t portFreeCycle[NUM_PORTS];
        // Serialization cursor for the blocking L1 / cRec weave invariant: the
        // response cycle of the last memory access threaded through the cache.
        uint64_t memRespCycle;
        uint32_t slotsUsed;  // uops already issued at the current issue cycle

        // Diagnostics (analogous to the OOO core)
        uint64_t decodedBbls;    // BBLs run through the decoded in-order scoreboard
        uint64_t syntheticBbls;  // BBLs run through the 1-CPI synthetic fallback
        uint64_t depStalls;      // cycles lost to RAW dependency stalls
        uint64_t issueStalls;    // cycles lost to issue-width/port stalls
        uint64_t memMismatchLoads;   // decoded/runtime load-count divergences drained
        uint64_t memMismatchStores;  // decoded/runtime store-count divergences drained
        // Expected rep-string (movs/stos) accesses drained through the
        // block-copy cost model (DynBbl.repInstrs > 0) -- NOT divergences.
        uint64_t repDrainedLoads;
        uint64_t repDrainedStores;

        // ---- Branch prediction (decode path only) ----
        // Same predictor structure as OOOCore (2-level PAg).
        BranchPredictorPAg<11, 18, 14> branchPred;
        Address branchPc;        // 0 if the BBL being simulated did not end in a jcc
        bool branchTaken;
        uint32_t mispredPenalty; // front-end flush/refill bubble (cycles)
        uint64_t branches;           // resolved conditional branches fed to the predictor
        uint64_t mispredBranches;    // mispredicted branches
        uint64_t mispredStallCycles; // total cycles charged for mispredict bubbles

        // Indirect control-flow prediction: same minimal structures as OOOCore
        // (direct-mapped PC-tagged 512-entry BTB + 16-entry RAS). A wrong
        // target charges the same mispredPenalty bubble as a conditional
        // mispredict. Gated by PIMID_INORDER_NOBRANCH like all branch modeling.
        IndirectPredictor<9, 16> indirPred;
        bool indirMispredPend;
        uint64_t indirBranches;
        uint64_t indirMispreds;
        uint64_t rasReturns;
        uint64_t rasMispreds;

        // ROI baselines: snapshot at roi_begin so reported cycles/instrs reflect
        // ONLY the region of interest. roiBaseCycle snapshots the unhalted-cycle
        // metric (cRec.getUnhaltedCycles(curCycle)). 0 until roi_begin fires.
        uint64_t roiBaseInstrs = 0;
        uint64_t roiBaseCycle  = 0;

    public:
        // _issueWidth: in-order superscalar issue width (YAML pim.pe.issue_width,
        // plumbed via the ZSim config key issueWidth; default 2). Precedence:
        // PIMID_INORDER_WIDTH env var (if set) > YAML/ctor value > default 2.
        InOrderCore(FilterCache* _l1i, FilterCache* _l1d, uint32_t domain, g_string& _name,
                    uint32_t _issueWidth = 2);
        void initStats(AggregateStat* parentStat);

        uint64_t getInstrs() const {return instrs;}
        uint64_t getPhaseCycles() const;
        uint64_t getCycles() const {return cRec.getUnhaltedCycles(curCycle);}

        void contextSwitch(int32_t gid);
        virtual void join();
        virtual void leave();

        InstrFuncPtrs GetFuncPtrs();

        // Snapshot current counters as the ROI baseline (called on roi_begin).
        void markRoiBegin() override { roiBaseInstrs = instrs; roiBaseCycle = cRec.getUnhaltedCycles(curCycle); }

        // Virtual type check for use without RTTI (Pin 4.x requires -fno-rtti)
        InOrderCore* asInOrderCore() override { return this; }

        // Contention simulation interface
        bool hasContentionSim() const override { return true; }
        EventRecorder* getEventRecorder() override {return cRec.getEventRecorder();}
        void cSimStart() override {curCycle = cRec.cSimStart(curCycle);}
        void cSimEnd() override {curCycle = cRec.cSimEnd(curCycle);}

    private:
        inline void loadAndRecord(Address addr);
        inline void storeAndRecord(Address addr);
        inline void bblAndRecord(Address bblAddr, BblInfo* bblInstrs);
        inline void record(uint64_t startCycle);

        // Decoded in-order scoreboard simulation of one (previous) BBL.
        inline void simulateDecodedBbl(BblInfo* bblInfo);
        // Synthetic 1-CPI fallback for a BBL without decoded uops.
        inline void simulateSyntheticBbl(BblInfo* bblInfo);
        // Instruction fetch of the current BBL (serialized through the L1I).
        inline void ifetch(Address bblAddr, BblInfo* bblInfo);

        // Records the resolved direction of the branch terminating the BBL that
        // is about to be simulated (the plugin calls this right before bblPtr).
        inline void branch(Address pc, bool taken);

        // Indirect control-flow resolution (kind >= CF_IND_JMP, CtrlFlowKind in
        // ooo_core.h): BTB/RAS query+update; arms the mispredict bubble.
        inline void ctrlFlow(uint32_t kind, Address pc, Address target, Address retAddr);

        static void LoadAndRecordFunc(THREADID tid, ADDRINT addr);
        static void StoreAndRecordFunc(THREADID tid, ADDRINT addr);
        static void BblAndRecordFunc(THREADID tid, ADDRINT bblAddr, BblInfo* bblInfo);
        static void PredLoadAndRecordFunc(THREADID tid, ADDRINT addr, BOOL pred);
        static void PredStoreAndRecordFunc(THREADID tid, ADDRINT addr, BOOL pred);

        static void BranchFunc(THREADID tid, ADDRINT pc, BOOL taken, ADDRINT takenNpc, ADDRINT notTakenNpc);
} ATTR_LINE_ALIGNED;

#endif  // IN_ORDER_CORE_H_
