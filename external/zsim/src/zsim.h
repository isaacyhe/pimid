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

#ifndef ZSIM_H_
#define ZSIM_H_

#include <stdint.h>
#include <sys/time.h>
#include "constants.h"
#include "debug.h"
#include "locks.h"
#include "pad.h"

class Core;
class Scheduler;
class AggregateStat;
class StatsBackend;
class ProcessTreeNode;
class GarnetNetwork;
class ProcessStats;
class ProcStats;
class EventQueue;
class ContentionSim;
class EventRecorder;
class PortVirtualizer;
class VectorCounter;
class AccessTraceWriter;
class TraceDriver;
template <typename T> class g_vector;

struct ClockDomainInfo {
    uint64_t realtimeOffsetNs;
    uint64_t monotonicOffsetNs;
    uint64_t processOffsetNs;
    uint64_t rdtscOffset;
    lock_t lock;
};

class TimeBreakdownStat;
enum ProfileStates {
    PROF_INIT = 0,
    PROF_BOUND = 1,
    PROF_WEAVE = 2,
    PROF_FF = 3,
};

enum ProcExitStatus {
    PROC_RUNNING = 0,
    PROC_EXITED = 1,
    PROC_RESTARTME  = 2
};

struct GlobSimInfo {
    //System configuration values, all read-only, set at initialization
    uint32_t numCores;
    uint32_t lineSize;

    //Cores
    Core** cores;

    PAD();

    EventQueue* eventQueue;
    Scheduler* sched;

    //Contention simulation
    uint32_t numDomains;
    ContentionSim* contentionSim;
    EventRecorder** eventRecorders; //CID->EventRecorder* array

    PAD();

    //World-readable
    uint32_t phaseLength;
    uint32_t statsPhaseInterval;
    uint32_t freqMHz;

    //Maxima/termination conditions
    uint64_t maxPhases; //terminate when this many phases have been reached
    uint64_t maxMinInstrs; //terminate when all threads have reached this many instructions
    uint64_t maxTotalInstrs; //terminate when the aggregate number of instructions reaches this number
    uint64_t maxSimTimeNs; //terminate when the simulation time (bound+weave) exceeds this many ns
    uint64_t maxProcEventualDumps; //term if the number of heartbeat-triggered process dumps reached this (MP/MT)

    bool ignoreHooks;
    bool blockingSyscalls;
    bool perProcessCpuEnum; //if true, cpus are enumerated according to per-process masks (e.g., a 16-core mask in a 64-core sim sees 16 cores)
    bool oooDecode; //if true, Decoder does OOO (instr->uop) decoding
    uint32_t simdWidth; //SIMD vector width in bits (128, 256, 512), default 128 (SSE)

    PAD();

    //Writable, rarely read, unshared in a single phase
    uint64_t numPhases;
    uint64_t globPhaseCycles; //just numPhases*phaseCycles. It behooves us to precompute it, since it is very frequently used in tracing code.

    uint64_t procEventualDumps;

    PAD();

    ClockDomainInfo clockDomainInfo[MAX_CLOCK_DOMAINS];
    PortVirtualizer* portVirt[MAX_PORT_DOMAINS];

    lock_t ffLock; //global, grabbed in all ff entry/exit ops.

    volatile uint32_t globalActiveProcs; //used for termination
    //Counters below are used for deadlock detection
    volatile uint32_t globalSyncedFFProcs; //count of processes that are in synced FF
    volatile uint32_t globalFFProcs; //count of processes that are in either synced or unsynced FF

    volatile bool terminationConditionMet;

    const char* outputDir; //all the output files mst be dumped here. Stored because complex workloads often change dir, then spawn...

    AggregateStat* rootStat;
    g_vector<StatsBackend*>* statsBackends; // used for termination dumps
    StatsBackend* periodicStatsBackend;
    StatsBackend* eventualStatsBackend;
    ProcessStats* processStats;
    ProcStats* procStats;

