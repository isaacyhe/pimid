/**
 * pe_memory_interface.h — PE Memory Interface
 *
 * Each PE-MI is bound to a group of PEs and covers a defined set of memory
 * units (banks, subarrays, etc.).  Accesses within coverage are served locally;
 * accesses outside trigger hierarchy traversal to the authoritative PE-MI.
 *
 * M/D/1 queuing is always active for bandwidth contention modeling.
 * (The old SimplePEMemoryController and MD1PEMemoryController are merged.)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PE_MEMORY_INTERFACE_H_
#define PE_MEMORY_INTERFACE_H_

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <list>
#include "g_std/g_string.h"
#include "garnet_network.h"
#include "memory_hierarchy.h"
#include "hierarchy_util.h"
#include "pad.h"
#include "stats.h"
#include "zsim.h"

// Per-PE LRU residency for WSS/capacity-gated near-data (SOFT limit): the PE's
// local store at the placement level holds `cap` lines; a resident line hits
// (served local), a miss evicts the LRU line and spills to the remote path.
// Driven by the real access stream, so subarray (small cap) is fastest only
// while the working set fits; large WSS -> capacity misses -> coarser wins.
struct PELocalCache {
    size_t cap;
    std::list<uint64_t> order;  // front = MRU, back = LRU
    std::unordered_map<uint64_t, std::list<uint64_t>::iterator> pos;
    explicit PELocalCache(size_t c) : cap(c ? c : 1) {}
    bool access(uint64_t key) {
        auto it = pos.find(key);
        if (it != pos.end()) { order.splice(order.begin(), order, it->second); return true; }
        order.push_front(key); pos[key] = order.begin();
        if (pos.size() > cap) { pos.erase(order.back()); order.pop_back(); }
        return false;  // miss
    }
};

/**
 * PE Memory Interface: coverage routing + hierarchy traversal + M/D/1 queuing.
 * Supports both contiguous [start, end) and non-contiguous coverage sets.
 */
class PEMemoryInterface : public MemObject {
protected:
    g_string name_;
    uint32_t mcId_;
    uint32_t coverageStart_;  // [start, end) unit range (contiguous fast path)
    uint32_t coverageEnd_;
    uint32_t totalUnits_;     // total units at placement level

    // Non-contiguous coverage support
    bool nonContiguous_ = false;
    std::vector<uint32_t> coverageSet_;  // sorted for binary search

    // Connection mode link latency
    uint32_t localLinkLat_ = 0;  // additional PE↔mem hop for SEPARATE_ENDPOINTS

    // Phase tracking for Garnet batch processing
    uint64_t batchLastPhase_ = 0;

    // M/D/1 queuing (always active)
    double maxRequestsPerCycle_;
    // TEMP (pre-arXiv): parallel server count for M/D/c queuing — the number of
    // DRAM channels serving requests concurrently (HBM3=16, HBM2=8, others=1).
    // Effective queuing load is divided by this so high-channel technologies stay
    // unsaturated under contention. A proper N-parallel-network model is future
    // work; this is a stop-gap. 1 = ordinary M/D/1.
    uint32_t numChannels_ = 1;
    uint32_t localLatency_;
    uint32_t curLatency_;
    uint64_t lastPhase_;
    double smoothedPhaseAccesses_;
    uint32_t curPhaseAccesses_;
    lock_t updateLock_;

    PAD();

    // Stats
    Counter profLocalAccesses_;
    Counter profRemoteAccesses_;
    Counter profLocalLatency_;
    Counter profRemoteLatency_;
    Counter profReads_;
    Counter profWrites_;

public:
    // Contiguous coverage
    PEMemoryInterface(uint32_t id, uint32_t start, uint32_t end,
                      uint32_t totalUnits, uint32_t localLatency,
                      uint32_t linkLat, uint64_t bandwidthMBs,
                      uint32_t lineSize, uint32_t freqMHz, g_string& name,
                      uint32_t numChannels = 1)
        : name_(name), mcId_(id),
          coverageStart_(start), coverageEnd_(end),
          totalUnits_(totalUnits),
          localLinkLat_(linkLat),
          numChannels_(numChannels >= 1 ? numChannels : 1),
          localLatency_(localLatency),
          curLatency_(localLatency),
          lastPhase_(0), smoothedPhaseAccesses_(0.0), curPhaseAccesses_(0)
    {
        futex_init(&updateLock_);
        double bytesPerCycle = (bandwidthMBs > 0) ? (bandwidthMBs * 1e6) / (freqMHz * 1e6) : 0.0;
        maxRequestsPerCycle_ = (bytesPerCycle > 0.0) ? bytesPerCycle / lineSize : 0.0;
    }

    // Non-contiguous coverage
    PEMemoryInterface(uint32_t id, const std::vector<uint32_t>& coverage,
                      uint32_t totalUnits, uint32_t localLatency,
                      uint32_t linkLat, uint64_t bandwidthMBs,
                      uint32_t lineSize, uint32_t freqMHz, g_string& name,
                      uint32_t numChannels = 1)
        : name_(name), mcId_(id),
          coverageStart_(0), coverageEnd_(0),
          totalUnits_(totalUnits),
          nonContiguous_(true), coverageSet_(coverage),
          localLinkLat_(linkLat),
          numChannels_(numChannels >= 1 ? numChannels : 1),
          localLatency_(localLatency),
          curLatency_(localLatency),
          lastPhase_(0), smoothedPhaseAccesses_(0.0), curPhaseAccesses_(0)
    {
        std::sort(coverageSet_.begin(), coverageSet_.end());
        if (!coverageSet_.empty()) {
            coverageStart_ = coverageSet_.front();
            coverageEnd_ = coverageSet_.back() + 1;
        }
        futex_init(&updateLock_);
        double bytesPerCycle = (bandwidthMBs > 0) ? (bandwidthMBs * 1e6) / (freqMHz * 1e6) : 0.0;
        maxRequestsPerCycle_ = (bytesPerCycle > 0.0) ? bytesPerCycle / lineSize : 0.0;
    }

