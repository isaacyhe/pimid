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

#include "in_order_core.h"
#include <cstdlib>
#include "bithacks.h"
#include "decoder_simple.h"
#include "filter_cache.h"
#include "zsim.h"

#define DEBUG_MSG(args...)
//#define DEBUG_MSG(args...) info(args)

InOrderCore::InOrderCore(FilterCache* _l1i, FilterCache* _l1d, uint32_t _domain, g_string& _name,
                         uint32_t _issueWidth)
    : Core(_name), l1i(_l1i), l1d(_l1d), instrs(0), uops(0), bbls(0),
      curCycle(0), cRec(_domain, _name) {
    // PIMID_INORDER_NODECODE=1 -> legacy IPC=1 immediate path (A/B baseline).
    decodeMode = (getenv("PIMID_INORDER_NODECODE") == nullptr);
    // In-order superscalar issue width. Precedence (highest wins):
    //   1. PIMID_INORDER_WIDTH env var  (experimentation override)
    //   2. _issueWidth ctor arg         (YAML pim.pe.issue_width -> ZSim issueWidth)
    //   3. default 2 (dual-issue)
    // Clamped to [1, NUM_PORTS]; out-of-range values fall back to the default.
    issueWidth = (_issueWidth >= 1 && _issueWidth <= NUM_PORTS) ? _issueWidth : 2;
    const char* w = getenv("PIMID_INORDER_WIDTH");
    if (w) {
        int v = atoi(w);
        if (v >= 1 && v <= (int)NUM_PORTS) issueWidth = (uint32_t)v;
    }

    prevBbl = nullptr;
    loads = stores = 0;
    for (uint32_t i = 0; i < MAX_REGISTERS; i++) regScoreboard[i] = 0;
    for (uint32_t i = 0; i < NUM_PORTS; i++) portFreeCycle[i] = 0;
    memRespCycle = 0;
    slotsUsed = 0;

    decodedBbls = syntheticBbls = depStalls = issueStalls = 0;
    memMismatchLoads = memMismatchStores = 0;
    repDrainedLoads = repDrainedStores = 0;
    phaseEndCycle = 0;

    // Branch misprediction: front-end flush/refill bubble. Default 7 cycles ~=
    // the OOO model's fetch-to-issue depth (ISSUE_STAGE), i.e. the redirect
    // cost of a short in-order pipeline (Cortex-A53 class is ~8 cycles).
    // Overridable via PIMID_INORDER_MISPRED_PENALTY for sensitivity studies.
    // The whole feed is disabled by PIMID_INORDER_NOBRANCH=1 (plugin-side gate).
    branchPc = 0;
    branchTaken = false;
    mispredPenalty = 7;
    const char* mp = getenv("PIMID_INORDER_MISPRED_PENALTY");
    if (mp) {
        int v = atoi(mp);
        if (v >= 0 && v <= 1000) mispredPenalty = (uint32_t)v;
    }
    branches = mispredBranches = mispredStallCycles = 0;

    // Indirect control flow (BTB + RAS; IndirectPredictor default-constructs).
    indirMispredPend = false;
    indirBranches = indirMispreds = rasReturns = rasMispreds = 0;
}

uint64_t InOrderCore::getPhaseCycles() const {
    return curCycle % zinfo->phaseLength;
}