    TimeBreakdownStat* profSimTime;
    VectorCounter* profHeartbeats; //global b/c number of processes cannot be inferred at init time; we just size to max

    uint64_t trigger; //code with what triggered the current stats dump

    ProcessTreeNode* procTree;
    ProcessTreeNode** procArray; //a flat view of the process tree, where each process is indexed by procIdx
    ProcExitStatus* procExited; //starts with all set to PROC_RUNNING, each process sets to PROC_EXITED or PROC_RESTARTME on exit. Used to detect untimely deaths (that don;t go thropugh SimEnd) in the harness and abort.
    uint32_t numProcs;
    uint32_t numProcGroups;

    // If true, threads start as shadow and have no effect on simulation until they call the register magic op
    bool registerThreads;

    //If true, do not output vectors in stats -- they're bulky and we barely need them
    bool skipStatsVectors;

    //If true, all the regular aggregate stats are summed before dumped, e.g. getting one thread record with instrs&cycles for all the threads
    bool compactPeriodicStats;

    bool attachDebugger;
    int harnessPid; //used for debugging purposes

    struct LibInfo libzsimAddrs;

    bool ffReinstrument; //true if we should reinstrument on ffwd, works fine with ST apps and it's faster since we run with basically no instrumentation, but it's not precise with MT apps

    //fftoggle stuff
    lock_t ffToggleLocks[256]; //f*ing Pin and its f*ing inability to handle external signals...
    lock_t pauseLocks[256]; //per-process pauses
    volatile bool globalPauseFlag; //if set, pauses simulation on phase end
    volatile bool externalTermPending;

    // Trace writers (stored globally because they need to be deleted when the simulation ends)
    g_vector<AccessTraceWriter*>* traceWriters;

    // Trace-driven simulation (no cores)
    bool traceDriven;
    TraceDriver* traceDriver;

    // Garnet network for device NoC simulation (PIMID integration)
    GarnetNetwork* garnetNetwork;

    // Garnet network for system-level inter-node network (separate instance)
    GarnetNetwork* systemGarnetNetwork = nullptr;

