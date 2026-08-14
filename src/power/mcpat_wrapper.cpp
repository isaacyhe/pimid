#include "power/mcpat_wrapper.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <stdexcept>
#include <vector>
#include <climits>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fcntl.h>

#include "globalvar.h"
#include "XML_Parse.h"
#include "processor.h"

namespace pimid {

//=============================================================================
// McPATWrapper Implementation
//=============================================================================

McPATWrapper::McPATWrapper(const SystemConfig& config)
    : config_(config)
    , mcpat_parser_(nullptr)
    , mcpat_processor_(nullptr)
    , total_cycles_(0)
    , busy_cycles_(0)
    , total_instructions_(0)
    , l1i_reads_(0)
    , l1i_read_misses_(0)
    , l1d_reads_(0)
    , l1d_writes_(0)
    , l1d_read_misses_(0)
    , l1d_write_misses_(0)
    , l2_reads_(0)
    , l2_writes_(0)
    , l2_read_misses_(0)
    , l2_write_misses_(0)
    , l3_reads_(0)
    , l3_writes_(0)
    , l3_read_misses_(0)
    , l3_write_misses_(0)
    , mc_reads_(0)
    , mc_writes_(0)
    , device_profile_(DeviceProfile::DEVICE_INORDER)
    , initialized_(false)
    , valid_(false)
    , power_computed_(false)
    , user_provided_xml_(!config.xml_file.empty())
    , error_message_("")
{
}

McPATWrapper::~McPATWrapper() {
    delete mcpat_processor_;
    delete mcpat_parser_;
    mcpat_processor_ = nullptr;
    mcpat_parser_ = nullptr;
}

void McPATWrapper::initialize() {
    if (initialized_) {
        std::cerr << "[McPATWrapper] Warning: Already initialized" << std::endl;
        return;
    }

    validateConfiguration();
    if (!valid_) {
        throw std::runtime_error("[McPATWrapper] Invalid configuration: " + error_message_);
    }

    try {
        createMcPATInput();
        initialized_ = true;

        std::cout << "[McPATWrapper] Initialized with:" << std::endl;
        std::cout << "  Cores: " << config_.num_cores << std::endl;
        std::cout << "  Core Clock: " << config_.core_clock_mhz << " MHz" << std::endl;
        std::cout << "  L1I/L1D: " << (config_.l1i_size_bytes/1024) << "/"
                  << (config_.l1d_size_bytes/1024) << " KB" << std::endl;
        std::cout << "  L2: " << (config_.l2_size_bytes/1024) << " KB" << std::endl;
        std::cout << "  L3: " << (config_.l3_size_bytes/(1024*1024)) << " MB" << std::endl;
        std::cout << "  Technology: " << config_.tech_node_nm << " nm" << std::endl;
        if (device_profile_ == DeviceProfile::OOO)
            std::cout << "  Profile: HOST_OoO (x86 out-of-order)" << std::endl;
        else if (device_profile_ == DeviceProfile::DEVICE_ALU)
            std::cout << "  Profile: DEVICE_ALU (no caches)" << std::endl;
        else
            std::cout << "  Profile: DEVICE_INORDER" << std::endl;

    } catch (const std::exception& e) {
        valid_ = false;
        error_message_ = std::string("Initialization failed: ") + e.what();
        throw;
    }
}

void McPATWrapper::reconfigure(const SystemConfig& config) {
    config_ = config;
    initialized_ = false;
    power_computed_ = false;
    initialize();
}

void McPATWrapper::validateConfiguration() {
    valid_ = true;
    error_message_ = "";

    if (config_.num_cores < 1 || config_.num_cores > 1024) {
        valid_ = false;
        error_message_ = "Number of cores out of range (1-1024)";
        return;
    }

    if (config_.core_clock_mhz < 100 || config_.core_clock_mhz > 10000) {
        valid_ = false;
        error_message_ = "Core clock out of range (100-10000 MHz)";
        return;
    }

    if (config_.tech_node_nm < 7 || config_.tech_node_nm > 90) {
        valid_ = false;
        error_message_ = "Technology node out of range (7nm - 90nm)";
        return;
    }
}

void McPATWrapper::setTotalCycles(uint64_t cycles) {
    total_cycles_ = cycles;
    power_computed_ = false;
}

void McPATWrapper::setBusyCycles(uint64_t cycles) {
    busy_cycles_ = cycles;
    power_computed_ = false;
}

void McPATWrapper::setTotalInstructions(uint64_t instructions) {
    total_instructions_ = instructions;
}

void McPATWrapper::setMeasuredCoreActivity(uint64_t uops, uint64_t branches,
                                           uint64_t mispredicted) {
    meas_uops_ = uops;
    meas_branches_ = branches;
    meas_mispred_ = mispredicted;
    power_computed_ = false;
}

void McPATWrapper::setL1IAccesses(uint64_t reads, uint64_t read_misses) {
    l1i_reads_ = reads;
    l1i_read_misses_ = read_misses;
    power_computed_ = false;
}

void McPATWrapper::setL1DAccesses(uint64_t reads, uint64_t writes,
                                   uint64_t read_misses, uint64_t write_misses) {
    l1d_reads_ = reads;
    l1d_writes_ = writes;
    l1d_read_misses_ = read_misses;
    l1d_write_misses_ = write_misses;
    power_computed_ = false;
}

void McPATWrapper::setL2Accesses(uint64_t reads, uint64_t writes,
                                  uint64_t read_misses, uint64_t write_misses) {
    l2_reads_ = reads;
    l2_writes_ = writes;
    l2_read_misses_ = read_misses;
    l2_write_misses_ = write_misses;
    power_computed_ = false;
}

void McPATWrapper::setL3Accesses(uint64_t reads, uint64_t writes,
                                  uint64_t read_misses, uint64_t write_misses) {
    l3_reads_ = reads;
    l3_writes_ = writes;
    l3_read_misses_ = read_misses;
    l3_write_misses_ = write_misses;
    power_computed_ = false;
}

void McPATWrapper::setMemControllerAccesses(uint64_t reads, uint64_t writes) {
    mc_reads_ = reads;
    mc_writes_ = writes;
    power_computed_ = false;
}

void McPATWrapper::setMCTechParams(const MCTechParams& params) {
    mc_tech_ = params;
    power_computed_ = false;
}

void McPATWrapper::setNoCLevels(const std::vector<NoCLevelConfig>& levels) {
    noc_levels_ = levels;
    power_computed_ = false;
}

void McPATWrapper::setNoCActivity(const NoCActivityStats& stats) {
    noc_activity_ = stats;
    power_computed_ = false;
}

/* 1.11.16 (verification audit): ONE authority for the DRAM-periphery family
 * factors. They were duplicated as literals in computePower's block-split,
 * which is exactly the aggregate-vs-parts drift trap 1.11.12 removed by
 * moving the transform into the tool -- a future edit to the emitted factors
 * would have left the printed core split scaled by stale numbers. */
static void periphFamilyFactors(int table_nm, double& fa, double& fd, double& fl) {
    if (table_nm == 32) { fa = 2.46; fd = 0.66; fl = 3.1e-6; }
    else                { fa = 2.44; fd = 0.82; fl = 1.0e-5; }
}

double McPATWrapper::linkEnergyPJPerBit(const std::string& link_type) {
    /* 1.11.15 (audit): match the TIMING side's vocabulary by family --
     * cxl_2_0/cxl_3_0, nvlink_3_0/4_0/c2c and interposer are accepted link
     * types that this table rejected, aborting completed co-sims at
     * power-analysis time. Family prefix match; interposer is the
     * silicon-interposer parallel link (HBM-class, no SerDes), published
     * ballpark ~0.5 pJ/bit. ualink and other unknowns return -1: the caller
     * warns and prices the transfer at zero rather than killing the run. */
    if (link_type.rfind("pcie_gen3", 0) == 0) return 5.0;
    if (link_type.rfind("pcie_gen4", 0) == 0) return 6.0;
    if (link_type.rfind("pcie_gen5", 0) == 0) return 7.0;
    if (link_type.rfind("cxl", 0)    == 0) return 8.4;  // gen5 PHY + coherence delta
    if (link_type.rfind("nvlink", 0) == 0) return 1.3;
    if (link_type.rfind("interposer", 0) == 0) return 0.5;
    return -1.0;
}

void McPATWrapper::setPCIeStats(const PCIeStats& stats) {
    pcie_stats_ = stats;
    power_computed_ = false;
}

void McPATWrapper::setDeviceProfile(DeviceProfile profile) {
    device_profile_ = profile;
    power_computed_ = false;
}

void McPATWrapper::createMcPATInput() {
    std::cout << "[McPATWrapper] Creating McPAT configuration" << std::endl;

    if (!config_.xml_file.empty() && user_provided_xml_) {
        std::cout << "  Using XML file: " << config_.xml_file << std::endl;
        valid_ = true;
        return;
    }

    // Generate XML configuration from parameters
    std::string xml_content = generateXMLConfig();

    // Write to temporary file for McPAT
    // Unique per call so cosim per-node analysis doesn't overwrite the host
    // XML before we can examine it (diagnostic only — no functional effect).
    static int xml_call_idx = 0;
    std::string temp_xml = "/tmp/mcpat_input_" +
                           std::to_string(xml_call_idx++) + ".xml";
    std::ofstream xml_file(temp_xml);
    if (!xml_file) {
        throw std::runtime_error("Failed to create McPAT XML file");
    }

    xml_file << xml_content;
    xml_file.close();

    config_.xml_file = temp_xml;
    valid_ = true;

    std::cout << "  Generated XML configuration: " << temp_xml << std::endl;
}

void McPATWrapper::runMcPAT() {
    // CACTI reliably supports tech nodes 22-180nm. Clamp before XML generation.
    if (config_.tech_node_nm < 22) {
        std::cerr << "[McPATWrapper] Warning: " << config_.tech_node_nm
                  << "nm not supported by CACTI (min 22nm). Clamping to 22nm for power estimation."
                  << std::endl;
        config_.tech_node_nm = 22;
    } else if (config_.tech_node_nm > 180) {
        std::cerr << "[McPATWrapper] Warning: " << config_.tech_node_nm
                  << "nm not supported by CACTI (max 180nm). Clamping to 180nm for power estimation."
                  << std::endl;
        config_.tech_node_nm = 180;
    }

    // Regenerate XML with current stats (uses clamped tech_node_nm)
    createMcPATInput();

    if (config_.xml_file.empty()) {
        throw std::runtime_error("[McPATWrapper] No XML file available for McPAT");
    }

    // Save CWD and stdout early so they can be restored in both success and error paths
    char saved_cwd[PATH_MAX];
    bool cwd_saved = (getcwd(saved_cwd, sizeof(saved_cwd)) != nullptr);
    std::streambuf* orig_cout = std::cout.rdbuf();

    try {
        // Clean up any previous McPAT objects
        delete mcpat_processor_;
        mcpat_processor_ = nullptr;
        delete mcpat_parser_;
        mcpat_parser_ = nullptr;

        // McPAT global: optimize for target clock rate
        opt_for_clk = true;

        // Parse XML
        mcpat_parser_ = new ParseXML();
        std::string xml_path = config_.xml_file;
        std::vector<char> path_buf(xml_path.begin(), xml_path.end());
        path_buf.push_back('\0');
        mcpat_parser_->parse(path_buf.data());

        // CACTI reads tech_params/*.dat via relative paths — chdir to CACTI data dir
#ifdef CACTI_DATA_DIR
        if (chdir(CACTI_DATA_DIR) != 0) {
            std::cerr << "[McPATWrapper] Warning: Could not chdir to CACTI data directory: "
                      << CACTI_DATA_DIR << std::endl;
        }
#endif

        // Suppress McPAT/CACTI verbose output.
        // When running inside a subprocess-isolated McPAT child (env var set by
        // computePower()), skip the rdbuf swap entirely: CACTI may call exit(0)
        // on error_checking() failure, and the atexit cleanup of std::cout
        // would then dereference an rdbuf whose stack lifetime ended via the
        // throw/unwind path — producing a spurious SIGSEGV in the child even
        // when CACTI itself exited cleanly. fd-level redirection (done by the
        // child) handles silencing for the subprocess case.
        const bool in_isolated_child = (getenv("PIMID_MCPAT_CHILD") != nullptr);
        std::ostringstream suppress;
        if (!in_isolated_child) {
            std::cout.rdbuf(suppress.rdbuf());
        }

        // Processor constructor does all computation
        mcpat_processor_ = new Processor(mcpat_parser_);

        // Restore stdout
        if (!in_isolated_child) {
            std::cout.rdbuf(orig_cout);
        }

        // Restore original working directory
        if (cwd_saved) {
            if (chdir(saved_cwd) != 0) { /* ignore */ }
        }

        valid_ = true;
        std::cout << "[McPATWrapper] McPAT power analysis complete" << std::endl;
    } catch (const std::exception& e) {
        // CRITICAL: Restore stdout and CWD before propagating error
        std::cout.rdbuf(orig_cout);
        if (cwd_saved) {
            if (chdir(saved_cwd) != 0) { /* ignore */ }
        }

        delete mcpat_processor_;
        mcpat_processor_ = nullptr;
        delete mcpat_parser_;
        mcpat_parser_ = nullptr;
        valid_ = false;
        throw std::runtime_error(std::string("[McPATWrapper] McPAT failed: ") + e.what());
    } catch (...) {
        std::cout.rdbuf(orig_cout);
        if (cwd_saved) {
            if (chdir(saved_cwd) != 0) { /* ignore */ }
        }

        delete mcpat_processor_;
        mcpat_processor_ = nullptr;
        delete mcpat_parser_;
        mcpat_parser_ = nullptr;
        valid_ = false;
        throw std::runtime_error("[McPATWrapper] McPAT failed with unknown error");
    }
}

// ---------------------------------------------------------------------------
// Subprocess-isolated power analysis.
//
// McPAT links CACTI 6.5-P, which carries non-reentrant globals (interface_ip
// in particular). A second McPAT::Processor() constructor in the same process
// inherits dirty globals from the first call and reliably crashes inside CACTI
// (e.g. "Must have at least one port") during dual-McPAT cosim runs. Restarting
// the process between calls is the only realistic isolation for that library.
//
// Implementation: fork() per computePower(). The child runs the existing
// in-process McPAT pipeline and serializes the result-cache fields it would
// have populated (POD doubles + a small vector of PowerMetrics) to a temp
// file, then _exits. The parent waits, reads the blob, populates its own
// cache. Existing getXxxPower()/area accessors are unchanged because they
// read from those cache fields, not the Processor object itself.
//
// On child crash (non-zero exit / signal), the parent throws — same surface
// as the original in-process exception path.
// ---------------------------------------------------------------------------
namespace {
struct ResultBlob {
    // Cached PowerMetrics for ComponentType enum entries we extract.
    // 7 entries today: CORE, L1_CACHE, L2_CACHE, L3_CACHE, MEMORY_CONTROLLER,
    // NOC, PCIE. Fixed-size to keep the blob layout trivially POD.
    static constexpr int kNumComponents = 7;
    McPATWrapper::PowerMetrics component_power[kNumComponents];
    McPATWrapper::PowerMetrics system_power;
    double peak_power;
    double core_area_mm2;
    double l2_area_mm2;
    double l3_area_mm2;
    double noc_area_mm2;
    double mc_area_mm2;
    double total_area_mm2;
    int num_noc_levels;          // entries that follow, can be 0
    /* 1.9.36: per-core intra-core split (dynamic+leakage, watts, ONE core).
     * Needed because a processing element declared as an ALU is emitted with
     * byte-identical input to an in-order core, so it is charged for an
     * instruction fetch unit, address translation, a load/store unit, a
     * scheduler and a floating-point unit it does not have. Replacing that with
     * a real ALU model cannot be VALIDATED without first knowing how much of the
     * present figure belongs to the parts being removed. Only the child holds
     * the model object, and its standard output is sent to the null device on
     * purpose, so the split has to travel back the same way the areas do. */
    double core_ifu_w, core_lsu_w, core_mmu_w, core_exu_w, core_pipe_w, core_undiff_w;
    // After this struct, num_noc_levels * sizeof(PowerMetrics) bytes follow.
};
constexpr McPATWrapper::ComponentType kComponentOrder[ResultBlob::kNumComponents] = {
    McPATWrapper::ComponentType::CORE,
    McPATWrapper::ComponentType::L1_CACHE,
    McPATWrapper::ComponentType::L2_CACHE,
    McPATWrapper::ComponentType::L3_CACHE,
    McPATWrapper::ComponentType::MEMORY_CONTROLLER,
    McPATWrapper::ComponentType::NOC,
    McPATWrapper::ComponentType::PCIE,
};
}  // namespace

void McPATWrapper::computePower() {
    if (!initialized_) {
        throw std::runtime_error("[McPATWrapper] Not initialized");
    }

    // Generate the XML in the parent so the child reads the same file path
    // we configured (avoids races on /tmp/mcpat_input.xml when callers chain
    // host/device power analyses).
    createMcPATInput();

    char blob_path[64];
    std::snprintf(blob_path, sizeof(blob_path),
                  "/tmp/pimid_mcpat_blob_%d.bin", (int)getpid());

    pid_t child = fork();
    if (child < 0) {
        throw std::runtime_error("[McPATWrapper] fork() failed for McPAT isolation");
    }

    if (child == 0) {
        // Child: do the in-process McPAT work, serialize, exit. Any CACTI
        // global pollution dies with this process.
        //
        // Silence McPAT/CACTI chatter at the fd level (not std::cout.rdbuf).
        // CACTI calls exit(0) on error_checking() failure; std::cout cleanup
        // during atexit then reads from an rdbuf whose stack lifetime ended
        // inside runMcPAT() — that produces a spurious SIGSEGV in the child
        // even when CACTI's own logic exited cleanly. fd-level redirect
        // survives atexit because it does not require any C++ object state.
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            close(devnull);
        }
        // Tell runMcPAT() to skip the std::cout.rdbuf swap (would crash in
        // atexit if CACTI calls exit() during the new Processor() ctor).
        setenv("PIMID_MCPAT_CHILD", "1", 1);
        try {
            runMcPAT();
            extractResults();

            FILE* f = std::fopen(blob_path, "wb");
            if (!f) std::_Exit(20);

            ResultBlob blob{};
            for (int i = 0; i < ResultBlob::kNumComponents; i++) {
                auto it = component_power_.find(kComponentOrder[i]);
                if (it != component_power_.end()) blob.component_power[i] = it->second;
            }
            blob.system_power    = system_power_;
            blob.peak_power      = peak_power_;
            blob.core_area_mm2   = mcpat_core_area_mm2_;
            blob.l2_area_mm2     = mcpat_l2_area_mm2_;
            blob.l3_area_mm2     = mcpat_l3_area_mm2_;
            blob.noc_area_mm2    = mcpat_noc_area_mm2_;
            blob.mc_area_mm2     = mcpat_mc_area_mm2_;
            blob.total_area_mm2  = mcpat_total_area_mm2_;
            blob.num_noc_levels  = static_cast<int>(noc_level_power_.size());
            blob.core_ifu_w = blob.core_lsu_w = blob.core_mmu_w = 0.0;
            blob.core_exu_w = blob.core_pipe_w = blob.core_undiff_w = 0.0;
            if (!mcpat_processor_->cores.empty()) {
                const Core& c0 = *mcpat_processor_->cores[0];
                auto w = [](const Component* p) -> double {
                    return p ? (p->rt_power.readOp.dynamic + p->rt_power.readOp.leakage) : 0.0;
                };
                /* 1.11.4: block weights carry the family core-power ratio so
                 * the printed split is consistent with the scaled core total
                 * (audit: they were captured unscaled). */
                /* 1.11.15 (audit): fam_core_power_ratio_ lost its assignment
                 * in the 1.11.12 migration, so the blocks printed logic-priced
                 * values under a family-priced total. Scale each block's
                 * dynamic and leakage by the family factors DIRECTLY -- the
                 * per-core sub-objects are not transformed by processor.cc's
                 * applyFam (it works on the Processor-level aggregates). */
                double fdW = 1.0, flW = 1.0;
                if (config_.process_family == 1) {
                    double faW = 1.0;   // 1.11.16: same authority as the XML emission
                    periphFamilyFactors(config_.dram_periph_table_nm, faW, fdW, flW);
                    (void)faW;
                }
                auto wf = [&](const Component* p) -> double {
                    if (!p) return 0.0;
                    double dyn = p->rt_power.readOp.dynamic;
                    double leak = p->rt_power.readOp.leakage;
                    if (!std::isfinite(dyn)) dyn = 0.0;
                    if (!std::isfinite(leak)) leak = 0.0;
                    return dyn * fdW + leak * flW;
                };
                blob.core_ifu_w    = wf(c0.ifu);
                blob.core_lsu_w    = wf(c0.lsu);
                blob.core_mmu_w    = wf(c0.mmu);
                blob.core_exu_w    = wf(c0.exu);
                blob.core_pipe_w   = wf(c0.corepipe);
                blob.core_undiff_w = wf(c0.undiffCore);
            }

            std::fwrite(&blob, sizeof(blob), 1, f);
            if (blob.num_noc_levels > 0) {
                std::fwrite(noc_level_power_.data(),
                            sizeof(PowerMetrics), blob.num_noc_levels, f);
            }
            std::fflush(f);
            std::fclose(f);
            std::_Exit(0);
        } catch (const std::exception& e) {
            // Write a one-line diagnostic to the original stderr so the
            // parent's output reflects WHY the child failed (otherwise we
            // see just "exit code 21" and lose the McPAT/CACTI message).
            const char* prefix = "[McPATWrapper child] exception: ";
            (void)!write(STDERR_FILENO, prefix, std::strlen(prefix));
            const char* msg = e.what();
            (void)!write(STDERR_FILENO, msg, std::strlen(msg));
            (void)!write(STDERR_FILENO, "\n", 1);
            std::_Exit(21);
        } catch (...) {
            std::_Exit(21);
        }
    }