void InOrderCore::initStats(AggregateStat* parentStat) {
    AggregateStat* coreStat = new AggregateStat();
    coreStat->init(name.c_str(), "Core stats");

    // Report cycles/instrs RELATIVE to the ROI baseline (roi_begin); roiBase* are
    // 0 until roi_begin, so non-ROI workloads are unaffected. cCycles (contention)
    // is left absolute as it is a diagnostic counter, not an ROI duration metric.
    auto x = [this]() { return cRec.getUnhaltedCycles(curCycle) - roiBaseCycle; };
    LambdaStat<decltype(x)>* cyclesStat = new LambdaStat<decltype(x)>(x);
    cyclesStat->init("cycles", "Simulated unhalted cycles");
    coreStat->append(cyclesStat);

    auto y = [this]() { return cRec.getContentionCycles(); };
    LambdaStat<decltype(y)>* cCyclesStat = new LambdaStat<decltype(y)>(y);
    cCyclesStat->init("cCycles", "Cycles due to contention stalls");
    coreStat->append(cCyclesStat);

    auto z = [this]() -> uint64_t { return instrs - roiBaseInstrs; };
    LambdaStat<decltype(z)>* instrsStat = new LambdaStat<decltype(z)>(z);
    instrsStat->init("instrs", "Simulated instructions");
    coreStat->append(instrsStat);

    // In-order pipeline diagnostics (analogous to the OOO core).
    ProxyStat* uopsStat = new ProxyStat();
    uopsStat->init("uops", "Retired micro-ops", &uops);
    coreStat->append(uopsStat);
    ProxyStat* bblsStat = new ProxyStat();
    bblsStat->init("bbls", "Basic blocks", &bbls);
    coreStat->append(bblsStat);
    ProxyStat* decodedBblsStat = new ProxyStat();
    decodedBblsStat->init("decodedBbls", "BBLs run through the decoded in-order scoreboard", &decodedBbls);
    coreStat->append(decodedBblsStat);
    ProxyStat* syntheticBblsStat = new ProxyStat();
    syntheticBblsStat->init("syntheticBbls", "BBLs run through the synthetic 1-CPI fallback", &syntheticBbls);
    coreStat->append(syntheticBblsStat);
    ProxyStat* depStallsStat = new ProxyStat();
    depStallsStat->init("depStalls", "Cycles lost to RAW dependency stalls", &depStalls);
    coreStat->append(depStallsStat);
    ProxyStat* issueStallsStat = new ProxyStat();
    issueStallsStat->init("issueStalls", "Cycles lost to issue-width/port stalls", &issueStalls);
    coreStat->append(issueStallsStat);
    ProxyStat* memMismatchLoadsStat = new ProxyStat();
    memMismatchLoadsStat->init("memMismatchLoads", "Decoded/runtime load-count divergences drained", &memMismatchLoads);
    coreStat->append(memMismatchLoadsStat);
    ProxyStat* memMismatchStoresStat = new ProxyStat();
    memMismatchStoresStat->init("memMismatchStores", "Decoded/runtime store-count divergences drained", &memMismatchStores);
    coreStat->append(memMismatchStoresStat);
    ProxyStat* repDrainedLoadsStat = new ProxyStat();
    repDrainedLoadsStat->init("repDrainedLoads", "Expected rep-string loads drained (block-copy model)", &repDrainedLoads);
    coreStat->append(repDrainedLoadsStat);
    ProxyStat* repDrainedStoresStat = new ProxyStat();
    repDrainedStoresStat->init("repDrainedStores", "Expected rep-string stores drained (block-copy model)", &repDrainedStores);
    coreStat->append(repDrainedStoresStat);
    ProxyStat* branchesStat = new ProxyStat();
    branchesStat->init("branches", "Resolved conditional branches fed to the predictor", &branches);
    coreStat->append(branchesStat);
    ProxyStat* mispredBranchesStat = new ProxyStat();
    mispredBranchesStat->init("mispredBranches", "Mispredicted branches", &mispredBranches);
    coreStat->append(mispredBranchesStat);
    ProxyStat* mispredStallCyclesStat = new ProxyStat();
    mispredStallCyclesStat->init("mispredStallCycles", "Cycles charged for mispredict flush/refill bubbles", &mispredStallCycles);
    coreStat->append(mispredStallCyclesStat);
    ProxyStat* indirBranchesStat = new ProxyStat();
    indirBranchesStat->init("indirBranches", "Indirect jmp/call resolutions fed to the BTB", &indirBranches);
    coreStat->append(indirBranchesStat);
    ProxyStat* indirMispredsStat = new ProxyStat();
    indirMispredsStat->init("indirMispreds", "Indirect jmp/call target mispredictions", &indirMispreds);
    coreStat->append(indirMispredsStat);
    ProxyStat* rasReturnsStat = new ProxyStat();
    rasReturnsStat->init("rasReturns", "Returns resolved against the RAS", &rasReturns);
    coreStat->append(rasReturnsStat);
    ProxyStat* rasMispredsStat = new ProxyStat();
    rasMispredsStat->init("rasMispreds", "Return-target mispredictions (RAS miss)", &rasMispreds);
    coreStat->append(rasMispredsStat);

    parentStat->append(coreStat);
}