    // PIMID internal DRAM hierarchy (pre-computed latencies from config)
    struct {
        bool enabled = false;
        uint32_t placementLevel = 1;
        uint32_t subarraysPerBank = 4;
        uint32_t banksPerBG = 4;
        uint32_t bgPerChip = 4;
        uint32_t chipsPerRank = 8;
        uint32_t ranksPerChannel = 1;    // >1 for multi-rank (L5)
        uint32_t channelsPerSystem = 1;  // >1 for multi-device (L6)
        // TEMP (pre-arXiv): number of independent DRAM channels that serve memory
        // requests in PARALLEL (HBM3=16, HBM2=8, DDR/LPDDR/GDDR=1). Used as the
        // server count `c` in the per-PE-MI M/D/c queuing model so high-channel
        // technologies stay unsaturated under load. A proper N-parallel-Garnet-
        // trees model is future work; this scalar is a stop-gap. 1 = no effect.
        uint32_t dramChannels = 1;
        // Shared detailed-MPI Garnet: this rank's ROI baseline, exported by the
        // plugin (snapshotRoiBaseCyc) so the PE-MI publishes shared-NoC records
        // on the ROI-RELATIVE clock -- the established floor-free cross-rank
        // axis (1.3.x rendezvous fix). Absolute per-rank cycles are NOT
        // comparable across ranks (startup skew pins the merged-replay cut and
        // overruns the rings). 0/unset = not yet baselined: publish nothing.
        volatile uint32_t mpiNocBaselined = 0;
        volatile uint64_t mpiNocRoiBase = 0;
        // 1.9.0 thread-MPI: global phase index (numPhases) at the ROI baseline.
        // Epochs are keyed ROI-RELATIVE ((numPhases - this) / E_phases) so the
        // wall-dependent PRE-ROI length (rank spawning, init) cannot shift the
        // ROI's epoch boundaries run-to-run. Pre-ROI traffic is not recorded.
        volatile uint64_t mpiNocRoiBasePhase = 0;
        // Real datasheet AGGREGATE sustainable DRAM bandwidth (MB/s) =
        // per-channel × channels (from Ramulator getBandwidth()). The detailed
        // NoC model uses this to cap effective DRAM bandwidth at the channel
        // bottleneck via a shared M/D/c queue across all bank-MIs, so detailed
        // is an accurate DRAM ground truth (without it, num_banks independent
        // per-MI M/D/1 servers permit ~10× the real DDR4 channel BW). 0 = no
        // cap (pre-fix behavior / non-DRAM).
        uint64_t nocAggBandwidthMBs = 0;
        // 1.10.6: time to reverse the shared channel DQ bus, ns x100 (0 = off)
        uint32_t dqTurnNsX100 = 0;
        // Frequency (MHz) used to convert DEVICE memory bandwidth (bytes/s) into
        // bytes/cycle for the M/D/1 + bandwidth-floor contention terms. In a
        // standalone device-scope run this equals sys.frequency (the single
        // node IS the device). In a coupled system-scope co-sim sys.frequency =
        // max(all node freqs) = the HOST clock, so converting the device's
        // bandwidth with sys.frequency leaks the host clock into the device's
        // cycle count (a faster host shrinks bytes/cycle -> longer device
        // contention waits). This field carries the DEVICE's own clock so the
        // device memory timing is clocked at the device frequency, matching the
        // standalone device-scope result. 0 => fall back to zinfo->freqMHz
        // (legacy / non-system paths where freqMHz already IS the device clock).
        uint32_t nocBandwidthFreqMHz = 0;
        uint32_t levelLatency[7] = {};
        uint32_t bridgeLatency[6] = {};
        // Bridge model strings: "auto", "simple", "md1", "detailed"
        // Stored as uint32_t enum: 0=auto, 1=simple, 2=md1, 3=detailed
        uint32_t bridgeModel[6] = {};  // 0=auto (default)

        // Distributed PE-MI fields
        uint32_t totalUnits = 128;        // total units at placement level
        uint32_t pagesPerUnit = 32;       // contiguous block (4KB-pages) per unit (device-local addressing)
        bool assumeLocal = false;         // perfect data prep: device computes on its local working set
        bool chargePrep = false;          // co-sim: first touch of a line pays reorg (cross-unit) + transfer
        uint32_t hostLinkXferCycles = 0;  // per first-touch host->device link transfer (0 = internal/on-package)
        uint32_t totalMCs = 0;            // 0 = no PE-MIs (host MC mode)
        uint32_t pesPerMC = 1;            // PEs sharing each MI
        uint32_t localLatency = 10;       // default PE-MI local access latency
        uint64_t defaultBandwidthMBs = 0; // default M/D/1 bandwidth (0 = auto)

        // M:N PE-to-memory-org mapping
        uint32_t connectionMode = 0;     // 0=SHARED_IO, 1=SEPARATE_ENDPOINTS
        uint32_t localLinkLatency = 0;   // cycles for PE↔local-mem hop
        // MC placement: false = with_core (MC co-located with core/cache cluster),
        // true = standalone (MC is its own NoC endpoint, one per memory org). When
        // true, even a local PE access incurs an extra NoC hop to reach the MC.
        bool mcStandalone = false;
        uint32_t totalMemOrgs = 0;       // total mem orgs at placement level
        uint32_t totalNetworkEndpoints = 0;

        // NoC topology-aware average one-way latency (cycles)
        // = ceil(avgHops * perHop) + (flitsPerPacket - 1)  [includes serialization]
        uint32_t nocAvgOneWayLatency = 0;

        // NoC bisection bandwidth (in links) — kept for backward compat
        uint32_t nocBisectionLinks = 1;