    // Parent: wait and reap.
    int status = 0;
    pid_t w;
    do { w = waitpid(child, &status, 0); } while (w < 0 && errno == EINTR);
    if (w < 0) {
        std::remove(blob_path);
        throw std::runtime_error("[McPATWrapper] waitpid failed for McPAT child");
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::remove(blob_path);
        std::string detail;
        if (WIFSIGNALED(status)) {
            detail = "killed by signal " + std::to_string(WTERMSIG(status));
        } else if (WIFEXITED(status)) {
            detail = "exit code " + std::to_string(WEXITSTATUS(status));
        } else {
            detail = "unknown status";
        }
        throw std::runtime_error(
            "[McPATWrapper] McPAT child failed (" + detail + ")");
    }

    // Read the serialized result blob back into our cache fields.
    FILE* f = std::fopen(blob_path, "rb");
    if (!f) {
        throw std::runtime_error("[McPATWrapper] Could not open result blob");
    }
    ResultBlob blob{};
    if (std::fread(&blob, sizeof(blob), 1, f) != 1) {
        std::fclose(f);
        std::remove(blob_path);
        throw std::runtime_error("[McPATWrapper] Short read on result blob");
    }
    component_power_.clear();
    for (int i = 0; i < ResultBlob::kNumComponents; i++) {
        component_power_[kComponentOrder[i]] = blob.component_power[i];
    }
    system_power_         = blob.system_power;
    peak_power_           = blob.peak_power;
    mcpat_core_area_mm2_  = blob.core_area_mm2;
    mcpat_l2_area_mm2_    = blob.l2_area_mm2;
    mcpat_l3_area_mm2_    = blob.l3_area_mm2;
    mcpat_noc_area_mm2_   = blob.noc_area_mm2;
    mcpat_mc_area_mm2_    = blob.mc_area_mm2;
    core_ifu_w_    = blob.core_ifu_w;      // 1.9.36
    core_lsu_w_    = blob.core_lsu_w;
    core_mmu_w_    = blob.core_mmu_w;
    core_exu_w_    = blob.core_exu_w;
    core_pipe_w_   = blob.core_pipe_w;
    core_undiff_w_ = blob.core_undiff_w;
    /* 1.9.36: report the split for a device profile. An ALU datapath has a
     * register file, an arithmetic unit and a result bus -- roughly the
     * execution unit alone. Everything else listed here is charged to it today
     * because the generator emits identical input for an ALU and an in-order
     * core. The percentage is what an ALU model has to remove, and is the
     * yardstick that model will be validated against. */
    if (device_profile_ != DeviceProfile::OOO) {
        double tot = core_ifu_w_ + core_lsu_w_ + core_mmu_w_
                   + core_exu_w_ + core_pipe_w_ + core_undiff_w_;
        double absent = core_ifu_w_ + core_lsu_w_ + core_mmu_w_;
        if (tot > 0.0) {
            std::cout << "  [CoreBreakdown] per core, "
                      << (device_profile_ == DeviceProfile::DEVICE_ALU
                              ? "DEVICE_ALU"
                          : (device_profile_ == DeviceProfile::OOO)
                              ? "OOO" : "DEVICE_INORDER")
                      << ": ifu=" << core_ifu_w_ << "W lsu=" << core_lsu_w_
                      << "W mmu=" << core_mmu_w_ << "W exu=" << core_exu_w_
                      << "W pipe=" << core_pipe_w_ << "W undiff=" << core_undiff_w_
                      << "W" << std::endl;
            /* Report the pieces, NOT a single removable percentage.
             *
             * Two McPAT conventions make a naive share misleading, and both were
             * verified in core.cc rather than assumed:
             *   - corepipe is APPORTIONED into ifu/lsu/exu/mmu/rnu (:3959-4023),
             *     so it reads zero afterwards and its cost is already inside
             *     those four. An ALU processing element still HAS a pipeline, so
             *     that share is not removable.
             *   - undiffCore is added straight to the CORE total (:4028) and to
             *     no sub-block, so the blocks do not sum to the core.
             * Consequently ifu+lsu+mmu is an upper bound on what could be
             * removed, and a loose one: instruction supply and address
             * generation are genuinely needed and sit inside it. */
            double core_tot = component_power_.count(ComponentType::CORE)
                ? component_power_[ComponentType::CORE].total_power : 0.0;
            std::cout << "  [CoreBreakdown] blocks sum=" << tot << "W";
            if (core_tot > 0.0) std::cout << "  core total=" << core_tot << "W"
                                          << " (undiffCore is in the core total, not in any block)";
            std::cout << std::endl;
            std::cout << "  [CoreBreakdown] ifu+lsu+mmu = " << absent << "W = "
                      << (100.0 * absent / tot) << "% of the block sum -- an UPPER BOUND on what an "
                         "ALU model removes, not the error: it includes apportioned pipeline power, "
                         "and instruction supply and address generation are still required."
                      << std::endl;
        }
    }
    mcpat_total_area_mm2_ = blob.total_area_mm2;