void InOrderCore::contextSwitch(int32_t gid) {
    if (gid == -1) {
        // Do not simulate the lingering previous BBL across a context switch.
        prevBbl = nullptr;
        loads = stores = 0;
        branchPc = 0;
        indirMispredPend = false;
        l1i->contextSwitch();
        l1d->contextSwitch();
    }
}

void InOrderCore::join() {
    DEBUG_MSG("[%s] Joining, curCycle %ld phaseEnd %ld", name.c_str(), curCycle, phaseEndCycle);
    curCycle = cRec.notifyJoin(curCycle);
    phaseEndCycle = zinfo->globPhaseCycles + zinfo->phaseLength;
    DEBUG_MSG("[%s] Joined, curCycle %ld phaseEnd %ld", name.c_str(), curCycle, phaseEndCycle);
}

void InOrderCore::leave() {
    cRec.notifyLeave(curCycle);
}

/* ---- Legacy immediate load/store (NODECODE path) ---- */

void InOrderCore::loadAndRecord(Address addr) {
    if (!decodeMode) {
        uint64_t startCycle = curCycle;
        curCycle = l1d->load(addr, curCycle);
        cRec.record(startCycle);
    } else {
        if (loads < 256) loadAddrs[loads] = addr;
        loads++;
    }
}

void InOrderCore::storeAndRecord(Address addr) {
    if (!decodeMode) {
        uint64_t startCycle = curCycle;
        curCycle = l1d->store(addr, curCycle);
        cRec.record(startCycle);
    } else {
        if (stores < 256) storeAddrs[stores] = addr;
        stores++;
    }
}

/* ---- Instruction fetch of the current BBL (serialized through L1I) ---- */

inline void InOrderCore::ifetch(Address bblAddr, BblInfo* bblInfo) {
    Address endBblAddr = bblAddr + bblInfo->bytes;
    for (Address fetchAddr = bblAddr; fetchAddr < endBblAddr; fetchAddr += (1 << lineBits)) {
        uint64_t startCycle = MAX(curCycle, memRespCycle);
        uint64_t resp = l1i->load(fetchAddr, startCycle);
        cRec.record(startCycle);
        memRespCycle = resp;
        if (resp > curCycle) curCycle = resp;  // fetch stalls the in-order front-end
    }
}

/* ---- Synthetic 1-CPI fallback (no decoded uops for this BBL) ---- */

inline void InOrderCore::simulateSyntheticBbl(BblInfo* bblInfo) {
    uint64_t commitCycle = curCycle + bblInfo->instrs;  // 1-CPI compute
    for (uint32_t i = 0; i < loads && i < 256; i++) {
        Address addr = loadAddrs[i];
        if (addr == (Address)-1L) continue;
        uint64_t startCycle = MAX(commitCycle, memRespCycle);
        uint64_t resp = l1d->load(addr, startCycle);
        cRec.record(startCycle);
        memRespCycle = resp;
        commitCycle = resp;
    }
    for (uint32_t i = 0; i < stores && i < 256; i++) {
        Address addr = storeAddrs[i];
        if (addr == (Address)-1L) continue;
        uint64_t startCycle = MAX(commitCycle, memRespCycle);
        uint64_t resp = l1d->store(addr, startCycle);
        cRec.record(startCycle);
        memRespCycle = resp;
        commitCycle = resp;
    }
    if (commitCycle > curCycle) curCycle = commitCycle;
    syntheticBbls++;
}

