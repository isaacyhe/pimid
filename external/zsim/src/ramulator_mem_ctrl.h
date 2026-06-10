#ifndef RAMULATOR_MEM_CTRL_H_
#define RAMULATOR_MEM_CTRL_H_

#include <map>
#include <string>
#include "g_std/g_string.h"
#include "memory_hierarchy.h"
#include "pad.h"
#include "stats.h"

namespace Ramulator { class IMemorySystem; class IFrontEnd; }

class RamulatorAccEvent;

class RamulatorMemory : public MemObject {
    private:
        g_string name;
        uint32_t minLatency;
        uint32_t domain;

        Ramulator::IMemorySystem* ramulatorSys;
        Ramulator::IFrontEnd* ramulatorFE;

        std::multimap<uint64_t, RamulatorAccEvent*> inflightRequests;

        uint64_t curCycle;

        PAD();
        Counter profReads;
        Counter profWrites;
        Counter profTotalRdLat;
        Counter profTotalWrLat;
        PAD();

    public:
        RamulatorMemory(const std::string& configFile,
                        uint64_t cpuFreqHz,
                        uint32_t _minLatency,
                        uint32_t _domain,
                        const g_string& _name);
        ~RamulatorMemory();

        const char* getName() { return name.c_str(); }
        void initStats(AggregateStat* parentStat);
        uint64_t access(MemReq& req);
        uint32_t tick(uint64_t cycle);
        void enqueue(RamulatorAccEvent* ev, uint64_t cycle);

    private:
        void completionCallback(RamulatorAccEvent* ev);
};

#endif  // RAMULATOR_MEM_CTRL_H_
