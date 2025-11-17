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

#ifndef ALU_CORE_H_
#define ALU_CORE_H_

// A minimal ALU-only core model with NO cache
// - Pure computational core (ALU operations only)
// - No memory hierarchy (loads/stores are not modeled)
// - Configurable ALU latency
// - SIMD support via global simdWidth parameter
// - Useful for modeling simple processing elements or compute-only accelerators

#include "core.h"
#include "pad.h"

class ALUCore : public Core {
    protected:
        uint64_t instrs;
        uint64_t curCycle;
        uint64_t phaseEndCycle; //next stopping point

        uint32_t aluLatency; //configurable ALU operation latency (cycles)

    public:
        // Constructor with configurable ALU latency
        ALUCore(g_string& _name, uint32_t _aluLatency = 1);
        void initStats(AggregateStat* parentStat);

        uint64_t getInstrs() const {return instrs;}
        uint64_t getPhaseCycles() const;
        uint64_t getCycles() const {return curCycle;}

        void contextSwitch(int32_t gid);
        virtual void join();

        InstrFuncPtrs GetFuncPtrs();

    protected:
        inline void bbl(BblInfo* bblInstrs);

        // Stubs for load/store (ALU core has NO memory operations)
        static void LoadFunc(THREADID tid, ADDRINT addr);
        static void StoreFunc(THREADID tid, ADDRINT addr);
        static void BblFunc(THREADID tid, ADDRINT bblAddr, BblInfo* bblInfo);
        static void PredLoadFunc(THREADID tid, ADDRINT addr, BOOL pred);
        static void PredStoreFunc(THREADID tid, ADDRINT addr, BOOL pred);

        static void BranchFunc(THREADID, ADDRINT, BOOL, ADDRINT, ADDRINT) {}
} ATTR_LINE_ALIGNED; //Cache line aligned to avoid false sharing

#endif  // ALU_CORE_H_