    bool isLocalAddress(Address lineAddr) const {
        // Device-only near-data (option 1): route ALL of a PE's accesses through
        // the PE-MI (the placement-aware local path), not the ALU core's flat
        // direct path -- so every access shares one comparable cost model and the
        // in-coverage / out-of-coverage split does not confound placement levels.
        if (zinfo->hierarchy.assumeLocal) return false;
        uint32_t unit = addrToUnit(lineAddr);
        return isLocal(unit);
    }

    uint64_t access(MemReq& req) override {
        // Update coherence state
        switch (req.type) {
            case PUTS: case PUTX: *req.state = I; break;
            case GETS: *req.state = req.is(MemReq::NOEXCL) ? S : E; break;
            case GETX: *req.state = M; break;
        }

        uint32_t targetUnit = addrToUnit(req.lineAddr);

        // Perfect data prep. assumeLocal => the data ends up in the PE's local
        // unit, so an access takes the LOCAL placement-sensitive fast path
        // (applied here in the PE-MI cost branch, NOT in isLocal()/isLocalAddress()
        // which the ALU core uses for routing). In co-sim (chargePrep), the host's
        // de-interleave is paid on FIRST TOUCH of each line: the first access
        // routes through the REMOTE path (cross-unit reorg + host-device link via
        // the network levels), every reuse is local. So reuse / arithmetic
        // intensity decides whether fine placement's prep is worth its faster
        // compute. Device-only (chargePrep=false) assumes prep done -> always local.
        // WSS/capacity-gated near-data (SOFT limit). With perfect data prep
        // (assumeLocal) the PE's local store at the placement level holds C lines
        // (C = the level-unit capacity = pagesPerUnit pages). A RESIDENT line is
        // served LOCAL (placement-sensitive fast path); a capacity MISS spills to
        // the REMOTE path (fetched from other units; in co-sim it also re-pays the
        // host->device transfer). Residency is tracked per-PE from the REAL access
        // stream (LRU) -- so subarray (small C) is fastest only while the WSS fits,
        // and coarser placement (larger C) wins for large WSS. First touch = first
        // miss. Without assumeLocal: the static coverage test (no capacity model).
        bool wantLocal;
        if (zinfo->hierarchy.assumeLocal) {
            static thread_local PELocalCache* lcache = nullptr;
            static thread_local std::unordered_set<uint64_t>* preppedEver = nullptr;
            if (!lcache) {
                uint64_t cap = (uint64_t)zinfo->hierarchy.pagesPerUnit * 4096ull
                               / (zinfo->lineSize ? zinfo->lineSize : 64);
                lcache = new PELocalCache((size_t)cap);
                preppedEver = new std::unordered_set<uint64_t>();
            }
            // Co-sim: the first-EVER touch of a line is the ONE-TIME host->device
            // prep (reorganize + transfer into the device's contiguous space),
            // charged once. The device has a flat address space across units joined
            // by the in-device net, so later CAPACITY overflow is device-internal --
            // the data is already on the device, just in another unit -> reached via
            // the net (remote path), NOT re-transferred from the host.
            if (zinfo->hierarchy.chargePrep && preppedEver->insert((uint64_t)req.lineAddr).second)
                req.cycle += zinfo->hierarchy.hostLinkXferCycles;
            // Local-unit residency: hit = local near-data fast path; miss = capacity
            // overflow -> remote via the in-device net (the existing remote path).
            wantLocal = lcache->access((uint64_t)req.lineAddr);
        } else {
            wantLocal = isLocal(targetUnit);
        }
        if (wantLocal) {
            uint32_t lat = localAccessLatency(req) + localLinkLat_;

            // Near-data PROXIMITY-LATENCY gradient (perfect-prep, option A).
            // PHYSICS (grounded): a near-data PE reads operands through the COLUMN
            // datapath, whose per-unit rate is the channel rate at EVERY level --
            // prefetch is a tCCD-gated serializer, so the internal datapath is
            // BANDWIDTH-NEUTRAL (a wider row buffer does NOT give one unit more read
            // BW; egress == channel rate regardless of placement). Finer placement
            // is faster NOT because of a wider per-unit datapath, but because the
            // access is served CLOSER to the data and crosses LESS of the shared,
            // contended egress path before reaching a private datapath (fewer
            // arbitration/queueing cycles). The aggregate-BANDWIDTH win of fine
            // placement (bypassing the shared channel-DQ) is modeled SEPARATELY in
            // channelBandwidthWait(); THIS term is the always-charged proximity
            // LATENCY (not saturation-gated), so it shapes MLP-bound (unsaturated)
            // throughput -> a monotone placement gradient (subarray fastest).
            // serBase ~ one internal-hop latency; the per-level factor is the
            // monotone proximity weight (levels of shared egress between the
            // placement point and the data). Tunable via PIMID_LOCAL_SER_BASE
            // (default 8 cyc); the span is calibrated (8 -> ~2.1x subarray:rank).
            {
                static const uint32_t serBase = []() {
                    const char* e = getenv("PIMID_LOCAL_SER_BASE");
                    return (uint32_t)(e && e[0] ? atoi(e) : 8);
                }();
                uint32_t lvl = zinfo->hierarchy.placementLevel;
                // strictly monotone proximity weight (finer = closer to data, less
                // shared-egress contention = fewer cycles): subarray 1x (its own
                // open row) < bank 2x < bank-group 3x < rank 4x < channel 6x <
                // logic-die 8x (host interface, most shared / most distant).
                uint32_t f = (lvl == 0) ? 1u : (lvl == 1) ? 2u : (lvl == 2) ? 3u
                           : (lvl == 4) ? 4u : (lvl == 5) ? 6u : (lvl >= 6) ? 8u : 4u;
                lat += serBase * f;
            }

            // DRAM channel bandwidth bottleneck (accuracy fix): even a "local"
            // bank access consumes the shared DRAM channel's DQ bandwidth. In
            // detailed (cycle-accurate) mode, charge the shared M/D/c channel-BW
            // queueing wait so the aggregate effective DRAM BW is capped at the
            // datasheet value across ALL bank-MIs.
            {
                GarnetNetwork* gnLocal = zinfo->garnetNetwork;
                if (gnLocal && gnLocal->isCycleAccurate() &&
                    zinfo->hierarchy.nocAggBandwidthMBs > 0) {
                    lat += channelBandwidthWait();
                }
            }

            // Standalone MC: the MC fronting this memory is a SEPARATE NoC
            // endpoint (one per memory org), so even a local PE access must
            // cross the NoC to reach the MC (core → MC node → memory). Charge an
            // extra one-RTT NoC hop to the MC node on top of the local access.
            // Standalone MC: each memory org fronts a separate NoC endpoint, so a
            // PE access crosses the NoC to its MC node. This hop is the near-data
            // LATENCY differentiator: finer placement co-locates the PE with its
            // own MC (fewest hops -> fastest), coarser placement reaches a distant
            // shared MC (more hops). Keep it for device-only AND co-sim.
            if (zinfo->hierarchy.mcStandalone) {
                lat += mcHopLatency(targetUnit, req.cycle);
            }

            profLocalAccesses_.inc();
            profLocalLatency_.inc(lat);
            return req.cycle + lat;
        } else {
            // Remote: route through network to destination MI
            uint32_t myUnit = representativeUnit();
            uint32_t remoteLat = localAccessLatency(req);

            GarnetNetwork* gn = zinfo->garnetNetwork;
            if (gn && gn->isCycleAccurate()) {
                uint32_t srcNode = myUnit % gn->getNumNodes();
                uint32_t dstNode = targetUnit % gn->getNumNodes();
                uint32_t networkLat;
                // Synchronous shared-Garnet accounting (the ONLY detailed NoC):
                // record this access (brief batchLock_) on the SHARED Garnet -- the
                // real cross-thread-contended network -- and drain the batch on the
                // critical path at each phase boundary. The drain flag keeps the
                // watchdog from misreading the frozen phase clock as a fake-leave
                // stall.
                gn->recordBatchAccess(srcNode, dstNode, req.cycle);

                if (zinfo->numPhases > batchLastPhase_) {
                    __sync_fetch_and_add(&zinfo->hierarchy.nocInlineDrain, 1);
                    gn->processBatch(zinfo->numPhases, zinfo->phaseLength);
                    __sync_fetch_and_sub(&zinfo->hierarchy.nocInlineDrain, 1);
                    batchLastPhase_ = zinfo->numPhases;
                }

                // Use Garnet-measured latency (RTT); bootstrap with analytical.
                uint32_t garnetLat = gn->getBatchAvgLatency();
                networkLat = (garnetLat > 0)
                    ? 2 * garnetLat
                    : 2 * zinfo->hierarchy.nocAvgOneWayLatency;

                uint32_t totalLat = networkLat + remoteLat + 2 * localLinkLat_;
                // DRAM channel bandwidth bottleneck (accuracy fix): the H-tree
                // Garnet replays accesses at their original (load-spread) cycles
                // and resets per drain, so it never models the SHARED DRAM
                // channel saturating — detailed otherwise permits ~num_banks ×
                // per-MI BW. Add a shared M/D/c channel-BW queueing wait so
                // effective aggregate DRAM BW is capped at the datasheet value.
                if (zinfo->hierarchy.nocAggBandwidthMBs > 0) {
                    totalLat += channelBandwidthWait();
                }
                // Standalone MC: extra core → MC node hop on top of routing.
                if (zinfo->hierarchy.mcStandalone) {
                    totalLat += mcHopLatency(targetUnit, req.cycle);
                }
                profRemoteAccesses_.inc();
                profRemoteLatency_.inc(totalLat);
                return req.cycle + totalLat;
            }

            // ── Simple model: hierarchy traversal + serialization + contention ──
            // hierLat = per-tier hop-based latency through LCA path (no double-counting)
            uint32_t hierLat = (uint32_t)computeHierTraversal(
                myUnit, targetUnit,
                zinfo->hierarchy.levelLatency, zinfo->hierarchy.bridgeLatency,
                zinfo->hierarchy.placementLevel, zinfo->hierarchy.subarraysPerBank,
                zinfo->hierarchy.banksPerBG, zinfo->hierarchy.bgPerChip,
                zinfo->hierarchy.chipsPerRank, zinfo->hierarchy.ranksPerChannel);

            // Wormhole serialization: body flits pipeline behind head (1 cycle each)
            uint32_t serLat = (zinfo->hierarchy.nocFlitsPerPacket > 1)
                ? (zinfo->hierarchy.nocFlitsPerPacket - 1) : 0;

            // One-way = hierarchy traversal + serialization; RTT = 2×
            uint32_t networkLat = 2 * (hierLat + serLat);
            uint32_t nocContentionLat = networkContentionLatency();

            uint32_t totalLat;
            if (zinfo->hierarchy.nocMlpModel) {
                // ── Unified analytical model: hop-count + M/D/1 + MLP ──────────
                //   t_eff = max( (L + W_q) / M ,  P*D/c )
                //   L   = unloaded per-access latency (network RTT + DRAM + links)
                //         = networkLat + remoteLat + 2*localLinkLat_   [hop-count]
                //   W_q = M/D/1 contention wait (= nocContentionLat)   [M/D/1]
                //   M   = PE outstanding-access window (MLP); M=1 == 'simple'.
                //   P*D/c = aggregate-bandwidth floor: P PEs share c channels of
                //         deterministic service time D = line/perChanBW; per-PE
                //         floor = P / (aggregate lines-per-cycle). Prevents the
                //         /M overlap from exceeding the physical channel rate.
                // M=1, low load -> L+W_q (today's simple). M large -> bandwidth.
                uint32_t L = networkLat + remoteLat + 2 * localLinkLat_;
                uint32_t M = zinfo->hierarchy.nocMlpDegree;
                if (M < 1) M = 1;
                double overlapped = (double)(L + nocContentionLat) / (double)M;
                double bwFloor = 0.0;
                if (zinfo->hierarchy.nocAggBandwidthMBs > 0) {
                    uint32_t lvl_ = zinfo->hierarchy.placementLevel;
                    uint32_t bwmul_ = (lvl_ == 0) ? 4u : ((lvl_ == 1 || lvl_ == 2) ? 2u : 1u);
                    double aggBps = (double)zinfo->hierarchy.nocAggBandwidthMBs * 1e6 * bwmul_;
                    // Convert at the DEVICE clock, not the global (host) clock:
                    // in system-scope co-sim sys.frequency = max = host, so using
                    // zinfo->freqMHz here would scale the device's bandwidth floor
                    // with the host clock (the host-clock leak). 0 => not system
                    // scope -> freqMHz already IS the device clock.
                    double bwFreqMHz = (zinfo->hierarchy.nocBandwidthFreqMHz > 0)
                        ? (double)zinfo->hierarchy.nocBandwidthFreqMHz
                        : (double)zinfo->freqMHz;
                    double freqHz = bwFreqMHz * 1e6;
                    double lsz    = (zinfo->lineSize > 0) ? (double)zinfo->lineSize : 64.0;
                    double aggLinesPerCyc =
                        (freqHz > 0.0) ? (aggBps / freqHz) / lsz : 0.0;
                    double P = (double)((zinfo->numCores > 0) ? zinfo->numCores : 1);
                    if (aggLinesPerCyc > 1e-12) bwFloor = P / aggLinesPerCyc;
                }
                double te = (overlapped > bwFloor) ? overlapped : bwFloor;
                totalLat = (te >= 1.0) ? (uint32_t)(te + 0.5) : 1;
                // Env-gated term dump (PIMID_MLP_DIAG=1): aggregated means so
                // per-tech cross-checks against detailed are one grep away.
                if (mlpDiagEnabled()) {
                    static std::atomic<uint64_t> dN(0), dL(0), dNet(0), dRem(0),
                                                 dWq(0), dHier(0);
                    dN.fetch_add(1, std::memory_order_relaxed);
                    dL.fetch_add(L, std::memory_order_relaxed);
                    dNet.fetch_add(networkLat, std::memory_order_relaxed);
                    dRem.fetch_add(remoteLat, std::memory_order_relaxed);
                    dWq.fetch_add(nocContentionLat, std::memory_order_relaxed);
                    dHier.fetch_add(hierLat, std::memory_order_relaxed);
                    uint64_t n = dN.load(std::memory_order_relaxed);
                    if ((n & 0xFFFF) == 0) {
                        fprintf(stderr, "[MLPDIAG] n=%lu avgL=%.1f avgNet=%.1f "
                                "avgHier=%.1f avgRem=%.1f avgWq=%.1f M=%u "
                                "bwFloor=%.2f te=%.1f\n",
                                (unsigned long)n, (double)dL/n, (double)dNet/n,
                                (double)dHier/n, (double)dRem/n, (double)dWq/n,
                                M, bwFloor, te);
                    }
                }
            } else if (zinfo->hierarchy.nocCurveModel && zinfo->hierarchy.nocCurveN > 0) {
                // Curve model (2b): estimate the offered per-node load and
                // interpolate the latency-vs-load curve probed at init. Unlike
                // calibrated (zero-load only), this captures CONTENTION from the
                // measured curve. O(1) lookup.
                uint32_t curveLat = curveNetworkLatency();   // one-way network lat
                // Fix 1: stop double-counting the memory path on non-grid.
                // On non-grid topologies (H-tree/DRAM) the synthetic probe
                // traffic already traverses the network to the memory-org node,
                // so the probed curveLat already embeds that path; adding the
                // destination memory remoteLat on top inflates DRAM (calibrated
                // wins there precisely because it drops remoteLat). Keep
                // remoteLat only for grid (MESH/TORUS), where detailed is HIGHER
                // than analytical and the memory term helps close the gap.
                if (zinfo->hierarchy.nocCurveIsGrid) {
                    totalLat = curveLat + remoteLat + 2 * localLinkLat_;
                } else {
                    totalLat = curveLat + 2 * localLinkLat_;
                }
            } else if (zinfo->hierarchy.nocCalQueue && zinfo->hierarchy.nocCurveBaseLat > 0) {
                // Fix 1 (calqueue): use the probe-measured zero-load base L0
                // (accurate base, wins H-tree) but ADD BACK simple's M/D/1
                // contention + memory terms (wins MESH). i.e. simple's full
                // formula with L0 replacing the hop-count networkLat.
                totalLat = zinfo->hierarchy.nocCurveBaseLat + nocContentionLat
                         + remoteLat + 2 * localLinkLat_;
            } else if (zinfo->hierarchy.nocCurveBaseLat > 0) {
                // Calibrated model: the injector-measured per-access traversal
                // latency L0 (from a cycle-accurate probe of this topology)
                // replaces the analytical network estimate. Profiling shows L0 ~=
                // the detailed per-access remote latency, so this tracks detailed
                // far better than the analytical hop count, at simple-mode speed.
                //
                // Fix B (topology-aware): on GRID topologies (MESH/TORUS) the
                // detailed ground truth is HIGHER than this stripped base because
                // of real network contention; the stripped form under-shoots
                // MESH by ~50%. So for grids ADD BACK the M/D/1 contention and
                // memory terms (i.e. simple's full formula, with L0 as the base
                // when it is a usable positive value, else the analytical hop
                // networkLat). Non-grid (H-tree) is LEFT UNTOUCHED: detailed is
                // low there and the stripped base already fits (~25%). Guarded by
                // PIMID_CAL_GRIDQ (default ON) for in-binary A/B validation.
                if (calGridQueueEnabled() && zinfo->hierarchy.nocCurveIsGrid) {
                    // Use the probed L0 as the base if it is a real positive
                    // value; otherwise fall back to the analytical hop networkLat
                    // (so grid degenerates to simple's formula, the ~22% floor).
                    uint32_t gridBase = (zinfo->hierarchy.nocCurveBaseLat > 0)
                        ? zinfo->hierarchy.nocCurveBaseLat : networkLat;
                    totalLat = gridBase + nocContentionLat + remoteLat
                             + 2 * localLinkLat_;
                } else {
                    totalLat = zinfo->hierarchy.nocCurveBaseLat + 2 * localLinkLat_;
                }
            } else {
                totalLat = networkLat + nocContentionLat + remoteLat + 2 * localLinkLat_;
            }
            // Standalone MC: extra core → MC node hop on top of routing.
            if (zinfo->hierarchy.mcStandalone) {
                totalLat += mcHopLatency(targetUnit, req.cycle);
            }
            profRemoteAccesses_.inc();
            profRemoteLatency_.inc(totalLat);
            return req.cycle + totalLat;
        }
    }