        // NoC physical parameters for topology-aware simple model
        uint32_t nocFlitsPerPacket = 5;    // ceil(data_msg_bits / link_width_bits)
        uint32_t nocPerHopCycles = 2;      // router_lat + link_lat
        uint32_t nocLinkWidthBits = 128;   // link width = flit size in bits
        uint32_t nocVcsPerVnet = 4;        // VCs per virtual network
        uint32_t nocAvgHopsTimes100 = 100; // avgHops × 100 (fixed-point, avoids float in shm)
        uint32_t nocNumNodes = 16;         // total network nodes

        // Topology-aware contention model parameters
        // nocTopologyClass: 0=BUS, 1=CROSSBAR, 2=MULTI_HOP (ring/mesh/torus/tree)
        uint32_t nocTopologyClass = 2;
        uint32_t nocTotalChannels = 32;     // total unidirectional channels in network
        uint32_t nocHotspotFactor100 = 100; // hotspot factor x 100 (fixed-point)

        // -- "calibrated" NoC model (noc.model: calibrated) ------------------
        // nocInjectorCalib: 1 => at startup, probe the network at zero load with
        //   the cycle-accurate injector to measure the true per-access traversal
        //   latency L0, then the simple remote path uses L0 instead of the
        //   analytical estimate. 0 => plain analytical (simple) or detailed.
        // nocCurveBaseLat: the measured L0 (cycles, one-way). 0 => not calibrated;
        //   the remote path keeps its analytical latency. Gating everything on
        //   (nocCurveBaseLat > 0) keeps simple/detailed byte-identical.
        uint32_t nocInjectorCalib = 0;
        uint32_t nocCurveBaseLat = 0;

        // nocCalQueue: 1 => "calqueue" model (Fix 1). Probes L0 exactly like
        //   calibrated (gated on nocInjectorCalib), but at runtime uses
        //   L0 + M/D/1 contention + memory remoteLat (simple's full formula with
        //   L0 replacing the hop-count networkLat). 0 => not calqueue.
        uint32_t nocCalQueue = 0;

        // -- "mlp" NoC model (noc.model: mlp, noc.mlp: M) --------------------
        // Unified analytical model: hop-count latency L + M/D/1 contention W_q +
        // memory-level parallelism. Per-access effective latency:
        //   t_eff = max( (L + W_q) / M , P*D/c )
        // where M = PE outstanding-access window (MLP). M=1 reproduces 'simple'
        // (L + W_q); large M -> bandwidth floor P*D/c. NEW model, kept SEPARATE
        // from 'simple' for A/B validation against detailed before replacing it.
        uint32_t nocMlpModel  = 0;   // 1 => use the unified hop+M/D/1+MLP model
        uint32_t nocMlpDegree = 1;   // M, the outstanding-access window (>=1)

        // Set (via __sync) while an inline Garnet batch drain runs on a PE
        // thread (detailed mode's only accounting path). The phase clock
        // legitimately stands still during the drain; the scheduler watchdog
        // must not count those wall-clock ticks toward its fake-leave stall
        // heuristic, or it blacklists hot futex sites mid-drain and every
        // later wait pays a full leave/join (the "performance cliff").
        volatile uint32_t nocInlineDrain = 0;

