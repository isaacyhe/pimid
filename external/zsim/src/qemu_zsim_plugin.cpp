/**
 * QEMU TCG Plugin for ZSim In-Process Simulation
 *
 * Replaces zsim.cpp's Pin-based instrumentation with QEMU user-mode
 * emulation callbacks. Drives the full ZSim core/cache/memory hierarchy.
 *
 * This plugin is loaded by QEMU via:
 *   qemu-x86_64 -plugin libzsim_qemu.so,shmid=N,cfg=<path>,out=<dir> -- workload
 *
 * Architecture:
 *   - qemu_plugin_install() attaches shared memory, calls SimInit()
 *   - tb_trans_cb() creates BblInfo per translation block
 *   - mem_cb() calls Core::load/store via function pointers
 *   - insn_exec_cb() calls Core::bbl, triggers phase barriers
 *   - vcpu_init_cb() registers threads with scheduler
 *   - atexit_cb() calls SimEnd() and dumps stats
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <unordered_map>
#include <mutex>

/* QEMU plugin API (C interface) */
extern "C" {
#include "qemu/qemu-plugin.h"
QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;
}

/* ZSim headers (compiled with ZSIM_USE_QEMU, no Pin dependency) */
#include "alu_core.h"
#include "null_core.h"
#include "constants.h"
#include "contention_sim.h"
#include "core.h"
#include "decoder_simple.h"
#include "event_queue.h"
#include "galloc.h"
#include "garnet_network.h"
#include "hierarchy_util.h"
#include "init.h"
#include "log.h"
#include "pad.h"
#include "process_tree.h"
#include "profile_stats.h"
#include "scheduler.h"
#include "stats.h"
#include "zsim.h"
#include "x86_decoder.h"  // minimal x86-64 decoder -> DynUops for the real OOO path

/* ---- Globals (equivalent to zsim.cpp globals) ---- */

GlobSimInfo* zinfo;
uint32_t procIdx;
uint32_t lineBits;
uint64_t procMask;
Core* cores[MAX_THREADS];
InstrFuncPtrs fPtrs[MAX_THREADS] ATTR_LINE_ALIGNED;

/* tid→cid mapping */
#define INVALID_CID ((uint32_t)-1)
#define UNINITIALIZED_CID ((uint32_t)-2)
static uint32_t cids[MAX_THREADS];

static volatile uint32_t perProcessEndFlag;

/* BblInfo cache: translation block vaddr → BblInfo* */
static std::unordered_map<uint64_t, BblInfo*> bblCache;
static std::mutex bblCacheMutex;

/* True if any OOO core exists in this config. Only then do we run the x86
 * decoder to populate BblInfo->oooBbl with real DynUops (which ONLY the OOO
 * core reads). When no OOO core is present we keep the byte-identical synthetic
 * BblInfo path, so alu/simple/in_order/null behavior and sim speed are
 * unchanged. Set once in qemu_plugin_install after cores are constructed. */
static bool g_ooo_present = false;
static bool g_ooo_decode_disabled = false;  /* PIMID_OOO_NODECODE=1 escape hatch */

/* True if any genuine in-order core exists. The in-order pipeline (in_order_core)
 * consumes the SAME x86-decoded DynUop stream as the OOO core (its oooBbl), so we
 * must also run the decoder when an in-order core is present. PIMID_INORDER_NODECODE
 * disables that decode (in-order falls back to the legacy IPC=1 path), mirroring
 * PIMID_OOO_NODECODE. Only OOOCore and InOrderCore read oooBbl, so alu/simple/null
 * stay byte-identical. */
static bool g_inorder_present = false;
static bool g_inorder_decode_disabled = false;  /* PIMID_INORDER_NODECODE=1 */
/* Combined gate: decode TBs into DynUops when EITHER a (non-disabled) OOO or a
 * (non-disabled) in-order core is present. Set once in qemu_plugin_install. */
static bool g_decode_enabled = false;

/* PIMID_OOO_DUMP=1: profile which opcodes fall to the generic/approx uop path,
 * weighted by DYNAMIC execution count, to prioritize decoder coverage. The
 * per-TB approx-key list is stored in the TbUserdata (set at decode, read at
 * execution -> no lock), and dynamic hits accumulate into a lock-free atomic
 * array indexed by the packed decode signature. Printed at plugin exit. Zero
 * overhead when unset. */
static bool g_ooo_dump = false;
static bool g_ooo_nobranch = false;  /* PIMID_OOO_NOBRANCH=1: skip branch-pred feed */
#define OOO_DUMP_SLOTS (4 * 256 * 8)   /* [mapIdx:2][op:8][reg:3] */
static std::atomic<uint64_t> g_dumpDyn[OOO_DUMP_SLOTS];
static std::atomic<uint64_t> g_dumpTotalInsns{0};
static std::atomic<uint64_t> g_dumpApproxInsns{0};
/* Per-TB cache of approx-key arrays for the dump profiler (keyed by tbAddr).
 * Guarded by bblCacheMutex. Declared here so getOrCreateBblInfo can populate it. */
static std::unordered_map<uint64_t, std::pair<uint16_t*, uint32_t>> g_tbApproxCache;

/* Branch-predictor diagnostics: total dynamic conditional branches fed to the
 * OOO predictor (per-core mispredicts live in the OOO stats / zsim.out). */
static std::atomic<uint64_t> g_brDynCount{0};

/* Per-vcpu → ZSim THREADID mapping */
#define MAX_VCPUS 256
static uint32_t vcpuToTid[MAX_VCPUS];
static uint32_t nextTid = 0;
static std::mutex tidMutex;

/* Plugin state */
static qemu_plugin_id_t pluginId;
static char* cfgPath = nullptr;
static char* outputDir = nullptr;
static uint32_t shmId = 0;
static bool useSharedMem = false;

/* Internal thread filtering — vCPUs created during SimInit() are ZSim's
 * own threads (watchdog, contention sim) and must NOT be instrumented. */
static std::atomic<bool> sim_initialized{false};
static bool vcpu_is_internal[MAX_VCPUS];  // initialized to false in qemu_plugin_install

/* Guard flag: true while executing ZSim code (bblPtr/loadPtr/storePtr).
 * Prevents reentrant syscall_cb from corrupting scheduler/barrier state
 * if QEMU ever delivers a callback during ZSim function pointer dispatch. */
static bool in_zsim[MAX_VCPUS];

/* ROI state — set by mov $op, %rcx + xchg %rcx, %rcx (zsim_hooks.h magic ops) */
static std::atomic<bool> in_roi{true};              /* true = record; default on for non-ROI workloads */
static std::atomic<uint64_t> roi_transition_count{0};

/* ZSim magic op codes (must match zsim_hooks.h) */
#define ZSIM_MAGIC_OP_ROI_BEGIN     1025
#define ZSIM_MAGIC_OP_ROI_END       1026
#define ZSIM_MAGIC_OP_WORK_BEGIN    1029
#define ZSIM_MAGIC_OP_WORK_END      1030
#define ZSIM_MAGIC_OP_MPI_REGISTER  2048
#define ZSIM_MAGIC_OP_MPI_SEND      2049
#define ZSIM_MAGIC_OP_MPI_RECV      2050
#define ZSIM_MAGIC_OP_MPI_BARRIER   2051
#define ZSIM_MAGIC_OP_REG_DEVBUF    2052  /* register a device-owned buffer range (v1.1.1) */
#define ZSIM_MAGIC_OP_PRICE_PAUSE   2053  /* suspend address-routed pricing (DMA window) */
#define ZSIM_MAGIC_OP_PRICE_RESUME  2054  /* resume address-routed pricing */
#define ZSIM_MAGIC_OP_MPI_COMM_BEGIN 2055 /* open MPI comm window (keep core attached) */
#define ZSIM_MAGIC_OP_MPI_COMM_END   2056 /* close MPI comm window */
#define ZSIM_MAGIC_OP_MPI_CONTEND    2057 /* charge sender cross-rank contention wait (Phase 2) */

/* Per-thread MPI comm-window flag. While set (bracketed by COMM_BEGIN/END from
 * libpimid_mpi), the rank's blocking transport sleeps do NOT trigger a
 * syscallLeave (the core stays attached, so post-comm compute is counted) and
 * the transport's own instructions are NOT counted (so the wall-clock poll is
 * invisible and per-rank counts stay deterministic). Set only by the MPI
 * transport; OMP and co-sim paths never touch it, so their behavior is
 * byte-for-byte unchanged. */
static std::atomic<bool> mpi_comm_window[MAX_THREADS];

/* Per-rank ROI baseline synthesis (see handleMpiMagicOp): the MPI benchmarks
 * call zsim_roi_begin() ONLY on rank 0, so ranks 1..N-1 never snapshot an ROI
 * baseline and would report whole-program (init-dominated, non-scaling) cycles/
 * instrs. This flag lets the plugin synthesize a per-rank baseline at the first
 * communication op (~kernel entry) for any rank that never saw roi_begin. Each
 * MPI rank is its own process, so a process-global flag is per-rank. */
static bool mpi_roi_baselined = false;

/* Per-core simulated cycle captured at the ROI baseline (markRoiBegin instant).
 * The sim-time rendezvous stamps/compares send-times RELATIVE to this baseline
 * so cross-rank timing is a function of real post-baseline work only -- NOT of
 * the wall-clock-coupled globPhaseCycles floor each rank happens to carry when
 * it reaches its baseline (that floor differs per rank by scheduling jitter, and
 * leaking it into the rendezvous made the cross-rank critical path nondet). */
static uint64_t mpi_roi_base_cyc[MAX_THREADS] = {0};

/* Snapshot every core's ROI baseline cycle (call right after markRoiBegin). */
static inline void snapshotRoiBaseCyc() {
    for (uint32_t c = 0; c < zinfo->numCores && c < MAX_THREADS; c++)
        if (zinfo->cores[c]) mpi_roi_base_cyc[c] = zinfo->cores[c]->getCycles();
}
/* This core's simulated cycle measured from its ROI baseline (floor-free). */
static inline uint64_t roiRelCycles(uint32_t tid) {
    uint64_t now = cores[tid] ? cores[tid]->getCycles() : 0;
    return (now > mpi_roi_base_cyc[tid]) ? (now - mpi_roi_base_cyc[tid]) : 0;
}

/* Co-simulation: per-thread domain tracking and core-type masks */
enum SimDomain { DOMAIN_HOST = 0, DOMAIN_DEVICE = 1 };
static std::atomic<int> thread_domain[MAX_THREADS];    // default HOST
static bool thread_initialized[MAX_THREADS];            // false until ensureThreadInit
static g_vector<bool> host_mask;     // true for OOO cores
static g_vector<bool> device_mask;   // true for ALU cores
static std::atomic<uint64_t> offload_count{0};
/* Co-sim mode: a real host and a real device coexist; ROI = offload region. */
static bool g_cosim_mode = false;
/* Env-gated thread-lifecycle trace for the in-image startup-hang hunt
 * (PIMID_COSIM_TRACE=1): timestamps around scheduler start/join at thread
 * birth and offload migration. Zero overhead when unset. */