    // Analytical-model term dump toggle (PIMID_MLP_DIAG=1). Diagnostic only.
    static bool mlpDiagEnabled() {
        static const bool en = []() {
            const char* e = getenv("PIMID_MLP_DIAG");
            return (e != nullptr) && (e[0] == '1');
        }();
        return en;
    }

    // detailed mode always drains the shared Garnet synchronously per phase
    // (see processBatch/runBatchDrain_); use noc.model=analytical when speed
    // matters more than cycle accounting.

    // Topology-aware calibrated toggle (Fix B). Default ON: on GRID topologies
    // the calibrated model ADDS the M/D/1 contention + memory terms back onto
    // the probed base L0 so it tracks MESH contention (old stripped form
    // under-shoots MESH ~50%). Set PIMID_CAL_GRIDQ=0 to restore the old
    // stripped calibrated behavior (for in-binary A/B validation).
    static bool calGridQueueEnabled() {
        static const bool en = []() {
            const char* e = getenv("PIMID_CAL_GRIDQ");
            return (e == nullptr) || (e[0] != '0');
        }();
        return en;
    }

    const char* getName() override { return name_.c_str(); }

    void initStats(AggregateStat* parentStat) override {
        AggregateStat* s = new AggregateStat();
        s->init(name_.c_str(), "PE memory interface stats");
        profLocalAccesses_.init("localAcc", "Local accesses (within coverage)");
        s->append(&profLocalAccesses_);
        profRemoteAccesses_.init("remoteAcc", "Remote accesses (hierarchy traversal)");
        s->append(&profRemoteAccesses_);
        profLocalLatency_.init("localLat", "Total local access latency cycles");
        s->append(&profLocalLatency_);
        profRemoteLatency_.init("remoteLat", "Total remote access latency cycles");
        s->append(&profRemoteLatency_);
        profReads_.init("rd", "Read requests");
        s->append(&profReads_);
        profWrites_.init("wr", "Write requests");
        s->append(&profWrites_);
        parentStat->append(s);
    }

protected:
    uint32_t localAccessLatency(MemReq& req) {
        // M/D/1 update on phase boundary (when bandwidth is configured)
        if (maxRequestsPerCycle_ > 0.0 && zinfo->numPhases > lastPhase_) {
            futex_lock(&updateLock_);
            if (zinfo->numPhases > lastPhase_) {
                double alpha = 0.5;
                smoothedPhaseAccesses_ = (1.0 - alpha) * smoothedPhaseAccesses_
                                         + alpha * (double)curPhaseAccesses_;
                curPhaseAccesses_ = 0;

                double load = smoothedPhaseAccesses_ /
                              (zinfo->phaseLength * maxRequestsPerCycle_);

                // TEMP (pre-arXiv) M/D/c stop-gap: numChannels_ independent DRAM
                // channels serve requests in parallel, so the offered load is
                // spread across c servers — effective per-server utilization is
                // load / c. High-channel technologies (HBM) therefore stay
                // unsaturated where a single-channel part (DDR) would queue.
                // Approximated by reducing the utilization that drives the
                // standard M/D/1 wait formula (a true M/D/c is future work).
                if (numChannels_ > 1) load /= (double)numChannels_;
                if (load > 0.95) load = 0.95;

                // M/D/1: E[T] = S + (rho * S) / (2 * (1 - rho))
                double svcTime = 1.0 / maxRequestsPerCycle_;
                double waitTime = (load > 0.01)
                    ? (load * svcTime) / (2.0 * (1.0 - load))
                    : 0.0;
                curLatency_ = localLatency_ + (uint32_t)(waitTime + 0.5);
                lastPhase_ = zinfo->numPhases;
            }
            futex_unlock(&updateLock_);
        }

        curPhaseAccesses_++;

        if (req.type == GETS || req.type == GETX) {
            profReads_.inc();
        } else {
            profWrites_.inc();
        }

        return curLatency_;
    }