        // -- "curve" NoC model (noc.model: curve) ----------------------------
        // nocCurveModel: 1 => at startup, probe the network at SEVERAL increasing
        //   injection rates (a latency-vs-load curve), store the (rate->avgLatency)
        //   points below, and at runtime estimate the offered per-node load and
        //   LINEARLY INTERPOLATE the curve for a load-dependent network latency.
        //   Unlike calibrated (zero-load only), curve keeps the memory remoteLat
        //   and captures contention via the measured curve. 0 => not curve.
        // nocCurveRates[i]/nocCurveLat[i]: the probed points (per-node injection
        //   rate -> measured round-trip-able one-way avg latency, cycles).
        // nocCurveN: number of valid points (after clamping/dropping garbage).
        enum { NOC_CURVE_MAX = 8 };
        uint32_t nocCurveModel = 0;
        uint32_t nocCurveN = 0;
        double   nocCurveRates[NOC_CURVE_MAX] = {};
        double   nocCurveLat[NOC_CURVE_MAX] = {};
        // nocCurveIsGrid: 1 if the probed topology is a grid (MESH_2D/TORUS_2D),
        //   0 for non-grid (H_TREE/RING/BUS/FAT_TREE/CROSSBAR/CUSTOM). Set at
        //   probe time (init.cpp). Curve fix-1: on non-grid the synthetic probe
        //   traffic already traverses to the memory-org node, so adding the
        //   destination memory remoteLat on top double-counts the memory path
        //   (inflates DRAM/H-tree). Drop remoteLat for non-grid; keep it for
        //   grid (where detailed > analytical and the memory term helps).
        uint32_t nocCurveIsGrid = 1;

        // Flattened mapping (shared-memory safe, no pointers)
        // peMemMapOffsets[pe] = start index in peMemMapData for PE pe
        // peMemMapOffsets[pe+1] - peMemMapOffsets[pe] = count of mem orgs for PE pe
        uint32_t peMemMapOffsets[MAX_THREADS + 1] = {};
        uint32_t peMemMapData[4096] = {};
        uint32_t peMemMapSize = 0;  // number of PEs in the map
    } hierarchy;

    // PCIe/CXL/NVLink host-device interconnect timing model
    struct {
        bool enabled = false;
        uint32_t baseLatencyCycles = 0;
        double bytesPerCycle = 0.0;
        uint32_t model = 0;              // 0=SIMPLE (includes M/D/1 queuing)
        uint32_t headerBytes = 20;       // protocol overhead per transaction
        uint32_t coherenceExtraCycles = 0; // average extra coherence latency
        // M/D/1 contention state (phase-based smoothing)
        volatile uint32_t curPhaseAccesses = 0;
        double smoothedPhaseRate = 0.0;
        volatile uint64_t lastPhase = 0;
        lock_t updateLock;
    } pcie;

    // Host<->device TWO-LAYER BRIDGE (1.7.1). Supersedes the flat pcie charge
    // in system-scope co-sim: phy layer = wire + command-interface/MC pipeline
    // latency (for HBM the low-clocked CA bus + pseudo-channel arbitration + MC
    // queueing -- NOT a SerDes/PHY); protocol layer = per-transaction overhead.
    // Deterministic charge (no phase M/D/1) so boundary costs reproduce exactly.
    struct {
        bool enabled = false;
        uint32_t phyLatencyCycles = 0;        // wire + command-interface/MC pipeline
        uint32_t protocolOverheadCycles = 0;  // per-transaction protocol handshake
        double   bytesPerCycle = 0.0;         // AGGREGATE bandwidth (per-ch x channels)
        uint32_t channels = 1;
        uint32_t uncachedCycles = 0;          // pure serialized cross-bridge access
    } bridge;

    // Case-1 COHERENCE flush accounting (1.7.2, HANDOFF ISSUE 3). In a unified
    // address space the PEs dereference host pointers, so at roi_begin (offload
    // start) the host must write back dirty INPUT lines (so the device sees
    // current data) and invalidate OUTPUT lines (so the host re-reads device
    // results). Analytic upper-bound charge on the HOST core before the device
    // clock starts: cycles = footprintBytes/writebackBytesPerCycle + flushFixed.
    // mode: 0 = unified (Case 1, flush) ; 1 = separate (Case 2, cache bypass, no
    // flush). NO_OFFLOAD baselines never enter the co-sim ROI block, so they are
    // never charged (self-zeroing).
    struct {
        bool enabled = false;
        uint32_t mode = 0;                    // 0=unified(flush), 1=separate(bypass)
        uint64_t footprintBytes = 0;          // input+output working-set (upper bound)
        double   writebackBytesPerCycle = 0.0;// host cache writeback BW @ ref clock
        uint32_t flushFixedCycles = 0;        // fixed flush/wbinvd latency
        // Stats (accumulated at each charged roi_begin flush)
        volatile uint64_t flushCount = 0;
        volatile uint64_t flushCyclesCharged = 0;
    } coherence;

