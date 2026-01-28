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
#include "zsim.h"

// ALU Core: Pure computational core with NO cache/memory hierarchy
// Designed for modeling simple processing elements or compute accelerators

ALUCore::ALUCore(g_string& _name, uint32_t _aluLatency) : Core(_name), aluLatency(_aluLatency) {
    instrs = 0;
    curCycle = 0;
    phaseEndCycle = 0;

    info("ALU Core '%s' initialized with ALU latency = %d cycles", name.c_str(), aluLatency);
}

uint64_t ALUCore::getPhaseCycles() const {
    return curCycle % zinfo->phaseLength;
}

void ALUCore::initStats(AggregateStat* parentStat) {
    AggregateStat* coreStat = new AggregateStat();
    coreStat->init(name.c_str(), "Core stats");

    auto x = [this]() -> uint64_t { return curCycle; };
    auto cyclesStat = makeLambdaStat(x);
    cyclesStat->init("cycles", "Simulated cycles");
    coreStat->append(cyclesStat);

    ProxyStat* instrsStat = new ProxyStat();
    instrsStat->init("instrs", "Simulated instructions", &instrs);
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

    // ALU-only model: cycles = instructions × ALU latency
    // This models a simple pipeline where each instruction takes aluLatency cycles
    curCycle += bblInfo->instrs * aluLatency;
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

// Load/Store stubs - ALU core has NO memory operations
// These will panic if ever called, as ALU core should not execute memory ops
void ALUCore::LoadFunc(THREADID tid, ADDRINT addr) {
    panic("ALU Core does not support memory loads! Core %d attempted load from 0x%lx",
          getCid(tid), addr);
}

void ALUCore::StoreFunc(THREADID tid, ADDRINT addr) {
    panic("ALU Core does not support memory stores! Core %d attempted store to 0x%lx",
          getCid(tid), addr);
}

void ALUCore::PredLoadFunc(THREADID tid, ADDRINT addr, BOOL pred) {
    if (pred) LoadFunc(tid, addr);
}

void ALUCore::PredStoreFunc(THREADID tid, ADDRINT addr, BOOL pred) {
    if (pred) StoreFunc(tid, addr);
}

// Return function pointers for Pin instrumentation
InstrFuncPtrs ALUCore::GetFuncPtrs() {
    return {LoadFunc, StoreFunc, BblFunc, BranchFunc, PredLoadFunc, PredStoreFunc, FPTR_ANALYSIS, {0}};
}