    // Standalone-MC extra hop: the MC fronting memory org `targetUnit` is a
    // distinct NoC endpoint (one per memory org), so reaching it costs one NoC
    // RTT (core → MC node → and back) charged on top of the access. In
    // cycle-accurate mode this routes real packets through Garnet so the MC
    // traffic shows up in network contention; otherwise it is analytical.
    uint32_t mcHopLatency(uint32_t targetUnit, uint64_t cycle) {
        GarnetNetwork* gn = zinfo->garnetNetwork;
        if (gn && gn->isCycleAccurate()) {
            uint32_t srcNode = representativeUnit() % gn->getNumNodes();
            // MC node for memory org `targetUnit` is modeled as node `targetUnit`.
            uint32_t mcNode = targetUnit % gn->getNumNodes();
            gn->recordBatchAccess(srcNode, mcNode, cycle);
            if (zinfo->numPhases > batchLastPhase_) {
                __sync_fetch_and_add(&zinfo->hierarchy.nocInlineDrain, 1);
                gn->processBatch(zinfo->numPhases, zinfo->phaseLength);
                __sync_fetch_and_sub(&zinfo->hierarchy.nocInlineDrain, 1);
                batchLastPhase_ = zinfo->numPhases;
            }
            uint32_t garnetLat = gn->getBatchAvgLatency();
            return (garnetLat > 0)
                ? 2 * garnetLat
                : 2 * zinfo->hierarchy.nocAvgOneWayLatency;
        }
        // Simple model: one analytical NoC RTT to the MC node.
        return 2 * zinfo->hierarchy.nocAvgOneWayLatency;
    }