/* ---- Real in-order scoreboard simulation of one BBL ---- */

inline void InOrderCore::simulateDecodedBbl(BblInfo* bblInfo) {
    DynBbl* db = &(bblInfo->oooBbl[0]);
    uint32_t nUops = db->uops;
    uint32_t loadIdx = 0, storeIdx = 0;
    uint64_t lastDone = curCycle;

    regScoreboard[0] = 0;  // SB_NONE is always ready

    for (uint32_t i = 0; i < nUops; i++) {
        DynUop* uop = &(db->uop[i]);

        // --- RAW hazard: sources must be ready before issue (in-order stall) ---
        uint64_t srcReady = MAX(regScoreboard[uop->rs[0]], regScoreboard[uop->rs[1]]);
        uint64_t iss = curCycle;
        if (srcReady > iss) { depStalls += (srcReady - iss); iss = srcReady; slotsUsed = 0; }

        // --- Issue width: at most issueWidth independent uops per cycle ---
        if (iss == curCycle) {
            if (slotsUsed >= issueWidth) { iss++; issueStalls++; slotsUsed = 0; }
        } else {
            slotsUsed = 0;  // moved to a new issue cycle due to the RAW stall
        }

        // --- Functional-unit port contention (in-order: whole front-end waits) ---
        uint8_t mask = uop->portMask ? uop->portMask : 0x01;
        uint64_t bestFree = (uint64_t)-1L; int bestPort = -1;
        for (uint32_t p = 0; p < NUM_PORTS; p++) {
            if (mask & (1u << p)) {
                if (portFreeCycle[p] < bestFree) { bestFree = portFreeCycle[p]; bestPort = p; }
            }
        }
        if (bestPort < 0) { bestPort = 0; bestFree = portFreeCycle[0]; }
        if (bestFree > iss) { issueStalls += (bestFree - iss); iss = bestFree; slotsUsed = 0; }

        // Commit the issue at cycle `iss`.
        portFreeCycle[bestPort] = iss + 1;  // 1 uop/cycle throughput on this port
        slotsUsed++;
        curCycle = iss;  // in-order issue cursor advances monotonically

        // --- Execute / complete ---
        uint64_t done;
        if (uop->type == UOP_LOAD) {
            Address addr;
            if (loadIdx < loads) addr = loadAddrs[loadIdx++];
            else { addr = (Address)-1L; memMismatchLoads++; }
            if (addr != (Address)-1L) {
                uint64_t startCycle = MAX(iss, memRespCycle);
                uint64_t resp = l1d->load(addr, startCycle);
                cRec.record(startCycle);
                memRespCycle = resp;
                done = resp;  // load-use latency gates the destination register
            } else {
                done = iss;   // predicated / mismatched -> 0-cycle
            }
        } else if (uop->type == UOP_STORE) {
            Address addr;
            if (storeIdx < stores) addr = storeAddrs[storeIdx++];
            else { addr = (Address)-1L; memMismatchStores++; }
            if (addr != (Address)-1L) {
                uint64_t startCycle = MAX(iss, memRespCycle);
                uint64_t resp = l1d->store(addr, startCycle);
                cRec.record(startCycle);
                memRespCycle = resp;
                done = resp;
            } else {
                done = iss;
            }
        } else {
            // UOP_GENERAL / UOP_STORE_ADDR / UOP_FENCE: fixed FU latency.
            done = iss + uop->lat;
        }

        // --- Scoreboard: destination registers become ready at `done` ---
        if (uop->rd[0]) regScoreboard[uop->rd[0]] = done;
        if (uop->rd[1]) regScoreboard[uop->rd[1]] = done;
        if (done > lastDone) lastDone = done;
    }

    // Drain any runtime memory accesses not consumed by the decoded uop stream.
    // Two populations, counted separately (same convention as the OOO core):
    //  - EXPECTED rep-string traffic (db->repInstrs > 0): rep movs/stos have
    //    dynamic iteration counts, so their per-iteration accesses are left to
    //    this serial drain (block-copy cost model) -> repDrained{Loads,Stores};
    //  - true decode/runtime divergences -> memMismatch{Loads,Stores}.
    // Either way they go through the cache so DRAM/NoC traffic stays accounted.
    bool repBbl = (db->repInstrs > 0);
    while (loadIdx < loads) {
        Address a = loadAddrs[loadIdx++];
        if (repBbl) repDrainedLoads++; else memMismatchLoads++;
        if (a != (Address)-1L) {
            uint64_t startCycle = MAX(lastDone, memRespCycle);
            uint64_t resp = l1d->load(a, startCycle);
            cRec.record(startCycle);
            memRespCycle = resp;
            if (resp > lastDone) lastDone = resp;
        }
    }
    while (storeIdx < stores) {
        Address a = storeAddrs[storeIdx++];
        if (repBbl) repDrainedStores++; else memMismatchStores++;
        if (a != (Address)-1L) {
            uint64_t startCycle = MAX(lastDone, memRespCycle);
            uint64_t resp = l1d->store(a, startCycle);
            cRec.record(startCycle);
            memRespCycle = resp;
            if (resp > lastDone) lastDone = resp;
        }
    }

    // In-order commit drains this BBL before the next: advance the core clock to
    // the last completion. (BBLs are loop bodies here; this models no overlap
    // across BBL boundaries -- a defensible simplification for a simple in-order
    // core; intra-BBL dependency/latency/issue effects are fully captured.)
    if (lastDone > curCycle) curCycle = lastDone;
    slotsUsed = 0;
    uops += nUops;
    decodedBbls++;
}