    // Kernel LAUNCH cost tree (1.7.3, HANDOFF ISSUE 2). The host launches a
    // device kernel at the offload doorbell (co-sim roi_begin / WORK_BEGIN).
    // Real launch overhead = a user-mode doorbell write + cmd-packet formation
    // (NO syscall) + a HIP/CUDA-runtime dispatch software cost + a small cmd
    // packet crossing the bridge + a small ack packet returning. Charged on the
    // HOST core BEFORE the device migration, so it lands on the host inside the
    // task-region window. Deterministic (fixed cycle counts + deterministic
    // bridge crossings) so the boundary cost reproduces exactly.
    //   total = doorbellCycles + dispatchCycles
    //           + getBridgeLatency(cmdBytes) + getBridgeLatency(ackBytes)
    // Real GPU kernel-launch latency is famously ~5-20us (driver + dispatch);
    // the decomposed defaults (~300ns doorbell + ~5us dispatch + ~tens of ns of
    // bridge) sit at the low end of that band. NO_OFFLOAD baselines never enter
    // the co-sim offload block, so they are never charged (self-zeroing).
    struct {
        bool enabled = false;
        uint32_t doorbellCycles = 0;   // doorbell write + cmd-packet formation (user-mode, no syscall)
        uint32_t dispatchCycles = 0;   // HIP/CUDA-launch-analog runtime dispatch software cost
        uint32_t cmdBytes = 64;        // cmd packet size (host->device, crosses bridge)
        uint32_t ackBytes = 64;        // ack packet size (device->host, crosses bridge)
        // Stats (accumulated at each charged offload doorbell)
        volatile uint64_t launchCount = 0;
        volatile uint64_t launchCyclesCharged = 0;
    } launch;

    // Multi-host/multi-device node map
    struct {
        uint32_t numNodes = 0;
        struct NodeInfo {
            char name[64] = {};
            uint32_t role = 0;        // 0=host, 1=device
            uint32_t coreStart = 0;
            uint32_t coreEnd = 0;
            uint32_t freqMHz = 0;
            uint32_t networkNodeId = 0;
            uint64_t addrStart = 0;
            uint64_t addrEnd = 0;
        } nodes[16];
    } nodeMap;

    // System-level network (inter-node, for multi-host/device)
    struct {
        bool enabled = false;
        uint32_t numNodes = 0;
        uint32_t model = 0;          // 0=SIMPLE (includes M/D/1), 2=DETAILED
        uint32_t linkWidthBits = 512; // link width for serialization computation
        // Pre-computed inter-node latency matrix (in reference cycles)
        uint32_t linkLatency[256] = {};  // up to 16x16
        // Core-to-node mapping (which system network node is this core at)
        uint32_t coreToNode[2048] = {};
    } systemNetwork;

    // MPI timing statistics (unified in-process MPI)
    struct {
        volatile uint64_t messages = 0;
        volatile uint64_t totalLatency = 0;
        volatile uint64_t barrierCount = 0;
    } mpiStats;

    // Per-thread MPI parameter block addresses (registered via magic op)
    uint64_t mpiParamsAddr[MAX_THREADS] = {};
};


//Process-wide global variables, defined in zsim.cpp
extern Core* cores[MAX_THREADS]; //tid->core array
extern uint32_t procIdx;
extern uint32_t lineBits; //process-local for performance, but logically global
extern uint64_t procMask;

extern GlobSimInfo* zinfo;

//Process-wide functions, defined in zsim.cpp
uint32_t getCid(uint32_t tid);
uint32_t TakeBarrier(uint32_t tid, uint32_t cid);
void SimEnd(); //only call point out of zsim.cpp should be watchdog threads

#endif  // ZSIM_H_