    uint32_t addrToUnit(Address lineAddr) const {
        // Device-local CONTIGUOUS block mapping (point-1 fix): each unit owns a
        // contiguous address range (pagesPerUnit 4KB-pages, sized by subarray
        // capacity x subarrays-per-unit), so a PE's contiguous working set lands
        // in its contiguous coverage and is served locally -- instead of the
        // host's page-interleaved layout (page % totalUnits) that scattered every
        // PE's data across all units (the flat-placement bug).
        uint32_t page = (uint32_t)(lineAddr >> 6);
        uint32_t ppu = (zinfo->hierarchy.pagesPerUnit > 0)
                       ? zinfo->hierarchy.pagesPerUnit : 1;
        return (page / ppu) % totalUnits_;
    }

    bool isLocal(uint32_t unit_id) const {
        if (!nonContiguous_) {
            return unit_id >= coverageStart_ && unit_id < coverageEnd_;
        }
        // Binary search for non-contiguous sets
        return std::binary_search(coverageSet_.begin(), coverageSet_.end(), unit_id);
    }

    uint32_t representativeUnit() const {
        return coverageStart_;
    }

    // ── Topology-aware network contention (shared across all PE-MIs) ──
    //
    // Three regimes matching cross-validated Garnet results:
    //  A) BUS: shared medium, M/M/1 with (1-ρ)^1.5 divergence
    //  B) CROSSBAR: per-output M/M/1 with HOL factor 1.58
    //  C) Multi-hop: per-channel M/M/1 × avgHops, hotspot-adjusted
    //
    // M/M/1 chosen over M/D/1 because wormhole blocking and backpressure
    // cascades create higher service time variance than deterministic.
    uint32_t networkContentionLatency() {
        static volatile uint32_t nocRemoteAccesses = 0;
        static volatile double   nocSmoothedRate = 0.0;
        static volatile uint64_t nocLastPhase = 0;
        static volatile uint32_t nocCurContentionLat = 0;
        static lock_t            nocLock = {0};

        __sync_fetch_and_add(&nocRemoteAccesses, 1);

        // Update on phase boundary
        if (zinfo->numPhases > nocLastPhase) {
            futex_lock(&nocLock);
            if (zinfo->numPhases > nocLastPhase) {
                double alpha = 0.5;
                double accesses = (double)nocRemoteAccesses;
                nocSmoothedRate = (1.0 - alpha) * nocSmoothedRate + alpha * accesses;
                nocRemoteAccesses = 0;

                uint32_t flitsPerPkt = zinfo->hierarchy.nocFlitsPerPacket;
                if (flitsPerPkt < 1) flitsPerPkt = 1;
                double svcTime = (double)flitsPerPkt;
                double N = (double)zinfo->hierarchy.nocNumNodes;
                if (N < 2.0) N = 2.0;
                double arrivalRate = nocSmoothedRate / (double)zinfo->phaseLength;

                // VC HOL-blocking reduction: 1/sqrt(VCs) factor (Dally & Towles)
                uint32_t vcs = zinfo->hierarchy.nocVcsPerVnet;
                double vcFactor = (vcs > 1) ? 1.0 / std::sqrt((double)vcs) : 1.0;

                double baseLatency = (double)zinfo->hierarchy.nocAvgOneWayLatency;
                if (baseLatency < 1.0) baseLatency = 1.0;

                uint32_t topoClass = zinfo->hierarchy.nocTopologyClass;
                double waitCycles = 0.0;

                // RATE-SEMANTICS FIX: arrivalRate above is the AGGREGATE
                // injection rate (total remote accesses/cycle across ALL
                // injecting PEs), NOT a per-node rate. The old code multiplied
                // it by N (total network endpoints, mostly passive memory
                // banks) as if it were per-node, inflating offered load by
                // N/P. Harmless when P ~= N (the calibrated mesh cells), but
                // on deep DRAM trees (4 PEs on a 256-endpoint HBM3 tree) it
                // overstated load ~64x and pinned W_q at the saturation clamp,
                // inverting the cross-tech ordering. All three branches now
                // use the aggregate rate directly.
                if (topoClass == 0) {
                    // BUS: every access crosses the single shared medium.
                    // Offered load = aggregate rate x service time.
                    // Steeper divergence: (1-rho)^1.5 for round-robin + HOL.
                    double rho = arrivalRate * svcTime;
                    if (rho >= 0.90) {
                        waitCycles = 10.0 * baseLatency;
                    } else if (rho > 0.01) {
                        double denom = std::pow(1.0 - rho, 1.5);
                        waitCycles = rho * svcTime / denom;
                    }
                } else if (topoClass == 1) {
                    // CROSSBAR: aggregate load spreads uniformly over N output
                    // ports; per-output M/M/1 with HOL factor 1.58.
                    double rhoPerOutput = (arrivalRate / N) * svcTime;
                    double rhoEff = std::min(rhoPerOutput * 1.58, 0.95);
                    if (rhoEff > 0.01) {
                        waitCycles = rhoEff * svcTime / (1.0 - rhoEff);
                    }
                } else {
                    // Multi-hop: per-channel M/M/1 × avgHops
                    double avgHops = (double)zinfo->hierarchy.nocAvgHopsTimes100 / 100.0;
                    if (avgHops < 0.01) avgHops = 0.01;
                    int channels = (int)zinfo->hierarchy.nocTotalChannels;
                    if (channels < 1) channels = 1;
                    double hotspot = (double)zinfo->hierarchy.nocHotspotFactor100 / 100.0;

                    // Aggregate flit-hops per cycle spread over all channels
                    double totalFlitHops = arrivalRate * svcTime * avgHops;
                    double rhoAvg = totalFlitHops / (double)channels;

                    // Bottleneck channel (hotspot-adjusted)
                    double rhoMax = rhoAvg * hotspot;

                    if (rhoMax >= 0.75) {
                        waitCycles = 10.0 * baseLatency;
                    } else if (rhoMax > 0.01) {
                        // M/M/1 per channel: E[W] = ρ×S/(1-ρ), with VC factor
                        double waitPerHop = vcFactor * rhoMax * svcTime / (1.0 - rhoMax);
                        waitCycles = waitPerHop * avgHops;
                    }
                }

                nocCurContentionLat = (uint32_t)(waitCycles + 0.5);
                nocLastPhase = zinfo->numPhases;
            }
            futex_unlock(&nocLock);
        }

        return nocCurContentionLat;
    }