/* ---- BBL entry point ---- */

void InOrderCore::bblAndRecord(Address bblAddr, BblInfo* bblInfo) {
    if (!decodeMode) {
        // Legacy IPC=1 immediate path (byte-identical A/B baseline).
        instrs += bblInfo->instrs;
        curCycle += bblInfo->instrs;
        Address endBblAddr = bblAddr + bblInfo->bytes;
        for (Address fetchAddr = bblAddr; fetchAddr < endBblAddr; fetchAddr += (1 << lineBits)) {
            uint64_t startCycle = curCycle;
            curCycle = l1i->load(fetchAddr, curCycle);
            cRec.record(startCycle);
        }
        return;
    }

    // Deferred decoded path: simulate the PREVIOUS BBL now that its memory
    // addresses have all been buffered (mem_cb fires between BBL callbacks).
    if (!prevBbl) {
        prevBbl = bblInfo;
        loads = stores = 0;
        branchPc = 0;  // any pending branch belongs to a BBL we never simulated
        indirMispredPend = false;
        return;
    }

    BblInfo* sim = prevBbl;
    prevBbl = bblInfo;

    instrs += sim->instrs;
    bbls++;

    if (sim->oooBbl[0].uops > 0) {
        simulateDecodedBbl(sim);
    } else {
        simulateSyntheticBbl(sim);
    }

    // Branch misprediction: the plugin delivered (right before this bblPtr) the
    // resolved direction of the conditional branch TERMINATING `sim`. Query and
    // update the predictor; on a mispredict, charge the front-end flush/refill
    // bubble. In-order cores resolve the branch at execute, so the redirect
    // lands after the BBL drains (curCycle is at last completion here).
    if (branchPc) {
        branches++;
        if (!branchPred.predict(branchPc, branchTaken)) {
            mispredBranches++;
            mispredStallCycles += mispredPenalty;
            curCycle += mispredPenalty;
        }
        branchPc = 0;
    }

    // Indirect jmp/call/ret target misprediction (BTB/RAS miss, armed by
    // ctrlFlow for the terminator of `sim`): same flush/refill bubble as a
    // conditional mispredict.
    if (indirMispredPend) {
        indirMispredPend = false;
        mispredStallCycles += mispredPenalty;
        curCycle += mispredPenalty;
    }

    loads = stores = 0;

    // Fetch the current BBL's instructions (models L1I traffic + stalls).
    ifetch(bblAddr, bblInfo);
}