    noc_level_power_.clear();
    if (blob.num_noc_levels > 0) {
        noc_level_power_.resize(blob.num_noc_levels);
        if (std::fread(noc_level_power_.data(),
                       sizeof(PowerMetrics), blob.num_noc_levels, f)
            != static_cast<size_t>(blob.num_noc_levels)) {
            std::fclose(f);
            std::remove(blob_path);
            throw std::runtime_error("[McPATWrapper] Short read on NoC level blob");
        }
    }
    std::fclose(f);
    std::remove(blob_path);

    power_computed_ = true;

    std::cout << "[McPATWrapper] Power analysis complete" << std::endl;
    std::cout << "  Total Power: " << system_power_.total_power << " W" << std::endl;
}

void McPATWrapper::extractResults() {
    if (!mcpat_processor_) {
        throw std::runtime_error("[McPATWrapper] McPAT processor object is null — cannot extract results");
    }

    // Extract real results from McPAT Processor object
    bool long_channel = (config_.longer_channel_device != 0);

    // Helper lambda to extract power from a McPAT Component
    auto extractComponent = [long_channel](const Component& comp) -> PowerMetrics {
        PowerMetrics pm;
        double dyn = comp.rt_power.readOp.dynamic;
        pm.runtime_dynamic = (std::isfinite(dyn)) ? dyn : 0.0;
        double sub = long_channel
            ? comp.power.readOp.longer_channel_leakage
            : comp.power.readOp.leakage;
        double gate = comp.power.readOp.gate_leakage;
        pm.subthreshold_leakage = (std::isfinite(sub)) ? sub : 0.0;   // 1.11.4: guarded like the peak path
        pm.gate_leakage         = (std::isfinite(gate)) ? gate : 0.0;
        double pgl = long_channel
            ? comp.power.readOp.power_gated_with_long_channel_leakage
            : comp.power.readOp.power_gated_leakage;                   // 1.11.8
        pm.power_gated_leakage = (std::isfinite(pgl)) ? pgl : 0.0;
        pm.total_leakage = pm.subthreshold_leakage + pm.gate_leakage;
        pm.total_dynamic = pm.runtime_dynamic;
        pm.total_power = pm.total_dynamic + pm.total_leakage;
        return pm;
    };

    // Core (includes L1 caches in McPAT's model)
    component_power_[ComponentType::CORE] = extractComponent(mcpat_processor_->core);
    // L1 is embedded in core — zero out separate L1 to avoid double-counting
    component_power_[ComponentType::L1_CACHE] = PowerMetrics();

    // L2
    component_power_[ComponentType::L2_CACHE] = extractComponent(mcpat_processor_->l2);

    // L3
    component_power_[ComponentType::L3_CACHE] = extractComponent(mcpat_processor_->l3);

    // Memory Controller
    component_power_[ComponentType::MEMORY_CONTROLLER] = extractComponent(mcpat_processor_->mcs);

    // NoC — use Processor-level aggregate (already normalized energy→Watts)
    // The per-NoC nocs[i]->rt_power stores raw energy; Processor multiplies
    // by 1/executionTime during aggregation into noc.rt_power (Watts).
    noc_level_power_.clear();
    if (mcpat_processor_->numNOC > 0) {
        // Per-level breakdown: divide each nocs[i] energy by executionTime
        for (int i = 0; i < mcpat_processor_->numNOC; i++) {
            double execTime = mcpat_processor_->nocs[i]->nocdynp.executionTime;
            if (execTime <= 0) execTime = 1.0;
            PowerMetrics level_pm;
            double rawDyn = mcpat_processor_->nocs[i]->rt_power.readOp.dynamic;
            level_pm.runtime_dynamic = (std::isfinite(rawDyn)) ? rawDyn / execTime : 0.0;
            level_pm.subthreshold_leakage = long_channel
                ? mcpat_processor_->nocs[i]->power.readOp.longer_channel_leakage
                : mcpat_processor_->nocs[i]->power.readOp.leakage;
            level_pm.gate_leakage = mcpat_processor_->nocs[i]->power.readOp.gate_leakage;
            level_pm.total_leakage = level_pm.subthreshold_leakage + level_pm.gate_leakage;
            level_pm.total_dynamic = level_pm.runtime_dynamic;
            level_pm.total_power = level_pm.total_dynamic + level_pm.total_leakage;
            noc_level_power_.push_back(level_pm);
        }
        // Aggregate: use Processor's pre-computed noc (already in Watts)
        component_power_[ComponentType::NOC] = extractComponent(mcpat_processor_->noc);
    } else {
        component_power_[ComponentType::NOC] = PowerMetrics();
    }

    // PCIe
    if (mcpat_processor_->pcie) {
        /* 1.11.7 (audit blocker): read the AGGREGATED component (pcies),
         * not the raw controller object -- the Processor multiplies by
         * number_units x clockRate during aggregation, and reading the raw
         * object under-reported dynamic by exactly that factor. */
        component_power_[ComponentType::PCIE] = extractComponent(mcpat_processor_->pcies);
    }

    // Store areas from McPAT (um^2 -> mm^2)
    mcpat_core_area_mm2_ = mcpat_processor_->core.area.get_area() * 1e-6;
    mcpat_l2_area_mm2_ = mcpat_processor_->l2.area.get_area() * 1e-6;
    mcpat_l3_area_mm2_ = mcpat_processor_->l3.area.get_area() * 1e-6;
    mcpat_noc_area_mm2_ = mcpat_processor_->noc.area.get_area() * 1e-6;
    mcpat_mc_area_mm2_ = mcpat_processor_->mcs.area.get_area() * 1e-6;
    mcpat_total_area_mm2_ = mcpat_processor_->area.get_area() * 1e-6;

    /* 1.11.12: the DRAM-periphery family MOVED INTO McPAT (processor.cc,
     * driven by the dram_periph_* XML params). What used to be a
     * post-scaling of extracted results is now a property of the machine
     * McPAT priced, so components, aggregate and areas are transformed by
     * the tool that owns them -- the borders rule, and the end of a class
     * of aggregate-vs-parts drift. Extraction reads what McPAT produced. */

    /* 1.11.8 (#84): power-gating interpolation at the tool boundary.
     * Endpoints are BOTH the tool's (active leakage and power_gated_leakage
     * from the same McPAT/CACTI run); the measured residency r weights them:
     * leak_eff = active*(1-r) + gated*r. Applied only to components the
     * described design gates (pg flags).
     *
     * 1.11.16 (verification audit) -- three corrections here.
     * (1) The interpolation runs on SUBTHRESHOLD leakage only. The gated
     *     endpoint is a subthreshold quantity (every McPAT producer derives
     *     it from power.readOp.leakage alone), while the old active side
     *     used sub+gate: the bases differed by the gate-leakage share, and
     *     gate-oxide tunnelling -- which a sleep rail does NOT remove -- was
     *     credited with PG savings. Gate leakage now stays at its active
     *     value.
     * (2) The 1.11.15 comment claimed savings were CONSERVATIVE because
     *     blocks without sleep-tx models "keep active leakage". They kept
     *     ZERO (the dead-endpoint defect, fixed in array.cc this release),
     *     which is anti-conservative; with the uniform analytic endpoint the
     *     residual is Vcc_min/Vdd (~0.35) of active, McPAT's own retention
     *     floor.
     * (3) Diagnostics go to STDERR: extractResults runs in the forked child
     *     whose stdout is /dev/null, so the 1.11.15 inversion warning was
     *     unreachable. The check is also two-sided now -- a near-zero gated
     *     endpoint (the failure mode that actually shipped) warns just like
     *     an inverted one. */
    {
        auto applyPG = [&](ComponentType t, double r) -> double {
            if (r <= 0.0) return 1.0;
            if (r > 1.0) r = 1.0;
            PowerMetrics& pm = component_power_[t];
            double active = pm.subthreshold_leakage;
            double gated  = pm.power_gated_leakage;
            if (gated > active) {
                std::cerr << "[pg] WARNING: gated leakage (" << gated
                          << " W) exceeds active subthreshold (" << active
                          << " W) -- endpoints on different bases; clamping, "
                             "no savings credited for this component."
                          << std::endl;
                gated = active;
            } else if (active > 0.0 && gated < 0.05 * active) {
                std::cerr << "[pg] WARNING: gated leakage (" << gated
                          << " W) is below 5% of active (" << active
                          << " W) -- a zero/degenerate gated endpoint (the "
                             "1.11.15 dead-endpoint class) credits near-total "
                             "leakage elimination; check the tool endpoints."
                          << std::endl;
            }
            double eff = active * (1.0 - r) + gated * r;
            double scale = (active > 0.0) ? eff / active : 1.0;
            pm.subthreshold_leakage *= scale;
            pm.total_leakage = pm.subthreshold_leakage + pm.gate_leakage;
            pm.total_power   = pm.total_dynamic + pm.total_leakage;
            return scale;
        };
        if (pg_spec_.pg_core) applyPG(ComponentType::CORE, pg_spec_.r_core);
        if (pg_spec_.pg_core) applyPG(ComponentType::L2_CACHE, pg_spec_.r_core);
        if (pg_spec_.pg_core) applyPG(ComponentType::L3_CACHE, pg_spec_.r_core);
        if (pg_spec_.pg_noc) {
            double s = applyPG(ComponentType::NOC, pg_spec_.r_noc);
            /* 1.11.16: the per-level NoC breakdown must ride the same scale
             * or the printed levels sum to the pre-gating leakage while the
             * aggregate shows the post-gating figure -- the aggregate-vs-
             * parts drift this train exists to end, one layer up. */
            for (auto& lv : noc_level_power_) {
                lv.subthreshold_leakage *= s;
                lv.total_leakage = lv.subthreshold_leakage + lv.gate_leakage;
                lv.total_power   = lv.total_dynamic + lv.total_leakage;
            }
        }
        if (pg_spec_.pg_mc)   applyPG(ComponentType::MEMORY_CONTROLLER, pg_spec_.r_mc);
    }

    // System total
    system_power_ = PowerMetrics();
    for (const auto& pair : component_power_) {
        system_power_.runtime_dynamic += pair.second.runtime_dynamic;
        system_power_.subthreshold_leakage += pair.second.subthreshold_leakage;
        system_power_.gate_leakage += pair.second.gate_leakage;
        system_power_.total_leakage += pair.second.total_leakage;
    }
    system_power_.total_dynamic = system_power_.runtime_dynamic;
    system_power_.total_power = system_power_.total_dynamic + system_power_.total_leakage;

    // Extract real peak power from McPAT design-time power (power.readOp.dynamic)
    // This is the maximum power assuming all units active at peak frequency,
    // as opposed to rt_power.readOp.dynamic which is scaled by runtime activity.
    bool long_channel_peak = long_channel;
    auto peakDynamic = [](const Component& comp) -> double {
        double d = comp.power.readOp.dynamic;
        return (std::isfinite(d)) ? d : 0.0;
    };
    auto peakLeakage = [long_channel_peak](const Component& comp) -> double {
        double sub = long_channel_peak
            ? comp.power.readOp.longer_channel_leakage
            : comp.power.readOp.leakage;
        double gate = comp.power.readOp.gate_leakage;
        return ((std::isfinite(sub)) ? sub : 0.0) + ((std::isfinite(gate)) ? gate : 0.0);
    };

    double peak_dyn = 0.0;
    double peak_leak = 0.0;
    {
        // 1.11.2/1.11.4: peak reads the raw processor structures, so the CORE
        // share carries the same DRAM-periphery factors as the runtime
        // metrics -- same values, same 80C-row derivation, same plain-leakage
        // rebase (no long-channel stacking) as the block above.
        double core_pd = peakDynamic(mcpat_processor_->core);
        double core_pl = peakLeakage(mcpat_processor_->core);
        // 1.11.12: peak reads the same family-priced structures; no rescale.
        peak_dyn += core_pd;
        peak_leak += core_pl;
    }
    peak_dyn += peakDynamic(mcpat_processor_->l2);
    peak_leak += peakLeakage(mcpat_processor_->l2);
    peak_dyn += peakDynamic(mcpat_processor_->l3);
    peak_leak += peakLeakage(mcpat_processor_->l3);
    peak_dyn += peakDynamic(mcpat_processor_->mcs);
    peak_leak += peakLeakage(mcpat_processor_->mcs);
    peak_dyn += peakDynamic(mcpat_processor_->noc);
    peak_leak += peakLeakage(mcpat_processor_->noc);
    if (mcpat_processor_->pcie) {
        peak_dyn += peakDynamic(mcpat_processor_->pcies);   // 1.11.7: aggregated
        peak_leak += peakLeakage(mcpat_processor_->pcies);
    }
    peak_power_ = peak_dyn + peak_leak;
}