    // ── Detailed-mode DRAM channel bandwidth bottleneck (accuracy fix) ──
    //
    // PROBLEM: in detailed mode each bank-MI runs an INDEPENDENT M/D/1 server at
    // its own per-MI bandwidth (defaultBandwidthMBs). With num_banks MIs (16),
    // the model permits ~num_banks × per-MI-BW aggregate (e.g. DDR4: 16×12.8 =
    // 204.8 GB/s) — ~10× the real DDR4-2400 single-channel 19.2 GB/s. The DRAM
    // CHANNEL (the shared 64-bit DQ bus) is the true bandwidth bottleneck and was
    // never modeled, so detailed under-contends and reports unrealistically high
    // effective DRAM bandwidth ("too optimistic").
    //
    // FIX: model the channel as a SHARED resource. All bank-MIs of a memory
    // technology share `c` channels (DDR4=1, HBM2=8, HBM3=16), each serving at
    // perChannelBW = aggBW / c. We keep a process-wide EWMA of the AGGREGATE
    // remote-access rate across ALL MIs and compute an M/D/c queueing wait that
    // diverges as the aggregate offered load approaches the aggregate channel
    // bandwidth. Added to every DRAM access in detailed mode, this caps the
    // effective aggregate DRAM bandwidth at the datasheet sustainable value.
    //
    // Only active when nocAggBandwidthMBs > 0 (DRAM techs) AND detailed cycle-
    // accurate mode (gated by the caller). Returns the queueing wait in cycles.
    uint32_t channelBandwidthWait() {
        static volatile uint64_t cbwAccesses = 0;     // aggregate accesses this phase
        static volatile double   cbwSmoothedRate = 0.0;  // EWMA accesses/phase
        static volatile uint64_t cbwLastPhase = 0;
        static volatile uint32_t cbwWait = 0;         // published wait cycles
        static lock_t            cbwLock = {0};

        // DEBUG: PIMID_NOC_CBW_FORCE=N forces a constant N-cycle wait per access
        // (bypasses the EWMA) to verify the wait actually reaches curCycle.
        {
            static const long forceW = []() {
                const char* e = getenv("PIMID_NOC_CBW_FORCE");
                return e && e[0] ? atol(e) : -1L;
            }();
            if (forceW >= 0) return (uint32_t)forceW;
        }

        __sync_fetch_and_add(&cbwAccesses, 1);

        if (zinfo->numPhases > cbwLastPhase) {
            futex_lock(&cbwLock);
            if (zinfo->numPhases > cbwLastPhase) {
                double alpha = 0.5;
                double acc = (double)cbwAccesses;
                cbwSmoothedRate = (1.0 - alpha) * cbwSmoothedRate + alpha * acc;
                cbwAccesses = 0;

                // Aggregate offered rate (lines/cycle across the whole device).
                double aggArrivalRate = cbwSmoothedRate / (double)zinfo->phaseLength;

                // Channel service rate (lines/cycle per channel).
                //   aggBW [B/s] -> bytes/cycle = aggBW / freqHz ; lines = /lineSize
                // Datasheet aggregate BW (MB/s); overridable via PIMID_NOC_AGGBW_MBS
                // for validation A/B (force-saturate to prove the cap engages).
                uint64_t aggMBs = zinfo->hierarchy.nocAggBandwidthMBs;
                // Near-data aggregate-BANDWIDTH uplift -- THE bandwidth win of fine
                // placement (the latency win is the separate proximity term above).
                // nocAggBandwidthMBs is the external channel-DQ datasheet ceiling:
                // the wall a HOST access hits. A near-data PE does NOT egress through
                // that shared DQ -- many fine units each drain their own open row in
                // parallel -- so the effective aggregate cap rises ABOVE the DQ
                // ceiling. Per-unit egress is still only the channel rate (prefetch
                // is BW-neutral); the gain is PARALLELISM, bounded by activation/
                // power limits (tFAW, refresh, shared global structures) -- i.e. the
                // ~4-16x realistic PIM regime (HBM-PIM measures ~4x), NOT the
                // page/tRC full-row figure (that is in-situ/Ambit compute, a
                // different machine). Conservative monotone uplift: subarray 4x,
                // bank/bank-group 2x, rank+ (still funnels to the DQ) 1x.
                {
                    uint32_t lvl = zinfo->hierarchy.placementLevel;
                    aggMBs *= (lvl == 0) ? 4u : ((lvl == 1 || lvl == 2) ? 2u : 1u);
                }
                {
                    const char* e = getenv("PIMID_NOC_AGGBW_MBS");
                    if (e && e[0]) { long long v = atoll(e); if (v > 0) aggMBs = (uint64_t)v; }
                }
                double aggBytesPerSec = (double)aggMBs * 1e6;
                // Device clock (not the global/host clock) for bytes->cycle. See
                // the matching note in the MLP bwFloor path: in system-scope
                // co-sim sys.frequency = host, so using it here would host-clock
                // the device channel-BW wait. 0 => freqMHz is already the device.
                double bwFreqMHz = (zinfo->hierarchy.nocBandwidthFreqMHz > 0)
                    ? (double)zinfo->hierarchy.nocBandwidthFreqMHz
                    : (double)zinfo->freqMHz;
                double freqHz = bwFreqMHz * 1e6;
                double lineSize = (double)zinfo->lineSize;
                if (lineSize < 1.0) lineSize = 64.0;
                double aggLinesPerCycle = (freqHz > 0.0)
                    ? (aggBytesPerSec / freqHz) / lineSize : 0.0;
                uint32_t c = zinfo->hierarchy.dramChannels;
                if (c < 1) c = 1;
                double perChanSvcRate = aggLinesPerCycle / (double)c;  // lines/cyc/channel

                double wait = 0.0;
                if (perChanSvcRate > 1e-9) {
                    double svcTime = 1.0 / perChanSvcRate;       // cyc/line per channel
                    // Per-channel utilization: aggregate load spread over c channels.
                    double rho = (aggArrivalRate / (double)c) / perChanSvcRate;
                    if (rho > 0.98) rho = 0.98;
                    // M/D/1 per channel: E[W] = ρ·S / (2·(1-ρ)). Diverges as the
                    // aggregate offered rate approaches c·perChanSvcRate (= aggBW),
                    // so steady-state throughput cannot exceed the channel BW.
                    if (rho > 0.01)
                        wait = (rho * svcTime) / (2.0 * (1.0 - rho));
                }
                cbwWait = (uint32_t)(wait + 0.5);
                cbwLastPhase = zinfo->numPhases;
                if (zinfo->numPhases <= 4 || zinfo->numPhases % 50 == 0)
                    info("[ChanBW] phase=%lu aggMBs=%lu c=%u offRate=%.5f "
                         "svcRate=%.5f rho=%.3f wait=%u",
                         zinfo->numPhases, (unsigned long)aggMBs, c,
                         aggArrivalRate, perChanSvcRate,
                         (perChanSvcRate>1e-9? (aggArrivalRate/(double)c)/perChanSvcRate : 0.0),
                         cbwWait);
            }
            futex_unlock(&cbwLock);
        }
        return cbwWait;
    }