InstrFuncPtrs InOrderCore::GetFuncPtrs() {
    return {LoadAndRecordFunc, StoreAndRecordFunc, BblAndRecordFunc, BranchFunc, PredLoadAndRecordFunc, PredStoreAndRecordFunc, FPTR_ANALYSIS, {0}};
}

void InOrderCore::LoadAndRecordFunc(THREADID tid, ADDRINT addr) {
    static_cast<InOrderCore*>(cores[tid])->loadAndRecord(addr);
}

void InOrderCore::StoreAndRecordFunc(THREADID tid, ADDRINT addr) {
    static_cast<InOrderCore*>(cores[tid])->storeAndRecord(addr);
}

void InOrderCore::BblAndRecordFunc(THREADID tid, ADDRINT bblAddr, BblInfo* bblInfo) {
    InOrderCore* core = static_cast<InOrderCore*>(cores[tid]);
    core->bblAndRecord(bblAddr, bblInfo);

    while (core->curCycle > core->phaseEndCycle) {
        core->phaseEndCycle += zinfo->phaseLength;
        uint32_t cid = getCid(tid);
        uint32_t newCid = TakeBarrier(tid, cid);
        if (newCid != cid) break; /*context-switch*/
    }
}

void InOrderCore::PredLoadAndRecordFunc(THREADID tid, ADDRINT addr, BOOL pred) {
    if (pred) static_cast<InOrderCore*>(cores[tid])->loadAndRecord(addr);
}

void InOrderCore::PredStoreAndRecordFunc(THREADID tid, ADDRINT addr, BOOL pred) {
    if (pred) static_cast<InOrderCore*>(cores[tid])->storeAndRecord(addr);
}

/* ---- Branch feed (plugin calls this right before bblPtr; see plugin gate) ---- */

void InOrderCore::branch(Address pc, bool taken) {
    branchPc = pc;
    branchTaken = taken;
}

/* Indirect control-flow resolution (kind >= CF_IND_JMP, see CtrlFlowKind in
 * ooo_core.h). target = resolved actual target (next TB start); retAddr = the
 * call's fall-through (calls only). Wrong prediction arms the mispredPenalty
 * bubble consumed after the terminator's BBL is simulated. */
void InOrderCore::ctrlFlow(uint32_t kind, Address pc, Address target, Address retAddr) {
    switch (kind) {
        case CF_DIR_CALL:
            indirPred.push(retAddr);  /* direct call: target always predicted */
            break;
        case CF_IND_CALL:
            indirBranches++;
            if (!indirPred.indirect(pc, target)) { indirMispreds++; indirMispredPend = true; }
            indirPred.push(retAddr);
            break;
        case CF_IND_JMP:
            indirBranches++;
            if (!indirPred.indirect(pc, target)) { indirMispreds++; indirMispredPend = true; }
            break;
        case CF_RET:
            rasReturns++;
            if (!indirPred.ret(target)) { rasMispreds++; indirMispredPend = true; }
            break;
        default: break;
    }
}

void InOrderCore::BranchFunc(THREADID tid, ADDRINT pc, BOOL taken, ADDRINT takenNpc, ADDRINT notTakenNpc) {
    InOrderCore* core = static_cast<InOrderCore*>(cores[tid]);
    // taken <= 1: conditional direction feed (wrong-path fetches not modeled).
    // taken >= 2: CtrlFlowKind for indirect jmp/call/ret and direct-call RAS
    // pushes (takenNpc = resolved target, notTakenNpc = call fall-through).
    if (taken <= CF_COND_T) core->branch(pc, taken != 0);
    else core->ctrlFlow((uint32_t)taken, pc, takenNpc, notTakenNpc);
}
