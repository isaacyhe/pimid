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

#include "alu_core.h"
#include "pe_memory_interface.h"
#include "zsim.h"
#include <algorithm>

// ALU Core: Pure computational core with optional PE memory interface

ALUCore::ALUCore(g_string& _name, double _computeFactor, double _accessFactor,
                 double _throughputFactor, int _operandWidth, double _energyFactor,
                 PEMemoryInterface* mi, uint32_t srcId, bool _bitSerial)
    : Core(_name), computeFactor(_computeFactor), accessFactor(_accessFactor),
      throughputFactor(std::max(_throughputFactor, 1e-9)),
      operandWidth(_operandWidth), energyFactor(_energyFactor), bitSerial(_bitSerial),
      mi_(mi), srcId_(srcId) {
    instrs = 0;
    curCycle = 0;
    phaseEndCycle = 0;

    info("ALU Core '%s': compute=%.2f, access=%.2f, throughput=%.2f, width=%d, serial=%s, energy=%.2f, mi=%s",
         name.c_str(), computeFactor, accessFactor, throughputFactor, operandWidth,
         bitSerial ? "yes" : "no", energyFactor, mi_ ? "wired" : "none");
}

uint64_t ALUCore::getPhaseCycles() const {
    return curCycle % zinfo->phaseLength;
}

void ALUCore::initStats(AggregateStat* parentStat) {
    AggregateStat* coreStat = new AggregateStat();
    coreStat->init(name.c_str(), "Core stats");

    // Report cycles/instrs RELATIVE to the ROI baseline (roi_begin), so serial
    // pre-ROI init/setup on the launcher PE is excluded from the kernel metric.
    // (roiBase* are 0 until roi_begin, so non-ROI workloads are unaffected.)
    auto x = [this]() -> uint64_t { return curCycle - roiBaseCycle; };
    auto cyclesStat = makeLambdaStat(x);
    cyclesStat->init("cycles", "Simulated cycles");
    coreStat->append(cyclesStat);

    auto xi = [this]() -> uint64_t { return instrs - roiBaseInstrs; };
    auto instrsStat = makeLambdaStat(xi);
    instrsStat->init("instrs", "Simulated instructions");
    coreStat->append(instrsStat);

    parentStat->append(coreStat);
}

void ALUCore::contextSwitch(int32_t gid) {
    if (gid == -1) {
        // No gid switch, just sync
    }
}

void ALUCore::join() {
    // Sync curCycle with phase
    phaseEndCycle += zinfo->phaseLength;
}

// Basic block execution - only ALU operations, NO memory
inline void ALUCore::bbl(BblInfo* bblInfo) {
    instrs += bblInfo->instrs;

    // Datapath mode: bit-serial charges ~W bit-steps per op (cost proportional to
    // operand width, W-bit op = W single-bit steps); bit-parallel (default) charges
    // one step regardless of width, like a full-width ALU. So bit_serial = false
    // leaves compute cost independent of operand_width (backward-compatible).
    uint64_t steps = bitSerial ? (uint64_t)std::max(operandWidth, 1) : 1ull;
    curCycle += (uint64_t)(bblInfo->instrs * computeFactor * steps / throughputFactor);
}

// Static callback functions for Pin instrumentation
void ALUCore::BblFunc(THREADID tid, ADDRINT bblAddr, BblInfo* bblInfo) {
    ALUCore* core = static_cast<ALUCore*>(cores[tid]);
    core->bbl(bblInfo);

    // Timing simulation only within phases
    while (core->curCycle > core->phaseEndCycle) {
        core->phaseEndCycle += zinfo->phaseLength;
        uint32_t cid = getCid(tid);

        // NOTE: TakeBarrier may take ownership of the core, and so it will be used by some other thread.
        // If TakeBarrier context-switches us, the *only* safe option is to return immediately after we
        // detect this, or we can race and corrupt core state.
        uint32_t newCid = TakeBarrier(tid, cid);
        if (newCid != cid) break; /*context-switch*/
    }
}

// Load — local: accessFactor cycles; remote: accessFactor + MI hierarchy
// traversal. ONE rule everywhere: a device PE's accesses go through ITS OWN
// device model (the PE-MI), in device scope and co-sim alike. The only
// exception is a DMA window (dmaWindow_): the staging copy's bulk cost is
// already charged on the host-device link, so the loop charges flat cost.
void ALUCore::LoadFunc(THREADID tid, ADDRINT addr) {
    ALUCore* core = static_cast<ALUCore*>(cores[tid]);
    Address lineAddr = addr >> lineBits;
    if (!core->mi_ || core->dmaWindow_ || core->mi_->isLocalAddress(lineAddr)) {
        // No MI, DMA window, or local address: direct array access cost
        core->curCycle += (uint64_t)(core->accessFactor);
    } else {
        // Remote: PE overhead + hierarchy traversal via MI
        core->curCycle += (uint64_t)(core->accessFactor);
        MESIState state = I;
        MemReq req = {lineAddr, GETS, 0, &state, core->curCycle, nullptr, state, core->srcId_, 0};
        core->curCycle = core->mi_->access(req);
    }
}

void ALUCore::StoreFunc(THREADID tid, ADDRINT addr) {
    ALUCore* core = static_cast<ALUCore*>(cores[tid]);
    Address lineAddr = addr >> lineBits;
    if (!core->mi_ || core->dmaWindow_ || core->mi_->isLocalAddress(lineAddr)) {
        core->curCycle += (uint64_t)(core->accessFactor);
    } else {
        core->curCycle += (uint64_t)(core->accessFactor);
        MESIState state = I;
        MemReq req = {lineAddr, GETX, 0, &state, core->curCycle, nullptr, state, core->srcId_, 0};
        core->curCycle = core->mi_->access(req);
    }
}

void ALUCore::PredLoadFunc(THREADID tid, ADDRINT addr, BOOL pred) {
    if (pred) {
        ALUCore* core = static_cast<ALUCore*>(cores[tid]);
        Address lineAddr = addr >> lineBits;
        if (!core->mi_ || core->dmaWindow_ || core->mi_->isLocalAddress(lineAddr)) {
            core->curCycle += (uint64_t)(core->accessFactor);
        } else {
            core->curCycle += (uint64_t)(core->accessFactor);
            MESIState state = I;
            MemReq req = {lineAddr, GETS, 0, &state, core->curCycle, nullptr, state, core->srcId_, 0};
            core->curCycle = core->mi_->access(req);
        }
    }
}

void ALUCore::PredStoreFunc(THREADID tid, ADDRINT addr, BOOL pred) {
    if (pred) {
        ALUCore* core = static_cast<ALUCore*>(cores[tid]);
        Address lineAddr = addr >> lineBits;
        if (!core->mi_ || core->dmaWindow_ || core->mi_->isLocalAddress(lineAddr)) {
            core->curCycle += (uint64_t)(core->accessFactor);
        } else {
            core->curCycle += (uint64_t)(core->accessFactor);
            MESIState state = I;
            MemReq req = {lineAddr, GETX, 0, &state, core->curCycle, nullptr, state, core->srcId_, 0};
            core->curCycle = core->mi_->access(req);
        }
    }
}

// Return function pointers for Pin instrumentation
InstrFuncPtrs ALUCore::GetFuncPtrs() {
    return {LoadFunc, StoreFunc, BblFunc, BranchFunc, PredLoadFunc, PredStoreFunc, FPTR_ANALYSIS, {0}};
}