static bool g_cosim_trace = false;
#define COSIMTRACE(fmt, ...) do { if (g_cosim_trace) { \
    struct timespec _ts; clock_gettime(CLOCK_MONOTONIC, &_ts); \
    fprintf(stderr, "[cosim-trace %ld.%03ld] " fmt "\n", \
            (long)_ts.tv_sec, _ts.tv_nsec/1000000, ##__VA_ARGS__); } } while (0)
/* True while the process is inside a host->device offload region (between
 * WORK_BEGIN and WORK_END on the launching thread). Threads spawned during
 * this window are device workers and must be born DOMAIN_DEVICE so they map
 * to device PEs rather than host cores. */
static std::atomic<bool> g_in_device_region{false};

/* Per-vCPU pending magic op from mov $imm, %rcx — persists across TB
 * boundaries.  This handles the rare case where mov and xchg are in different
 * TBs (e.g., page boundary between them).  Per-vCPU to avoid races when
 * multiple vCPUs translate TBs concurrently under MTTCG. */
static std::atomic<uint64_t> pending_magic_op[MAX_THREADS];

/* ---- Runtime %rcx readback for magic ops ----
 *
 * The translation-time instruction decoder only recognizes mov-immediate
 * forms of the %rcx setup.  When zsim_work_begin_sized(size) is called with a
 * RUNTIME size, the compiler builds %rcx by arithmetic (salq/orq) instead of a
 * mov-immediate, so the decoder sees nothing and payload_size is lost.
 *
 * The general, correct source for the magic op value is the ACTUAL %rcx
 * register at the moment the xchg %rcx,%rcx executes.  We read it via
 * qemu_plugin_read_register(), which requires the exec callback to be
 * registered with QEMU_PLUGIN_CB_R_REGS.  The register handle is resolved once
 * (per-vCPU context) and cached; a reusable GByteArray avoids per-op alloc.
 *
 * NOTE: the vendored qemu-plugin.h in this tree (external/zsim/include) is a
 * minimal subset that predates the register-read API and does NOT pull in
 * <glib.h>, whose include dir is also absent from the build flags.  We are
 * constrained to edit only this .cpp, so we locally declare the small glib ABI
 * (matching system glib-2.0 struct layouts) and the QEMU register API.  These
 * symbols are provided at runtime by qemu-x86_64 (which links libglib-2.0 and
 * exports qemu_plugin_get_registers / qemu_plugin_read_register). */
extern "C" {
typedef struct PimidGArray { char* data; unsigned int len; } PimidGArray;
typedef struct PimidGByteArray { uint8_t* data; unsigned int len; } PimidGByteArray;
struct qemu_plugin_register;
typedef struct {
    struct qemu_plugin_register* handle;
    const char* name;
    const char* feature;
} pimid_qemu_plugin_reg_descriptor;

PimidGArray* qemu_plugin_get_registers(void);
int qemu_plugin_read_register(struct qemu_plugin_register* handle,
                              PimidGByteArray* buf);
PimidGByteArray* g_byte_array_sized_new(unsigned int reserved_size);
PimidGByteArray* g_byte_array_set_size(PimidGByteArray* array, unsigned int length);
char* g_array_free(PimidGArray* array, int free_segment);
}

#define PIMID_GARRAY_INDEX(a, t, i) \
    (((t*)(void*)(a)->data)[(i)])

static std::atomic<struct qemu_plugin_register*> rcx_handle{nullptr};
static std::atomic<struct qemu_plugin_register*> rdx_handle{nullptr};
static std::atomic<bool> rcx_lookup_done{false};
static __thread PimidGByteArray* rcx_buf = nullptr;  /* per-host-thread reusable buf */

/* Resolve and cache the %rcx register handle.  Must run in vCPU context (i.e.
 * from within an exec callback registered with QEMU_PLUGIN_CB_R_REGS).  Safe to
 * call repeatedly; only the first successful lookup populates the cache. */
static void resolveRcxHandle() {
    if (rcx_lookup_done.load(std::memory_order_acquire)) return;
    PimidGArray* regs = qemu_plugin_get_registers();
    if (regs) {
        for (unsigned int i = 0; i < regs->len; i++) {
            pimid_qemu_plugin_reg_descriptor* d =
                &PIMID_GARRAY_INDEX(regs, pimid_qemu_plugin_reg_descriptor, i);
            if (d->name && strcmp(d->name, "rcx") == 0) {
                rcx_handle.store(d->handle, std::memory_order_release);
            } else if (d->name && strcmp(d->name, "rdx") == 0) {
                rdx_handle.store(d->handle, std::memory_order_release);
            }
        }
        g_array_free(regs, 1 /* TRUE */);
    }
    rcx_lookup_done.store(true, std::memory_order_release);
}

/* Read the current vCPU's %rcx as a uint64_t.  Returns true and writes *out on
 * success; false if the handle is unavailable or the read fails.  x86-64 is
 * little-endian and qemu_plugin_read_register returns target byte order, so the
 * low byte is first in the buffer. */
static bool readGuestReg(struct qemu_plugin_register* h, uint64_t* out) {
    if (!h) return false;
    if (!rcx_buf) rcx_buf = g_byte_array_sized_new(16);
    g_byte_array_set_size(rcx_buf, 0);
    int n = qemu_plugin_read_register(h, rcx_buf);
    if (n <= 0) return false;
    uint64_t v = 0;
    int bytes = (n < 8) ? n : 8;
    for (int i = 0; i < bytes; i++) {
        v |= ((uint64_t)rcx_buf->data[i]) << (8 * i);
    }
    *out = v;
    return true;
}

static bool readRcx(uint64_t* out) {
    return readGuestReg(rcx_handle.load(std::memory_order_acquire), out);
}

/* Read %rdx — used to convey the MPI params-block address at MPI_REGISTER. */
static bool readRdx(uint64_t* out) {
    return readGuestReg(rdx_handle.load(std::memory_order_acquire), out);
}

/* NOP function pointers for unscheduled threads */
static void NopLoad(THREADID, ADDRINT) {}
static void NopStore(THREADID, ADDRINT) {}
static void NopBbl(THREADID, ADDRINT, BblInfo*) {}
static void NopBranch(THREADID, ADDRINT, BOOL, ADDRINT, ADDRINT) {}
static void NopPredLoad(THREADID, ADDRINT, BOOL) {}
static void NopPredStore(THREADID, ADDRINT, BOOL) {}

static InstrFuncPtrs nopPtrs = {NopLoad, NopStore, NopBbl, NopBranch,
                                 NopPredLoad, NopPredStore, FPTR_NOP, {0}};

/* Join function pointers — used when a thread is waiting to be scheduled */
static void JoinLoad(THREADID tid, ADDRINT addr);
static void JoinStore(THREADID tid, ADDRINT addr);
static void JoinBbl(THREADID tid, ADDRINT addr, BblInfo* bbl);

static InstrFuncPtrs joinPtrs = {JoinLoad, JoinStore, JoinBbl, NopBranch,
                                  NopPredLoad, NopPredStore, FPTR_JOIN, {0}};

/* ---- CID/Core management (mirrors zsim.cpp) ---- */

static inline void clearCid(uint32_t tid) {
    cids[tid] = INVALID_CID;
    cores[tid] = nullptr;
}

static inline void setCid(uint32_t tid, uint32_t cid) {
    cids[tid] = cid;
    cores[tid] = zinfo->cores[cid];
}

uint32_t getCid(uint32_t tid) {
    return cids[tid];
}

/* ---- Termination checking (mirrors zsim.cpp) ---- */

static void CheckForTermination() {
    if (zinfo->terminationConditionMet) return;

    if (zinfo->maxPhases && zinfo->numPhases >= zinfo->maxPhases) {
        zinfo->terminationConditionMet = true;
        info("Max phases reached (%ld)", zinfo->numPhases);
        return;
    }

    if (zinfo->maxTotalInstrs) {
        uint64_t totalInstrs = 0;
        for (uint32_t i = 0; i < zinfo->numCores; i++) {
            totalInstrs += zinfo->cores[i]->getInstrs();
        }
        if (totalInstrs >= zinfo->maxTotalInstrs) {
            zinfo->terminationConditionMet = true;
            info("Max total instructions reached (%ld)", totalInstrs);
            return;
        }
    }

    if (zinfo->maxMinInstrs) {
        uint64_t minInstrs = zinfo->cores[0]->getInstrs();
        for (uint32_t i = 1; i < zinfo->numCores; i++) {
            uint64_t ci = zinfo->cores[i]->getInstrs();
            if (ci < minInstrs && ci > 0) minInstrs = ci;
        }
        if (minInstrs >= zinfo->maxMinInstrs) {
            zinfo->terminationConditionMet = true;
            info("Max min instructions reached (%ld)", minInstrs);
            return;
        }
    }

    if (zinfo->externalTermPending) {
        zinfo->terminationConditionMet = true;
        info("Terminating due to external notification");
    }
}

/* ---- Phase actions (called by scheduler at end of phase) ---- */

void EndOfPhaseActions() {
    zinfo->profSimTime->transition(PROF_WEAVE);

    if (zinfo->globalPauseFlag) {
        info("Simulation entering global pause");
        zinfo->profSimTime->transition(PROF_FF);
        while (zinfo->globalPauseFlag) usleep(20 * 1000);
        zinfo->profSimTime->transition(PROF_WEAVE);
    }

    CheckForTermination();
    zinfo->contentionSim->simulatePhase(zinfo->globPhaseCycles + zinfo->phaseLength);

    zinfo->eventQueue->tick();
    zinfo->profSimTime->transition(PROF_BOUND);
}

/* ---- Barrier (phase synchronization) ---- */

uint32_t TakeBarrier(uint32_t tid, uint32_t cid) {
    uint32_t newCid = zinfo->sched->sync(procIdx, tid, cid);
    clearCid(tid);
    setCid(tid, newCid);

    if (zinfo->terminationConditionMet) {
        info("Termination condition met, exiting");
        zinfo->sched->leave(procIdx, tid, newCid);
        clearCid(tid);
        SimEnd();
        /* SimEnd calls _exit(0), so we normally never reach here.
         * Return INVALID_CID as safety net for callers' break logic. */
        return INVALID_CID;
    } else {
        fPtrs[tid] = cores[tid]->GetFuncPtrs();
    }

    return newCid;
}

/* Dump the dynamic opcode profile for instructions that fell to the generic/
 * approx uop path (PIMID_OOO_DUMP). Prints the top signatures by execution
 * count so decoder coverage can be prioritized empirically. */
static void dumpApproxProfile() {
    if (!g_ooo_dump) return;
    uint64_t tot = g_dumpTotalInsns.load();
    uint64_t apx = g_dumpApproxInsns.load();
    static const char* mapName[4] = {"1B ", "0F ", "0F38", "0F3A"};
    // Collect nonzero slots
    std::vector<std::pair<uint64_t,uint16_t>> v;
    for (uint32_t s = 0; s < OOO_DUMP_SLOTS; s++) {
        uint64_t c = g_dumpDyn[s].load();
        if (c) v.push_back(std::make_pair(c, (uint16_t)s));
    }
    std::sort(v.begin(), v.end(), [](const std::pair<uint64_t,uint16_t>&a,
                                     const std::pair<uint64_t,uint16_t>&b){return a.first>b.first;});
    info("==== OOO DECODE DUMP: %lu dynamic instrs, %lu approx (%.1f%%), %zu distinct approx opcodes ====",
         (unsigned long)tot, (unsigned long)apx,
         tot ? (100.0*apx/tot) : 0.0, v.size());
    uint32_t lim = v.size() < 80 ? (uint32_t)v.size() : 80;
    for (uint32_t k = 0; k < lim; k++) {
        int mi, op, reg; x86dec::dbgUnpack(v[k].second, mi, op, reg);
        info("  APPROX map=%s op=0x%02x /%d  dyn=%lu (%.2f%%)",
             mapName[mi & 3], op, reg, (unsigned long)v[k].first,
             tot ? (100.0*v[k].first/tot) : 0.0);
    }
    info("==== END OOO DECODE DUMP ====");
}

/* ---- SimEnd ---- */

void SimEnd() {
    dumpApproxProfile();
    if (g_ooo_present) {
        uint64_t mispred = 0;
        for (uint32_t c = 0; c < zinfo->numCores; c++)
            if (zinfo->cores[c] && zinfo->cores[c]->asOOOCore()) {
                // mispredBranches is a private OOO counter surfaced via stats;
                // aggregate from the stats tree is complex here, so report the
                // fed-branch total and let zsim.out carry per-core mispredicts.
            }
        (void)mispred;
        uint64_t br = g_brDynCount.load();
        info("==== OOO BRANCH PREDICTOR: %lu dynamic conditional branches resolved "
             "and fed to the predictor (per-core mispredBranches in zsim.out) ====",
             (unsigned long)br);
    }
    if (__sync_bool_compare_and_swap(&perProcessEndFlag, 0, 1) == false) {
        while (true) {
            struct timespec tm;
            tm.tv_sec = 1;
            tm.tv_nsec = 0;
            nanosleep(&tm, nullptr);
        }
    }

    info("Dumping termination stats");
    zinfo->trigger = 20000;
    for (StatsBackend* backend : *(zinfo->statsBackends)) {
        backend->dump(false);
    }

    if (zinfo->garnetNetwork) {
        zinfo->garnetNetwork->setTotalCycles(zinfo->globPhaseCycles);
        std::string garnetStatsPath = std::string(zinfo->outputDir) + "/garnet_stats.txt";
        zinfo->garnetNetwork->writeStatsFile(garnetStatsPath.c_str());
        zinfo->garnetNetwork->printStats();
    }

    if (zinfo->mpiStats.messages > 0) {
        info("MPI Stats: %lu messages, %lu total latency cycles, %lu barriers",
             zinfo->mpiStats.messages, zinfo->mpiStats.totalLatency,
             zinfo->mpiStats.barrierCount);
    }

    if (zinfo->sched) zinfo->sched->notifyTermination();

    /* Terminate the process.  Background threads (contention sim, scheduler
     * watchdog) won't exit on their own, same as the original ZSim behaviour.
     * Use _exit() rather than exit() to avoid re-entering atexit/plugin_exit. */
    _exit(0);
}

/* ---- Thread management ---- */

static void SimThreadStart(uint32_t tid) {
    info("Thread %d starting", tid);
    if (tid > MAX_THREADS) {
        panic("tid > MAX_THREADS");
    }
    ProcessTreeNode* procTree = zinfo->procArray[procIdx];
    zinfo->sched->start(procIdx, tid, procTree->getMask());

    fPtrs[tid] = joinPtrs;
    clearCid(tid);
}

static void SimThreadFini(uint32_t tid) {
    zinfo->sched->finish(procIdx, tid);
    cids[tid] = UNINITIALIZED_CID;
}

/**
 * Deferred thread initialization — called on first execution callback.
 * Uses the thread's domain to select the appropriate core-type mask.
 */
static void ensureThreadInit(uint32_t tid) {
    if (thread_initialized[tid]) return;
    thread_initialized[tid] = true;

    const g_vector<bool>& mask =
        (thread_domain[tid].load() == DOMAIN_DEVICE) ? device_mask : host_mask;

    info("Thread %d starting (domain=%s)", tid,
         thread_domain[tid].load() == DOMAIN_DEVICE ? "DEVICE" : "HOST");

    if (tid >= MAX_THREADS) {
        panic("tid >= MAX_THREADS");
    }
    COSIMTRACE("tid=%u sched->start enter (domain=%s)", tid,
               thread_domain[tid].load() == DOMAIN_DEVICE ? "DEV" : "HOST");
    zinfo->sched->start(procIdx, tid, mask);
    COSIMTRACE("tid=%u sched->start done", tid);
    fPtrs[tid] = joinPtrs;
    clearCid(tid);
}

/* ---- Join functions (thread waits for scheduler) ---- */

static std::atomic<uint32_t> trace_join_count[MAX_THREADS];

static void Join(uint32_t tid) {
    /* trace only each thread's first few joins -- the startup hang window */
    bool tr = g_cosim_trace && tid < MAX_THREADS && trace_join_count[tid]++ < 6;
    if (tr) COSIMTRACE("tid=%u sched->join enter", tid);
    uint32_t cid = zinfo->sched->join(procIdx, tid);
    if (tr) COSIMTRACE("tid=%u sched->join done cid=%u", tid, cid);
    setCid(tid, cid);

    if (unlikely(zinfo->terminationConditionMet)) {
        info("Caught termination on join, exiting");
        zinfo->sched->leave(procIdx, tid, cid);
        SimEnd();
    }

    fPtrs[tid] = cores[tid]->GetFuncPtrs();
}

static void JoinLoad(THREADID tid, ADDRINT addr) {
    Join(tid);
    fPtrs[tid].loadPtr(tid, addr);
}

static void JoinStore(THREADID tid, ADDRINT addr) {
    Join(tid);
    fPtrs[tid].storePtr(tid, addr);
}

static void JoinBbl(THREADID tid, ADDRINT addr, BblInfo* bbl) {
    Join(tid);
    fPtrs[tid].bblPtr(tid, addr, bbl);
}

/* ---- BblInfo cache ---- */

static BblInfo* getOrCreateBblInfo(uint64_t tbAddr, uint32_t numInsns, uint32_t tbBytes,
                                   struct qemu_plugin_tb* tb) {
    std::lock_guard<std::mutex> lock(bblCacheMutex);

    auto it = bblCache.find(tbAddr);
    if (it != bblCache.end()) {
        return it->second;
    }

    BblInfo* bbl = nullptr;
    /* Decode into DynUops only when an OOO core is present (its oooBbl is the
     * only reader). Cap TB size to keep the transient decode buffers bounded;
     * oversized TBs fall back to the synthetic path (they are rare and
     * typically not hot compute loops). */
    if (g_decode_enabled && tb && numInsns > 0 &&
            numInsns <= 1024) {
        static const uint32_t MAXI = 1024;
        // Local per-call buffers (each thread translates independently; this is
        // on the stack, ~17KB, acceptable). Gather bytes+lengths for the decoder.
        static thread_local uint8_t bytesBuf[MAXI][16];
        static thread_local uint8_t lenBuf[MAXI];
        uint32_t n = numInsns;
        for (uint32_t k = 0; k < n; k++) {
            struct qemu_plugin_insn* insn = qemu_plugin_tb_get_insn(tb, k);
            size_t sz = qemu_plugin_insn_size(insn);
            if (sz > 16) sz = 16;
            lenBuf[k] = (uint8_t)sz;
            qemu_plugin_insn_data(insn, bytesBuf[k], 16);
        }
        if (g_ooo_dump) {
            std::vector<uint16_t> keys;
            bbl = x86dec::createDecodedBblInfo(tbAddr, bytesBuf, lenBuf, n, tbBytes, &keys);
            uint16_t* arr = nullptr;
            if (!keys.empty()) {
                arr = (uint16_t*)malloc(keys.size() * sizeof(uint16_t));
                memcpy(arr, keys.data(), keys.size() * sizeof(uint16_t));
            }
            g_tbApproxCache[tbAddr] = std::make_pair(arr, (uint32_t)keys.size());
        } else {
            bbl = x86dec::createDecodedBblInfo(tbAddr, bytesBuf, lenBuf, n, tbBytes);
        }
    } else {
        bbl = createSimpleBblInfo(numInsns, tbBytes);
    }
    bblCache[tbAddr] = bbl;
    return bbl;
}

/* ---- QEMU vcpu → ZSim tid mapping ---- */

static uint32_t getOrAssignTid(unsigned int vcpu_index) {
    if (vcpu_index < MAX_VCPUS && vcpuToTid[vcpu_index] != UNINITIALIZED_CID) {
        return vcpuToTid[vcpu_index];
    }

    std::lock_guard<std::mutex> lock(tidMutex);
    /* Double-check under lock */
    if (vcpu_index < MAX_VCPUS && vcpuToTid[vcpu_index] != UNINITIALIZED_CID) {
        return vcpuToTid[vcpu_index];
    }

    uint32_t tid = nextTid++;
    if (vcpu_index < MAX_VCPUS) {
        vcpuToTid[vcpu_index] = tid;
    }
    return tid;
}

/* ---- Userdata struct passed from tb_trans to callbacks ---- */

struct TbUserdata {
    BblInfo* bblInfo;
    uint64_t tbAddr;
    /* PIMID_OOO_DUMP only: packed signatures of this TB's approx instructions,
     * plus its total instruction count, for dynamic opcode profiling. */
    uint16_t* approxKeys;
    uint32_t  numApproxKeys;
    uint32_t  numInsns;
    /* Branch-predictor wiring (OOO only): if this TB ends in a conditional
     * jcc, its PC and the two successor addresses, so the real taken/not-taken
     * direction can be resolved from the actual next TB and fed to the OOO
     * core's branch predictor. endsInCondBranch=false otherwise. */
    bool      endsInCondBranch;
    uint64_t  brPc;
    uint64_t  brTakenTarget;
    uint64_t  brFallthrough;
};

/* Per-thread pending conditional branch awaiting direction resolution (set when
 * a jcc-terminated TB executes, resolved by the NEXT TB's address). OOO only. */
static bool     g_brPending[MAX_THREADS];
static uint64_t g_brPc[MAX_THREADS];
static uint64_t g_brTaken[MAX_THREADS];
static uint64_t g_brFall[MAX_THREADS];

/* ---- QEMU Callbacks ---- */

/**
 * Memory access callback — dispatches to ZSim's load/store function pointers.
 */
static void mem_cb(unsigned int vcpu_index,
                   qemu_plugin_meminfo_t info,
                   uint64_t vaddr,
                   void *userdata) {
    if (!in_roi) return;
    if (vcpu_index < MAX_VCPUS && vcpu_is_internal[vcpu_index]) return;

    uint32_t tid = getOrAssignTid(vcpu_index);
    if (tid >= MAX_THREADS) return;
    ensureThreadInit(tid);

    /* Inside an MPI comm window: skip counting the transport's own accesses
     * (keeps the core attached but the wall-clock poll invisible). */
    if (mpi_comm_window[tid].load(std::memory_order_acquire)) return;

    if (vcpu_index < MAX_VCPUS) in_zsim[vcpu_index] = true;
    if (qemu_plugin_mem_is_store(info)) {
        fPtrs[tid].storePtr(tid, vaddr);
    } else {
        fPtrs[tid].loadPtr(tid, vaddr);
    }
    if (vcpu_index < MAX_VCPUS) in_zsim[vcpu_index] = false;
}

/**
 * Instruction execution callback — called on first instruction of each TB.
 * Drives the BBL function pointer which triggers phase counting.
 */
static void insn_exec_cb(unsigned int vcpu_index, void *userdata) {
    if (!in_roi) return;
    if (vcpu_index < MAX_VCPUS && vcpu_is_internal[vcpu_index]) return;

    TbUserdata* tud = (TbUserdata*)userdata;
    uint32_t tid = getOrAssignTid(vcpu_index);
    if (tid >= MAX_THREADS) return;
    ensureThreadInit(tid);

    /* Inside an MPI comm window: keep the core attached but do not count the
     * transport's own instructions (the wall-clock poll stays invisible). */
    if (mpi_comm_window[tid].load(std::memory_order_acquire)) return;

    /* Dynamic opcode profiling (PIMID_OOO_DUMP): tally this TB's approx insns
     * weighted by execution count. Lock-free (per-TB array + atomic slots). */
    if (unlikely(g_ooo_dump)) {
        g_dumpTotalInsns.fetch_add(tud->numInsns, std::memory_order_relaxed);
        g_dumpApproxInsns.fetch_add(tud->numApproxKeys, std::memory_order_relaxed);
        for (uint32_t k = 0; k < tud->numApproxKeys; k++) {
            uint16_t key = tud->approxKeys[k];
            if (key < OOO_DUMP_SLOTS)
                g_dumpDyn[key].fetch_add(1, std::memory_order_relaxed);
        }
    }

    if (vcpu_index < MAX_VCPUS) in_zsim[vcpu_index] = true;
    /* OOO branch predictor: resolve the previous TB's conditional branch using
     * THIS TB's address as the real next-PC, then feed direction+targets to the
     * core BEFORE bbl() (bbl() consumes branchPc when timing the prev BBL). Only
     * OOO cores get branch callbacks, so other core types stay byte-identical. */
    if (g_ooo_present && !g_ooo_nobranch && cores[tid] && cores[tid]->asOOOCore()) {
        if (g_brPending[tid]) {
            bool taken = (tud->tbAddr == g_brTaken[tid]);
            fPtrs[tid].branchPtr(tid, g_brPc[tid], taken ? 1 : 0,
                                 g_brTaken[tid], g_brFall[tid]);
            g_brDynCount.fetch_add(1, std::memory_order_relaxed);
        }
        if (tud->endsInCondBranch) {
            g_brPending[tid] = true;
            g_brPc[tid] = tud->brPc;
            g_brTaken[tid] = tud->brTakenTarget;
            g_brFall[tid] = tud->brFallthrough;
        } else {
            g_brPending[tid] = false;
        }
    }
    fPtrs[tid].bblPtr(tid, tud->tbAddr, tud->bblInfo);
    if (vcpu_index < MAX_VCPUS) in_zsim[vcpu_index] = false;
}

/**
 * Handle an MPI magic op for the given thread.
 * Computes NoC + hierarchy latency and injects it as a synthetic BBL.
 */
static void handleMpiMagicOp(uint64_t op, uint32_t tid) {
    if (tid >= MAX_THREADS) return;

    if (op == ZSIM_MAGIC_OP_MPI_REGISTER) {
        /* Registration: the caller put the address of its thread-local
         * PimidMpiParams block in %rdx (opcode is in %rcx).  Record it so
         * subsequent MPI_SEND/RECV can read {src_pe, dst_pe, msg_size} and
         * charge the device NoC.  Guest and host share the address space in
         * QEMU user-mode, so the host-side pointer is directly usable. */
        uint64_t addr = 0;
        if (zinfo && readRdx(&addr) && addr != 0) {
            zinfo->mpiParamsAddr[tid] = addr;
            info("Thread %d: MPI_REGISTER (params@%p)", tid, (void*)addr);
        } else {
            info("Thread %d: MPI_REGISTER (no params addr)", tid);
        }
        return;
    }

    if (op == ZSIM_MAGIC_OP_MPI_COMM_BEGIN) {
        /* Open the comm window: keep the core attached across transport sleeps
         * and stop counting the transport's own instructions until COMM_END. */
        mpi_comm_window[tid].store(true, std::memory_order_release);
        return;
    }
    if (op == ZSIM_MAGIC_OP_MPI_COMM_END) {
        mpi_comm_window[tid].store(false, std::memory_order_release);
        return;
    }

    if (op == ZSIM_MAGIC_OP_REG_DEVBUF) {
        /* Retired (the device model owns its addresses; nothing to register).
         * Kept as an explicit no-op so old binaries don't fall through to
         * the MPI send/recv params path. */
        return;
    }

    if (op == ZSIM_MAGIC_OP_PRICE_PAUSE || op == ZSIM_MAGIC_OP_PRICE_RESUME) {
        /* DMA window: an explicit staging memcpy is about to run whose
         * transfer cost is charged as a sized link transfer -- suspend
         * per-access pricing so the copy loop is not double-charged. */
        if (zinfo && cores[tid]) {
            cores[tid]->setMemPricingPaused(op == ZSIM_MAGIC_OP_PRICE_PAUSE);
        }
        return;
    }

    if (op == ZSIM_MAGIC_OP_MPI_BARRIER) {
        /* Per-rank ROI baseline (collective-first benchmarks). Some MPI
         * kernels (e.g. bfs) reach their communication ONLY through
         * collectives/barriers -- their frontier exchange is Allgather/
         * Allgatherv (no MPI_Send/Recv), so the SEND/RECV baseline further
         * down never fires and ranks 1..N-1 keep the ~40M MPI_Init wall-clock
         * floor. Synthesize the baseline here too, on the FIRST barrier the
         * plugin observes. Both bfs and stencil place a sync MPI_Barrier at
         * kernel entry, immediately before rank 0's zsim_roi_begin(), so this
         * lands at ~kernel entry (post-init: MPI_Init emits no barrier magic
         * op, only MPI_REGISTER). rank 0 hits that same barrier first but then
         * re-baselines at its real roi_begin (markRoiBegin is idempotent -- it
         * just re-snapshots the current instant), so rank 0 stays exact; a
         * rank that later also does SEND/RECV skips the block below (flag
         * already set), preserving the 1.3.3 stencil path. */
        if (zinfo && !mpi_roi_baselined) {
            mpi_roi_baselined = true;
            for (uint32_t c = 0; c < zinfo->numCores; c++)
                if (zinfo->cores[c]) zinfo->cores[c]->markRoiBegin();
            snapshotRoiBaseCyc();
            if (getenv("PIMID_DEBUG_RDV"))
                info("Thread %d: synthesized per-rank ROI baseline at first "
                     "MPI BARRIER (cyc=%lu)", tid,
                     (unsigned long)roiRelCycles(tid));
        }
        /* Barrier: all ranks synchronize via tree reduction.
         * With Garnet: inject real traffic for log2(N) rounds of exchanges.
         * Without: analytical estimate proportional to rank count. */
        if (zinfo) {
            __sync_fetch_and_add(&zinfo->mpiStats.barrierCount, 1);
            uint32_t barrierLat = 0;
            GarnetNetwork* gn = zinfo->garnetNetwork;

            if (gn && gn->isCycleAccurate()) {
                /* parallel mode charges on this thread's own isolated network;
                 * single-threaded MPI ranks get `this` back, so unchanged. */
                GarnetNetwork* anet = zinfo->hierarchy.nocParallel
                                          ? gn->threadLocalContext() : gn;
                /* Tree-reduction barrier: log2(N) rounds.
                 * In each round, half the nodes send to the other half.
                 * Model this PE's perspective: one send per round. */
                uint32_t N = gn->getNumNodes();
                uint32_t myNode = tid % N;
                uint32_t rounds = 0;
                for (uint32_t s = 1; s < N; s <<= 1) rounds++;
                uint64_t issueTime = gem5::curTickRef();
                for (uint32_t r = 0; r < rounds; r++) {
                    uint32_t partner = myNode ^ (1u << r);
                    if (partner < N) {
                        uint32_t oneWay = anet->accessNetwork(myNode, partner,
                                                             issueTime + barrierLat);
                        barrierLat += 2 * oneWay;  /* RTT per round */
                    }
                }
            } else {
                barrierLat = zinfo->numCores * 2;  /* ~2 cycles per rank */
                if (zinfo->hierarchy.enabled) {
                    barrierLat += zinfo->hierarchy.levelLatency[4];
                }
            }

            if (barrierLat > 0) {
                BblInfo* bbl = createSimpleBblInfo(barrierLat, barrierLat * 4);
                fPtrs[tid].bblPtr(tid, 0, bbl);
            }

            __sync_fetch_and_add(&zinfo->mpiStats.messages, 1);
            __sync_fetch_and_add(&zinfo->mpiStats.totalLatency, barrierLat);
        }
        return;
    }

    /* MPI_SEND or MPI_RECV — read params from registered address */
    if (!zinfo || !zinfo->mpiParamsAddr[tid]) return;

    struct MpiParamsBlock {
        uint32_t src_pe, dst_pe;
        uint64_t msg_size, msg_id;
        uint64_t sim_now;          /* plugin -> guest: current sim-time (SEND) */
        uint64_t sim_send_time;    /* guest -> plugin: message send-time (RECV) */
        uint64_t sim_contend_wait; /* guest -> plugin: cross-rank contention wait */
        uint64_t noc_lat;          /* plugin -> guest: real per-message NoC cost
                                    * (Garnet-computed RTT) = the cross-rank
                                    * occupancy reservation duration (SEND) */
        uint32_t noc_detailed;     /* plugin -> guest: 1 if the NoC model is the
                                    * cycle-accurate (detailed) Garnet, 0 for the
                                    * analytical/simple model -> contention is
                                    * applied ONLY when this is 1 (SEND) */
        uint32_t _pad;
    };
    MpiParamsBlock* gp = (MpiParamsBlock*)zinfo->mpiParamsAddr[tid];
    MpiParamsBlock params;
    /* In QEMU user-mode, guest and host share address space for data */
    memcpy(&params, gp, sizeof(params));

    /* MPI_CONTEND (Phase 2): the sender computed a cross-rank queueing wait
     * against the shared occupancy table; charge it as idle CYCLES on this
     * core (the shared outbound link to the destination was busy). Cycles only
     * via addDelay -- instruction counts stay pure compute. */
    if (op == ZSIM_MAGIC_OP_MPI_CONTEND) {
        uint64_t wait = params.sim_contend_wait;
        Core* cc = cores[tid];
        if (cc && wait > 0) {
            cc->addDelay((uint32_t)std::min<uint64_t>(wait, 0xFFFFFFFFull));
        }
        if (zinfo && wait > 0) {
            __sync_fetch_and_add(&zinfo->mpiStats.totalLatency, wait);
        }
        if (getenv("PIMID_DEBUG_CONTEND")) {
            info("Thread %d: MPI_CONTEND src=%u dst=%u size=%lu wait=%lu",
                 tid, params.src_pe, params.dst_pe,
                 (unsigned long)params.msg_size, (unsigned long)wait);
        }
        return;
    }

    const bool isRecv = (op == ZSIM_MAGIC_OP_MPI_RECV);

    /* Per-rank ROI baseline (THE MPI cycle-accounting fix). The benchmarks call
     * zsim_roi_begin() only on rank 0, so ranks 1..N-1 never snapshot an ROI
     * baseline: their reported cycles/instrs are whole-program, dominated by the
     * fixed serial init + the wall-clock-pumped globPhaseCycles floor that forms
     * during MPI_Init -- so every rank>0 reads a fixed ~40M regardless of --size
     * or kernel, while only rank 0 scaled. On the FIRST real communication op
     * (SEND/RECV ~= kernel entry, mirroring rank 0's roi_begin at the compute
     * loop) synthesize the baseline for any rank that never saw roi_begin, so
     * ALL ranks report kernel-relative, workload-scaling cycles/instrs. Each MPI
     * rank is its own process, so mpi_roi_baselined is per-rank; rank 0 sets it
     * in the roi_begin handler and is therefore left untouched here. */
    if (!mpi_roi_baselined) {
        mpi_roi_baselined = true;
        for (uint32_t c = 0; c < zinfo->numCores; c++)
            if (zinfo->cores[c]) zinfo->cores[c]->markRoiBegin();
        snapshotRoiBaseCyc();
        if (getenv("PIMID_DEBUG_RDV"))
            info("Thread %d: synthesized per-rank ROI baseline at first MPI %s "
                 "(cyc=%lu)", tid, isRecv ? "RECV" : "SEND",
                 (unsigned long)(cores[tid] ? cores[tid]->getCycles() : 0));
    }

    /* SEND: publish the current simulated time so the sender can stamp the
     * outgoing message with its send-time (read back by libpimid_mpi), plus
     * tell the guest whether the NoC model is the cycle-accurate (detailed)
     * Garnet -- cross-rank contention is applied ONLY in detailed mode
     * (analytical/simple is the single intentional approximation, and a native
     * run with no plugin never reaches this and leaves the flag at its default
     * 0). The real per-message NoC cost (noc_lat) is filled in below once
     * computed and used by the guest as the occupancy reservation duration.
     *
     * The send-time stamp is the sender core's ROI-baseline-RELATIVE work clock
     * (roiRelCycles = getCycles() - this rank's baseline), NOT globPhaseCycles
     * and NOT the raw getCycles(). Raw getCycles() carries the per-rank wall-
     * clock floor; subtracting the baseline makes the stamp a function of real
     * post-baseline work only, so the receiver's rendezvous (arrival = send_time
     * + latency) resolves deterministically and floor-free across ranks. */
    if (!isRecv) {
        gp->sim_now = roiRelCycles(tid);
        gp->noc_detailed =
            (zinfo->garnetNetwork && zinfo->garnetNetwork->isCycleAccurate())
                ? 1u : 0u;
        gp->noc_lat = 0;
    }

    if (params.src_pe == params.dst_pe) return;

    /* 1. Hierarchy latency (PE-to-PE LCA routing with M:N mapping) */
    uint32_t hierLat = 0;
    if (zinfo->hierarchy.enabled) {
        hierLat = (uint32_t)computePEtoPELatency(
            params.src_pe, params.dst_pe,
            zinfo->hierarchy.levelLatency, zinfo->hierarchy.bridgeLatency,
            zinfo->hierarchy.placementLevel, zinfo->hierarchy.subarraysPerBank,
            zinfo->hierarchy.banksPerBG, zinfo->hierarchy.bgPerChip,
            zinfo->hierarchy.peMemMapSize > 0 ? zinfo->hierarchy.peMemMapOffsets : nullptr,
            zinfo->hierarchy.peMemMapSize > 0 ? zinfo->hierarchy.peMemMapData : nullptr,
            zinfo->hierarchy.peMemMapSize,
            zinfo->hierarchy.connectionMode,
            zinfo->hierarchy.localLinkLatency,
            zinfo->hierarchy.chipsPerRank,
            zinfo->hierarchy.ranksPerChannel);
    }

    /* 2. NoC latency (if Garnet is available) */
    uint32_t nocLat = 0;
    if (zinfo->garnetNetwork) {
        GarnetNetwork* gn = zinfo->garnetNetwork;
        uint32_t srcNode = params.src_pe % gn->getNumNodes();
        uint32_t dstNode = params.dst_pe % gn->getNumNodes();

        if (gn->isCycleAccurate()) {
            /* parallel mode charges on this thread's own isolated network;
             * single-threaded MPI ranks get `this` back, so unchanged. */
            GarnetNetwork* anet = zinfo->hierarchy.nocParallel
                                      ? gn->threadLocalContext() : gn;
            /* Direct Garnet injection — real traffic with real contention.
             * Each packet represents a cache-line (64B) transfer.
             * Large messages inject multiple packets sequentially. */
            uint64_t issueTime = gem5::curTickRef();
            uint32_t numPackets = std::max(1u, (uint32_t)((params.msg_size + 63) / 64));
            uint32_t totalOneWay = 0;
            for (uint32_t p = 0; p < numPackets; p++) {
                uint32_t oneWay = anet->accessNetwork(srcNode, dstNode,
                                                     issueTime + totalOneWay);
                totalOneWay += oneWay;
            }
            nocLat = 2 * totalOneWay;  /* RTT */
        } else {
            /* Simple mode: analytical RTT with message size scaling */
            nocLat = gn->getRTT(
                std::to_string(params.src_pe).c_str(),
                std::to_string(params.dst_pe).c_str());
            if (params.msg_size > 64) {
                nocLat += (uint32_t)((params.msg_size / 64) * 2);
            }
        }
    }

    /* SEND: publish the real per-message NoC cost so the guest can use it as the
     * cross-rank occupancy reservation duration (no magic-number flit constant).
     * In detailed mode this is the Garnet-computed RTT; in analytical mode the
     * guest ignores it (noc_detailed==0 -> contention skipped). */
    if (!isRecv) {
        gp->noc_lat = nocLat;
    }

    /* 3. Charge timing as CYCLES (not instructions) so instr counts reflect
     *    pure compute and stay deterministic.  Use addDelay(), which advances
     *    the core's clock without inflating its instruction count. */
    uint32_t totalLat = nocLat + hierLat;
    uint64_t chargedCycles = totalLat;
    Core* c = cores[tid];
    if (isRecv) {
        /* Sim-time rendezvous: advance the receiver's clock to the
         * deterministic arrival time = send_time + latency. If the receiver is
         * already past that point in sim-time, the message is already available
         * and we charge nothing. Falls back to plain latency when the message
         * carries no stamp (e.g. legacy path). curCyc is the receiver's ROI-
         * baseline-RELATIVE work clock so it compares like-for-like against the
         * sender's baseline-relative send-time stamp (both floor-free). */
        uint64_t curCyc = roiRelCycles(tid);
        /* Phase-2: the receiver's rendezvous arrival includes the cross-rank
         * contention wait stamped by the sender:
         *   arrival = send_time + contend_wait + hierLat + nocLat            */
        uint64_t arrival = (params.sim_send_time > 0)
                               ? (params.sim_send_time + params.sim_contend_wait
                                  + (uint64_t)totalLat)
                               : (curCyc + (uint64_t)totalLat);
        uint64_t delta = (arrival > curCyc) ? (arrival - curCyc) : 0;
        chargedCycles = delta;
        if (c && delta > 0) c->addDelay((uint32_t)std::min<uint64_t>(delta, 0xFFFFFFFFull));
        if (getenv("PIMID_DEBUG_RDV")) {
            info("Thread %d: MPI_RECV src=%u dst=%u size=%lu send_time=%lu cwait=%lu "
                 "cur=%lu lat=%u arrival=%lu delta=%lu", tid, params.src_pe,
                 params.dst_pe, (unsigned long)params.msg_size,
                 (unsigned long)params.sim_send_time,
                 (unsigned long)params.sim_contend_wait,
                 (unsigned long)curCyc, totalLat, (unsigned long)arrival,
                 (unsigned long)delta);
        }
    } else {
        if (c && totalLat > 0) c->addDelay(totalLat);
        if (getenv("PIMID_DEBUG_RDV")) {
            info("Thread %d: MPI_SEND src=%u dst=%u size=%lu sim_now=%lu lat=%u",
                 tid, params.src_pe, params.dst_pe, (unsigned long)params.msg_size,
                 (unsigned long)roiRelCycles(tid), totalLat);
        }
    }

    /* 4. Stats */
    __sync_fetch_and_add(&zinfo->mpiStats.messages, 1);
    __sync_fetch_and_add(&zinfo->mpiStats.totalLatency, chargedCycles);
}

/**
 * Check if a magic op code is an MPI op.
 */
static inline bool isMpiMagicOp(uint64_t op) {
    return op >= ZSIM_MAGIC_OP_MPI_REGISTER && op <= ZSIM_MAGIC_OP_MPI_CONTEND;
}

/**
 * Compute PCIe/CXL transfer latency (SIMPLE or M/D/1 contention model).
 *
 * @param transfer_bytes Data payload size in bytes. 0 means default (64B cache line).
 *
 * SIMPLE (model==0): baseLatencyCycles + serialization(transfer_bytes).
 * MD1 (model==1): phase-based M/D/1 queuing model using bytesPerCycle for
 *   service rate. Offload count per phase drives arrival rate.
 *   Service time scales with actual transfer_bytes.
 */
static uint32_t getPCIeLatency(uint32_t transfer_bytes = 0) {
    if (!zinfo || !zinfo->pcie.enabled || zinfo->pcie.baseLatencyCycles == 0) return 0;

    uint32_t base = zinfo->pcie.baseLatencyCycles;
    double data_bytes = (transfer_bytes > 0) ? static_cast<double>(transfer_bytes) : 64.0;

    // Protocol overhead: header bytes reduce effective throughput
    double total_bytes = data_bytes + zinfo->pcie.headerBytes;

    // Serialization: ceil(total_bytes / bytesPerCycle) — including protocol overhead
    uint32_t serCycles = 0;
    if (zinfo->pcie.bytesPerCycle > 0.0) {
        serCycles = static_cast<uint32_t>(
            std::ceil(total_bytes / zinfo->pcie.bytesPerCycle));
    }

    // Coherence overhead (average extra latency for coherent access)
    base += zinfo->pcie.coherenceExtraCycles;

    // SIMPLE model: hop count base + M/D/1 queuing contention
    // At zero load: rho≈0, latMul≈1.0 → result ≈ base+serCycles
    futex_lock(&zinfo->pcie.updateLock);

    zinfo->pcie.curPhaseAccesses++;

    // Phase-based smoothing (same approach as ZSim SimpleMemory M/D/1)
    uint64_t curPhase = zinfo->numPhases;
    if (curPhase != zinfo->pcie.lastPhase) {
        double alpha = 0.5;
        uint32_t phaseLen = zinfo->phaseLength;
        double curRate = (phaseLen > 0)
            ? static_cast<double>(zinfo->pcie.curPhaseAccesses) / phaseLen
            : 0.0;
        zinfo->pcie.smoothedPhaseRate =
            alpha * curRate + (1.0 - alpha) * zinfo->pcie.smoothedPhaseRate;
        zinfo->pcie.curPhaseAccesses = 0;
        zinfo->pcie.lastPhase = curPhase;
    }

    // Service time from actual transfer size
    double serviceTime = (zinfo->pcie.bytesPerCycle > 0.0)
        ? data_bytes / zinfo->pcie.bytesPerCycle
        : static_cast<double>(base);
    if (serviceTime < 1.0) serviceTime = 1.0;
    double serviceRate = 1.0 / serviceTime;

    double rho = zinfo->pcie.smoothedPhaseRate / serviceRate;
    if (rho > 0.95) rho = 0.95;
    if (rho < 0.0) rho = 0.0;

    // P-K formula for M/D/1: E[T] = D * (1 + rho / (2*(1-rho)))
    double latMul = 1.0 + 0.5 * rho / (1.0 - rho);
    uint32_t result = static_cast<uint32_t>((base + serCycles) * latMul + 0.5);

    futex_unlock(&zinfo->pcie.updateLock);
    return result;
}

/**
 * Magic instruction callback — dispatches ZSim magic ops detected at
 * translation time.  The opcode (from the preceding mov $imm, %rcx) is
 * passed as userdata.
 */
static void magic_insn_exec_cb(unsigned int vcpu_index, void *userdata) {
    if (vcpu_index < MAX_VCPUS && vcpu_is_internal[vcpu_index]) return;
    uint64_t op = (uint64_t)(uintptr_t)userdata;
    uint32_t tid = getOrAssignTid(vcpu_index);

    /* Authoritative source: read the ACTUAL %rcx at the xchg.  This captures
     * runtime-built magic ops (e.g. zsim_work_begin_sized with a runtime size,
     * where %rcx is built by salq/orq and the translation-time decoder sees
     * nothing).  Falls back to the translation-time decoded value if the
     * register read is unavailable.  This callback is registered R_REGS. */
    resolveRcxHandle();
    uint64_t rcx;
    if (readRcx(&rcx) && rcx != 0) {
        op = rcx;
    }

    if (isMpiMagicOp(op)) {
        if (tid < MAX_THREADS) {
            ensureThreadInit(tid);
            handleMpiMagicOp(op, tid);
        }
        return;
    }

    // Extract low 32 bits as opcode, high 32 bits as optional payload size
    uint32_t opcode = static_cast<uint32_t>(op & 0xFFFFFFFF);
    uint32_t payload_size = static_cast<uint32_t>(op >> 32);

    if (opcode == ZSIM_MAGIC_OP_ROI_BEGIN) {
        in_roi.store(true);
        // Snapshot each core's ROI baseline so the reported cycles/instrs cover
        // ONLY the kernel (roi_begin..roi_end), excluding the serial pre-ROI
        // array-init/setup that otherwise runs on the launcher PE and dominates.
        for (uint32_t c = 0; c < zinfo->numCores; c++)
            if (zinfo->cores[c]) zinfo->cores[c]->markRoiBegin();
        // This rank got a real roi_begin -> don't also synthesize a baseline at
        // the first MPI op (see handleMpiMagicOp). rank 0 takes this path.
        mpi_roi_baselined = true;
        snapshotRoiBaseCyc();
        // Co-sim: the ROI IS the offload region. The launching thread (and
        // every thread spawned inside the region) executes on the DEVICE;
        // out-of-ROI code executes on the host. Ordinary workloads are
        // co-sim workloads -- there is no special kind.
        if (g_cosim_mode && tid < MAX_THREADS) {
            thread_domain[tid].store(DOMAIN_DEVICE);
            g_in_device_region.store(true);
            ++offload_count;
            if (thread_initialized[tid]) {
                uint32_t cid = cids[tid];
                if (cid != INVALID_CID && cid != UNINITIALIZED_CID) {
                    zinfo->sched->leave(procIdx, tid, cid);
                }
                zinfo->sched->finish(procIdx, tid);
                thread_initialized[tid] = false;
                clearCid(tid);
            }
            ensureThreadInit(tid);  // device mask
            info("Thread %d: ROI offload begin (co-sim)", tid);
        }
    } else if (opcode == ZSIM_MAGIC_OP_ROI_END) {
        // Co-sim: return the launching thread to a host core BEFORE the
        // termination flag, so the host core rejoin fast-forwards to the
        // global phase clock and host cycles absorb the device execution.
        if (g_cosim_mode && tid < MAX_THREADS) {
            g_in_device_region.store(false);
            thread_domain[tid].store(DOMAIN_HOST);
            if (thread_initialized[tid]) {
                uint32_t cid = cids[tid];
                if (cid != INVALID_CID && cid != UNINITIALIZED_CID) {
                    zinfo->sched->leave(procIdx, tid, cid);
                }
                zinfo->sched->finish(procIdx, tid);
                thread_initialized[tid] = false;
                clearCid(tid);
            }
            ensureThreadInit(tid);  // host mask
            info("Thread %d: ROI offload end (co-sim)", tid);
        }
        in_roi.store(false);
        zinfo->terminationConditionMet = true;  // Let watchdog trigger SimEnd()
    } else if (opcode == ZSIM_MAGIC_OP_WORK_BEGIN) {
        if (tid < MAX_THREADS) {
            thread_domain[tid].store(DOMAIN_DEVICE);
            g_in_device_region.store(true);
            uint64_t cnt = ++offload_count;
            /* If thread already running on a host core, leave it and
             * re-initialize with device_mask so it joins a device PE. */
            if (thread_initialized[tid]) {
                uint32_t cid = cids[tid];
                if (cid != INVALID_CID && cid != UNINITIALIZED_CID) {
                    zinfo->sched->leave(procIdx, tid, cid);
                }
                zinfo->sched->finish(procIdx, tid);
                thread_initialized[tid] = false;
                clearCid(tid);
            }
            ensureThreadInit(tid);  // will use device_mask
            /* PCIe/CXL offload timing: host→device transfer latency */
            uint32_t pcieLat = getPCIeLatency(payload_size);
            if (pcieLat > 0) {
                BblInfo* bbl = createSimpleBblInfo(pcieLat, pcieLat * 4);
                fPtrs[tid].bblPtr(tid, 0, bbl);
            }
            info("Thread %d: WORK_BEGIN (device offload #%lu, %u bytes)", tid,
                 (unsigned long)cnt, payload_size);
        }
        return;
    } else if (opcode == ZSIM_MAGIC_OP_WORK_END) {
        g_in_device_region.store(false);
        if (tid < MAX_THREADS) {
            /* Migrate the offloading thread BACK to a host core (mirror of
             * WORK_BEGIN). Without this it stays on a device PE forever, so
             * all post-offload host work (final merge, verification) accrues
             * on device_pes-0 -- and under address-routed pricing is
             * mispriced as device-issued host accesses (host latency plus
             * attach tolls per read). The host-core rejoin also fast-forwards
             * its cycle counter to the global phase clock, so host cycles
             * correctly absorb the device execution it waited for. */
            thread_domain[tid].store(DOMAIN_HOST);
            if (thread_initialized[tid]) {
                uint32_t cid = cids[tid];
                if (cid != INVALID_CID && cid != UNINITIALIZED_CID) {
                    zinfo->sched->leave(procIdx, tid, cid);
                }
                zinfo->sched->finish(procIdx, tid);
                thread_initialized[tid] = false;
                clearCid(tid);
            }
            ensureThreadInit(tid);  // will use host_mask
            /* PCIe/CXL return timing: device→host transfer latency,
             * charged on the host core (it pays for the return DMA wait) */
            uint32_t pcieLat = getPCIeLatency(payload_size);
            if (pcieLat > 0) {
                BblInfo* bbl = createSimpleBblInfo(pcieLat, pcieLat * 4);
                fPtrs[tid].bblPtr(tid, 0, bbl);
            }
            info("Thread %d: WORK_END (%u bytes)", tid, payload_size);
        }
        return;
    } else {
        return;
    }
    uint64_t count = ++roi_transition_count;
    info("ROI %s (transition #%lu)", opcode == ZSIM_MAGIC_OP_ROI_BEGIN ? "BEGIN" : "END",
         (unsigned long)count);
}

/**
 * Execution callback for mov $imm, %rcx at end of a TB — stores the opcode
 * into the per-vCPU pending slot so the next TB's xchg can consume it.
 */
static void mov_magic_exec_cb(unsigned int vcpu_index, void *userdata) {
    uint64_t op = (uint64_t)(uintptr_t)userdata;
    uint32_t tid = getOrAssignTid(vcpu_index);
    if (tid < MAX_THREADS) {
        pending_magic_op[tid].store(op);
    }
}

/**
 * Execution callback for xchg %rcx, %rcx without a preceding mov in the
 * same TB.  Checks the per-vCPU pending slot for a cross-TB magic op.
 */
static void xchg_pending_exec_cb(unsigned int vcpu_index, void *userdata) {
    (void)userdata;
    if (vcpu_index < MAX_VCPUS && vcpu_is_internal[vcpu_index]) return;
    uint32_t tid = getOrAssignTid(vcpu_index);
    if (tid >= MAX_THREADS) return;
    /* Always drain the pending slot to keep cross-TB mov state consistent. */
    uint64_t op = pending_magic_op[tid].exchange(0);

    /* Authoritative source: read the ACTUAL %rcx at the xchg.  This is the
     * general fix for runtime-built magic ops (the pending slot only ever
     * holds translation-time decoded mov-immediates).  This callback is
     * registered R_REGS. */
    resolveRcxHandle();
    uint64_t rcx;
    if (readRcx(&rcx) && rcx != 0) {
        op = rcx;
    }

    if (isMpiMagicOp(op)) {
        ensureThreadInit(tid);
        handleMpiMagicOp(op, tid);
        return;
    }
    // Extract low 32 bits as opcode, high 32 bits as optional payload size
    uint32_t opcode = static_cast<uint32_t>(op & 0xFFFFFFFF);
    uint32_t payload_size = static_cast<uint32_t>(op >> 32);

    if (opcode == ZSIM_MAGIC_OP_ROI_BEGIN) {
        in_roi.store(true);
        // Snapshot each core's ROI baseline so the reported cycles/instrs cover
        // ONLY the kernel (roi_begin..roi_end), excluding the serial pre-ROI
        // array-init/setup that otherwise runs on the launcher PE and dominates.
        for (uint32_t c = 0; c < zinfo->numCores; c++)
            if (zinfo->cores[c]) zinfo->cores[c]->markRoiBegin();
        // This rank got a real roi_begin -> don't also synthesize a baseline at
        // the first MPI op (see handleMpiMagicOp). rank 0 takes this path.
        mpi_roi_baselined = true;
        snapshotRoiBaseCyc();
        // Co-sim: ROI = offload region (see the twin handler above).
        if (g_cosim_mode && tid < MAX_THREADS) {
            thread_domain[tid].store(DOMAIN_DEVICE);
            g_in_device_region.store(true);
            ++offload_count;
            if (thread_initialized[tid]) {
                uint32_t cid = cids[tid];
                if (cid != INVALID_CID && cid != UNINITIALIZED_CID) {
                    zinfo->sched->leave(procIdx, tid, cid);
                }
                zinfo->sched->finish(procIdx, tid);
                thread_initialized[tid] = false;
                clearCid(tid);
            }
            ensureThreadInit(tid);  // device mask
            info("Thread %d: ROI offload begin (co-sim)", tid);
        }
    } else if (opcode == ZSIM_MAGIC_OP_ROI_END) {
        if (g_cosim_mode && tid < MAX_THREADS) {
            g_in_device_region.store(false);
            thread_domain[tid].store(DOMAIN_HOST);
            if (thread_initialized[tid]) {
                uint32_t cid = cids[tid];
                if (cid != INVALID_CID && cid != UNINITIALIZED_CID) {
                    zinfo->sched->leave(procIdx, tid, cid);
                }
                zinfo->sched->finish(procIdx, tid);
                thread_initialized[tid] = false;
                clearCid(tid);
            }
            ensureThreadInit(tid);  // host mask
            info("Thread %d: ROI offload end (co-sim)", tid);
        }
        in_roi.store(false);
        zinfo->terminationConditionMet = true;  // Let watchdog trigger SimEnd()
    } else if (opcode == ZSIM_MAGIC_OP_WORK_BEGIN) {
        thread_domain[tid].store(DOMAIN_DEVICE);
        g_in_device_region.store(true);
        uint64_t cnt = ++offload_count;
        /* If thread already running on a host core, leave it and
         * re-initialize with device_mask so it joins a device PE. */
        if (thread_initialized[tid]) {
            uint32_t cid = cids[tid];
            if (cid != INVALID_CID && cid != UNINITIALIZED_CID) {
                zinfo->sched->leave(procIdx, tid, cid);
            }
            zinfo->sched->finish(procIdx, tid);
            thread_initialized[tid] = false;
            clearCid(tid);
        }
        ensureThreadInit(tid);
        /* PCIe/CXL offload timing: host→device transfer latency */
        uint32_t pcieLat = getPCIeLatency(payload_size);
        if (pcieLat > 0) {
            BblInfo* bbl = createSimpleBblInfo(pcieLat, pcieLat * 4);
            fPtrs[tid].bblPtr(tid, 0, bbl);
        }
        info("Thread %d: WORK_BEGIN (device offload #%lu, %u bytes)", tid,
             (unsigned long)cnt, payload_size);
        return;
    } else if (opcode == ZSIM_MAGIC_OP_WORK_END) {
        g_in_device_region.store(false);
        /* Migrate back to a host core -- see the WORK_END handler above. */
        thread_domain[tid].store(DOMAIN_HOST);
        if (thread_initialized[tid]) {
            uint32_t cid = cids[tid];
            if (cid != INVALID_CID && cid != UNINITIALIZED_CID) {
                zinfo->sched->leave(procIdx, tid, cid);
            }
            zinfo->sched->finish(procIdx, tid);
            thread_initialized[tid] = false;
            clearCid(tid);
        }
        ensureThreadInit(tid);  // will use host_mask
        /* PCIe/CXL return timing: device→host transfer latency */
        uint32_t pcieLat = getPCIeLatency(payload_size);
        if (pcieLat > 0) {
            BblInfo* bbl = createSimpleBblInfo(pcieLat, pcieLat * 4);
            fPtrs[tid].bblPtr(tid, 0, bbl);
        }
        info("Thread %d: WORK_END (%u bytes)", tid, payload_size);
        return;
    } else {
        return;
    }
    uint64_t count = ++roi_transition_count;
    info("ROI %s (transition #%lu)", opcode == ZSIM_MAGIC_OP_ROI_BEGIN ? "BEGIN" : "END",
         (unsigned long)count);
}

/**
 * Translation block callback — instruments each TB with:
 *   1. A BBL callback on the first instruction
 *   2. Memory callbacks on each instruction
 */
static void tb_trans_cb(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
    size_t n_insns = qemu_plugin_tb_n_insns(tb);
    uint64_t tb_addr = qemu_plugin_tb_vaddr(tb);

    /* Compute total TB size */
    uint32_t tb_bytes = 0;
    for (size_t i = 0; i < n_insns; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        tb_bytes += (uint32_t)qemu_plugin_insn_size(insn);
    }

    /* Get or create BblInfo for this TB (decodes DynUops when OOO is present) */
    BblInfo* bblInfo = getOrCreateBblInfo(tb_addr, (uint32_t)n_insns, tb_bytes, tb);

    /* Allocate persistent userdata for this TB's callbacks.
     * QEMU may re-translate TBs, so we use the cached BblInfo.
     * The TbUserdata is leaked intentionally — it persists for the
     * lifetime of the process. */
    TbUserdata* tud = (TbUserdata*)malloc(sizeof(TbUserdata));
    tud->bblInfo = bblInfo;
    tud->tbAddr = tb_addr;
    tud->approxKeys = nullptr;
    tud->numApproxKeys = 0;
    tud->numInsns = (uint32_t)n_insns;
    tud->endsInCondBranch = false;
    tud->brPc = tud->brTakenTarget = tud->brFallthrough = 0;

    /* OOO branch-predictor wiring: if this TB's last instruction is a conditional
     * jcc, record its PC + both successor addresses so the real direction can be
     * resolved from the next TB and driven into the OOO branch predictor. */
    if (g_ooo_present && !g_ooo_decode_disabled && n_insns > 0) {
        struct qemu_plugin_insn* last = qemu_plugin_tb_get_insn(tb, n_insns - 1);
        uint64_t lpc = qemu_plugin_insn_vaddr(last);
        size_t lsz = qemu_plugin_insn_size(last);
        uint8_t lb[16];
        qemu_plugin_insn_data(last, lb, sizeof(lb));
        uint32_t p = 0;
        while (p < lsz && (lb[p] == 0x2E || lb[p] == 0x3E)) p++;  /* branch hints */
        int64_t rel = 0; bool isCond = false;
        if (p < lsz && lb[p] >= 0x70 && lb[p] <= 0x7F && (p + 1) < lsz) {
            rel = (int8_t)lb[p + 1]; isCond = true;                /* jcc rel8 */
        } else if ((p + 5) < lsz + 1 && lb[p] == 0x0F &&
                   lb[p + 1] >= 0x80 && lb[p + 1] <= 0x8F && (p + 5) < lsz) {
            rel = (int32_t)((uint32_t)lb[p + 2] | ((uint32_t)lb[p + 3] << 8) |
                            ((uint32_t)lb[p + 4] << 16) | ((uint32_t)lb[p + 5] << 24));
            isCond = true;                                        /* jcc rel32 */
        }
        if (isCond) {
            tud->endsInCondBranch = true;
            tud->brPc = lpc;
            tud->brFallthrough = lpc + lsz;
            tud->brTakenTarget = lpc + lsz + (uint64_t)rel;
        }
    }

    if (g_ooo_dump) {
        std::lock_guard<std::mutex> lock(bblCacheMutex);
        auto ai = g_tbApproxCache.find(tb_addr);
        if (ai != g_tbApproxCache.end()) {
            tud->approxKeys = ai->second.first;
            tud->numApproxKeys = ai->second.second;
        }
    }

    bool bbl_registered = false;

    /* Track in-TB mov $imm, %rcx → xchg %rcx, %rcx pairs.
     * prev_magic_op is local (no cross-vCPU race).  Cross-TB communication
     * is deferred to execution time via per-vCPU pending_magic_op[]. */
    uint64_t prev_magic_op = 0;
    struct qemu_plugin_insn *last_magic_mov_insn = NULL;

    for (size_t i = 0; i < n_insns; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        size_t insn_sz = qemu_plugin_insn_size(insn);
        uint8_t insn_buf[16];
        qemu_plugin_insn_data(insn, insn_buf, sizeof(insn_buf));
        const uint8_t *bytes = insn_buf;

        /* Detect xchg %rcx, %rcx (48 87 c9) */
        if (insn_sz == 3 && bytes[0] == 0x48 && bytes[1] == 0x87
                && bytes[2] == 0xc9) {
            uint32_t prev_opcode = static_cast<uint32_t>(prev_magic_op & 0xFFFFFFFF);
            if (prev_opcode == ZSIM_MAGIC_OP_ROI_BEGIN
                    || prev_opcode == ZSIM_MAGIC_OP_ROI_END
                    || prev_opcode == ZSIM_MAGIC_OP_WORK_BEGIN
                    || prev_opcode == ZSIM_MAGIC_OP_WORK_END
                    || isMpiMagicOp(prev_magic_op)) {
                /* In-TB case: mov+xchg in same TB — opcode known at
                 * translation time, passed as fallback via userdata.  The
                 * callback reads the real %rcx at execution time (R_REGS) as
                 * the authoritative value so runtime-built sizes are captured. */
                qemu_plugin_register_vcpu_insn_exec_cb(
                    insn, magic_insn_exec_cb, QEMU_PLUGIN_CB_R_REGS,
                    (void *)(uintptr_t)prev_magic_op);
            } else {
                /* Cross-TB case: no preceding mov in this TB — read the real
                 * %rcx at execution time (R_REGS); the per-vCPU pending slot
                 * serves only as a fallback. */
                qemu_plugin_register_vcpu_insn_exec_cb(
                    insn, xchg_pending_exec_cb, QEMU_PLUGIN_CB_R_REGS,
                    NULL);
            }
            prev_magic_op = 0;
            last_magic_mov_insn = NULL;
            continue;
        }

        /* Check if this instruction is mov $imm, %rcx/%ecx (magic op setup).
         * Three possible encodings:
         *   b9 xx xx xx xx          — mov $imm32, %ecx  (5 bytes)
         *   48 c7 c1 xx xx xx xx   — mov $imm32, %rcx  (7 bytes)
         *   48 b9 xx*8             — movabs $imm64, %rcx (10 bytes) */
        prev_magic_op = 0;
        last_magic_mov_insn = NULL;
        if (insn_sz == 5 && bytes[0] == 0xb9) {
            prev_magic_op = (uint32_t)bytes[1] | ((uint32_t)bytes[2] << 8)
                          | ((uint32_t)bytes[3] << 16) | ((uint32_t)bytes[4] << 24);
        } else if (insn_sz == 7 && bytes[0] == 0x48 && bytes[1] == 0xc7
                   && bytes[2] == 0xc1) {
            prev_magic_op = (uint32_t)bytes[3] | ((uint32_t)bytes[4] << 8)
                          | ((uint32_t)bytes[5] << 16) | ((uint32_t)bytes[6] << 24);
        } else if (insn_sz == 10 && bytes[0] == 0x48 && bytes[1] == 0xb9) {
            /* movabs $imm64,%rcx — read the FULL 8-byte immediate. The high 32
             * bits carry the optional WORK_BEGIN payload size; reading only the
             * low 4 bytes (as before) silently dropped it, so sized transfers
             * were charged as 0 bytes. Cast each byte to uint64_t before shifting
             * past bit 31 to avoid 32-bit overflow. */
            prev_magic_op = (uint64_t)bytes[2]        | ((uint64_t)bytes[3] << 8)
                          | ((uint64_t)bytes[4] << 16) | ((uint64_t)bytes[5] << 24)
                          | ((uint64_t)bytes[6] << 32) | ((uint64_t)bytes[7] << 40)
                          | ((uint64_t)bytes[8] << 48) | ((uint64_t)bytes[9] << 56);
        }
        if (prev_magic_op != 0) {
            last_magic_mov_insn = insn;
        }

        /* Register BBL callback on first non-magic instruction of TB */
        if (!bbl_registered) {
            qemu_plugin_register_vcpu_insn_exec_cb(
                insn, insn_exec_cb, QEMU_PLUGIN_CB_NO_REGS, tud);
            bbl_registered = true;
        }

        /* Register memory callback for every instruction */
        qemu_plugin_register_vcpu_mem_cb(
            insn, mem_cb, QEMU_PLUGIN_CB_NO_REGS,
            QEMU_PLUGIN_MEM_RW, NULL);
    }

    /* If the TB ends with a mov $imm, %rcx that wasn't consumed by an xchg
     * in this TB, register an execution callback to store the opcode into
     * the per-vCPU pending slot for the next TB's xchg to pick up. */
    if (prev_magic_op != 0 && last_magic_mov_insn != NULL) {
        qemu_plugin_register_vcpu_insn_exec_cb(
            last_magic_mov_insn, mov_magic_exec_cb, QEMU_PLUGIN_CB_NO_REGS,
            (void *)(uintptr_t)prev_magic_op);
    }
}

/**
 * vCPU init callback — assigns tid but defers SimThreadStart until first
 * execution callback, so we know the thread's domain (HOST vs DEVICE).
 */
static void vcpu_init_cb(qemu_plugin_id_t id, unsigned int vcpu_index) {
    /* vCPUs created during SimInit are ZSim-internal (watchdog, contention sim).
     * They must not participate in the phase barrier. */
    if (!sim_initialized && vcpu_index > 0) {
        if (vcpu_index < MAX_VCPUS) {
            vcpu_is_internal[vcpu_index] = true;
        }
        return;  // don't assign TID
    }

    uint32_t tid = getOrAssignTid(vcpu_index);
    if (tid < MAX_THREADS) {
        thread_initialized[tid] = false;
        /* Inherit DEVICE domain if spawned inside an active offload region,
         * so device-worker threads map to device PEs, not host cores. */
        thread_domain[tid].store(
            g_in_device_region.load() ? DOMAIN_DEVICE : DOMAIN_HOST);
    }
}

/**
 * vCPU exit callback — thread finish.
 */
static void vcpu_exit_cb(qemu_plugin_id_t id, unsigned int vcpu_index) {
    if (vcpu_index < MAX_VCPUS && vcpu_is_internal[vcpu_index]) return;
    if (vcpu_index < MAX_VCPUS) {
        uint32_t tid = vcpuToTid[vcpu_index];
        if (tid != UNINITIALIZED_CID && tid < MAX_THREADS) {
            if (!thread_initialized[tid]) return;  // never started
            /* Leave scheduler if still scheduled */
            uint32_t cid = cids[tid];
            if (cid != INVALID_CID && cid != UNINITIALIZED_CID) {
                zinfo->sched->leave(procIdx, tid, cid);
            }
            SimThreadFini(tid);
        }
    }
}

/**
 * Syscall callback — scheduler leave/join around blocking syscalls.
 * For thread-creating syscalls (clone/fork), we don't need special
 * handling since QEMU fires vcpu_init for new threads.
 */
static void syscall_cb(qemu_plugin_id_t id,
                       unsigned int vcpu_index,
                       int64_t num,
                       uint64_t a1, uint64_t a2,
                       uint64_t a3, uint64_t a4,
                       uint64_t a5, uint64_t a6,
                       uint64_t a7, uint64_t a8) {
    if (vcpu_index >= MAX_VCPUS) return;
    if (vcpu_is_internal[vcpu_index]) return;
    /* Guard: skip if we're already inside ZSim code (e.g., reentrant
     * callback from barrier/scheduler futex operations). */
    if (in_zsim[vcpu_index]) return;
    uint32_t tid = vcpuToTid[vcpu_index];
    if (tid == UNINITIALIZED_CID || tid >= MAX_THREADS) return;
    if (!thread_initialized[tid]) return;  // not yet started

    uint32_t cid = cids[tid];
    if (cid == INVALID_CID || cid == UNINITIALIZED_CID) return;

    /* For potentially blocking syscalls, use the scheduler's adaptive
     * syscallLeave mechanism (matching ZSim's Pin-mode behavior):
     * - Short/non-blocking syscalls get a "fake leave" — the barrier
     *   stays in RUNNING state, avoiding the expensive leave/join cycle.
     * - Truly blocking syscalls are detected by the watchdog thread,
     *   which forces a real leave after a timeout.
     * This prevents barrier state corruption from the real leave/join
     * cycle while still handling blocking syscalls correctly. */
    switch (num) {
        case 60:  /* exit — single thread */
            /* A worker (non-main) thread finishing must NOT end the sim and
             * must NOT be torn down here: vcpu_exit_cb already does
             * leave()+SimThreadFini() when the vCPU exits. Ending here
             * truncates the other device PEs; finishing here double-frees
             * the scheduler gid (scheduler.h:234 assert). */
            if (tid != 0) return;
            /* main thread: fall through to terminate the whole simulation */
        case 231: /* exit_group — whole process */
            info("Caught exit syscall (%ld), terminating simulation", (long)num);
            zinfo->sched->leave(procIdx, tid, cid);
            SimEnd();
            break;  /* not reached */
        case 0:   /* read */
        case 1:   /* write */
        case 7:   /* poll */
        case 23:  /* select */
        case 35:  /* nanosleep */
        case 43:  /* accept */
        case 45:  /* recvfrom */
        case 47:  /* recvmsg */
        case 202: /* futex */
        case 230: /* clock_nanosleep — glibc >= 2.31 implements ALL sleeps
                   * with this, never plain nanosleep(35) */
        case 232: /* epoll_wait */
        case 270: /* pselect6 */
        case 271: /* ppoll — glibc poll() uses this on modern kernels */
        case 281: /* epoll_pwait */
        case 288: /* accept4 */
        case 449: /* futex_waitv — glibc >= 2.35 pthread waits use this on
                   * kernels >= 5.16. Without a syscallLeave here a guest
                   * thread blocking in it kept its core slot and starved the
                   * phase barrier: cosim hung at device-thread startup with
                   * image-built (glibc 2.39) workloads on new kernels, while
                   * the same run passed natively with glibc-2.34 binaries
                   * (those fall back to futex(202)). */
        {
            /* MPI comm window: the rank is polling its shm mailbox. Keep the
             * core ATTACHED (no leave) so post-comm compute is counted and
             * cycles do not couple to the wall-clock poll duration. Gated to
             * the bracketed MPI transport only -- OMP/co-sim never set this. */
            if (tid < MAX_THREADS &&
                mpi_comm_window[tid].load(std::memory_order_acquire)) {
                break;
            }
            /* Use syscall number as synthetic PC for the blacklist.
             * Offset by 0x1000 to avoid address 0 (which could confuse
             * the blacklist's unordered_set). */
            uint64_t synth_pc = 0x1000 + (uint64_t)num;
            zinfo->sched->syscallLeave(procIdx, tid, cid, synth_pc,
                                       (int)num, a1, a2);
            /* Whether fake or real leave occurred, set joinPtrs so the
             * next execution callback triggers Join(), which handles
             * both cases: fakeLeave fast-path or full bar.join(). */
            clearCid(tid);
            fPtrs[tid] = joinPtrs;
            break;
        }
        default:
            break;
    }
}

/**
 * Plugin exit — dump stats and clean up.
 */
static void plugin_exit(qemu_plugin_id_t id, void *userdata) {
    (void)userdata;
    /* Finalize any threads still registered */
    for (uint32_t v = 0; v < MAX_VCPUS; v++) {
        uint32_t tid = vcpuToTid[v];
        if (tid != UNINITIALIZED_CID && tid < MAX_THREADS) {
            if (!thread_initialized[tid]) continue;  // never started
            uint32_t cid = cids[tid];
            if (cid != INVALID_CID && cid != UNINITIALIZED_CID) {
                zinfo->sched->leave(procIdx, tid, cid);
            }
            SimThreadFini(tid);
        }
    }

    SimEnd();

    char msg[256];
    uint64_t totalInstrs = 0;
    for (uint32_t i = 0; i < zinfo->numCores; i++) {
        totalInstrs += zinfo->cores[i]->getInstrs();
    }
    snprintf(msg, sizeof(msg),
             "ZSim+QEMU: simulation complete. %lu total instructions, %lu phases\n",
             (unsigned long)totalInstrs, (unsigned long)zinfo->numPhases);
    qemu_plugin_outs(msg);

    if (roi_transition_count.load() > 0) {
        snprintf(msg, sizeof(msg),
                 "ZSim+QEMU ROI: %lu transitions, final state: %s\n",
                 (unsigned long)roi_transition_count.load(),
                 in_roi.load() ? "recording" : "not recording");
        qemu_plugin_outs(msg);
    }

    uint64_t oc = offload_count.load();
    if (oc > 0) {
        snprintf(msg, sizeof(msg),
                 "ZSim+QEMU co-sim: %lu offload transitions\n",
                 (unsigned long)oc);
        qemu_plugin_outs(msg);
    }
}

/* ---- Plugin Install ---- */

extern "C" QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id,
                        const qemu_info_t *info,
                        int argc, char **argv) {
    pluginId = id;

    /* Parse arguments */
    uint32_t argProcIdx = 0;
    for (int i = 0; i < argc; i++) {
        char *arg = argv[i];
        if (strncmp(arg, "shmid=", 6) == 0) {
            shmId = (uint32_t)atoi(arg + 6);
            useSharedMem = true;
        } else if (strncmp(arg, "cfg=", 4) == 0) {
            cfgPath = strdup(arg + 4);
        } else if (strncmp(arg, "out=", 4) == 0) {
            outputDir = strdup(arg + 4);
        } else if (strncmp(arg, "procIdx=", 8) == 0) {
            argProcIdx = (uint32_t)atoi(arg + 8);
        }
    }

    if (!cfgPath) {
        qemu_plugin_outs("ERROR: zsim_qemu plugin requires cfg=<path> argument\n");
        return -1;
    }

    /* Default output directory */
    if (!outputDir) {
        outputDir = strdup("/tmp/zsim_qemu");
    }

    /* Ensure output directory exists */
    char mkdirCmd[512];
    snprintf(mkdirCmd, sizeof(mkdirCmd), "mkdir -p %s", outputDir);
    (void)system(mkdirCmd);

    /* Initialize vcpu→tid mapping and internal-vCPU tracking */
    for (uint32_t i = 0; i < MAX_VCPUS; i++) {
        vcpuToTid[i] = UNINITIALIZED_CID;
        vcpu_is_internal[i] = false;
        in_zsim[i] = false;
    }
    for (uint32_t i = 0; i < MAX_THREADS; i++) {
        cids[i] = UNINITIALIZED_CID;
    }

    /* Register vcpu init/exit BEFORE SimInit so that internal threads
     * (watchdog, contention sim) created by SimInit() get vcpu_init_cb
     * and are marked as internal vCPUs. */
    qemu_plugin_register_vcpu_init_cb(id, vcpu_init_cb);
    qemu_plugin_register_vcpu_exit_cb(id, vcpu_exit_cb);

    if (argProcIdx == 0) {
        /* Primary process: create shared memory and run SimInit */
        if (useSharedMem) {
            gm_attach(shmId);
        } else {
            int newShmId = gm_init(1 << 28);  /* 256 MB segment */
            (void)newShmId;
        }

        procIdx = 0;
        SimInit(cfgPath, outputDir, shmId);

        /* Write sentinel file so secondary processes know init is done */
        char sentinelPath[512];
        snprintf(sentinelPath, sizeof(sentinelPath), "%s/.zsim_shmid", outputDir);
        FILE* sf = fopen(sentinelPath, "w");
        if (sf) {
            fprintf(sf, "%d\n", gm_get_shmid());
            fclose(sf);
        }
    } else {
        /* Secondary process: wait for primary's sentinel, then attach */
        char sentinelPath[512];
        snprintf(sentinelPath, sizeof(sentinelPath), "%s/.zsim_shmid", outputDir);
        int attached_shmid = -1;
        for (int attempt = 0; attempt < 6000; attempt++) {  /* 60 sec max wait */
            FILE* sf = fopen(sentinelPath, "r");
            if (sf) {
                if (fscanf(sf, "%d", &attached_shmid) == 1 && attached_shmid >= 0) {
                    fclose(sf);
                    break;
                }
                fclose(sf);
            }
            usleep(10000);  /* 10ms poll */
        }
        if (attached_shmid < 0) {
            qemu_plugin_outs("ERROR: Secondary process timed out waiting for primary init\n");
            return -1;
        }

        gm_attach(attached_shmid);
        zinfo = static_cast<GlobSimInfo*>(gm_get_glob_ptr());
        procIdx = argProcIdx;
    }

    sim_initialized = true;  // mark init complete — subsequent vCPUs are application threads

    lineBits = __builtin_ctz(zinfo->lineSize);
    procMask = ((uint64_t)procIdx) << 32;
    perProcessEndFlag = 0;

    /* Compute core-type masks for co-sim domain routing.
     *
     * Valid host cores:   OoO, Simple, Timing  (need cache hierarchy)
     * Invalid for host:   ALU (no cache), Null (no memory modeling)
     * Valid device cores:  ALL five types (ALU, OoO, Simple, Timing, Null)
     *
     * ALU and Null cores are always placed in the device mask.
     * All other core types (OoO, Simple, Timing) go to the host mask. */
    /* Detect any OOO core -> enable x86 decode of TBs into DynUops so the OOO
     * engine runs its real dataflow pipeline instead of the synthetic path. */
    g_ooo_decode_disabled = (getenv("PIMID_OOO_NODECODE") != nullptr);
    g_inorder_decode_disabled = (getenv("PIMID_INORDER_NODECODE") != nullptr);
    g_ooo_dump = (getenv("PIMID_OOO_DUMP") != nullptr);
    g_ooo_nobranch = (getenv("PIMID_OOO_NOBRANCH") != nullptr);
    for (uint32_t i = 0; i < zinfo->numCores; i++) {
        if (!zinfo->cores[i]) continue;
        if (zinfo->cores[i]->asOOOCore()) g_ooo_present = true;
        if (zinfo->cores[i]->asInOrderCore()) g_inorder_present = true;
    }
    /* Decode when either engine wants the DynUop stream (and is not disabled). */
    g_decode_enabled = (g_ooo_present && !g_ooo_decode_disabled) ||
                       (g_inorder_present && !g_inorder_decode_disabled);
    if (g_ooo_present) {
        info("[ZSim] OOO core present: x86 decode -> DynUops enabled (real out-of-order path)%s",
             g_ooo_decode_disabled ? " [DISABLED via PIMID_OOO_NODECODE]" : "");
    }
    if (g_inorder_present) {
        info("[ZSim] In-order core present: x86 decode -> DynUops enabled (real in-order scoreboard)%s",
             g_inorder_decode_disabled ? " [DISABLED via PIMID_INORDER_NODECODE]" : "");
    }

    host_mask.resize(zinfo->numCores, false);
    device_mask.resize(zinfo->numCores, false);
    for (uint32_t i = 0; i < zinfo->numCores; i++) {
        if (dynamic_cast<ALUCore*>(zinfo->cores[i])) {
            device_mask[i] = true;
        } else if (dynamic_cast<NullCore*>(zinfo->cores[i])) {
            device_mask[i] = true;
        } else {
            /* OoO, Simple, Timing — valid host cores */
            host_mask[i] = true;
        }
    }

    bool has_device = false;
    for (auto b : device_mask) if (b) { has_device = true; break; }
    bool has_host = false;
    for (auto b : host_mask) if (b) { has_host = true; break; }

    if (!has_device) {
        /* Non-cosim mode — no ALU/Null cores found.
         * Place all cores in host mask (unchanged behavior). */
        for (uint32_t i = 0; i < zinfo->numCores; i++) {
            host_mask[i] = true;
        }
    } else if (!has_host) {
        /* ALU/Null-only config (e.g. PUM/PNM exploration).
         * Promote device cores to host so QEMU can execute on them. */
        for (uint32_t i = 0; i < zinfo->numCores; i++) {
            if (device_mask[i]) host_mask[i] = true;
        }
        info("[ZSim] ALU/Null-only config: all device cores promoted to host for QEMU execution");
    }

    /* Initialize co-sim thread state */
    for (uint32_t i = 0; i < MAX_THREADS; i++) {
        thread_initialized[i] = false;
        thread_domain[i].store(DOMAIN_HOST);
    }

    /* Count host vs device cores for reporting */
    uint32_t nHostCores = 0, nDeviceCores = 0;
    for (uint32_t i = 0; i < zinfo->numCores; i++) {
        if (host_mask[i]) nHostCores++;
        if (device_mask[i]) nDeviceCores++;
    }

    /* Co-sim = a real host AND a real device are both present. Then the ROI
     * IS the offload region: any ordinary workload's kernel executes on the
     * device, its out-of-ROI code on the host. No special cosim workloads. */
    g_cosim_mode = (nHostCores > 0 && nDeviceCores > 0 && nDeviceCores < zinfo->numCores);
    if (g_cosim_mode) {
        info("[ZSim] Co-sim mode: ROI = offload region (host runs out-of-ROI code, device runs the kernel)");
    }
    g_cosim_trace = (getenv("PIMID_COSIM_TRACE") != nullptr);
    if (g_cosim_trace) {
        info("[ZSim] Co-sim thread-lifecycle trace enabled (PIMID_COSIM_TRACE)");
    }

    char msg[512];
    snprintf(msg, sizeof(msg),
             "ZSim+QEMU plugin loaded: %u cores (%u host, %u device), %u MHz, phase=%u\n"
             "  Config: %s\n  Output: %s\n",
             zinfo->numCores, nHostCores, nDeviceCores,
             zinfo->freqMHz, zinfo->phaseLength,
             cfgPath, outputDir);
    qemu_plugin_outs(msg);

    /* Register remaining QEMU callbacks (vcpu_init/exit already registered above) */
    qemu_plugin_register_vcpu_tb_trans_cb(id, tb_trans_cb);
    qemu_plugin_register_vcpu_syscall_cb(id, syscall_cb);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

    return 0;
}