//=============================================================================
// Query functions
//=============================================================================

McPATWrapper::PowerMetrics McPATWrapper::getComponentPower(ComponentType component) const {
    auto it = component_power_.find(component);
    if (it != component_power_.end()) {
        return it->second;
    }
    return PowerMetrics();
}

McPATWrapper::PowerMetrics McPATWrapper::getSystemPower() const {
    return system_power_;
}

double McPATWrapper::getCorePower() const {
    return getComponentPower(ComponentType::CORE).total_power;
}

double McPATWrapper::getCachePower() const {
    double total = 0.0;
    total += getComponentPower(ComponentType::L1_CACHE).total_power;
    total += getComponentPower(ComponentType::L2_CACHE).total_power;
    total += getComponentPower(ComponentType::L3_CACHE).total_power;
    return total;
}

double McPATWrapper::getMemoryControllerPower() const {
    return getComponentPower(ComponentType::MEMORY_CONTROLLER).total_power;
}

double McPATWrapper::getNoCPower() const {
    return getComponentPower(ComponentType::NOC).total_power;
}

double McPATWrapper::getComponentArea(ComponentType component) const {
    if (power_computed_) {   // 1.9.34: see getTotalArea()
        switch (component) {
            case ComponentType::CORE:
                return mcpat_core_area_mm2_;
            case ComponentType::L1_CACHE:
                return 0.0;  // included in core
            case ComponentType::L2_CACHE:
                return mcpat_l2_area_mm2_;
            case ComponentType::L3_CACHE:
                return mcpat_l3_area_mm2_;
            case ComponentType::MEMORY_CONTROLLER:
                return mcpat_mc_area_mm2_;
            case ComponentType::NOC:
                return mcpat_noc_area_mm2_;
            default:
                return 0.0;
        }
    }
    return 0.0;
}

double McPATWrapper::getTotalArea() const {
    /* 1.9.34: gate on power_computed_, not on the Processor object.
     *
     * computePower() became fork()-isolated to survive a co-simulation crash.
     * mcpat_processor_ is only ever assigned inside runMcPAT(), i.e. only in the
     * CHILD; the child extracts the areas and ships them back through the result
     * blob, and the parent caches them into mcpat_*_area_mm2_. But these two
     * accessors kept testing the Processor pointer, which in the parent is
     * permanently null -- so the areas were transported correctly and then
     * discarded, and every reported area was 0.00 mm^2 in BOTH scopes. Every
     * other cached field (system_power_, peak_power_, component_power_) is read
     * without such a guard, which is exactly why power survived and only area
     * was lost. */
    if (power_computed_) {
        return mcpat_total_area_mm2_;
    }
    return 0.0;
}

double McPATWrapper::getPeakPower() const {
    return peak_power_;
}

double McPATWrapper::getEnergyForPeriod(double time_seconds) const {
    return system_power_.total_power * time_seconds;
}

bool McPATWrapper::isValid() const {
    return valid_;
}

std::string McPATWrapper::getErrorMessage() const {
    return error_message_;
}

void McPATWrapper::printDetailedResults() const {
    if (!power_computed_) {
        std::cout << "[McPATWrapper] Power not yet computed" << std::endl;
        return;
    }

    std::cout << "\n=== McPAT Power Analysis Results ===" << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Cores: " << config_.num_cores << " @ " << config_.core_clock_mhz << " MHz" << std::endl;
    std::cout << "  Technology: " << config_.tech_node_nm << " nm" << std::endl;

    printComponentBreakdown();

    // Per-level NoC breakdown
    if (!noc_level_power_.empty() && !noc_levels_.empty()) {
        std::cout << "\nNoC Power Breakdown (" << noc_level_power_.size() << " levels):" << std::endl;
        for (size_t i = 0; i < noc_level_power_.size() && i < noc_levels_.size(); i++) {
            const auto& pm = noc_level_power_[i];
            const auto& cfg = noc_levels_[i];
            std::cout << "  " << cfg.name
                      << " (" << cfg.horizontal_nodes << "x" << cfg.vertical_nodes
                      << (cfg.type == 0 ? " bus" : " NoC") << "):"
                      << "  " << pm.runtime_dynamic << "W dynamic, "
                      << pm.total_leakage << "W leakage" << std::endl;
        }
    }

    std::cout << "\nSystem Totals:" << std::endl;
    std::cout << "  Total Dynamic Power: " << system_power_.total_dynamic << " W" << std::endl;
    std::cout << "  Total Leakage Power: " << system_power_.total_leakage << " W" << std::endl;
    std::cout << "  Total Power: " << system_power_.total_power << " W" << std::endl;
    std::cout << "  Peak Power: " << getPeakPower() << " W" << std::endl;
    std::cout << "  Total Area: " << getTotalArea() << " mm^2" << std::endl;
    std::cout << "=====================================\n" << std::endl;
}

void McPATWrapper::printSummaryLine() const {
    if (!power_computed_) return;
    std::cout << "Power: " << std::fixed << std::setprecision(2)
              << system_power_.total_power << "W ("
              << system_power_.total_dynamic << "W dynamic + "
              << system_power_.total_leakage << "W leakage), Area: "
              << getTotalArea() << " mm^2"
              << std::defaultfloat << std::endl;
}

void McPATWrapper::printComponentBreakdown() const {
    std::cout << "\nComponent Power Breakdown:" << std::endl;

    auto print_component = [](const std::string& name, const PowerMetrics& power) {
        std::cout << "  " << name << ":" << std::endl;
        std::cout << "    Dynamic: " << power.runtime_dynamic << " W" << std::endl;
        std::cout << "    Leakage: " << power.total_leakage << " W" << std::endl;
        std::cout << "    Total: " << power.total_power << " W" << std::endl;
    };

    print_component("Cores", getComponentPower(ComponentType::CORE));
    print_component("L1 Caches", getComponentPower(ComponentType::L1_CACHE));
    print_component("L2 Caches", getComponentPower(ComponentType::L2_CACHE));
    print_component("L3 Cache", getComponentPower(ComponentType::L3_CACHE));
    print_component("Memory Controllers", getComponentPower(ComponentType::MEMORY_CONTROLLER));
    if (config_.has_noc || !noc_levels_.empty()) {
        print_component("NoC", getComponentPower(ComponentType::NOC));
    }
    if (pcie_stats_.number_units > 0) {
        print_component("PCIe", getComponentPower(ComponentType::PCIE));
    }
}

//=============================================================================
// XML Generation for McPAT
//=============================================================================