    // ── Curve model (2b): load-dependent network latency by interpolation ──
    //
    // The init-time probe injected synthetic UNIFORM traffic at several
    // per-node injection rates and recorded (rate -> avg one-way latency).
    // Here we estimate the current offered per-node load and linearly
    // interpolate that curve.
    //
    // Offered-load estimate: like networkContentionLatency(), we keep a shared
    // EWMA of remote accesses per phase across all PE-MIs. The probe's
    // injectionRate is PER-NODE packets/cycle, so:
    //     perNodeRate = (smoothedRemoteAccesses / phaseLength) / numNodes
    // which is directly comparable to the probed rate axis.
    uint32_t curveNetworkLatency() {
        static volatile uint32_t curveRemoteAccesses = 0;
        static volatile double   curveSmoothedRate = 0.0;
        static volatile uint64_t curveLastPhase = 0;
        static volatile uint32_t curvePublishedLat = 0;
        static lock_t            curveLock = {0};

        __sync_fetch_and_add(&curveRemoteAccesses, 1);

        if (zinfo->numPhases > curveLastPhase) {
            futex_lock(&curveLock);
            if (zinfo->numPhases > curveLastPhase) {
                double alpha = 0.5;
                double accesses = (double)curveRemoteAccesses;
                curveSmoothedRate = (1.0 - alpha) * curveSmoothedRate + alpha * accesses;
                curveRemoteAccesses = 0;

                double N = (double)zinfo->hierarchy.nocNumNodes;
                if (N < 1.0) N = 1.0;
                double aggRate = curveSmoothedRate / (double)zinfo->phaseLength;
                double perNodeRate = aggRate / N;

                curvePublishedLat = (uint32_t)(interpCurve(perNodeRate) + 0.5);
                curveLastPhase = zinfo->numPhases;
            }
            futex_unlock(&curveLock);
        }

        // Bootstrap (phase 0, before any update): use zero-load point.
        if (curvePublishedLat == 0)
            return (uint32_t)(zinfo->hierarchy.nocCurveLat[0] + 0.5);
        return curvePublishedLat;
    }

    // Linear interpolation of the probed latency-vs-load curve. Clamps below the
    // lowest probed rate to L0 and above the highest probed rate to the last
    // (saturated) point — the curve is intentionally bounded at probe time.
    double interpCurve(double rate) const {
        uint32_t n = zinfo->hierarchy.nocCurveN;
        const double* R = zinfo->hierarchy.nocCurveRates;
        const double* L = zinfo->hierarchy.nocCurveLat;
        if (n == 0) return 0.0;
        if (n == 1 || rate <= R[0]) return L[0];
        if (rate >= R[n - 1]) return L[n - 1];
        for (uint32_t i = 1; i < n; ++i) {
            if (rate <= R[i]) {
                double t = (rate - R[i - 1]) / (R[i] - R[i - 1]);
                return L[i - 1] + t * (L[i] - L[i - 1]);
            }
        }
        return L[n - 1];
    }
};

#endif  // PE_MEMORY_INTERFACE_H_
