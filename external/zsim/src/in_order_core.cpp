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
#include "filter_cache.h"
#include "zsim.h"

#define DEBUG_MSG(args...)
//#define DEBUG_MSG(args...) info(args)

InOrderCore::InOrderCore(FilterCache* _l1i, FilterCache* _l1d, uint32_t _domain, g_string& _name)
    : Core(_name), l1i(_l1i), l1d(_l1d), instrs(0), curCycle(0), cRec(_domain, _name) {}

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

    parentStat->append(coreStat);
}


void InOrderCore::contextSwitch(int32_t gid) {
    if (gid == -1) {
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

void InOrderCore::loadAndRecord(Address addr) {
    uint64_t startCycle = curCycle;
    curCycle = l1d->load(addr, curCycle);
    cRec.record(startCycle);
}

void InOrderCore::storeAndRecord(Address addr) {
    uint64_t startCycle = curCycle;
    curCycle = l1d->store(addr, curCycle);
    cRec.record(startCycle);
}

void InOrderCore::bblAndRecord(Address bblAddr, BblInfo* bblInfo) {
    instrs += bblInfo->instrs;
    curCycle += bblInfo->instrs;

    Address endBblAddr = bblAddr + bblInfo->bytes;
    for (Address fetchAddr = bblAddr; fetchAddr < endBblAddr; fetchAddr+=(1 << lineBits)) {
        uint64_t startCycle = curCycle;
        curCycle = l1i->load(fetchAddr, curCycle);
        cRec.record(startCycle);
    }
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