std::string McPATWrapper::generateXMLConfig() const {
    std::ostringstream xml;

    // ALU-only config: no caches; also skip L3 if size is 0
    bool alu_only = (config_.l1i_size_bytes == 0);
    int num_l2s = alu_only ? 0 : config_.num_cores;
    bool has_l3 = !alu_only && (config_.l3_size_bytes > 0);
    int num_l3s = has_l3 ? 1 : 0;
    int num_cache_levels = alu_only ? 0 : (has_l3 ? 3 : 2);

    // Determine number of NoC instances
    // McPAT XML parser uses positional component counting; with 0 NoCs + 0 L2s +
    // 0 L3s the counter never advances past core0, causing MC parse to fail.
    // Always emit at least 1 NoC (with minimal activity) to keep parsing correct.
    int num_nocs = 1;
    if (!noc_levels_.empty()) {
        num_nocs = static_cast<int>(noc_levels_.size());
    } else if (config_.has_noc) {
        num_nocs = 1;
    }

    // Device profile settings
    /* 1.9.37: an out-of-order PROCESSING ELEMENT is out-of-order too.
     * is_ooo gates every speculative structure in the generated description --
     * machine type, reorder buffer, instruction window, renaming, physical
     * register count, load/store ordering, FP issue width, and the reorder and
     * rename activity statistics. Testing only for the host profile meant a
     * device element declared out-of-order was described with NO reorder
     * buffer, NO instruction window and NO renaming, while the timing model
     * simulated all of it. Two halves of one model describing different
     * machines -- the recurring failure of this release train. */
    bool is_ooo = (device_profile_ == DeviceProfile::OOO);
    /* 1.9.36: DEVICE_ALU is no longer emitted as an in-order core.
     *
     * McPAT offers exactly two core models, OOO and Inorder
     * (basic_components.h:88), and both describe a PROCESSOR -- fetch, decode,
     * control. A processing element here is a DATAPATH. Until now the generator
     * branched only on is_ooo, so an ALU element and an in-order core produced
     * BYTE-IDENTICAL input and the element was charged for a branch predictor,
     * caches, address translation and a scheduler it does not have.
     *
     * This does not invent a third McPAT core type -- it cannot -- but it stops
     * describing the element as something it is not, and makes the description
     * parametric from pe_lanes / pe_element_bits / pe_has_fp / pe_imem_bytes.
     *
     * WHAT THIS DOES NOT FIX, and it is the larger term: McPAT's undifferentiated
     * core is a regression on pipeline depth fitted to 90 nm commercial parts
     * (logic.cc:752+; the in-order branch -2.19*log(d)+6.55 crosses zero at 19.9
     * stages). It accounted for roughly 99% of measured element core power at
     * every profile, because a shallow datapath pipeline is evaluated off the
     * low end of that fit where it is largest. Sizing the structures correctly
     * addresses the other ~1%. Removing the lump needs the element composed from
     * McPAT primitives (ArrayST, FunctionalUnit, interconnect), which inherit no
     * undifferentiated term. Recorded in the release notes rather than hidden. */
    const bool is_alu = (device_profile_ == DeviceProfile::DEVICE_ALU);
    const int  lanes  = (config_.pe_lanes > 0) ? config_.pe_lanes : 1;

    int machine_type = is_ooo ? 0 : 1;
    int x86 = is_ooo ? 1 : 0;
    int rob_size = is_ooo ? 192 : 0;
    int inst_window_size = is_ooo ? 64 : 0;
    int fp_inst_window_size = is_ooo ? 32 : 0;
    int phy_regs_irf = is_ooo ? 180 : 32;
    int phy_regs_frf = is_ooo ? 180 : 32;
    int rename_scheme = is_ooo ? 0 : 0;  // RAT-based for both
    const char* lsu_order = is_ooo ? "OOO" : "inorder";
    int pipeline_depth = is_ooo ? 19 : config_.pipeline_depth;
    int issue_width = is_ooo ? 4 : config_.issue_width;
    // FP issue width: McPAT never defaults fp_issue_width, so it MUST be emitted
    // for the OOO profile or the FPIssueQueue array is built with zero ports and
    // CACTI aborts ("Must have at least one port", exit 21). 2 matches McPAT's
    // own 4-wide x86 OOO references (Xeon, Penryn). Emitted only for is_ooo so
    // the inorder/ALU device XML stays byte-identical (those profiles never
    // build the FP issue queue, so they neither need nor previously emitted it).
    int fp_issue_width = is_ooo ? 2 : 0;
    int store_buffer = is_ooo ? 32 : 4;
    int load_buffer = is_ooo ? 32 : 4;
    int num_alus = config_.num_alus;
    int num_muls = config_.num_muls;
    int num_fpus = config_.num_fpus;
    if (is_alu) {
        /* A datapath: a register file, arithmetic units, a result bus and an
         * instruction store. No speculation, no dynamic scheduling, no caches.
         * Register file scales with lanes -- for a wide element the file and the
         * result bus dominate, which is why real parts share one compute unit
         * between banks rather than widening indefinitely. */
        phy_regs_irf   = 8 * lanes;
        phy_regs_frf   = config_.pe_has_fp ? (8 * lanes) : 1;  // 0 aborts CACTI
        issue_width    = lanes;
        fp_issue_width = config_.pe_has_fp ? lanes : 0;
        store_buffer   = 1;      // request issue only; no queueing, no cache
        load_buffer    = 1;
        lsu_order      = "inorder";
        /* 1.9.32: arithmetic composed from the datapath instead of inherited.
         * The element previously declared one integer ALU, no multiplier and NO
         * FLOATING-POINT UNIT AT ALL -- while the timing model ran stream_triad,
         * gemv and stencil_2d on it, every one of them FP32. The two halves were
         * describing different machines again: the timing side retired
         * single-cycle floating-point operations on hardware the power side said
         * did not exist, so their energy was never charged.
         *
         * One of each unit per lane is what makes a lane a lane: a lane that
         * cannot multiply or cannot do floating point stalls on the kernels we
         * run, and the timing model charges no such stall. The multiplier is not
         * optional for the same reason -- gemv and stencil are multiply-
         * accumulate in their inner loop.
         *
         * pe_has_fp exists because an integer-only element is a real design
         * point (histogram and bfs are INT32 throughout); it is a configuration
         * choice, not a default. */
        num_alus = lanes;
        num_muls = lanes;
        num_fpus = config_.pe_has_fp ? lanes : 0;
    }

    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<component id=\"root\" name=\"root\">\n";

    // System parameters
    xml << "  <component id=\"system\" name=\"system\">\n";
    xml << "    <param name=\"number_of_cores\" value=\"" << config_.num_cores << "\"/>\n";
    xml << "    <param name=\"number_of_L1Directories\" value=\"0\"/>\n";
    xml << "    <param name=\"number_of_L2Directories\" value=\"0\"/>\n";
    xml << "    <param name=\"number_of_L2s\" value=\"" << num_l2s << "\"/>\n";
    xml << "    <param name=\"Private_L2\" value=\"" << (alu_only ? 0 : 1) << "\"/>\n";
    xml << "    <param name=\"number_of_L3s\" value=\"" << num_l3s << "\"/>\n";
    xml << "    <param name=\"number_of_NoCs\" value=\"" << num_nocs << "\"/>\n";
    xml << "    <param name=\"homogeneous_cores\" value=\"1\"/>\n";
    xml << "    <param name=\"homogeneous_L2s\" value=\"" << (num_l2s > 0 ? 1 : 0) << "\"/>\n";
    xml << "    <param name=\"homogeneous_L1Directories\" value=\"0\"/>\n";
    xml << "    <param name=\"homogeneous_L2Directories\" value=\"0\"/>\n";
    xml << "    <param name=\"homogeneous_L3s\" value=\"" << (num_l3s > 0 ? 1 : 0) << "\"/>\n";
    xml << "    <param name=\"homogeneous_ccs\" value=\"1\"/>\n";
    xml << "    <param name=\"homogeneous_NoCs\" value=\"" << (num_nocs == 1 ? 1 : 0) << "\"/>\n";
    xml << "    <param name=\"core_tech_node\" value=\"" << config_.tech_node_nm << "\"/>\n";
    xml << "    <param name=\"target_core_clockrate\" value=\"" << static_cast<int>(config_.core_clock_mhz) << "\"/>\n";
    xml << "    <param name=\"temperature\" value=\"" << config_.temperature_k << "\"/>\n";
    xml << "    <param name=\"number_cache_levels\" value=\"" << num_cache_levels << "\"/>\n";
    xml << "    <param name=\"interconnect_projection_type\" value=\"" << config_.interconnect_projection_type << "\"/>\n";
    /* 1.11.3 gate-0 verdict: device_type=4 (comm-dram) as a GLOBAL device is
     * rejected by CACTI itself -- UCA asserts power.readOp.dynamic > 0 because
     * the retention-grade device (Vth 1.0 > Vdd 0.9) only functions inside the
     * DRAM-array machinery where wordline boost exists. General logic cannot
     * be priced in that device by these tools; the factor harness below is
     * therefore the mechanism, not an interim. */
    xml << "    <param name=\"device_type\" value=\"" << config_.device_type << "\"/>\n";
    xml << "    <param name=\"longer_channel_device\" value=\"" << config_.longer_channel_device << "\"/>\n";
    /* 1.9.32: reference class, emitted explicitly instead of inherited.
     * McPAT defaults Embedded to false, i.e. "this die belongs to the server
     * population". Nothing ever overrode it, so a processing element was priced
     * against Niagara/Merom/Opteron die measurements: a five-stage element drew
     * an undifferentiated-core term of about three square millimetres at the
     * fit's 90 nm base, which is larger than everything the description
     * actually names by orders of magnitude and which supplied nearly all of
     * the element's power -- all of it leakage, since that term has no dynamic
     * component at all.
     *
     * Device scope now selects the embedded population. That is not a discount
     * applied to make a number smaller; it is the other of the two populations
     * McPAT was calibrated against, and it is the one a memory-die element
     * belongs to. It changes three things together, which is why it has to be
     * one flag rather than three edits: the undifferentiated term switches to
     * Sandia's parametrized-processor fit, the functional units switch to ARM
     * areas and per-access energies, and the wires switch from top-level global
     * to the local/semi-global stack an on-die element would actually use.
     *
     * Host scope stays on the server population, because the host is a server
     * part. */
    xml << "    <param name=\"Embedded\" value=\"" << (config_.device_scope ? 1 : 0) << "\"/>\n";
    /* Only ever read inside the embedded undifferentiated-core term, where it
     * decides whether that term exists at all. Kept at 1 so the element still
     * carries clocking and control overhead; 0 would zero it, which is a
     * cheaper answer than the truth. */
    xml << "    <param name=\"opt_clockrate\" value=\"1\"/>\n";
    /* 1.11.12 (borders rule): the DRAM-periphery family is DESCRIBED to
     * McPAT, which owns the components and applies it internally; the
     * wrapper no longer post-scales results. Scope says which components sit
     * on the memory die: PE cores + their caches + the on-die fabric + the
     * element controllers (subarray..chip placements). Rank/channel and
     * base-die designs keep their logic on a buffer die and stay family 0. */
    if (config_.process_family == 1) {
        double fa, fd, fl;
        periphFamilyFactors(config_.dram_periph_table_nm, fa, fd, fl);   // 1.11.16: single authority
        double pitch = (config_.subarray_pitch_factor > 0.0)
                           ? config_.subarray_pitch_factor : 1.0;
        xml << "    <param name=\"dram_periph_family\" value=\"1\"/>\n";
        xml << "    <param name=\"dram_periph_area\" value=\"" << (fa * pitch) << "\"/>\n";
        xml << "    <param name=\"dram_periph_dyn\" value=\"" << fd << "\"/>\n";
        xml << "    <param name=\"dram_periph_leak\" value=\"" << fl << "\"/>\n";
        xml << "    <param name=\"dram_periph_scope\" value=\"15\"/>\n";
    }
    /* 1.11.8: sys.power_gating enables McPAT/CACTI's sleep-transistor
     * model so per-component power_gated_leakage endpoints are computed.
     * Emitted only when the described design gates SOMETHING -- all-false
     * keeps the XML byte-identical to 1.11.7. */
    xml << "    <param name=\"power_gating\" value=\""
        << ((pg_spec_.pg_core || pg_spec_.pg_noc || pg_spec_.pg_mc) ? 1 : 0)
        << "\"/>\n";
    /* 1.9.32: datapath width. McPAT reads machine_bits ONLY to size the
     * datapath -- the integer and floating-point register widths, the load/store
     * queue entries, and the integer, multiply and floating-point bypass buses.
     * It has nothing to do with the address widths, which are emitted
     * separately below.
     *
     * A 64 was emitted for every scope, so a 32-bit processing element carried
     * 64-bit registers and 64-bit result buses. Device scope now states the
     * element's own width; host scope stays 64, which the host is. */
    int machine_bits = config_.device_scope ? config_.pe_element_bits : 64;
    /* McPAT quantises the datapath to 32-bit granularity (it computes
     * ceil(bits/32)*32 internally), so a narrower element -- an 8- or 16-bit
     * bit-serial datapath, which is a real in-memory design point the TIMING
     * model already supports via operand_width -- is priced as 32-bit. Say so
     * once rather than let the two halves quietly disagree about how wide the
     * element is; refusing would remove a capability the timing side has. */
    if (config_.device_scope && machine_bits < 32 && !warned_narrow_datapath_) {
        warned_narrow_datapath_ = true;
        std::cerr << "[power] WARNING: operand_width=" << machine_bits
                  << " is narrower than the power model's 32-bit granularity; "
                  << "the datapath is priced as 32-bit while the timing model "
                  << "charges " << machine_bits << " bits. Register files, "
                  << "queues and result buses are therefore OVERSTATED for this "
                  << "element.\n";
    }
    xml << "    <param name=\"machine_bits\" value=\"" << machine_bits << "\"/>\n";
    xml << "    <param name=\"virtual_address_width\" value=\"48\"/>\n";
    xml << "    <param name=\"physical_address_width\" value=\"48\"/>\n";
    xml << "    <param name=\"virtual_memory_page_size\" value=\"4096\"/>\n";

    // System statistics
    xml << "    <stat name=\"total_cycles\" value=\"" << total_cycles_ << "\"/>\n";
    xml << "    <stat name=\"idle_cycles\" value=\"" << (total_cycles_ - busy_cycles_) << "\"/>\n";
    xml << "    <stat name=\"busy_cycles\" value=\"" << busy_cycles_ << "\"/>\n";

    // Core component — homogeneous_cores=1 means emit exactly ONE core template
    for (int i = 0; i < 1; i++) {
        xml << "    <component id=\"system.core" << i << "\" name=\"core" << i << "\">\n";
        xml << "      <param name=\"clock_rate\" value=\"" << static_cast<int>(config_.core_clock_mhz) << "\"/>\n";
        xml << "      <param name=\"vdd\" value=\"0\"/>\n";
        xml << "      <param name=\"power_gating_vcc\" value=\"-1\"/>\n";
        xml << "      <param name=\"opt_local\" value=\"0\"/>\n";
        xml << "      <param name=\"instruction_length\" value=\"32\"/>\n";
        xml << "      <param name=\"opcode_width\" value=\"7\"/>\n";
        xml << "      <param name=\"x86\" value=\"" << x86 << "\"/>\n";
        xml << "      <param name=\"micro_opcode_width\" value=\"8\"/>\n";
        xml << "      <param name=\"machine_type\" value=\"" << machine_type << "\"/>\n";
        xml << "      <param name=\"number_hardware_threads\" value=\"" << config_.number_hardware_threads << "\"/>\n";
        xml << "      <param name=\"fetch_width\" value=\"" << issue_width << "\"/>\n";
        xml << "      <param name=\"number_instruction_fetch_ports\" value=\"1\"/>\n";
        xml << "      <param name=\"decode_width\" value=\"" << issue_width << "\"/>\n";
        xml << "      <param name=\"issue_width\" value=\"" << issue_width << "\"/>\n";
        xml << "      <param name=\"peak_issue_width\" value=\"" << issue_width << "\"/>\n";
        if (is_ooo) {
            xml << "      <param name=\"fp_issue_width\" value=\"" << fp_issue_width << "\"/>\n";
        }
        xml << "      <param name=\"commit_width\" value=\"" << issue_width << "\"/>\n";
        xml << "      <param name=\"pipelines_per_core\" value=\"1,1\"/>\n";
        xml << "      <param name=\"pipeline_depth\" value=\"" << pipeline_depth << ","
            << pipeline_depth << "\"/>\n";
        xml << "      <param name=\"ALU_per_core\" value=\"" << num_alus << "\"/>\n";
        xml << "      <param name=\"MUL_per_core\" value=\"" << num_muls << "\"/>\n";
        xml << "      <param name=\"FPU_per_core\" value=\"" << num_fpus << "\"/>\n";
        xml << "      <param name=\"instruction_buffer_size\" value=\"32\"/>\n";
        xml << "      <param name=\"decoded_stream_buffer_size\" value=\"16\"/>\n";
        xml << "      <param name=\"instruction_window_scheme\" value=\"0\"/>\n";
        xml << "      <param name=\"instruction_window_size\" value=\"" << inst_window_size << "\"/>\n";
        xml << "      <param name=\"fp_instruction_window_size\" value=\"" << fp_inst_window_size << "\"/>\n";
        xml << "      <param name=\"ROB_size\" value=\"" << rob_size << "\"/>\n";
        xml << "      <param name=\"archi_Regs_IRF_size\" value=\"32\"/>\n";
        xml << "      <param name=\"archi_Regs_FRF_size\" value=\"32\"/>\n";
        xml << "      <param name=\"phy_Regs_IRF_size\" value=\"" << phy_regs_irf << "\"/>\n";
        xml << "      <param name=\"phy_Regs_FRF_size\" value=\"" << phy_regs_frf << "\"/>\n";
        xml << "      <param name=\"rename_scheme\" value=\"" << rename_scheme << "\"/>\n";
        xml << "      <param name=\"register_windows_size\" value=\"0\"/>\n";
        xml << "      <param name=\"LSU_order\" value=\"" << lsu_order << "\"/>\n";
        xml << "      <param name=\"store_buffer_size\" value=\"" << store_buffer << "\"/>\n";
        xml << "      <param name=\"load_buffer_size\" value=\"" << load_buffer << "\"/>\n";
        xml << "      <param name=\"memory_ports\" value=\"1\"/>\n";
        xml << "      <param name=\"RAS_size\" value=\"16\"/>\n";

        // Core statistics
        uint64_t inst_per_core = total_instructions_ / std::max(1, config_.num_cores);
        double pipeline_duty_cycle = (total_cycles_ > 0)
            ? static_cast<double>(busy_cycles_) / total_cycles_
            : 0.0;

        xml << "      <stat name=\"total_instructions\" value=\"" << inst_per_core << "\"/>\n";
        /* 1.9.28: prefer MEASURED activity.
         *
         * 1.9.33: the branch count is now the simulator's OWN branch counter,
         * not the basic-block count. 1.9.28 used bbls on the reasoning that a
         * BBL terminates at a control transfer -- true for a real
         * multi-instruction basic block, but on this QEMU-fed decode path a BBL
         * averages ~1.09 instructions, so bbls ~= instrs and the "measured"
         * branch rate came out at 91.5%. The in-order core always exported a
         * real `branches` counter; the OOO core ran a predictor but exported
         * only its MISSES, so 1.9.33 added the total there too. Both cores now
         * report it, and both report it ROI-windowed like instrs.
         * The integer/floating split is still an assumption (zsim does not
         * classify retired ops), but it is now applied to the NON-branch
         * remainder instead of to the whole stream, so a branch-heavy workload
         * is no longer forced to the same mix as a floating-point one.
         *
         * 1.9.29 CORRECTION -- 1.9.28 SHIPPED THIS WITHOUT CHECKING THE BASES.
         * The activity counters and the instruction count are NOT on the same
         * base. Measured on a co-simulation dump: instrs 52,093 against uops
         * 4,247,062, bbls 649,332 and branches 580,744 -- every activity counter
         * is 10-80x the instruction count it is supposed to be a fraction of.
         * The consequence was a degenerate mix: the co-simulation host was
         * described to the power model as 91.5% branches, and the device-scope
         * path (added in 1.9.29) hit the clamp below and was described as 100%
         * branches with zero integer and zero floating-point operations. That is
         * worse than the fixed fractions it replaced -- 1.9.28 exchanged an
         * invented-but-plausible mix for a measured-but-impossible one, and the
         * A/B did not catch it because a degenerate mix barely moves the total.
         *
         * WHY THIS IS A GATE AND NOT A CORRECTION: the reason instrs sits on a
         * different base is not yet understood, and scaling it by a factor
         * chosen to make the ratio look right would be exactly the class of
         * invented constant this release train exists to remove. So the measured
         * set is used ONLY when it is self-consistent, and otherwise the
         * documented fractions are used and the output says so. */
        const bool meas_self_consistent =
            (meas_branches_ > 0) &&
            /* branches cannot exceed instructions retired */
            (meas_branches_ / std::max(1, config_.num_cores) <= inst_per_core) &&
            /* every instruction decodes to at least one micro-op */
            (meas_uops_ == 0 ||
             meas_uops_ / std::max(1, config_.num_cores) >= inst_per_core);

        uint64_t br_per_core = meas_self_consistent
            ? meas_branches_ / std::max(1, config_.num_cores)
            : inst_per_core * 10 / 100;
        if (!meas_self_consistent && meas_branches_ > 0 && !warned_mix_) {
            warned_mix_ = true;
            std::cout << "  [Activity] WARNING: measured instruction mix rejected as "
                         "inconsistent (instrs/core=" << inst_per_core
                      << " branches/core=" << (meas_branches_ / std::max(1, config_.num_cores))
                      << " uops/core=" << (meas_uops_ / std::max(1, config_.num_cores))
                      << "); using documented fractions instead. The counters are "
                         "on different bases -- see mcpat_wrapper.cpp." << std::endl;
        }
        if (br_per_core > inst_per_core) br_per_core = inst_per_core;
        uint64_t nonbr = inst_per_core - br_per_core;
        uint64_t int_per_core = nonbr * 875 / 1000;   // 87.5% of non-branch
        uint64_t fp_per_core  = nonbr - int_per_core;
        /* 1.11.10 (#112): use the COUNTED mix when the decoder produced one.
         * The 87.5/12.5 split above was a documented stand-in for exactly this
         * measurement; it stays as the fallback for core models that never
         * decode. int carries mul/div (McPAT has no separate integer-multiply
         * stat at this level; the FU it drives is the same execution unit). */
        const uint64_t ncores = std::max(1, config_.num_cores);
        bool mix_measured = (meas_int_ + meas_fp_ + meas_mul_) > 0;
        if (mix_measured) {
            uint64_t mi = (meas_int_ + meas_mul_) / ncores;
            uint64_t mf = meas_fp_ / ncores;
            /* 1.11.15 (audit): when the census is measured, its BRANCH class
             * replaces the core counter -- the decoder classifies ALL control
             * transfers (call/ret/jmp/indirect) while the core counter is
             * conditional-only, and the difference was silently priced at
             * zero execution energy. With census branches the classes sum to
             * the census total by construction, so the check becomes
             * SYMMETRIC: any deficit against retired instructions is printed,
             * and a deficit above 5% rejects the measurement.
             *
             * 1.11.16 (verification audit): the value logic is now independent
             * of the print-once latch, and every fallback is a REAL fallback.
             * The 1.11.15 shape had three defects: (1) the reject branch
             * zeroed mi/mf and then flowed into the assignment below -- the
             * XML carried a 0-int/0-fp/100%-branch mix, the exact 1.9.28
             * degeneracy this guard exists to prevent; (2) the <=5% residual
             * correction sat INSIDE the !warned_mix_ guard, so the number
             * depended on whether a message had been printed (and the child's
             * regenerated XML could differ from the parent's); (3) fallbacks
             * kept int/fp from the PRE-census nonbr while branch kept the
             * census value, so the classes no longer summed to retirement. */
            uint64_t pre_br = br_per_core, pre_int = int_per_core, pre_fp = fp_per_core;
            if (meas_mix_br_ > 0) {
                br_per_core = meas_mix_br_ / ncores;
                if (br_per_core > inst_per_core) br_per_core = inst_per_core;
                nonbr = inst_per_core - br_per_core;
                int_per_core = nonbr * 875 / 1000;   // fraction fallback, census-branch base
                fp_per_core  = nonbr - int_per_core;
            }
            bool census_ok = true;
            uint64_t classified = mi + mf + br_per_core;
            if (classified < inst_per_core) {
                uint64_t deficit = inst_per_core - classified;
                if (deficit * 20 > inst_per_core) {   // >5%: reject the census
                    census_ok = false;
                    if (!warned_mix_) {
                        warned_mix_ = true;
                        std::cout << "  [Activity] measured mix rejected: "
                                  << deficit << "/" << inst_per_core
                                  << " retired instructions per core are in NO "
                                     "class (>5%) -- census and retirement "
                                     "disagree; using documented fractions."
                                  << std::endl;
                    }
                } else if (deficit > 0) {
                    mi += deficit;    // conservative: residual as int-class
                    if (!warned_mix_) {
                        warned_mix_ = true;
                        std::cout << "  [Activity] mix census covers "
                                  << classified << "/" << inst_per_core
                                  << " per core (deficit " << deficit
                                  << ", <5%); residual priced as integer."
                                  << std::endl;
                    }
                }
            }
            if (!census_ok) {
                /* Full fraction fallback: the BRANCH class reverts too -- a
                 * census that disagrees with retirement by >5% is not trusted
                 * for any of its classes. */
                br_per_core  = pre_br;
                nonbr        = inst_per_core - br_per_core;
                int_per_core = pre_int;
                fp_per_core  = pre_fp;
            } else if (mi + mf > nonbr) {
                /* More classified instructions than retired ones means the two
                 * counters are on different bases -- the 1.9.28/1.11.9 defect
                 * class. Say so and keep the fractions (recomputed above on
                 * the census-branch base, so int+fp+branch still sums). */
                if (!warned_mix_) {
                    warned_mix_ = true;
                    std::cout << "  [Activity] measured mix (" << mi << " int+mul, "
                              << mf << " fp per core) exceeds retired non-branch "
                              << nonbr << " -- bases disagree, using fractions"
                              << std::endl;
                }
            } else {
                int_per_core = mi;
                fp_per_core  = mf;
                if (!warned_mix_) {
                    warned_mix_ = true;
                    std::cout << "  [Activity] instruction mix COUNTED: "
                              << int_per_core << " int+mul, " << fp_per_core
                              << " fp, " << br_per_core << " branch per core "
                              << "(decoder-classified; the 87.5/12.5 stand-in "
                                 "is not used)" << std::endl;
                }
            }
        }
        xml << "      <stat name=\"int_instructions\" value=\"" << int_per_core << "\"/>\n";
        xml << "      <stat name=\"fp_instructions\" value=\"" << fp_per_core << "\"/>\n";
        xml << "      <stat name=\"branch_instructions\" value=\"" << br_per_core << "\"/>\n";
        /* 1.9.28: measured, not a fixed 1%. Mispredicts drive pipeline-flush
         * energy, and their rate varies enormously by workload -- a regular
         * stencil and an irregular graph traversal are not the same machine. */
        /* 1.9.29: gated on the same self-consistency test. A mispredict count
         * taken from a different base than the branch count it is a subset of
         * would report a mispredict RATE that is meaningless. */
        uint64_t mispred_per_core = (meas_mispred_ > 0 && meas_self_consistent)
            ? meas_mispred_ / std::max(1, config_.num_cores)
            : inst_per_core * 1 / 100;
        if (mispred_per_core > br_per_core) mispred_per_core = br_per_core;
        xml << "      <stat name=\"branch_mispredictions\" value=\"" << mispred_per_core << "\"/>\n";
        xml << "      <stat name=\"load_instructions\" value=\"" << (inst_per_core * 20 / 100) << "\"/>\n";
        xml << "      <stat name=\"store_instructions\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "      <stat name=\"committed_instructions\" value=\"" << inst_per_core << "\"/>\n";
        xml << "      <stat name=\"committed_int_instructions\" value=\"" << int_per_core << "\"/>\n";
        xml << "      <stat name=\"committed_fp_instructions\" value=\"" << fp_per_core << "\"/>\n";
        xml << "      <stat name=\"pipeline_duty_cycle\" value=\"" << pipeline_duty_cycle << "\"/>\n";
        xml << "      <stat name=\"total_cycles\" value=\"" << total_cycles_ << "\"/>\n";
        xml << "      <stat name=\"idle_cycles\" value=\"" << (total_cycles_ - busy_cycles_) << "\"/>\n";
        xml << "      <stat name=\"busy_cycles\" value=\"" << busy_cycles_ << "\"/>\n";
        xml << "      <stat name=\"ROB_reads\" value=\"" << (is_ooo ? inst_per_core : 0ULL) << "\"/>\n";
        xml << "      <stat name=\"ROB_writes\" value=\"" << (is_ooo ? inst_per_core : 0ULL) << "\"/>\n";
        xml << "      <stat name=\"rename_reads\" value=\"" << (is_ooo ? inst_per_core : 0ULL) << "\"/>\n";
        xml << "      <stat name=\"rename_writes\" value=\"" << (is_ooo ? inst_per_core : 0ULL) << "\"/>\n";
        xml << "      <stat name=\"fp_rename_reads\" value=\"" << (is_ooo ? inst_per_core * 10 / 100 : 0ULL) << "\"/>\n";
        xml << "      <stat name=\"fp_rename_writes\" value=\"" << (is_ooo ? inst_per_core * 10 / 100 : 0ULL) << "\"/>\n";
        xml << "      <stat name=\"inst_window_reads\" value=\"" << (is_ooo ? inst_per_core : 0ULL) << "\"/>\n";
        xml << "      <stat name=\"inst_window_writes\" value=\"" << (is_ooo ? inst_per_core : 0ULL) << "\"/>\n";
        xml << "      <stat name=\"inst_window_wakeup_accesses\" value=\"" << (is_ooo ? inst_per_core : 0ULL) << "\"/>\n";
        xml << "      <stat name=\"fp_inst_window_reads\" value=\"" << (is_ooo ? inst_per_core * 10 / 100 : 0ULL) << "\"/>\n";
        xml << "      <stat name=\"fp_inst_window_writes\" value=\"" << (is_ooo ? inst_per_core * 10 / 100 : 0ULL) << "\"/>\n";
        xml << "      <stat name=\"fp_inst_window_wakeup_accesses\" value=\"" << (is_ooo ? inst_per_core * 10 / 100 : 0ULL) << "\"/>\n";
        xml << "      <stat name=\"int_regfile_reads\" value=\"" << (inst_per_core * 2) << "\"/>\n";
        xml << "      <stat name=\"int_regfile_writes\" value=\"" << inst_per_core << "\"/>\n";
        xml << "      <stat name=\"float_regfile_reads\" value=\"" << (inst_per_core * 20 / 100) << "\"/>\n";
        xml << "      <stat name=\"float_regfile_writes\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "      <stat name=\"function_calls\" value=\"" << (inst_per_core / 100) << "\"/>\n";
        xml << "      <stat name=\"context_switches\" value=\"0\"/>\n";
        xml << "      <stat name=\"ialu_accesses\" value=\"" << (inst_per_core * 70 / 100) << "\"/>\n";
        xml << "      <stat name=\"fpu_accesses\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "      <stat name=\"mul_accesses\" value=\"" << (inst_per_core * 5 / 100) << "\"/>\n";
        xml << "      <stat name=\"cdb_alu_accesses\" value=\"" << inst_per_core << "\"/>\n";
        xml << "      <stat name=\"cdb_mul_accesses\" value=\"" << (inst_per_core * 5 / 100) << "\"/>\n";
        xml << "      <stat name=\"cdb_fpu_accesses\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";

        if (alu_only) {
            /* 1.9.32: the element's instruction store, stated instead of
             * inherited. McPAT builds an instruction-fetch unit for every core
             * unconditionally, and this path emitted no icache parameters at
             * all -- so the store was constructed from ParseXML's initialize(),
             * which fills the whole config vector with the literal value 1. The
             * element's instruction supply was therefore a ONE-BYTE, one-line,
             * one-way cache: not a modelling choice, an uninitialised default
             * that nothing had ever looked at.
             *
             * What a processing element really has is a resident instruction
             * memory, sized by pe_imem_bytes -- roughly a hundred bytes for a
             * command-driven in-bank engine, kilobytes for a programmable
             * near-bank one. That size is the interesting design axis: it
             * decides which kernels can run on the element at all.
             *
             * Direct-mapped, one instruction word per line, and ZERO misses:
             * the program is resident, so there is no refill path to charge.
             *
             * RESIDUAL, named rather than hidden: this is still McPAT's cache
             * constructor, so the store carries a tag array and one-entry miss,
             * fill and prefetch structures that a scratchpad does not have.
             * That overstates the instruction store by the tag overhead. Fixing
             * it needs a pure-RAM instruction store inside McPAT's fetch unit,
             * which is the remaining half of this item. */
            int imem_bytes = config_.pe_imem_bytes;
            if (imem_bytes < 64) {
                std::cerr << "[power] WARNING: pe_imem_bytes=" << imem_bytes
                          << " is below the smallest array the model can build; "
                          << "using 64 bytes. An element this small is command-"
                          << "driven and should be described as such, not as a "
                          << "degenerate instruction memory.\n";
                imem_bytes = 64;
            }
            uint64_t ifetch_per_core = inst_per_core;
            xml << "      <component id=\"system.core" << i << ".icache\" name=\"icache\">\n";
            xml << "        <param name=\"icache_config\" value=\"" << imem_bytes
                << ",8,1,1,1,1,64,0\"/>\n";
            /* 1.11.3 (#111): buffer_sizes[0]=0 is the fork's marker for a
             * RESIDENT store -- InstFetchU builds it as a pure RAM (no tag)
             * and does not build miss/fill/prefetch buffers at all. */
            xml << "        <param name=\"buffer_sizes\" value=\"0,0,0,0\"/>\n";
            xml << "        <stat name=\"read_accesses\" value=\"" << ifetch_per_core << "\"/>\n";
            xml << "        <stat name=\"read_misses\" value=\"0\"/>\n";
            xml << "        <stat name=\"conflicts\" value=\"0\"/>\n";
            xml << "      </component>\n";
        }

        if (!alu_only) {
            // L1 icache — use actual per-core stats
            uint64_t l1i_reads_per_core = l1i_reads_ / std::max(1, config_.num_cores);
            uint64_t l1i_misses_per_core = l1i_read_misses_ / std::max(1, config_.num_cores);
            xml << "      <component id=\"system.core" << i << ".icache\" name=\"icache\">\n";
            xml << "        <param name=\"icache_config\" value=\"" << config_.l1i_size_bytes
                << ",64,8,1,1,3,64,0\"/>\n";
            xml << "        <param name=\"buffer_sizes\" value=\"16,16,16,0\"/>\n";
            xml << "        <stat name=\"read_accesses\" value=\"" << l1i_reads_per_core << "\"/>\n";
            xml << "        <stat name=\"read_misses\" value=\"" << l1i_misses_per_core << "\"/>\n";
            xml << "        <stat name=\"conflicts\" value=\"0\"/>\n";
            xml << "      </component>\n";

            // L1 dcache — use actual per-core stats
            uint64_t l1d_reads_per_core = l1d_reads_ / std::max(1, config_.num_cores);
            uint64_t l1d_writes_per_core = l1d_writes_ / std::max(1, config_.num_cores);
            uint64_t l1d_rmisses_per_core = l1d_read_misses_ / std::max(1, config_.num_cores);
            uint64_t l1d_wmisses_per_core = l1d_write_misses_ / std::max(1, config_.num_cores);
            xml << "      <component id=\"system.core" << i << ".dcache\" name=\"dcache\">\n";
            xml << "        <param name=\"dcache_config\" value=\"" << config_.l1d_size_bytes
                << ",64,8,1,1,3,64,0\"/>\n";
            xml << "        <param name=\"buffer_sizes\" value=\"16,16,16,16\"/>\n";
            xml << "        <stat name=\"read_accesses\" value=\"" << l1d_reads_per_core << "\"/>\n";
            xml << "        <stat name=\"write_accesses\" value=\"" << l1d_writes_per_core << "\"/>\n";
            xml << "        <stat name=\"read_misses\" value=\"" << l1d_rmisses_per_core << "\"/>\n";
            xml << "        <stat name=\"write_misses\" value=\"" << l1d_wmisses_per_core << "\"/>\n";
            xml << "        <stat name=\"conflicts\" value=\"0\"/>\n";
            xml << "      </component>\n";
        }
        xml << "    </component>\n";  // close coreN
    }

    // L2 cache — homogeneous_L2s=1 means emit exactly ONE L2 template
    if (!alu_only) {
        for (int i = 0; i < 1; i++) {
            uint64_t l2_reads_per = l2_reads_ / std::max(1, config_.num_cores);
            uint64_t l2_writes_per = l2_writes_ / std::max(1, config_.num_cores);
            uint64_t l2_rmisses_per = l2_read_misses_ / std::max(1, config_.num_cores);
            uint64_t l2_wmisses_per = l2_write_misses_ / std::max(1, config_.num_cores);
            xml << "    <component id=\"system.L2" << i << "\" name=\"L2" << i << "\">\n";
            xml << "      <param name=\"L2_config\" value=\"" << config_.l2_size_bytes
                << ",64,8,8,8,23,64,1\"/>\n";
            xml << "      <param name=\"buffer_sizes\" value=\"16,16,16,16\"/>\n";
            xml << "      <param name=\"clockrate\" value=\"" << static_cast<int>(config_.core_clock_mhz) << "\"/>\n";
            xml << "      <param name=\"ports\" value=\"1,1,1\"/>\n";
            xml << "      <param name=\"device_type\" value=\"0\"/>\n";
            xml << "      <stat name=\"read_accesses\" value=\"" << l2_reads_per << "\"/>\n";
            xml << "      <stat name=\"write_accesses\" value=\"" << l2_writes_per << "\"/>\n";
            xml << "      <stat name=\"read_misses\" value=\"" << l2_rmisses_per << "\"/>\n";
            xml << "      <stat name=\"write_misses\" value=\"" << l2_wmisses_per << "\"/>\n";
            xml << "      <stat name=\"conflicts\" value=\"0\"/>\n";
            xml << "      <stat name=\"duty_cycle\" value=\""
                << (total_cycles_ > 0 ? static_cast<double>(busy_cycles_) / total_cycles_ * 0.5 : 0.0) << "\"/>\n";
            xml << "    </component>\n";
        }
    }

    // L3 cache (shared) — skip if ALU-only or no L3
    if (has_l3) {
        xml << "    <component id=\"system.L3\" name=\"L3\">\n";
        // 1.9.10 fix: McPAT/CACTI cache_config field 1 is capacity in BYTES (as L2
        // above correctly emits). This path emitted MEGABYTES, so a 32MB LLC became a
        // "32-byte" array and CACTI's ArrayST::error_checking() rejected the geometry
        // (the standalone-L3 failure that forced host LLC to be literature-anchored).
        xml << "      <param name=\"L3_config\" value=\"" << config_.l3_size_bytes
            << ",64,16,16,16,23,64,1\"/>\n";
        xml << "      <param name=\"clockrate\" value=\"" << static_cast<int>(config_.core_clock_mhz) << "\"/>\n";
        xml << "      <param name=\"ports\" value=\"1,1,1\"/>\n";
        xml << "      <param name=\"device_type\" value=\"0\"/>\n";
        xml << "      <param name=\"buffer_sizes\" value=\"16,16,16,16\"/>\n";
        xml << "      <stat name=\"read_accesses\" value=\"" << l3_reads_ << "\"/>\n";
        xml << "      <stat name=\"write_accesses\" value=\"" << l3_writes_ << "\"/>\n";
        xml << "      <stat name=\"read_misses\" value=\"" << l3_read_misses_ << "\"/>\n";
        xml << "      <stat name=\"write_misses\" value=\"" << l3_write_misses_ << "\"/>\n";
        xml << "      <stat name=\"conflicts\" value=\"0\"/>\n";
        xml << "      <stat name=\"duty_cycle\" value=\""
            << (total_cycles_ > 0 ? static_cast<double>(busy_cycles_) / total_cycles_ * 0.3 : 0.0) << "\"/>\n";
        xml << "    </component>\n";
    }

    // NoC — N instances from noc_levels_ (or single legacy instance)
    if (!noc_levels_.empty()) {
        // N heterogeneous NoC instances
        for (size_t ni = 0; ni < noc_levels_.size(); ni++) {
            const auto& lvl = noc_levels_[ni];
            xml << "    <component id=\"system.noc" << ni << "\" name=\"noc" << ni << "\">\n";
            xml << "      <param name=\"clockrate\" value=\"" << static_cast<int>(lvl.clock_mhz) << "\"/>\n";
            xml << "      <param name=\"vdd\" value=\"0\"/>\n";
            xml << "      <param name=\"power_gating_vcc\" value=\"-1\"/>\n";
            xml << "      <param name=\"type\" value=\"" << lvl.type << "\"/>\n";
            xml << "      <param name=\"horizontal_nodes\" value=\"" << lvl.horizontal_nodes << "\"/>\n";
            xml << "      <param name=\"vertical_nodes\" value=\"" << lvl.vertical_nodes << "\"/>\n";
            xml << "      <param name=\"has_global_link\" value=\"0\"/>\n";
            xml << "      <param name=\"link_throughput\" value=\"1\"/>\n";
            xml << "      <param name=\"link_latency\" value=\"1\"/>\n";
            xml << "      <param name=\"input_ports\" value=\"" << lvl.input_ports << "\"/>\n";
            xml << "      <param name=\"output_ports\" value=\"" << lvl.output_ports << "\"/>\n";
            xml << "      <param name=\"virtual_channel_per_port\" value=\""
                << std::max(1, config_.noc_vcs_per_vnet) << "\"/>\n";
            xml << "      <param name=\"input_buffer_entries_per_vc\" value=\""
                << std::max(1, config_.noc_vc_buffer_size) << "\"/>\n";
            xml << "      <param name=\"flit_bits\" value=\"" << lvl.flit_bits << "\"/>\n";
            xml << "      <param name=\"chip_coverage\" value=\"" << lvl.chip_coverage << "\"/>\n";
            xml << "      <param name=\"link_routing_over_percentage\" value=\"0.5\"/>\n";
            xml << "      <stat name=\"total_accesses\" value=\"" << lvl.total_accesses << "\"/>\n";
            xml << "      <stat name=\"duty_cycle\" value=\"" << lvl.duty_cycle << "\"/>\n";
            xml << "    </component>\n";
        }
    } else if (config_.has_noc) {
        // Single legacy NoC instance
        int grid = static_cast<int>(std::sqrt(config_.num_cores));
        if (grid < 1) grid = 1;
        int noc_type = (config_.noc_topology == 2) ? 0 : 1;  // bus=0, else router=1
        uint64_t noc_accesses = (noc_activity_.total_packets > 0) ? noc_activity_.total_packets
                                : (mc_reads_ + mc_writes_);
        double noc_duty_cycle = (total_cycles_ > 0)
            ? static_cast<double>(noc_accesses) / total_cycles_
            : 0.0;

        xml << "    <component id=\"system.noc0\" name=\"noc0\">\n";
        xml << "      <param name=\"clockrate\" value=\"" << static_cast<int>(config_.core_clock_mhz) << "\"/>\n";
        xml << "      <param name=\"vdd\" value=\"0\"/>\n";
        xml << "      <param name=\"power_gating_vcc\" value=\"-1\"/>\n";
        xml << "      <param name=\"type\" value=\"" << noc_type << "\"/>\n";
        xml << "      <param name=\"horizontal_nodes\" value=\"" << grid << "\"/>\n";
        xml << "      <param name=\"vertical_nodes\" value=\"" << grid << "\"/>\n";
        xml << "      <param name=\"has_global_link\" value=\"0\"/>\n";
        xml << "      <param name=\"link_throughput\" value=\"1\"/>\n";
        xml << "      <param name=\"link_latency\" value=\"1\"/>\n";
        xml << "      <param name=\"input_ports\" value=\"5\"/>\n";
        xml << "      <param name=\"output_ports\" value=\"5\"/>\n";
        xml << "      <param name=\"virtual_channel_per_port\" value=\""
            << std::max(1, config_.noc_vcs_per_vnet) << "\"/>\n";
        xml << "      <param name=\"input_buffer_entries_per_vc\" value=\""
            << std::max(1, config_.noc_vc_buffer_size) << "\"/>\n";
        xml << "      <param name=\"flit_bits\" value=\"128\"/>\n";
        xml << "      <param name=\"chip_coverage\" value=\"1\"/>\n";
        xml << "      <param name=\"link_routing_over_percentage\" value=\"0.5\"/>\n";
        xml << "      <stat name=\"total_accesses\" value=\"" << noc_accesses << "\"/>\n";
        xml << "      <stat name=\"duty_cycle\" value=\"" << noc_duty_cycle << "\"/>\n";
        xml << "    </component>\n";
    } else {
        // Minimal NoC stub — McPAT's positional XML parser requires at least 1 NoC
        // component to correctly offset to the MC section
        xml << "    <component id=\"system.noc0\" name=\"noc0\">\n";
        xml << "      <param name=\"clockrate\" value=\"" << static_cast<int>(config_.core_clock_mhz) << "\"/>\n";
        xml << "      <param name=\"vdd\" value=\"0\"/>\n";
        xml << "      <param name=\"power_gating_vcc\" value=\"-1\"/>\n";
        xml << "      <param name=\"type\" value=\"0\"/>\n";
        xml << "      <param name=\"horizontal_nodes\" value=\"1\"/>\n";
        xml << "      <param name=\"vertical_nodes\" value=\"1\"/>\n";
        xml << "      <param name=\"has_global_link\" value=\"0\"/>\n";
        xml << "      <param name=\"link_throughput\" value=\"1\"/>\n";
        xml << "      <param name=\"link_latency\" value=\"1\"/>\n";
        xml << "      <param name=\"input_ports\" value=\"1\"/>\n";
        xml << "      <param name=\"output_ports\" value=\"1\"/>\n";
        xml << "      <param name=\"flit_bits\" value=\"64\"/>\n";
        xml << "      <param name=\"chip_coverage\" value=\"0\"/>\n";
        xml << "      <param name=\"link_routing_over_percentage\" value=\"0\"/>\n";
        xml << "      <stat name=\"total_accesses\" value=\"0\"/>\n";
        xml << "      <stat name=\"duty_cycle\" value=\"0\"/>\n";
        xml << "    </component>\n";
    }

    // Memory controller — uses actual mc_reads_/mc_writes_ and mc_tech_ params
    xml << "    <component id=\"system.mc\" name=\"mc\">\n";
    /* 1.11.15 (audit): McPAT builds an MCPHY -- a per-bit OFF-CHIP I/O
     * driver charge on every access -- whenever type==0. An on-die element
     * MC drives no pins at all, so no off-chip driver model applies to it;
     * on-die MCs are emitted as type=1 (embedded class) with no PHY.
     * (1.11.16 correction to the recorded rationale: at the on-die
     * placements the termination term was ALREADY zero, so the MCPHY was a
     * single wrong-in-kind charge, not a double count. At RANK+ the PHY
     * driver and the ODT termination are genuinely distinct charges and
     * both belong -- do NOT "complete" this fix by stripping the PHY there.
     * The redundant conditional withPHY emission was deleted: line below
     * already emits withPHY=0 unconditionally, and McPAT ignores withPHY
     * for type=0 anyway.) */
    xml << "      <param name=\"type\" value=\""
        << (config_.mc_offchip_phy ? 0 : 1) << "\"/>\n";
    xml << "      <param name=\"mc_clock\" value=\"" << static_cast<int>(config_.mc_clock_mhz) << "\"/>\n";
    xml << "      <param name=\"vdd\" value=\"0\"/>\n";
    xml << "      <param name=\"power_gating_vcc\" value=\"-1\"/>\n";
    xml << "      <param name=\"peak_transfer_rate\" value=\"" << mc_tech_.peak_transfer_rate << "\"/>\n";
    xml << "      <param name=\"block_size\" value=\"64\"/>\n";
    xml << "      <param name=\"number_mcs\" value=\"" << mc_tech_.number_mcs << "\"/>\n";
    xml << "      <param name=\"memory_channels_per_mc\" value=\"1\"/>\n";
    xml << "      <param name=\"number_ranks\" value=\"" << mc_tech_.number_ranks << "\"/>\n";
    xml << "      <param name=\"withPHY\" value=\"0\"/>\n";
    xml << "      <param name=\"req_window_size_per_channel\" value=\"32\"/>\n";
    xml << "      <param name=\"IO_buffer_size_per_channel\" value=\"32\"/>\n";
    xml << "      <param name=\"databus_width\" value=\"" << mc_tech_.databus_width << "\"/>\n";
    xml << "      <param name=\"addressbus_width\" value=\"51\"/>\n";
    {
        int num_mcs = std::max(1, mc_tech_.number_mcs);
        xml << "      <stat name=\"memory_accesses\" value=\"" << (mc_reads_ + mc_writes_) / num_mcs << "\"/>\n";
        xml << "      <stat name=\"memory_reads\" value=\"" << mc_reads_ / num_mcs << "\"/>\n";
        xml << "      <stat name=\"memory_writes\" value=\"" << mc_writes_ / num_mcs << "\"/>\n";
    }
    xml << "    </component>\n";

    // NIU — mandatory stub
    xml << "    <component id=\"system.niu\" name=\"niu\">\n";
    xml << "      <param name=\"type\" value=\"1\"/>\n";
    xml << "      <param name=\"clockrate\" value=\"350\"/>\n";
    xml << "      <param name=\"vdd\" value=\"0\"/>\n";
    xml << "      <param name=\"power_gating_vcc\" value=\"-1\"/>\n";
    xml << "      <param name=\"number_units\" value=\"0\"/>\n";
    xml << "      <stat name=\"duty_cycle\" value=\"0\"/>\n";
    xml << "      <stat name=\"total_load_perc\" value=\"0\"/>\n";
    xml << "    </component>\n";

    // PCIe — active when co-sim transfers present, otherwise stub.
    // 1.11.7: clock from the configured link (350 was a hardwired literal),
    // measured bytes + per-link-type pJ/bit drive the dynamic term inside
    // the fork (iocontrollers.cc) -- zero traffic = zero link dynamic.
    {
        int link_clk = (pcie_stats_.link_clock_mhz > 0)
                           ? pcie_stats_.link_clock_mhz : 350;
        xml << "    <component id=\"system.pcie\" name=\"pcie\">\n";
        xml << "      <param name=\"type\" value=\"1\"/>\n";
        xml << "      <param name=\"withPHY\" value=\"1\"/>\n";
        xml << "      <param name=\"clockrate\" value=\"" << link_clk << "\"/>\n";
        xml << "      <param name=\"vdd\" value=\"0\"/>\n";
        xml << "      <param name=\"power_gating_vcc\" value=\"-1\"/>\n";
        xml << "      <param name=\"number_units\" value=\"" << pcie_stats_.number_units << "\"/>\n";
        xml << "      <param name=\"num_channels\" value=\"" << pcie_stats_.num_channels << "\"/>\n";
        xml << "      <stat name=\"duty_cycle\" value=\"" << pcie_stats_.duty_cycle << "\"/>\n";
        xml << "      <stat name=\"total_load_perc\" value=\"" << pcie_stats_.total_load_perc << "\"/>\n";
        xml << "      <stat name=\"transferred_bytes\" value=\"" << pcie_stats_.transferred_bytes << "\"/>\n";
        xml << "      <stat name=\"link_pj_per_bit\" value=\"" << pcie_stats_.link_pj_per_bit << "\"/>\n";
        xml << "    </component>\n";
    }

    // Flash controller — mandatory stub
    xml << "    <component id=\"system.flashc\" name=\"flashc\">\n";
    xml << "      <param name=\"number_flashcs\" value=\"0\"/>\n";
    xml << "      <param name=\"type\" value=\"1\"/>\n";
    xml << "      <param name=\"withPHY\" value=\"1\"/>\n";
    xml << "      <param name=\"peak_transfer_rate\" value=\"200\"/>\n";
    xml << "      <param name=\"vdd\" value=\"0\"/>\n";
    xml << "      <param name=\"power_gating_vcc\" value=\"-1\"/>\n";
    xml << "      <stat name=\"duty_cycle\" value=\"0\"/>\n";
    xml << "    </component>\n";

    xml << "  </component>\n";
    xml << "</component>\n";

    return xml.str();
}

} // namespace pimid
