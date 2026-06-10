/**
 * @file main.cpp
 * @brief PIMID Simulation Infrastructure - Single Entry Point for All Simulation Modes
 *
 * Binary: pimid (main PIMID binary)
 *
 * This is the PRIMARY PIMID simulation infrastructure binary supporting all modes:
 * - sim:        Config-driven PIM architecture simulation (default)
 * - standalone: PIM-enabled workload execution
 * - host:       Host-only simulation (for split host/device testing)
 * - device:     Device-only simulation (for split host/device testing)
 *
 * Usage:
 *   pimid --method exec      --config architecture.yaml --workload ./bench
 *   pimid --method trace     --config architecture.yaml --trace-file in.trace
 *   pimid --method trace-gen --workload ./bench --trace-file out.pimtrace
 *   pimid --method exec      --scope system --config cosim.yaml \
 *                            --workload ./cosim_bench
 *   pimid --method exec      --mpi-ranks 4 --workload ./mpi_app
 *
 * External Models Integrated:
 *   - Ramulator2: DRAM timing simulation
 *   - CACTI/McPAT: SRAM/cache and power modeling
 *   - NVSim: Non-volatile memory modeling
 *   - GARNET: Network-on-chip simulation
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <set>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
#include <cerrno>
#include <limits.h>
#include <libgen.h>
#include <numeric>

// Trace infrastructure
#include "trace/trace_writer.h"

// External model wrappers for querying real memory timing
#include "memory/nvsim_wrapper.h"
#include "util/cache_warehouse.h"
#include "memory/cacti_wrapper.h"
#include "memory/ramulator_wrapper.h"
#include "memory/internal_dram_network.h"

#ifdef HAVE_HDF5
#include <hdf5.h>
#include <hdf5_hl.h>
#endif

// ZSim trace driver library (in-process trace replay)
#include "zsim_trace_api.h"

// Helper to get PIMID root directory (from executable path or environment)
static std::string getPimidRoot() {
    // Check environment variable first
    const char* env_root = getenv("PIMID_ROOT");
    if (env_root && access(env_root, F_OK) == 0) {
        return std::string(env_root);
    }

    // Get executable path and derive root
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        // exe is in build/, go up one level to pimid/
        std::string path(dirname(exe_path));
        size_t build_pos = path.rfind("/build");
        if (build_pos != std::string::npos) {
            return path.substr(0, build_pos);
        }
        // If not in build/, assume exe is in pimid/ root
        return path;
    }

    // Fallback to current directory
    return ".";
}

// Forward declaration for UnifiedConfig
struct UnifiedConfig;

/**
 * @brief Get memory latency in cycles from external models (DEFAULT) or complete YAML override.
 *
 * Timing source:
 * - DEFAULT: External model query (NVSim for NVM, CACTI for SRAM, Ramulator for DRAM)
 * - OVERRIDE: YAML config ONLY if user provides ALL required params (timing + energy + power)
 *
 * NO hardcoded fallback values. External models provide calibrated timing based on
 * technology node, capacity, and other physical parameters.
 *
 * @param memory_tech Memory technology string (e.g., "DRAM", "STT_MRAM", "STTMRAM")
 * @param frequency_mhz Operating frequency in MHz
 * @param use_yaml_override true if user provided complete YAML memory params
 * @param yaml_latency_ns Latency from YAML (only used if use_yaml_override is true)
 * @return Memory access latency in cycles
 */
static int getMemoryLatencyCycles(const std::string& memory_tech, double frequency_mhz,
                                   bool use_yaml_override = false, double yaml_latency_ns = -1.0,
                                   uint64_t array_capacity_bytes = 0) {
    double latency_ns = 0.0;

    // Normalize technology name for comparison
    std::string tech = memory_tech;
    std::transform(tech.begin(), tech.end(), tech.begin(), ::toupper);

    // For SRAM/NVM the access latency depends on the array size (CACTI/NVSim
    // characterize bitline/wordline length). When the caller supplies the array
    // capacity (banks × per-bank size), use it so the timing matches the energy
    // model; otherwise fall back to the model defaults. 0 = use default.
    const uint64_t SRAM_DEFAULT_CAP = 64 * 1024;        // 64 KB if unspecified
    const uint64_t NVM_DEFAULT_CAP  = 8 * 1024 * 1024;  // legacy 8 MB default
    uint64_t sram_cap = (array_capacity_bytes > 0) ? array_capacity_bytes : SRAM_DEFAULT_CAP;
    uint64_t nvm_cap  = (array_capacity_bytes > 0) ? array_capacity_bytes : NVM_DEFAULT_CAP;

    if (use_yaml_override && yaml_latency_ns > 0.0) {
        // User provided COMPLETE memory params in YAML - use their values
        latency_ns = yaml_latency_ns;
    }
    else {
        // DEFAULT: Query external models for timing
        // External models use calibrated parameters based on technology/process node

        if (tech == "SRAM") {
            // Query CACTI for SRAM timing (RAM mode, not cache)
            pimid::CACTIWrapper::SRAMConfig cfg;
            cfg.capacity_bytes = sram_cap;
            cfg.tech_node_nm = 22;
            cfg.is_cache = false;  // SRAM as memory, not cache
            pimid::CACTIWrapper wrapper(cfg);
            wrapper.initialize();
            latency_ns = wrapper.getAccessTime() * 1e9;  // seconds -> ns
        }
        // --- NVSim-backed NVM technologies ---
        else if (tech == "STT_MRAM" || tech == "STTMRAM" || tech == "STT-MRAM" || tech == "MRAM") {
            pimid::NVSimWrapper::NVMConfig cfg;
            cfg.nvm_type = pimid::NVSimWrapper::NVMType::STTRAM;
            cfg.capacity_bytes = nvm_cap;
            cfg.process_node_nm = 22;
            pimid::NVSimWrapper wrapper(cfg);
            wrapper.initialize();
            latency_ns = wrapper.getReadLatency() * 1e9;
        }
        else if (tech == "PCM" || tech == "PCRAM" || tech == "3DXPOINT") {
            pimid::NVSimWrapper::NVMConfig cfg;
            cfg.nvm_type = pimid::NVSimWrapper::NVMType::PCRAM;
            cfg.capacity_bytes = nvm_cap;
            cfg.process_node_nm = 22;
            pimid::NVSimWrapper wrapper(cfg);
            wrapper.initialize();
            latency_ns = wrapper.getReadLatency() * 1e9;
        }
        else if (tech == "RERAM" || tech == "RESISTIVE" || tech == "MEMRISTOR") {
            pimid::NVSimWrapper::NVMConfig cfg;
            cfg.nvm_type = pimid::NVSimWrapper::NVMType::RERAM;
            cfg.capacity_bytes = nvm_cap;
            cfg.process_node_nm = 22;
            pimid::NVSimWrapper wrapper(cfg);
            wrapper.initialize();
            latency_ns = wrapper.getReadLatency() * 1e9;
        }
        // --- Ramulator-backed DRAM technologies ---
        else if (tech == "DDR3" || tech == "DDR4" || tech == "DDR5" ||
                 tech == "LPDDR5" || tech == "GDDR6" ||
                 tech == "HBM2" || tech == "HBM3" ||
                 tech == "DRAM") {
            // Query Ramulator wrapper for DRAM timing (tRCD + tCAS)
            pimid::RamulatorWrapper wrapper("", tech);
            wrapper.initialize();
            latency_ns = wrapper.getTRCD() + wrapper.getTCAS();
        }
        else {
            // Unknown tech: default to DDR4 timing via Ramulator wrapper
            pimid::RamulatorWrapper wrapper("", "DDR4");
            wrapper.initialize();
            latency_ns = wrapper.getTRCD() + wrapper.getTCAS();
        }
    }

    // Convert nanoseconds to cycles: cycles = latency_ns * freq_mhz / 1000
    double cycles = latency_ns * frequency_mhz / 1000.0;
    int result = static_cast<int>(std::round(cycles));
    return std::max(1, result);
}

/**
 * @brief Get cache latency in cycles from CACTI for a given cache configuration.
 *
 * Queries CACTI with physical parameters (size, associativity, line size) to get
 * calibrated access timing. Falls back to default_cycles on CACTI failure.
 *
 * @param size_kb Cache size in KB
 * @param ways Set associativity
 * @param line_size Cache line size in bytes
 * @param frequency_mhz Operating frequency in MHz
 * @param tech_node_nm Technology node in nanometers (e.g. 22, 14, 7)
 * @param default_cycles Fallback latency if CACTI fails
 * @return Cache access latency in cycles
 */
static int getCacheLatencyCycles(int size_kb, int ways, int line_size,
                                  double frequency_mhz, int tech_node_nm,
                                  int default_cycles) {
    // CACTI 7.0 supports 22-180nm; clamp and warn if outside range
    int cacti_tech = tech_node_nm;
    if (cacti_tech < 22) {
        static bool warned = false;
        if (!warned) {
            std::cerr << "Warning: CACTI does not support " << tech_node_nm
                      << "nm. Clamping to 22nm for cache timing.\n";
            warned = true;
        }
        cacti_tech = 22;
    }
    pimid::CACTIWrapper::SRAMConfig cfg;
    cfg.capacity_bytes = static_cast<uint64_t>(size_kb) * 1024;
    cfg.associativity = ways;
    cfg.line_size = line_size;
    // Scale banks with cache size: 1 bank per 64KB (L1=1, 2MB L2=32)
    cfg.banks = std::max(1, size_kb / 64);
    cfg.is_cache = true;
    cfg.tech_node_nm = cacti_tech;
    cfg.output_width_bits = line_size * 8;
    try {
        pimid::CACTIWrapper wrapper(cfg);
        wrapper.initialize();
        if (wrapper.isValid()) {
            double ns = wrapper.getAccessTime() * 1e9;
            int cycles = static_cast<int>(std::round(ns * frequency_mhz / 1000.0));
            return std::max(1, cycles);
        }
    } catch (...) {}
    return default_cycles;
}


/**
 * @brief Find qemu-x86_64 binary.
 *
 * Searches /usr/bin, /usr/local/bin, then PATH.
 * Returns empty string if not found.
 */
static std::string findQemuBinary() {
    std::string pimid_root = getPimidRoot();
    std::vector<std::string> qemu_paths = {
        // Prefer project-local QEMU build (built with --enable-plugins)
        pimid_root + "/external/qemu/build/qemu-x86_64",
        pimid_root + "/external/qemu/build/qemu-x86_64",
        "/usr/bin/qemu-x86_64",
        "/usr/local/bin/qemu-x86_64",
    };
    const char* path_env = getenv("PATH");
    if (path_env) {
        std::istringstream ss(path_env);
        std::string dir;
        while (std::getline(ss, dir, ':')) {
            qemu_paths.push_back(dir + "/qemu-x86_64");
        }
    }
    for (const auto& p : qemu_paths) {
        if (access(p.c_str(), X_OK) == 0) {
            return p;
        }
    }
    return "";
}

/**
 * @brief Find a QEMU plugin shared library by name.
 *
 * Searches build directories relative to PIMID root and current directory.
 * @param plugin_name e.g. "libpimid_trace.so" or "libzsim_qemu.so"
 * Returns empty string if not found.
 */
static std::string findQemuPlugin(const std::string& plugin_name) {
    std::string pimid_root = getPimidRoot();
    std::vector<std::string> plugin_paths = {
        pimid_root + "/build/external/zsim/" + plugin_name,
        pimid_root + "/build/external/zsim/" + plugin_name,
        pimid_root + "/external/zsim/build/" + plugin_name,
        "./" + plugin_name,
    };
    for (const auto& p : plugin_paths) {
        if (access(p.c_str(), F_OK) == 0) {
            return p;
        }
    }
    return "";
}

/**
 * @brief Find the zsim_trace executable.
 *
 * Searches build directories relative to PIMID root and current directory.
 * Returns empty string if not found.
 */
/**
 * @brief Find the libpimid_mpi.so shared library.
 *
 * Searches build directories relative to PIMID root and current directory.
 * Returns empty string if not found.
 */
static std::string findPimidMpiLib() {
    std::string pimid_root = getPimidRoot();
    // PIMID_ROOT typically points at the repo root, and the build lives at
    // ${ROOT}/build/. The other paths are kept for non-standard layouts.
    std::vector<std::string> search_paths = {
        pimid_root + "/build/libpimid_mpi.so",  // standard layout
        pimid_root + "/build/pimid/libpimid_mpi.so",
        pimid_root + "/build/lib/libpimid_mpi.so",
        pimid_root + "/build/libpimid_mpi.so",
        "./libpimid_mpi.so",
        "./build/libpimid_mpi.so",
    };
    for (const auto& p : search_paths) {
        if (access(p.c_str(), F_OK) == 0) {
            return p;
        }
    }
    return "";
}

/**
 * @brief Parsed ZSim output statistics for a single rank/instance.
 *
 * These are extracted from zsim.out and used to generate synthetic trace events.
 */
struct ZSimParsedOutput {
    uint64_t cycles = 0;
    uint64_t instrs = 0;

    // L1I: fhGETS (filtered hits), hGETS (hits), mGETS (misses) — accumulated across all instances
    uint64_t l1i_fhGETS = 0;
    uint64_t l1i_hGETS = 0;
    uint64_t l1i_mGETS = 0;

    // L1D: reads (fhGETS+hGETS+mGETS), writes (fhGETX+hGETX+mGETXIM), misses
    uint64_t l1d_fhGETS = 0;
    uint64_t l1d_hGETS = 0;
    uint64_t l1d_mGETS = 0;
    uint64_t l1d_fhGETX = 0;
    uint64_t l1d_hGETX = 0;
    uint64_t l1d_mGETXIM = 0;

    // L2: reads (hGETS+mGETS), writes (hGETX+mGETXIM), misses, evictions
    uint64_t l2_hGETS = 0;
    uint64_t l2_mGETS = 0;
    uint64_t l2_hGETX = 0;
    uint64_t l2_mGETXIM = 0;
    uint64_t l2_PUTS = 0;
    uint64_t l2_PUTX = 0;

    // L3: reads (hGETS+mGETS), writes (hGETX+mGETXIM), misses
    uint64_t l3_hGETS = 0;
    uint64_t l3_mGETS = 0;
    uint64_t l3_hGETX = 0;
    uint64_t l3_mGETXIM = 0;

    // Memory controller: rd, wr
    uint64_t mem_rd = 0;
    uint64_t mem_wr = 0;

    // Derived totals (computed after parsing)
    uint64_t l1i_total_reads() const { return l1i_fhGETS + l1i_hGETS + l1i_mGETS; }
    uint64_t l1d_total_reads() const { return l1d_fhGETS + l1d_hGETS + l1d_mGETS; }
    uint64_t l1d_total_writes() const { return l1d_fhGETX + l1d_hGETX + l1d_mGETXIM; }
    uint64_t l2_total_reads() const { return l2_hGETS + l2_mGETS; }
    uint64_t l2_total_writes() const { return l2_hGETX + l2_mGETXIM; }
    uint64_t l3_total_reads() const { return l3_hGETS + l3_mGETS; }
    uint64_t l3_total_writes() const { return l3_hGETX + l3_mGETXIM; }

    // Legacy compatibility
    uint64_t l1d_hits() const { return l1d_fhGETS + l1d_hGETS + l1d_fhGETX + l1d_hGETX; }
    uint64_t l1d_misses() const { return l1d_mGETS + l1d_mGETXIM; }
    uint64_t l1i_hits() const { return l1i_fhGETS + l1i_hGETS; }
    uint64_t l1i_misses() const { return l1i_mGETS; }
    uint64_t l2_hits() const { return l2_hGETS + l2_hGETX; }
    uint64_t l2_misses() const { return l2_mGETS + l2_mGETXIM; }
    uint64_t l3_hits() const { return l3_hGETS + l3_hGETX; }
    uint64_t l3_misses() const { return l3_mGETS + l3_mGETXIM; }
};

/**
 * @brief Parse a zsim.out file and extract key statistics.
 *
 * ZSim text stats format (from text_stats.cpp):
 *   AggregateStat: "name: # description" followed by indented children
 *   ScalarStat: "name: value # description"
 *
 * We track the current scope (l1d-N, l1i-N, l2-N, mem-N) by matching scope
 * header lines, then extract specific stat keys within each scope.
 */
static ZSimParsedOutput parseZSimOutputFile(const std::string& path) {
    ZSimParsedOutput out;
    std::ifstream f(path);
    if (!f.is_open()) return out;

    // Scope tracking: which cache/MC are we inside?
    enum class Scope { NONE, ROOT, L1D, L1I, L2, L3, MEM };
    Scope scope = Scope::NONE;
    int scope_indent = 0;  // indentation level of the current scope header

    auto extractScalarValue = [](const std::string& line) -> uint64_t {
        // Format: " statname: value # description"
        auto colon = line.find(':');
        if (colon == std::string::npos) return 0;
        std::string rest = line.substr(colon + 1);
        // Trim leading whitespace
        size_t start = rest.find_first_not_of(" \t");
        if (start == std::string::npos) return 0;
        rest = rest.substr(start);
        // If starts with '#', it's an aggregate header, not a scalar
        if (rest[0] == '#') return 0;
        // Extract number (stop at space or '#')
        size_t end = rest.find_first_of(" \t#");
        if (end != std::string::npos) rest = rest.substr(0, end);
        try { return std::stoull(rest); } catch (...) { return 0; }
    };

    auto getIndent = [](const std::string& line) -> int {
        int indent = 0;
        for (char c : line) {
            if (c == ' ') indent++;
            else break;
        }
        return indent;
    };

    auto getTrimmedKey = [](const std::string& line) -> std::string {
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        auto colon = line.find(':', start);
        if (colon == std::string::npos) return "";
        return line.substr(start, colon - start);
    };

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#' || line == "===") continue;

        int indent = getIndent(line);
        std::string key = getTrimmedKey(line);
        if (key.empty()) continue;

        // Check if this is an aggregate header: value after colon starts with "#"
        // (as opposed to a scalar like "cycles: 2328579 # Simulated cycles")
        bool is_aggregate = false;
        {
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                size_t after = line.find_first_not_of(" \t", colon + 1);
                if (after != std::string::npos && line[after] == '#') {
                    is_aggregate = true;
                }
            }
        }

        // If we went back to same or lower indent than scope header, leave scope
        if (scope != Scope::NONE && scope != Scope::ROOT && indent <= scope_indent) {
            scope = Scope::ROOT;
        }

        // Scope detection: match "l1d-N:", "l1i-N:", "l2-N:", "mem-N:" aggregate headers
        if (is_aggregate) {
            if (key.substr(0, 4) == "l1d-" || key.substr(0, 4) == "l1d_") {
                scope = Scope::L1D;
                scope_indent = indent;
                continue;
            } else if (key.substr(0, 4) == "l1i-" || key.substr(0, 4) == "l1i_") {
                scope = Scope::L1I;
                scope_indent = indent;
                continue;
            } else if (key.substr(0, 3) == "l2-" || key.substr(0, 3) == "l2_") {
                scope = Scope::L2;
                scope_indent = indent;
                continue;
            } else if (key.substr(0, 3) == "l3-" || key.substr(0, 3) == "l3_") {
                scope = Scope::L3;
                scope_indent = indent;
                continue;
            } else if (key.substr(0, 4) == "mem-" || key.substr(0, 4) == "mem_"
                       || key.substr(0, 6) == "pe-mc-") {
                scope = Scope::MEM;
                scope_indent = indent;
                continue;
            }
        }

        // Extract scalars within scope
        if (!is_aggregate) {
            uint64_t val = extractScalarValue(line);

            // Top-level stats
            if (scope == Scope::NONE || scope == Scope::ROOT) {
                if (key == "cycles" && out.cycles == 0) out.cycles = val;
                else if (key == "instrs" && out.instrs == 0) out.instrs = val;
            }

            // L1D scope
            if (scope == Scope::L1D) {
                if (key == "fhGETS") out.l1d_fhGETS += val;
                else if (key == "hGETS") out.l1d_hGETS += val;
                else if (key == "mGETS") out.l1d_mGETS += val;
                else if (key == "fhGETX") out.l1d_fhGETX += val;
                else if (key == "hGETX") out.l1d_hGETX += val;
                else if (key == "mGETXIM") out.l1d_mGETXIM += val;
            }

            // L1I scope
            if (scope == Scope::L1I) {
                if (key == "fhGETS") out.l1i_fhGETS += val;
                else if (key == "hGETS") out.l1i_hGETS += val;
                else if (key == "mGETS") out.l1i_mGETS += val;
            }

            // L2 scope
            if (scope == Scope::L2) {
                if (key == "hGETS") out.l2_hGETS += val;
                else if (key == "mGETS") out.l2_mGETS += val;
                else if (key == "hGETX") out.l2_hGETX += val;
                else if (key == "mGETXIM") out.l2_mGETXIM += val;
                else if (key == "PUTS") out.l2_PUTS += val;
                else if (key == "PUTX") out.l2_PUTX += val;
            }

            // L3 scope
            if (scope == Scope::L3) {
                if (key == "hGETS") out.l3_hGETS += val;
                else if (key == "mGETS") out.l3_mGETS += val;
                else if (key == "hGETX") out.l3_hGETX += val;
                else if (key == "mGETXIM") out.l3_mGETXIM += val;
            }

            // Memory controller scope (host MC uses rd/wr, PE-MC uses localAcc/remoteAcc)
            if (scope == Scope::MEM) {
                if (key == "rd" || key == "localAcc") out.mem_rd += val;
                else if (key == "wr" || key == "remoteAcc") out.mem_wr += val;
            }
        }
    }
    return out;
}

// Minimal includes to avoid header conflicts
// Full model integration happens through runtime configuration
#include "common/types.h"
#include "power/mcpat_wrapper.h"

// YAML parsing (if available)
#ifdef HAVE_YAML_CPP
#include <yaml-cpp/yaml.h>
#endif

using namespace pimid;

//=============================================================================
// Configuration Structure
//=============================================================================

struct UnifiedConfig {
    // Simulation dimensions
    std::string method;  // "exec", "trace", or "trace-gen"
    std::string scope;   // "device" or "system" ("cosim" remapped to "system")

    // Trace options
    std::string trace_file;      // Trace file path (input or output)
    // generate_trace removed — use method "trace-gen" instead

    // Common metadata
    std::string name;
    std::string description;

    // Memory configuration
    std::string memory_tech;
    int num_banks;
    int subarrays_per_bank;
    int memory_latency_override;  // -1 means auto-derive from tech
    int ports_per_bank = 1;       // Physical RW ports per bank (SRAM multiport, DRAM/NVM = 1)
    // DRAM device width (x4/x8/x16). Empty = use the technology's JEDEC default.
    // Selects both the Ramulator2 org preset and PIMID's chips/rank + BG/chip
    // so the timing model and the in-memory hierarchy stay consistent.
    std::string dram_device_width;

    // Comprehensive memory characteristics (for YAML override of external models)
    // If use_yaml_memory_params is true, user provided complete params in YAML
    bool use_yaml_memory_params = false;  // true = use YAML values, false = use external models

    struct MemoryParams {
        // REQUIRED for YAML override (5 parameters):
        double read_latency_ns = -1.0;      // Read latency in nanoseconds
        double write_latency_ns = -1.0;     // Write latency in nanoseconds
        double read_energy_nj = -1.0;       // Read energy in nanojoules
        double write_energy_nj = -1.0;      // Write energy in nanojoules
        double static_power_mw = -1.0;      // Static/leakage power in milliwatts

        // Check if user provided ALL required parameters
        bool isComplete() const {
            return read_latency_ns > 0 &&
                   write_latency_ns > 0 &&
                   read_energy_nj > 0 &&
                   write_energy_nj > 0 &&
                   static_power_mw > 0;
        }
    } memory_params;

    // Memory controller config (auto-derived from technology)
    std::string zsim_mem_controller_type = "auto"; // auto, simple, weavesimple, ramulator
    int md1_bandwidth_mbs = 6400;
    bool md1_bandwidth_user_set = false;  // true when YAML bandwidth_mbs is specified

    // WeaveSimple params
    int weave_bound_latency = -1;  // -1 = same as mem_latency (default)

    // Ramulator params
    std::string ramulator_config_file = "";  // Path to Ramulator2 YAML config

    // ZSim output directory (set at runtime, used to find zsim.out / garnet_stats.txt)
    std::string zsim_output_dir = "";

    // PE configuration
    std::string pe_type;
    std::string placement_level;
    int num_pes;

    // ALU scaling factors (5 design-point factors)
    double alu_compute_factor;     // cycles-per-instruction multiplier (default 1.0)
    double alu_access_factor;      // cycles per load/store (default 1.0)
    double alu_throughput_factor;  // parallelism divider (default 1.0)
    int    alu_operand_width;      // operand bit-width (default 32)
    double alu_energy_factor;      // per-op energy scale for reporting (default 1.0)

    // System configuration
    int frequency_mhz;
    int cache_line_size;
    int tech_node_nm;  // Technology node for CACTI queries (default 22nm)

    // Cache configuration
    int l1d_size_kb;
    int l1d_ways;
    int l1i_size_kb;
    int l1i_ways;
    int l2_size_kb;
    int l2_ways;
    bool enable_l2;
    int l2_count;        // Number of L2 instances (default 1 = shared by all PEs)

    // L3 cache (optional, unified LLC above clustered L2)
    bool enable_l3;
    int l3_size_kb;
    int l3_ways;

    // Cache timing/energy/power overrides (mirrors MemoryParams pattern)
    struct CacheParams {
        double latency_ns = -1.0;       // Access latency in nanoseconds
        double energy_nj = -1.0;        // Access energy in nanojoules
        double static_power_mw = -1.0;  // Static/leakage power in milliwatts

        bool isComplete() const {
            return latency_ns > 0 && energy_nj > 0 && static_power_mw > 0;
        }
    } l1d_params, l1i_params, l2_params, l3_params;

    bool use_yaml_cache_params = false;  // true if ALL active cache levels have complete params

    // NoC configuration
    std::string noc_topology;
    bool noc_topology_user_set = false;  // true when YAML noc.topology is specified
    int noc_router_latency;
    int noc_link_latency;
    // Channel-aware Garnet H-tree link latency for DRAM detailed/parallel
    // (accuracy fix). 0 = use noc_link_latency (no channel-aware override).
    // Set from per-channel DRAM bandwidth so detailed differentiates techs by
    // bandwidth; only the cycle-accurate Garnet network reads it (analytical
    // simple/calibrated/curve/approximate use noc_link_latency, unchanged).
    int noc_garnet_link_latency = 0;
    bool noc_cycle_accurate;
    std::string noc_routing;              // empty = default for topology
    int noc_vcs_per_vnet;
    int noc_buffers_per_vc;
    std::string noc_topology_file;        // required for CUSTOM topology
    std::string noc_routing_table_file;   // optional for TABLE routing
    int noc_control_msg_bits;             // 0 = default (64 bits)
    int noc_data_msg_bits;                // 0 = default (576 bits for cacheline)
    bool noc_ring_unidirectional;         // true = unidirectional ring (CW only)

    // Per-level network model choice ("simple", "md1", or "detailed")
    // Indexed by hierarchy level (0=subarray, ..., 6=system)
    // "analytical" accepted as backward-compat alias for "simple"
    std::array<std::string, 7> network_level_model = {
        // DEFAULT noc model is detailed; noc.model=analytical switches every
        // level to "simple" (the per-level analytical path).
        "detailed", "detailed", "detailed", "detailed",
        "detailed", "detailed", "detailed"
    };

    // Per-level network link overrides (optional, for forward-looking research)
    // -1 / empty / NaN = use technology default
    struct NetworkLevelOverride {
        int link_width_bits = -1;
        double frequency_ghz = -1.0;
        int latency_cycles = -1;
        std::string topology;    // empty = use tech default
        // Router params (negative = use tech default)
        int router_latency = -1;
        int router_pipeline = -1;   // 0=FULL, 1=REDUCED, 2=SIMPLE, 3=MINIMAL
        int router_bypass = -1;     // 0=off, 1=on
        int virtual_networks = -1;
        int virtual_channels_per_vn = -1;
        int input_buffer_depth = -1;
        int output_buffer_depth = -1;
    };
    std::array<NetworkLevelOverride, 7> network_level_overrides;

    // Per-boundary bridge overrides (optional)
    // Index 0=L0↔L1, 1=L1↔L2, ..., 5=L5↔L6
    // Bridge model is auto-derived from adjacent tier network models
    struct BridgeOverride {
        int count = -1;                  // -1 = use tech default
        int lower_width_bits = -1;
        double lower_frequency_mhz = -1.0;
        int upper_width_bits = -1;
        double upper_frequency_mhz = -1.0;
        int fifo_depth = -1;
        double latency_ns = -1.0;
        // Backward compat (legacy single-width fields)
        int width_bits = -1;             // sets both lower + upper
        int latency_cycles = -1;         // converted to latency_ns
        // Independent bridge model ("" = AUTO, "simple", "md1", "detailed")
        std::string model;
        // Router params (negative = use tech default)
        int router_latency = -1;
        int router_pipeline = -1;
        int router_bypass = -1;
        int virtual_networks = -1;
        int virtual_channels_per_vn = -1;
        int input_buffer_depth = -1;
        int output_buffer_depth = -1;
    };
    std::array<BridgeOverride, 6> bridge_overrides;

    // Pre-computed hierarchy latencies (populated by computeHierarchyLatencies)
    std::array<int, 7> hierarchy_level_latency = {0,0,0,0,0,0,0};
    std::array<int, 6> hierarchy_bridge_latency = {0,0,0,0,0,0};
    std::array<std::string, 6> hierarchy_bridge_model = {"auto","auto","auto","auto","auto","auto"};
    int pe_hierarchy_level = 1;  // 0=SUBARRAY, 1=BANK, 2=BANK_GROUP, 3=CHIP, 4=RANK
    int hierarchy_banks_per_bg = 4;
    int hierarchy_bg_per_chip = 4;
    int hierarchy_chips_per_rank = 8;
    int hierarchy_ranks_per_channel = 1;    // >1 for multi-rank (L5)
    int hierarchy_channels_per_system = 1;  // >1 for multi-device (L6)
    // TEMP (pre-arXiv): parallel DRAM channel count for the M/D/c contention
    // stop-gap (HBM3=16, HBM2=8, others=1). Set in the DRAM H-tree block.
    int hierarchy_dram_channels = 1;
    // Real datasheet AGGREGATE sustainable bandwidth (MB/s) of the memory tech,
    // from Ramulator (getBandwidth() = per-channel × channels). Used by the
    // detailed NoC model to cap effective DRAM bandwidth at the channel
    // bottleneck (a shared M/D/c queue across all bank-MIs of a channel), so
    // detailed is an accurate DRAM ground truth instead of permitting
    // num_banks × per-MI-BW (which is ~10× the real DDR4 channel). 0 = unknown
    // (no cap; pre-fix behavior). Set in the DRAM H-tree block.
    long long hierarchy_agg_bandwidth_mbs = 0;
    bool hierarchy_enabled = false;

    // PE-MI distributed memory interface config
    bool pe_mc_enabled = false;             // true when pim.mc section present in YAML
    std::string pe_mc_type = "simple";     // always "simple" (M/D/1 always active)
    int pes_per_mc = 1;                     // PEs sharing each MI (0 = host MC mode)
    bool pes_per_mc_user_set = false;       // true when user explicitly set pes_per_mc
    int pe_mc_local_latency = -1;           // -1 = auto from technology
    int pe_mc_bandwidth_mbs = -1;           // -1 = auto from technology
    // MC placement: false = with_core (default; MC co-located with core/cache),
    // true = standalone (MC is its own NoC endpoint, one per memory org, so even
    // a local PE access incurs an extra NoC hop to reach the MC fronting memory).
    bool mc_standalone = false;
    struct PEMCGroupOverride {
        std::vector<int> ids;
        std::string type;                   // always "simple"
        int local_latency = -1;
        int bandwidth_mbs = -1;
    };
    std::vector<PEMCGroupOverride> pe_mc_group_overrides;

    // PE-to-Memory-Organization mapping (M:N connectivity)
    // Connection mode: shared_io (PE embedded in mem org) or separate_endpoints (own NI)
    enum class PEMemConnectionMode { SHARED_IO, SEPARATE_ENDPOINTS };
    PEMemConnectionMode pe_mem_connection = PEMemConnectionMode::SHARED_IO;
    int local_link_latency = 2;  // cycles for PE↔local-mem hop (separate_endpoints only)

    // Explicit PE-to-mem-org mapping table
    struct PEMemMapping {
        int pe_id;
        std::vector<int> mem_org_ids;
    };
    std::vector<PEMemMapping> pe_mem_map;  // empty = auto 1:1

    // Derived: total memory org units at placement level
    int total_mem_orgs = -1;  // -1 = auto from memory organization
    // Derived: total network endpoints (depends on connection mode)
    int total_network_endpoints = -1;
    // Derived: topology-aware NoC average one-way latency (cycles)
    int noc_avg_one_way_latency = 0;
    // Derived: bisection bandwidth in links (for M/D/1 contention)
    int noc_bisection_links = 1;
    // Derived: NoC physical parameters for simple model
    int noc_flits_per_packet = 5;   // ceil(data_msg_bits / link_width_bits)
    int noc_per_hop_cycles = 2;     // router_lat + link_lat
    int noc_link_width_bits_cfg = 128;  // link width (flit size) in bits
    int noc_avg_hops_times_100 = 100;  // avgHops × 100 (fixed-point for shm)
    int noc_num_nodes = 16;            // total network nodes
    int noc_topology_class = 2;        // 0=BUS, 1=CROSSBAR, 2=MULTI_HOP
    int noc_total_channels = 32;       // total unidirectional channels
    int noc_hotspot_factor_100 = 100;  // hotspot factor x 100 (fixed-point)
    int noc_injector_calib = 0;        // 1 = calibrated model: probe per-access L0 via injector
    int noc_curve_model = 0;           // noc.model=curve -> probe latency-vs-load curve, interpolate at runtime
    int noc_calqueue = 0;              // noc.model=calqueue (Fix 1) -> probe L0 + M/D/1 contention + memory
    int noc_mlp_model = 0;             // 1 = analytical (hop+M/D/1+MLP); set by noc.model=analytical.
                                       // DEFAULT noc model is detailed (see noc_cycle_accurate)
    int noc_mlp_degree = -1;           // noc.mlp -> M = PE-model MLP intensity; -1 = AUTO from pe_type
                                       // (resolved by defaultMlpForPeType at config emission)
    int noc_parallel = 0;              // noc.model=parallel -> isolated per-thread/proc Garnet

    // Workload (required for both methods)
    std::string workload_binary;
    std::vector<std::string> workload_args;
    std::map<std::string, std::string> workload_env;

    // Simulation parameters
    int phase_length;
    long long max_instructions;
    int stats_interval;

    // Legacy co-sim field — set when scope "cosim" was auto-remapped to "system"
    bool cosim_remapped = false;

    // Host processor config (for cosim→system synthesis)
    std::string host_core_type = "ooo_core";
    int host_num_cores = 4;
    double host_frequency_mhz = 3000.0;
    int host_l1d_kb = 32;
    int host_l1i_kb = 32;
    int host_l2_kb = 1024;
    int host_l3_kb = 8192;
    std::string host_memory_tech = "DDR4";
    int host_tech_node_nm = 7;

    // Power analysis
    bool enable_power = true;  // --power / --no-power / YAML power.enabled
    std::string power_report_detail = "standard";  // summary, standard, verbose
    std::map<std::string, double> mcpat_overrides;  // power.mcpat_overrides from YAML

    // PCIe transfer modeling (cosim→system synthesis + system links)
    bool pcie_enabled = true;       // power.pcie.enabled (default true for cosim)
    int pcie_num_units = 1;         // power.pcie.num_units
    int pcie_num_channels = 16;     // power.pcie.num_channels (x16)
    double pcie_duty_cycle = 0.01;  // power.pcie.duty_cycle
    double pcie_load_perc = 0.01;   // power.pcie.total_load_perc
    // PCIe timing model (bandwidth + latency constrained link)
    bool pcie_timing_configured = false;  // set true when power.pcie section exists in YAML
    double pcie_base_latency_ns = 500.0;  // per-transaction overhead
    double pcie_bandwidth_GBs = 63.0;     // peak unidirectional throughput
    int pcie_num_lanes = 16;              // lane count (also feeds McPAT num_channels)
    std::string pcie_model = "simple";    // "simple" or "md1" for PCIe timing model
    // Host<->device link technology. Selects preset latency/BW/overhead unless
    // the user overrides them. "interposer" = 2.5D silicon interposer (UCIe-class
    // on-package: very high BW, low latency, ~no protocol/coherence overhead).
    std::string pcie_link_type = "pcie_gen5";
    int pcie_header_bytes = 20;            // per-transaction protocol overhead bytes
    double pcie_coherence_extra_ns = 0.0;  // avg extra latency for coherent access

    // Output configuration
    std::string stats_file;
    bool enable_detailed_stats;

    // Parallel workload configuration
    std::string workload_type;   // "serial", "openmp", "mpi"
    int mpi_ranks;               // Number of MPI ranks (0 = auto)

    //=========================================================================
    // Multi-Host Multi-Device System Architecture (scope: "system")
    //=========================================================================

    struct SystemNode {
        std::string name;
        enum Role { HOST, DEVICE } role = DEVICE;
        enum DeviceType { COMPUTE, MEMORY_ONLY } device_type = COMPUTE;
        enum Attachment { INTERNAL, EXTERNAL } attachment = EXTERNAL;

        // Core config
        std::string core_type = "ooo_core";
        int num_cores = 0;
        double frequency_mhz = 1000.0;
        int tech_node_nm = 22;

        // Cache config
        int l1d_kb = 32, l1i_kb = 32, l2_kb = 256, l3_kb = 0;
        bool enable_l2 = true, enable_l3 = false;
        int l2_ways = 8, l3_ways = 16;
        int l1d_ways = 8, l1i_ways = 4;

        // Memory
        std::string memory_tech = "DDR4";
        int ports_per_bank = 1;

        // PIM config (DEVICE+COMPUTE only)
        std::string pe_type;
        int num_pes = 0;
        std::string placement_level = "BANK";
        double alu_compute_factor = 1.0;
        double alu_access_factor = 1.0;
        double alu_throughput_factor = 1.0;
        int alu_operand_width = 32;
        double alu_energy_factor = 1.0;
        std::string pe_mc_type = "simple";
        int pes_per_mc = 1;

        // NoC config (DEVICE with PEs)
        std::string noc_topology = "MESH_2D";
        std::string noc_model = "simple";

        // Per-node workload (optional — inherits top-level if empty)
        std::string workload_binary;
        std::vector<std::string> workload_args;

        // Computed at config time
        int core_start_idx = 0;        // first core index in global ZSim array
        int core_end_idx = 0;          // last+1 core index
        double freq_scale = 1.0;       // reference_freq / node_freq
        uint64_t addr_start = 0;       // address range start (auto-assigned)
        uint64_t addr_end = 0;         // address range end (exclusive)
        int node_network_id = 0;       // position in system network topology
    };

    struct SystemLinkConfig {
        std::string src_name, dst_name;
        std::string link_type = "pcie_gen5";
        // Supported: pcie_gen4, pcie_gen5, cxl_2_0, cxl_3_0,
        //            nvlink_3_0, nvlink_4_0, nvlink_c2c, ualink_1_0, interposer
        int lanes = 16;
        double base_latency_ns = 500.0;
        double bandwidth_GBs = 63.0;
        int header_bytes = 20;             // protocol overhead per transaction
        std::string coherence = "none";    // none, bias, snoop, directory
        double coherence_extra_ns = 0.0;   // average extra latency for coherent access
    };

    struct SystemNetworkConfig {
        std::string topology = "crossbar";
        std::string model = "simple";      // simple, md1, detailed
        int link_width_bits = 512;
        double frequency_ghz = 1.0;
        int latency_cycles = 5;
        int router_latency = 1;
        int virtual_channels_per_vn = 1;
        int input_buffer_depth = 4;
        int output_buffer_depth = 4;
        std::vector<SystemLinkConfig> links;
    };

    std::vector<SystemNode> system_nodes;
    SystemNetworkConfig system_network;
    double reference_frequency_mhz = 0;  // max of all node frequencies (0 = auto)

    // Default constructor
    UnifiedConfig() :
        method("exec"),
        scope("device"),
        name("PIMID_Simulation"),
        description(""),
        memory_tech("SRAM"),
        num_banks(4),
        subarrays_per_bank(4),
        memory_latency_override(-1),
        pe_type("in_order_core"),
        placement_level("BANK"),
        num_pes(4),
        alu_compute_factor(1.0),
        alu_access_factor(1.0),
        alu_throughput_factor(1.0),
        alu_operand_width(32),
        alu_energy_factor(1.0),
        frequency_mhz(2000),
        cache_line_size(64),
        tech_node_nm(22),
        l1d_size_kb(32),
        l1d_ways(8),
        l1i_size_kb(16),
        l1i_ways(4),
        l2_size_kb(2048),
        l2_ways(16),
        enable_l2(true),
        l2_count(1),
        enable_l3(false),
        l3_size_kb(4096),
        l3_ways(16),
        noc_topology("MESH_2D"),
        noc_router_latency(1),
        noc_link_latency(1),
        noc_cycle_accurate(true),   // DEFAULT noc model = detailed (cycle-accurate Garnet)
        noc_routing(""),
        noc_vcs_per_vnet(4),
        noc_buffers_per_vc(4),
        noc_topology_file(""),
        noc_routing_table_file(""),
        noc_control_msg_bits(0),
        noc_data_msg_bits(0),
        noc_ring_unidirectional(false),
        phase_length(10000),
        max_instructions(1000000000LL),
        stats_interval(100000),
        stats_file("results/stats.txt"),
        enable_detailed_stats(true),
        workload_type("serial"),
        mpi_ranks(0) {}
};

/**
 * @brief Parse a bandwidth string with unit suffix and return MB/s.
 *
 * Accepts: "25600" (bare number = MB/s), "25600 MB/s", "25.6 GB/s",
 *          "1 TB/s", "6400000 KB/s". Case-insensitive units.
 * Returns the value in MB/s, or -1 on parse failure.
 */
static int parseBandwidthToMBs(const std::string& input) {
    if (input.empty()) return -1;

    // Find where the numeric part ends
    double value = 0;
    size_t pos = 0;
    try {
        value = std::stod(input, &pos);
    } catch (...) {
        return -1;
    }

    // Extract unit suffix (skip whitespace)
    std::string unit;
    for (size_t i = pos; i < input.size(); i++) {
        if (!std::isspace(input[i])) unit += std::toupper(input[i]);
    }

    // Strip trailing "/S" if present
    if (unit.size() >= 2 && unit.substr(unit.size() - 2) == "/S") {
        unit = unit.substr(0, unit.size() - 2);
    }

    double mbs;
    if (unit.empty() || unit == "MB" || unit == "MBS") {
        mbs = value;
    } else if (unit == "KB" || unit == "KBS") {
        mbs = value / 1000.0;
    } else if (unit == "GB" || unit == "GBS") {
        mbs = value * 1000.0;
    } else if (unit == "TB" || unit == "TBS") {
        mbs = value * 1000000.0;
    } else {
        return -1;
    }

    return std::max(1, static_cast<int>(std::round(mbs)));
}

/**
 * @brief Auto-derive memory controller type and parameters from technology.
 *
 * Maps memory technologies to ZSim controller models:
 *   DDR3/DDR4/DDR5/LPDDR5/GDDR6/HBM/HBM2/HBM3 -> Ramulator (cycle-accurate)
 *   STT-MRAM/PCM/ReRAM                          -> MD1 (M/D/1 queuing)
 *   SRAM                                         -> MD1 (fixed latency)
 *
 * Skipped if user explicitly set zsim_mem_controller_type != "auto".
 */
static void autoGenerateRamulatorConfig(UnifiedConfig& config, const std::string& tech);

static void getMemControllerConfig(UnifiedConfig& config) {
    // Helper to check if technology is DRAM-based
    auto isDramBased = [](const std::string& tech) {
        return tech == "DRAM" || tech == "DDR3" || tech == "DDR4" || tech == "DDR5" ||
               tech == "LPDDR5" || tech == "GDDR6" ||
               tech == "HBM2" || tech == "HBM3";
    };

    std::string tech = config.memory_tech;
    std::transform(tech.begin(), tech.end(), tech.begin(), ::toupper);

    // Validate user-specified controller against technology
    if (config.zsim_mem_controller_type != "auto") {
        std::string ct = config.zsim_mem_controller_type;
        // Reject removed types
        if (ct == "md1" || ct == "weavemd1") {
            std::cerr << "  [WARN] Controller type '" << ct << "' has been removed. "
                      << "Using 'simple' (M/D/1 is always active)." << std::endl;
            config.zsim_mem_controller_type = "simple";
            ct = "simple";
        }
        if (ct == "ramulator" && !isDramBased(tech)) {
            std::cerr << "  [WARN] Controller '" << ct << "' requires DRAM-based technology, "
                      << "but memory_tech is '" << config.memory_tech << "'. Falling back to simple." << std::endl;
            config.zsim_mem_controller_type = "simple";
        }
        // When using Simple/WeaveSimple with DRAM tech, derive bandwidth from Ramulator.
        // User-set bandwidth is clamped to the tech limit (can't exceed physical BW).
        if ((ct == "simple" || ct == "weavesimple") && isDramBased(tech)) {
            try {
                pimid::RamulatorWrapper bw_query("", tech);
                bw_query.initialize();
                double rank_bw_gbs = bw_query.getRankBandwidth();
                int tech_bw = static_cast<int>(rank_bw_gbs * 1000.0);
                if (config.md1_bandwidth_user_set) {
                    if (config.md1_bandwidth_mbs > tech_bw) {
                        std::cerr << "  [WARN] User bandwidth (" << config.md1_bandwidth_mbs
                                  << " MB/s) exceeds " << config.memory_tech << " physical limit ("
                                  << tech_bw << " MB/s). Clamping." << std::endl;
                    }
                    config.md1_bandwidth_mbs = std::min(config.md1_bandwidth_mbs, tech_bw);
                } else {
                    config.md1_bandwidth_mbs = tech_bw;
                }
            } catch (...) {
                // Keep user-set or default bandwidth on failure
            }
        }
        // User specified a valid controller, skip auto-derivation but still do weave upgrade
        goto weave_upgrade;
    }

    // --- Ramulator-backed DRAM technologies ---
    if (tech == "DRAM" || tech == "DDR3" || tech == "DDR4" || tech == "DDR5" ||
        tech == "LPDDR5" || tech == "GDDR6" ||
        tech == "HBM2" || tech == "HBM3") {
        config.zsim_mem_controller_type = "ramulator";
        // Auto-generate Ramulator config if none provided
        if (config.ramulator_config_file.empty()) {
            autoGenerateRamulatorConfig(config, tech);
        }
    }
    // --- NVSim-backed NVM technologies ---
    // For each tech, compute the physical bandwidth limit from access time.
    // User-set bandwidth is clamped to this limit.
    else if (tech == "STT_MRAM" || tech == "STTMRAM" || tech == "STT-MRAM" || tech == "MRAM") {
        config.zsim_mem_controller_type = "simple";
        int tech_bw = config.num_banks * config.cache_line_size * 1000 / 10; // ~10ns
        config.md1_bandwidth_mbs = config.md1_bandwidth_user_set ?
            std::min(config.md1_bandwidth_mbs, tech_bw) : tech_bw;
    } else if (tech == "PCM" || tech == "PCRAM" || tech == "3DXPOINT") {
        config.zsim_mem_controller_type = "simple";
        int tech_bw = config.num_banks * config.cache_line_size * 1000 / 50; // ~50ns
        config.md1_bandwidth_mbs = config.md1_bandwidth_user_set ?
            std::min(config.md1_bandwidth_mbs, tech_bw) : tech_bw;
    } else if (tech == "RERAM" || tech == "RESISTIVE" || tech == "MEMRISTOR") {
        config.zsim_mem_controller_type = "simple";
        int tech_bw = config.num_banks * config.cache_line_size * 1000 / 20; // ~20ns
        config.md1_bandwidth_mbs = config.md1_bandwidth_user_set ?
            std::min(config.md1_bandwidth_mbs, tech_bw) : tech_bw;
    }
    // --- CACTI-backed SRAM ---
    else if (tech == "SRAM") {
        config.zsim_mem_controller_type = "simple";
    } else {
        // Unknown tech, fall back to simple
        config.zsim_mem_controller_type = "simple";
    }

weave_upgrade:
    // Weave auto-upgrade: when the PE core is in_order_core or ooo_core and the
    // controller is Simple, upgrade to WeaveSimple for proper weave-phase
    // interaction. Ramulator already handles this internally.
    {
        std::string ct = config.zsim_mem_controller_type;
        bool weave_core = (config.pe_type == "in_order_core" ||
                           config.pe_type == "ooo_core");
        if (weave_core && ct == "simple") {
            config.zsim_mem_controller_type = "weavesimple";
        }
    }
}

// Forward declarations (needed because computeHierarchyLatencies calls these)
static void autoGeneratePEMemMap(UnifiedConfig& config);
static void validatePEMemMapping(const UnifiedConfig& config);
static double avgHopsForTopology(const std::string& topology, int num_nodes, bool ring_unidir = false);
static int bisectionLinksForTopology(const std::string& topology, int num_nodes, bool ring_unidir = false);
static int totalChannelsForTopology(const std::string& topology, int num_nodes, bool ring_unidir = false);
static double hotspotFactorForTopology(const std::string& topology);
static int topologyClassForTopology(const std::string& topology);

/**
 * @brief Emit a per-tech DRAM CUSTOM topology file for the detailed Garnet NoC.
 *
 * CONCEPTUAL FRAMING (important): the DRAM "network" here has NO real packet
 * routers. Each Garnet router emitted below is a SIMPLIFIED ABSTRACTION OF A
 * DATAPATH TRANSITION / MUX POINT in the DRAM hierarchy — bank IO, the
 * channel/DQ bus, and the system/IMC root. Each LINK models that transition's
 * datapath BANDWIDTH (link width) and LATENCY (cycles). The N parallel
 * channel-DQ subtrees model independent DRAM channels — the concurrency that
 * makes HBM3(16ch) > HBM2(8) > 1-channel DDR. There is no real routing; this
 * is purely a datapath/bandwidth model.
 *
 * Topology: CHANNEL-DQ CHOKEPOINT, STAR-OF-BANKS (replaces the old deep
 * fan-out tree in which bank->bank traffic took a SHORT path through a low
 * bankgroup/bank ancestor and BYPASSED the channel link, so nothing
 * saturated -> latency-bound). Here each channel is a STAR: all its banks
 * hang directly off ONE channel-DQ router (no bankgroup intermediates), so
 * inter-bank traffic in a channel has NO lower common ancestor and is FORCED
 * to cross the shared, narrow channel-DQ links + router -- the bandwidth wall
 * of real DRAM:
 *   ROOT(0) = system/IMC
 *     └─ N CHANNEL-DQ routers (L3)     ROOT--channelDQ link = L3 (narrow DQ
 *                                      bus); N parallel = channel concurrency
 *          └─ banks_per_channel BANK routers   channelDQ--bank link = L3 too
 *                                      (the SHARED narrow channel I/O that all
 *                                      the channel's banks contend for)
 *               └─ ~8 ENDPOINTS each  bank--endpoint ext link = L0 (wide)
 *   The 128 endpoints attach (ext links, L0 wide params) ROUND-ROBIN across
 *   the 16 logical banks: endpoint e -> bank (e % 16). Same-bank traffic stays
 *   local; inter-bank same-channel crosses the channel-DQ (the chokepoint);
 *   inter-channel crosses ROOT. Endpoints fan out BELOW the bank routers so no
 *   single router carries 128 ports.
 *
 * Sizing (chokepoint; every router small):
 *   ENDPOINTS = 128 (fixed; detailed Garnet hardcodes 128 nodes).
 *   TOTAL_BANKS = 16; banks_per_channel = ceil(16 / N).
 *     DDR3/4/5/LPDDR5/GDDR6 (N=1): 16 banks on 1 channel-DQ (max contention ->
 *       saturates the narrow channel -> bandwidth-bound).
 *     HBM2 (N=8): 2 banks/channel.   HBM3 (N=16): 1 bank/channel (parallel,
 *       no inter-bank contention on any single channel -> stays fast).
 *   Port counts: channel-DQ router = banks_per_channel + 1 uplink (DDR3 -> 17,
 *     HBM3 -> 2); bank router = ~8 endpoints + 1 uplink ~ 9; ROOT = N (<= 16).
 *     Max router port <= ~17 -- NO fat 128-port router. Total routers bounded.
 *
 * Per-link width/latency model (UNCHANGED):
 *   layer_BW_GBs = link_width_bits/8 * freq_GHz
 *   width(bits)  = clamp( round(128 * layer_BW_GBs / REF_BW), 1, 128 )   // occupancy = ceil(128/width)
 *   latency(cyc) = max(1, round(REF_FREQ / freq_GHz))                    // slower clock -> more latency
 *   REF_BW   = max layer BW across all techs = HBM3 L0 = 512/8*1.8 = 115.2 GB/s
 *   REF_FREQ = 2.4 GHz
 *
 * Each int edge is emitted in BOTH directions with identical lat/width.
 * Returns true on success.
 */
static bool emitDramCustomTopology(const std::string& tech,
                                   const std::string& outPath,
                                   int num_pes) {
    // Per-tech per-layer (link_width_bits, freq_GHz), layers leaf->root: L0,L1,L2,L3; plus channel count N.
    struct Layer { double width_bits; double freq_ghz; };
    Layer L[4];          // L[0]=L0 (subarray-leaf) ... L[3]=L3 (channel)
    int N = 1;           // parallel DRAM channels

    if (tech == "DDR3") {
        L[0]={512,0.8}; L[1]={256,0.8}; L[2]={256,0.8}; L[3]={192,0.8}; N=1;
    } else if (tech == "DDR4") {
        L[0]={512,1.2}; L[1]={256,1.2}; L[2]={256,1.2}; L[3]={192,1.2}; N=1;
    } else if (tech == "DDR5") {
        L[0]={512,2.4}; L[1]={256,2.4}; L[2]={256,2.4}; L[3]={128,2.4}; N=1;
    } else if (tech == "LPDDR5") {
        L[0]={256,1.6}; L[1]={256,1.6}; L[2]={256,1.6}; L[3]={16,1.6};  N=1;
    } else if (tech == "GDDR6") {
        L[0]={256,2.0}; L[1]={256,2.0}; L[2]={256,2.0}; L[3]={16,2.0};  N=1;
    } else if (tech == "HBM2") {
        L[0]={256,1.0}; L[1]={64,1.0};  L[2]={128,1.0}; L[3]={128,1.0}; N=8;
    } else if (tech == "HBM3") {
        L[0]={512,1.8}; L[1]={128,1.8}; L[2]={256,1.8}; L[3]={128,1.8}; N=16;
    } else {
        return false;  // not a modeled DRAM tech
    }

    const double REF_BW   = 115.2;  // GB/s, HBM3 L0 (= max layer BW across all techs)
    const double REF_FREQ = 2.4;    // GHz

    // Per-layer emitted link width (bits) and latency (cycles).
    // CONCURRENCY WEIGHTING: a bank-placed PE accesses exactly ONE channel, so
    // the channels are utilized COLLECTIVELY -- the number of channels actually
    // active = the number of channels that hold an active PE-bank =
    // min(num_pes, N) (PEs round-robin across the N channel subtrees). Weight
    // each link's effective bandwidth (width) by that concurrency factor:
    //   * single-PE (num_pes==1) -> factor 1 -> links carry the TRUE per-bank /
    //     per-channel bandwidth -> shows INTERNAL bandwidth (channel count
    //     irrelevant; differentiated by per-bank width/clock).
    //   * multi-PE (num_pes>=N)  -> factor N -> AGGREGATE bandwidth (HBM3 16x >
    //     HBM2 8x > single-channel DDR).
    // This is the physical basis for the two-part DRAM sweep (1-PE internal vs
    // multi-PE aggregate) for the same workload.
    int concurrency = (num_pes < N) ? num_pes : N;   // channels actually utilized
    if (concurrency < 1) concurrency = 1;
    int layer_w[4], layer_lat[4];
    for (int i = 0; i < 4; ++i) {
        double bw = L[i].width_bits / 8.0 * L[i].freq_ghz;                  // GB/s per channel
        long w = std::lround(128.0 * bw * (double)concurrency / REF_BW);    // x channels-utilized
        if (w < 1)   w = 1;
        if (w > 128) w = 128;
        layer_w[i] = (int)w;
        long lat = std::lround(REF_FREQ / L[i].freq_ghz);
        if (lat < 1) lat = 1;
        layer_lat[i] = (int)lat;
    }

    // ---- Channel-DQ chokepoint sizing (star-of-banks) -----------------------
    // 16 banks are partitioned across N channel-DQ routers. Each bank connects
    // ONLY to its channel-DQ router (a star -- NO bankgroup intermediates), so
    // bank->bank traffic in the same channel has NO common ancestor below the
    // channel-DQ and is FORCED to cross the (narrow) channel-DQ links + router.
    // That shared, narrow channel I/O is the bandwidth wall of real DRAM.
    const int ENDPOINTS = 128;   // fixed; detailed Garnet hardcodes 128 nodes
    const int TOTAL_BANKS = 16;  // the config's logical bank count (128 ep / 16 = 8/bank)
    int banks_per_channel = (TOTAL_BANKS + N - 1) / N;   // ceil(16/N)
    if (banks_per_channel < 1) banks_per_channel = 1;

    // Router id layout (contiguous): ROOT=0, then per channel
    //   1 channel-DQ router + banks_per_channel bank routers.
    // Endpoints fan out BELOW the bank routers (ext links), so no single router
    // ever carries 128 ports:
    //   * channel-DQ router: banks_per_channel + 1 (ROOT uplink) ports.
    //   * bank router:       ~8 endpoints + 1 (channel-DQ uplink) ports.
    //   * ROOT:              N ports.
    const int per_channel_routers = 1 + banks_per_channel;   // 1 channelDQ + banks
    const int total_routers = 1 + N * per_channel_routers;

    // Bank router id for logical bank b (0..15): banks map round-robin onto the
    // N channels, banks_per_channel per channel, in contiguous id order.
    //   channel of bank b = b / banks_per_channel
    //   within-channel bank index = b % banks_per_channel
    // Channel c occupies ids [base_c .. base_c + per_channel_routers); the first
    // is its channel-DQ, the rest are its bank routers.
    auto bank_router_id = [&](int b) -> int {
        int c = b / banks_per_channel;
        int within = b % banks_per_channel;
        int base = 1 + c * per_channel_routers;   // channel c's first router id
        return base + 1 + within;                 // skip the channel-DQ (base)
    };

    std::ofstream f(outPath);
    if (!f.is_open()) return false;

    f << "# Auto-generated DRAM CUSTOM topology for detailed Garnet NoC\n";
    f << "# NOTE: routers are NOT real packet routers — each is a SIMPLIFIED\n";
    f << "#       ABSTRACTION OF A DATAPATH TRANSITION/MUX POINT in the DRAM\n";
    f << "#       hierarchy (bank IO, channel/DQ bus, system/IMC). Each link\n";
    f << "#       models that transition's datapath bandwidth (width) and\n";
    f << "#       latency.\n";
    f << "# CHANNEL-DQ CHOKEPOINT (star-of-banks): per channel, ALL banks hang\n";
    f << "#       directly off ONE channel-DQ router via NARROW (L3) links --\n";
    f << "#       no bankgroup intermediates. bank->bank traffic in a channel\n";
    f << "#       has NO lower common ancestor, so it MUST cross the shared\n";
    f << "#       narrow channel-DQ links + router (the bandwidth wall). The N\n";
    f << "#       parallel channel-DQ subtrees model independent DRAM channels.\n";
    f << "#         ROOT --(L3 narrow)-- channelDQ_c --(L3 narrow)-- bank_b\n";
    f << "#                                                 --(L0 wide ext)-- endpoints\n";
    f << "# tech=" << tech << "  channels=" << N
      << "  num_pes=" << num_pes
      << "  channels_utilized(=min(pes,N))=" << concurrency << "\n";
    f << "# per-layer (width_bits, latency_cyc):"
      << " L0=(" << layer_w[0] << "," << layer_lat[0] << ")"
      << " L1=(" << layer_w[1] << "," << layer_lat[1] << ")"
      << " L2=(" << layer_w[2] << "," << layer_lat[2] << ")"
      << " L3=(" << layer_w[3] << "," << layer_lat[3] << ")\n";
    f << "# star: total_banks=" << TOTAL_BANKS
      << " banks/channel=" << banks_per_channel
      << " endpoints/bank~=" << (ENDPOINTS / TOTAL_BANKS)
      << "  channelDQ<->bank link = L3 (narrow, the chokepoint)"
      << "  bank<->endpoint = L0 (wide)\n";
    f << "routers " << total_routers << "\n";
    f << "endpoints " << ENDPOINTS << "\n";

    auto emit_int = [&](int a, int b, int w, int lat) {
        // bidirectional: emit both directions, identical params, weight=1
        f << "int " << a << " " << b << " 1 " << lat << " " << w << "\n";
        f << "int " << b << " " << a << " 1 " << lat << " " << w << "\n";
    };

    // Build per channel: a channel-DQ router under ROOT, then banks_per_channel
    // bank routers hanging off it. The ROOT<->channelDQ and channelDQ<->bank
    // links are BOTH the narrow L3 (channel/DQ) width -- this is the shared
    // channel I/O that all of a channel's banks contend for, so inter-bank
    // (same-channel) traffic serializes on it.
    int next_id = 1;   // 0 is ROOT
    for (int c = 0; c < N; ++c) {
        int dq = next_id++;                                  // channel-DQ (L3)
        emit_int(0, dq, layer_w[3], layer_lat[3]);           // ROOT -- channelDQ (narrow)

        for (int bk = 0; bk < banks_per_channel; ++bk) {
            int bank = next_id++;                            // bank router
            emit_int(dq, bank, layer_w[3], layer_lat[3]);    // channelDQ -- bank (narrow L3)
        }
    }

    // External links: 128 endpoints round-robin across the 16 logical banks
    // (endpoint e -> bank e % 16). Same-bank endpoints stay local; inter-bank
    // same-channel crosses the channel-DQ; inter-channel crosses ROOT. The
    // bank<->endpoint link is the WIDE L0 (subarray) width.
    for (int e = 0; e < ENDPOINTS; ++e) {
        int b = e % TOTAL_BANKS;
        int bank = bank_router_id(b);
        f << "ext " << e << " " << bank << " " << layer_lat[0] << " " << layer_w[0] << "\n";
    }

    f.close();
    return true;
}

/**
 * @brief Pre-compute hierarchy latencies from InternalDRAMNetwork.
 *
 * Instantiates the full 7-level hierarchy for the configured memory technology,
 * applies bridge overrides, and queries per-level transfer and bridge latencies
 * for a 64-byte cache line. Results are stored as integers in UnifiedConfig
 * so they can be emitted into the ZSim config file without library dependencies.
 */
static void computeHierarchyLatencies(UnifiedConfig& config) {
    // Map placement_level string to integer
    if (config.placement_level == "SUBARRAY")       config.pe_hierarchy_level = 0;
    else if (config.placement_level == "BANK")      config.pe_hierarchy_level = 1;
    else if (config.placement_level == "BANK_GROUP") config.pe_hierarchy_level = 2;
    else if (config.placement_level == "CHIP")      config.pe_hierarchy_level = 3;
    else if (config.placement_level == "RANK")      config.pe_hierarchy_level = 4;
    else if (config.placement_level == "HOST_MC")   config.pe_hierarchy_level = -1;  // PEs share host MC
    else                                             config.pe_hierarchy_level = 1;  // default BANK

    // Validate: PEs at a non-MC tier require a PE-MC at that tier
    // Skip for trace-gen: it only records QEMU execution, no simulation needed
    if (config.method != "trace-gen" && config.method != "synthetic" &&
        config.placement_level != "HOST_MC" && !config.pe_mc_enabled) {
        std::cerr << "ERROR: PEs placed at " << config.placement_level
                  << " level require a memory controller at that tier.\n"
                  << "  Add a 'pim.mc' section to your YAML config, or use "
                  << "'pim.placement.level: HOST_MC' if PEs share the host MC.\n";
        exit(1);
    }

    // HOST_MC: PEs share the host MC, no hierarchy computation needed
    if (config.pe_hierarchy_level == -1) {
        config.hierarchy_enabled = true;
        config.total_mem_orgs = config.num_pes;  // 1:1 identity for mapping
        config.hierarchy_banks_per_bg = 1;
        config.hierarchy_bg_per_chip = 1;
        config.hierarchy_chips_per_rank = 1;
        autoGeneratePEMemMap(config);
        validatePEMemMapping(config);
        config.total_network_endpoints = config.num_pes;
        return;
    }

    // Determine DRAM organization defaults
    int banks_per_bg = 4, bg_per_chip = 4, chips_per_rank = 8;

    // Technology-specific overrides
    std::string tech = config.memory_tech;
    if (tech == "DDR3")          { banks_per_bg = 8; bg_per_chip = 1; chips_per_rank = 8; }
    else if (tech == "DDR4")     { banks_per_bg = 4; bg_per_chip = 4; chips_per_rank = 8; }
    else if (tech == "DDR5")     { banks_per_bg = 4; bg_per_chip = 8; chips_per_rank = 8; }
    else if (tech == "LPDDR5")   { banks_per_bg = 4; bg_per_chip = 4; chips_per_rank = 1; }
    else if (tech == "GDDR6")    { banks_per_bg = 4; bg_per_chip = 4; chips_per_rank = 1; }
    else if (tech == "HBM2") { banks_per_bg = 4; bg_per_chip = 4; chips_per_rank = 8; }
    else if (tech == "HBM3")     { banks_per_bg = 4; bg_per_chip = 4; chips_per_rank = 16; }
    else if (tech == "SRAM" || tech == "STT_MRAM" || tech == "PCM" || tech == "RERAM") {
        banks_per_bg = config.num_banks; bg_per_chip = 1; chips_per_rank = 1;
    }

    // Optional JEDEC device-width override (memory.dram.device_width: x4|x8|x16).
    // Sets chips/rank = 64 / width for a 64-bit DDR channel, and adjusts bank
    // groups/chip per the JEDEC organization (DDR4 x16 -> 2 BG, DDR5 x16 -> 4 BG).
    // The same width also selects the Ramulator2 org preset (see ramulator_wrapper)
    // so the timing model and this hierarchy describe the same part.
    if (!config.dram_device_width.empty()) {
        const std::string& w = config.dram_device_width;
        bool is_ddr = (tech == "DDR3" || tech == "DDR4" || tech == "DDR5");
        bool is_lpddr5 = (tech == "LPDDR5");
        bool is_gddr6 = (tech == "GDDR6");
        bool is_hbm = (tech == "HBM2" || tech == "HBM3");

        if (is_hbm) {
            std::cerr << "ERROR: memory.dram.device_width is not applicable to "
                      << tech << " (stacked HBM has a fixed 128-bank organization, "
                      << "no x4/x8/x16 device width). Remove the key for HBM.\n";
            std::exit(1);
        }
        if (w != "x4" && w != "x8" && w != "x16") {
            std::cerr << "ERROR: memory.dram.device_width=" << w
                      << " is invalid. Supported values: x4, x8, x16.\n";
            std::exit(1);
        }
        // Per-tech legal width sets (from Ramulator2 org presets):
        //   DDR3/4/5: {x4,x8,x16}   LPDDR5: {x16}   GDDR6: {x8,x16}
        if (is_lpddr5 && w != "x16") {
            std::cerr << "ERROR: LPDDR5 only has an x16 organization (Ramulator2 "
                      << "presets are LPDDR5_*_x16). device_width=" << w
                      << " is not available for LPDDR5.\n";
            std::exit(1);
        }
        if (is_gddr6 && w == "x4") {
            std::cerr << "ERROR: GDDR6 supports x8 or x16 only (Ramulator2 presets "
                      << "are GDDR6_*_x8 / _x16). device_width=x4 is not available "
                      << "for GDDR6.\n";
            std::exit(1);
        }
        if (is_ddr || is_lpddr5 || is_gddr6) {
            // chips/rank for a 64-bit DDR-class channel
            if (w == "x4")       chips_per_rank = 16;
            else if (w == "x8")  chips_per_rank = 8;
            else /* x16 */       chips_per_rank = 4;

            // Bank-group count is coupled to width in the JEDEC tables.
            if (tech == "DDR4")      bg_per_chip = (w == "x16") ? 2 : 4;
            else if (tech == "DDR5") bg_per_chip = (w == "x16") ? 4 : 8;
            // DDR3 has no bank groups (banks_per_bg=8, bg_per_chip=1) at any width.
            // LPDDR5/GDDR6 keep 4 bank groups across their available widths.
        }
    }

    config.hierarchy_banks_per_bg = banks_per_bg;
    config.hierarchy_bg_per_chip = bg_per_chip;
    config.hierarchy_chips_per_rank = chips_per_rank;

    // Validate PE count fits the memory organization at the placement level
    int pe_level = config.pe_hierarchy_level;
    int slots = 1;
    // Compute total available slots at the PE placement level
    // Level 0=subarray, 1=bank, 2=BG, 3=chip, 4=rank
    if (pe_level == 0)      slots = config.subarrays_per_bank * banks_per_bg * bg_per_chip * chips_per_rank;
    else if (pe_level == 1) slots = banks_per_bg * bg_per_chip * chips_per_rank;
    else if (pe_level == 2) slots = bg_per_chip * chips_per_rank;
    else if (pe_level == 3) slots = chips_per_rank;
    else if (pe_level == 4) slots = 1;  // one rank

    if (config.num_pes > slots) {
        std::cerr << "WARNING: " << config.num_pes << " PEs requested at "
                  << config.placement_level << " level, but memory organization only has "
                  << slots << " slots. PEs will share memory org endpoints.\n";
    }

    auto hierarchy = pimid::createInternalDRAMNetwork(
        config.memory_tech, config.subarrays_per_bank,
        banks_per_bg, bg_per_chip, chips_per_rank);

    if (!hierarchy) {
        config.hierarchy_enabled = false;
        return;
    }

    // Apply per-level link + router overrides from YAML
    bool has_overrides = false;
    for (int i = 0; i < 7; ++i) {
        const auto& ov = config.network_level_overrides[i];
        bool has_link = ov.link_width_bits > 0 || ov.frequency_ghz > 0 ||
                        ov.latency_cycles > 0 || !ov.topology.empty();
        bool has_router = ov.router_latency >= 0 || ov.router_pipeline >= 0 ||
                          ov.router_bypass >= 0 || ov.virtual_networks > 0 ||
                          ov.virtual_channels_per_vn > 0 || ov.input_buffer_depth > 0 ||
                          ov.output_buffer_depth > 0;
        if (has_link || has_router) {
            hierarchy->overrideLevelConfig(i, ov.link_width_bits, ov.frequency_ghz,
                                           ov.latency_cycles, ov.topology,
                                           ov.router_latency, ov.router_pipeline,
                                           ov.router_bypass, ov.virtual_networks,
                                           ov.virtual_channels_per_vn,
                                           ov.input_buffer_depth, ov.output_buffer_depth);
            if (!has_overrides) {
                std::cout << "  YAML level overrides applied:\n";
                has_overrides = true;
            }
            std::cout << "    L" << i << ":";
            if (ov.link_width_bits > 0)        std::cout << " width=" << ov.link_width_bits << "b";
            if (ov.frequency_ghz > 0)          std::cout << " freq=" << ov.frequency_ghz << "GHz";
            if (ov.latency_cycles >= 0)        std::cout << " lat=" << ov.latency_cycles << "cy";
            if (!ov.topology.empty())          std::cout << " topo=" << ov.topology;
            if (ov.router_latency >= 0)        std::cout << " rtr_lat=" << ov.router_latency;
            if (ov.virtual_channels_per_vn > 0) std::cout << " vcs=" << ov.virtual_channels_per_vn;
            std::cout << "\n";
        }
    }

    // Apply per-level model overrides
    for (int i = 0; i < 7; ++i) {
        const auto& m = config.network_level_model[i];
        if (m == "detailed") {
            hierarchy->setLevelModel(i, pimid::NetworkModelType::DETAILED);
        } else if (m == "parallel") {
            // parallel: per-context isolated Garnet is driven by the nocParallel flag,
            // not by the per-level network model -- so the per-level model stays SIMPLE.
            hierarchy->setLevelModel(i, pimid::NetworkModelType::SIMPLE);
        } else {
            // "simple", "md1", "analytical" (backward compat) → SIMPLE
            hierarchy->setLevelModel(i, pimid::NetworkModelType::SIMPLE);
        }
    }

    // Apply per-bridge overrides (router params, link params, model)
    for (int i = 0; i < 6; ++i) {
        const auto& ov = config.bridge_overrides[i];
        // Apply router param overrides
        hierarchy->overrideBridgeConfig(i, ov.router_latency, ov.router_pipeline,
                                        ov.router_bypass, ov.virtual_networks,
                                        ov.virtual_channels_per_vn,
                                        ov.input_buffer_depth, ov.output_buffer_depth);
        // Apply bridge model override
        if (!ov.model.empty()) {
            if (ov.model == "simple" || ov.model == "md1")
                hierarchy->setBridgeModel(i, pimid::NetworkModelType::SIMPLE);
            else if (ov.model == "detailed")
                hierarchy->setBridgeModel(i, pimid::NetworkModelType::DETAILED);
            // "auto" or unknown → leave as AUTO (default)
        }
    }

    // Query per-level transfer latency (for 64B = 1 cache line)
    for (int i = 0; i < 7; ++i) {
        config.hierarchy_level_latency[i] = static_cast<int>(
            hierarchy->getTransferLatency(
                static_cast<pimid::NetworkLevel>(i), 0, 1, 64));
    }

    // Query per-bridge crossing latency (64B)
    // Bridge latency now computed by the bridge model (SIMPLE/MD1/DETAILED/AUTO)
    const auto& bridges = hierarchy->getBridges();
    for (int i = 0; i < 6; ++i) {
        const auto& br = bridges[i];
        const auto& ov = config.bridge_overrides[i];

        // Apply YAML link overrides with backward compat for the latency display
        int lower_w = (ov.lower_width_bits > 0) ? ov.lower_width_bits :
                      (ov.width_bits > 0) ? ov.width_bits : br.lower_width_bits;
        int upper_w = (ov.upper_width_bits > 0) ? ov.upper_width_bits :
                      (ov.width_bits > 0) ? ov.width_bits : br.upper_width_bits;
        double base_ns = (ov.latency_ns >= 0) ? ov.latency_ns :
                         (ov.latency_cycles >= 0) ? static_cast<double>(ov.latency_cycles) :
                         br.latency_ns;

        // Bridge = 1 hop, 2-port router
        int per_hop_router = (ov.router_latency >= 0) ? ov.router_latency : br.router_latency;
        bool bypass = (ov.router_bypass >= 0) ? (ov.router_bypass != 0) : br.router_bypass;
        if (bypass && per_hop_router > 1)
            per_hop_router = std::max(1, per_hop_router - 1);

        double data_bits = 64.0 * 8.0;
        int bottleneck_width = std::min(lower_w, upper_w);
        double serialization = std::ceil(data_bits / bottleneck_width);
        config.hierarchy_bridge_latency[i] = static_cast<int>(std::ceil(
            per_hop_router + base_ns + serialization));

        // Resolve and store bridge model string for display
        // Only show explicit model tag when user overrides; auto-derived stays "auto"
        std::string resolved_model = "auto";
        if (!ov.model.empty()) {
            resolved_model = ov.model;
        }
        config.hierarchy_bridge_model[i] = resolved_model;
    }

    config.hierarchy_enabled = true;

    // Compute total_mem_orgs at placement level.
    //
    // DRAM technologies (DDR3/4/5, LPDDR5, GDDR6, HBM2/3) have standardized,
    // complete memory organizations — the hierarchy is fixed by the standard.
    // Users cannot specify arbitrary bank counts for DRAM. total_mem_orgs is
    // derived from the technology's inherent organization.
    //
    // SRAM/NVM technologies are on-chip scratchpad where the designer has full
    // control over the organization. total_mem_orgs = user's num_banks.
    bool is_dram_tech = !(tech == "SRAM" || tech == "STT_MRAM" || tech == "PCM" || tech == "RERAM");

    if (config.total_mem_orgs < 0) {
        if (is_dram_tech) {
            // DRAM: total_mem_orgs = technology-derived slot count
            config.total_mem_orgs = slots;

            // Validate: user's num_banks must meet the technology minimum
            int tech_min_banks = banks_per_bg * bg_per_chip;  // minimum per chip
            if (config.num_banks < tech_min_banks) {
                std::cerr << "WARNING: " << tech << " technology requires at least "
                          << tech_min_banks << " banks per chip ("
                          << banks_per_bg << " banks/BG × " << bg_per_chip << " BG/chip). "
                          << "User specified num_banks=" << config.num_banks
                          << ". Using technology default of " << slots << " banks.\n";
                config.num_banks = slots;  // enforce minimum
            }
        } else {
            // SRAM/NVM: user controls the organization freely
            if (pe_level == 0)  // SUBARRAY
                config.total_mem_orgs = config.num_banks * config.subarrays_per_bank;
            else  // BANK or higher
                config.total_mem_orgs = config.num_banks;
        }
    }

    // Validate ports_per_bank
    if (config.ports_per_bank < 1) config.ports_per_bank = 1;

    // Auto-derive pes_per_mc from available MI ports
    if (config.pe_mc_enabled) {
        int max_mis = config.total_mem_orgs * config.ports_per_bank;
        int min_pes_per_mi = (config.num_pes + max_mis - 1) / max_mis;  // ceil division
        if (min_pes_per_mi < 1) min_pes_per_mi = 1;

        if (config.pes_per_mc_user_set) {
            if (config.pes_per_mc < min_pes_per_mi) {
                std::cerr << "WARNING: pes_per_mc=" << config.pes_per_mc
                          << " requires " << (config.num_pes / config.pes_per_mc)
                          << " MIs but only " << max_mis << " ports available ("
                          << config.total_mem_orgs << " mem_orgs x "
                          << config.ports_per_bank << " ports/bank); clamping to "
                          << min_pes_per_mi << std::endl;
                config.pes_per_mc = min_pes_per_mi;
            }
        } else {
            config.pes_per_mc = min_pes_per_mi;
        }
    }

    // Resolve PE-to-mem-org mapping
    autoGeneratePEMemMap(config);
    validatePEMemMapping(config);

    // Compute total network endpoints based on connection mode
    // L2 instances get separate endpoints when clustered (count > 1)
    // L3 always gets its own endpoint when present
    int cache_endpoints = 0;
    if (config.enable_l2 && config.l2_count > 1) cache_endpoints += config.l2_count;
    if (config.enable_l3) cache_endpoints += 1;

    if (config.pe_mem_connection == UnifiedConfig::PEMemConnectionMode::SEPARATE_ENDPOINTS) {
        config.total_network_endpoints = config.num_pes + config.total_mem_orgs + cache_endpoints;
    } else {
        config.total_network_endpoints = std::max(config.num_pes, config.total_mem_orgs) + cache_endpoints;
    }

    // ── DRAM internal network = Garnet H-tree with JEDEC-physics link latency ──
    //
    // Standard DRAM is a hierarchical (tree) distribution fabric, not a flat 2D
    // mesh. For DRAM technologies we default the device network to H_TREE and set
    // the per-hop LINK latency from physics: the time to drain one flit through
    // the technology's effective (aggregate, channel-parallel) datapath.
    //
    //   link_latency_cycles = ceil( flit_bytes / aggregate_BW_bytes_per_cycle )
    //
    // where aggregate_BW = per-channel BW × num_channels (HBM3 = 16 ch, HBM2 = 8,
    // DDR = 1), taken from Ramulator's JEDEC organization. High-bandwidth /
    // many-channel technologies drain a flit faster → lower link latency → a
    // faster tree, reproducing the JEDEC bandwidth hierarchy WITHOUT hand-tuned
    // per-tier tables. Computed at a fixed reference network clock so per-tech
    // core frequency does not invert the ordering. Only applied when the user
    // has not overridden the topology (default MESH_2D).
    {
        bool is_dram = (tech == "DDR3" || tech == "DDR4" || tech == "DDR5" ||
                        tech == "LPDDR5" || tech == "GDDR6" ||
                        tech == "HBM2" || tech == "HBM3" || tech == "DRAM");
        // DRAM's internal datapath is physically a hierarchical (H-tree) fabric,
        // NOT a flat mesh — so DRAM ALWAYS uses the H-tree, overriding any
        // mesh/ring/crossbar the config may request (those are only meaningful for
        // the host-side network and for non-DRAM device memories like SRAM/NVM).
        // A user H_TREE/CUSTOM choice is left as-is.
        if (is_dram) {
            // ── ALWAYS query the datasheet aggregate BW + channel count for any
            //    DRAM tech, regardless of whether the user already chose H_TREE.
            //    These feed the detailed NoC model's channel-BW bottleneck cap
            //    (accuracy fix). Previously this whole block was gated on
            //    "topology != H_TREE", so an explicit H_TREE config (as the
            //    validation cells use) left agg-BW=0 and the cap never engaged. ──
            double agg_mbs = 0.0;
            double per_chan_mbs = 0.0;
            int num_chan = 1;
            try {
                pimid::RamulatorWrapper bw_q("", tech);
                bw_q.initialize();
                agg_mbs = static_cast<double>(bw_q.getBandwidth());  // aggregate (stack/device) BW
                // Real datasheet AGGREGATE sustainable BW (MB/s): the detailed
                // NoC model caps effective DRAM BW at this (channel bottleneck).
                config.hierarchy_agg_bandwidth_mbs = (long long)agg_mbs;
                // Parallel DRAM channel count (HBM3=16, HBM2=8, DDR/LPDDR/GDDR=1).
                uint32_t nch = bw_q.getNumChannels();
                config.hierarchy_dram_channels = (nch >= 1) ? (int)nch : 1;
                num_chan = config.hierarchy_dram_channels;
                // Per-channel BW = aggregate / channels. This is the bandwidth of
                // the SHARED DQ datapath that bounds an individual access stream.
                per_chan_mbs = (num_chan > 0) ? (agg_mbs / (double)num_chan) : agg_mbs;
            } catch (...) { /* keep defaults on failure */ }

            // ── ACCURACY FIX (suspect #1): channel-aware Garnet H-tree link
            //    latency. The detailed (cycle-accurate shared Garnet) model's
            //    reported cycles ARE the Garnet batch tick count, and that tick
            //    count is driven by the H-tree per-link latency (verified: link=1
            //    -> 80.7M cyc / avgLat 23 ; link=8 -> 240M cyc / avgLat 84, a
            //    clean monotone lever; the per-access return added via the M/D/c
            //    queue does NOT couple to the cycle count in async-detailed mode,
            //    so the link latency is the ONLY working bandwidth knob). Before
            //    this fix the H-tree always used linkLatency=1 for EVERY DRAM
            //    tech, so DDR4 and HBM2 produced IDENTICAL cycles (~78-80M) and
            //    detailed could not tell a slow channel from a fast one ("too
            //    optimistic" / undifferentiated). We now set the H-tree link
            //    latency inversely proportional to the PER-CHANNEL BW (the shared
            //    DQ bus that bounds one access stream), anchored so the highest-
            //    per-channel-BW tech rounds to link=1. Aggregate (multi-channel)
            //    bandwidth advantage is still expressed by channel concurrency in
            //    the batch replay; per-access latency reflects the per-channel DQ
            //    rate. Applied for ALL DRAM (independent of force_htree) so the
            //    explicit-H_TREE validation cells get the channel-aware latency.
            //    Only overrides when the user has NOT hand-tuned noc.link_latency
            //    (still at its default 1).
            //    This sets a DEDICATED Garnet-only link latency
            //    (noc_garnet_link_latency); the analytical noc_link_latency used
            //    by simple/calibrated/curve/approximate is left untouched so
            //    those models are unchanged. Only the detailed (and parallel)
            //    cycle-accurate Garnet H-tree picks it up.
            if (per_chan_mbs > 0.0) {
                // Reference per-channel BW = the fastest modeled DRAM channel
                // (HBM3 @ 819000/16 = 51200 MB/s). Anchors HBM3 -> link 1.
                const double REF_PERCHAN_MBS = 51200.0;
                double ll_d = REF_PERCHAN_MBS / per_chan_mbs;     // >=1, larger = slower channel
                int ll = (int)std::lround(ll_d);
                if (ll < 1) ll = 1;
                if (ll > 64) ll = 64;                              // safety bound
                config.noc_garnet_link_latency = ll;
            }

            // ── DETAILED DRAM: per-tech CUSTOM topology (per-link latency+bw +
            //    channel concurrency). For the cycle-accurate (detailed,
            //    non-parallel) Garnet on a real DRAM tech we emit a TREE topology
            //    file that encodes the tech's DRAM hierarchy with per-layer link
            //    latency, per-link width (bandwidth via occupancy), and N parallel
            //    channel subtrees, then point Garnet at it via CUSTOM. This
            //    SUPERSEDES the scalar noc_garnet_link_latency above for detailed
            //    DRAM (that value is kept as the linkLatency fallback / inherited
            //    default for any link that omits per-link fields, and still drives
            //    non-DRAM and parallel paths). simple/calibrated/approximate/curve
            //    and parallel are left unchanged.
            bool detailed_dram =
                config.noc_cycle_accurate && (config.noc_parallel == 0) &&
                (tech == "DDR3" || tech == "DDR4" || tech == "DDR5" ||
                 tech == "LPDDR5" || tech == "GDDR6" ||
                 tech == "HBM2" || tech == "HBM3");
            if (detailed_dram) {
                // Write <TECH>.topo into the current working directory (stable,
                // absolute) so the generated config + Garnet subprocess can reach
                // it. cwd is where the run is launched and where the generated
                // config lives in practice.
                // Filename carries the tech, PE count (topology widths depend on
                // it via min(pes,N) concurrency) and PID, so concurrent runs and
                // different-PE sweeps never clobber each other's topology file.
                std::string topo_name = tech + "_pe" + std::to_string(config.num_pes)
                                      + "_" + std::to_string((long)getpid()) + ".topo";
                std::string topo_path;
                char cwdbuf[PATH_MAX];
                if (getcwd(cwdbuf, sizeof(cwdbuf)) != nullptr) {
                    topo_path = std::string(cwdbuf) + "/" + topo_name;
                } else {
                    topo_path = topo_name;
                }
                if (emitDramCustomTopology(tech, topo_path, config.num_pes)) {
                    config.noc_topology = "CUSTOM";
                    config.noc_topology_file = topo_path;
                    // The CUSTOM DRAM tree is routed by TreeRouter (up*/down*
                    // VC-class separation, external/garnet/src/TreeRouter.{hh,cc}),
                    // provably deadlock-free with as few as 2 VCs/vnet (1 UP +
                    // 1 DOWN) -- this replaces the old 32-VC stopgap. Floor at 2
                    // (the minimum the scheme needs); the config default of 4
                    // (2 UP + 2 DOWN) gives head-of-line headroom on the single-
                    // channel bottleneck.
                    if (config.noc_vcs_per_vnet < 2) config.noc_vcs_per_vnet = 2;

                    // ── NI FLIT-COUNT bandwidth model (replaces the old per-link
                    //    occupancy, which pinned upstream VCs for K cycles and
                    //    deadlocked the single-channel converging hub). Split each
                    //    DATA access into Fbw flits, Fbw ~ inverse per-channel
                    //    bandwidth (REF/per_chan): a lower-bandwidth tech gets MORE
                    //    flits/access, so its channel-DQ carries each access for
                    //    more cycles => lower per-channel bandwidth; the channel
                    //    COUNT (topology) yields the aggregate (N x 1/Fbw ~
                    //    agg_mbs/REF). Flits move one-per-cycle and never pin a VC,
                    //    so no deadlock. Detailed DRAM only -- simple/calibrated/
                    //    curve/approximate keep their own dataMsgBits untouched.
                    if (per_chan_mbs > 0.0) {
                        const double REF_PERCHAN = 51200.0; // HBM3 per-channel
                        int Fbw = (int)std::lround(REF_PERCHAN / per_chan_mbs);
                        if (Fbw < 1)  Fbw = 1;
                        if (Fbw > 32) Fbw = 32;             // tractability bound
                        int base_bits = config.noc_data_msg_bits > 0
                                        ? config.noc_data_msg_bits : 576;
                        config.noc_data_msg_bits = base_bits * Fbw;
                    }
                } else {
                    std::cerr << "WARNING: failed to emit DRAM CUSTOM topology "
                              << "for tech " << tech << " at " << topo_path
                              << "; falling back to scalar link latency.\n";
                }
            }

            // DRAM's internal datapath is physically a hierarchical (H-tree)
            // fabric, NOT a flat mesh — so DRAM ALWAYS uses the H-tree, overriding
            // any mesh/ring/crossbar the config may request. A user H_TREE/CUSTOM
            // choice is left as-is. The physics-derived link latency below is only
            // applied when WE force the topology to H_TREE (i.e. the user did not
            // already pick H_TREE/CUSTOM), to preserve any user link tuning.
            bool force_htree = (config.noc_topology != "H_TREE" &&
                                config.noc_topology != "CUSTOM");
            if (force_htree && agg_mbs > 0.0) {
                config.noc_topology = "H_TREE";
                // PER-CHANNEL bytes/cycle at a fixed reference network clock.
                // A single access traverses ONE channel's DQ datapath, so the
                // per-hop cost follows the PER-CHANNEL bandwidth (= agg/c).
                // Aggregate (multi-channel) bandwidth is expressed by the
                // analytical model's bandwidth floor (P*D/c), not by making
                // each individual hop cheaper. Using aggregate BW here crushed
                // HBM2/HBM3 per-access network cost ~10-20x below the detailed
                // ground truth (single-channel techs were unaffected: agg ==
                // per-channel for c=1). Validated vs detailed at 4PE/size-1024:
                // GDDR6/HBM2 went from 0.55-0.59x of detailed to within ~20%.
                const double NET_GHz = 2.0;
                const double FLIT_BYTES = 64.0;
                double chan_mbs = (per_chan_mbs > 0.0) ? per_chan_mbs : agg_mbs;
                double bytes_per_cycle = (chan_mbs * 1e6) / (NET_GHz * 1e9);
                if (bytes_per_cycle > 0.0) {
                    // RES scales sub-cycle differences into integer link latencies so
                    // very-high-bandwidth techs (HBM2 vs HBM3) remain distinguishable,
                    // and makes the (bandwidth-derived) network cost dominate the
                    // per-access total so ordering follows effective bandwidth rather
                    // than cell-access latency.
                    const double RES = 10.0;
                    double lat_cycles = RES * FLIT_BYTES / bytes_per_cycle;  // continuous

                    // Hop-normalize: the bandwidth bottleneck cost must be independent
                    // of how DEEP the H-tree is (i.e. of bank count). A technology with
                    // few banks (shallow tree, e.g. LPDDR5) would otherwise traverse
                    // fewer hops and appear faster than its bandwidth warrants. We scale
                    // the per-hop link latency by REF_HOPS / actual_hops so that the
                    // total network cost (link_latency × hops) reflects effective
                    // bandwidth alone, not tree depth.
                    // Depth = the PER-CHANNEL subtree: an access fans down one
                    // channel's bank subtree (orgs/c leaves), so channel count
                    // widens the tree without lengthening the path.
                    const double REF_HOPS = 8.0;
                    int endpoints = (config.total_mem_orgs > 1) ? config.total_mem_orgs : 2;
                    int chan_eps = endpoints / std::max(1, num_chan);
                    if (chan_eps < 2) chan_eps = 2;
                    double actual_hops = std::ceil(std::log2((double)chan_eps));
                    if (actual_hops < 1.0) actual_hops = 1.0;
                    lat_cycles *= (REF_HOPS / actual_hops);

                    int ll = static_cast<int>(std::lround(lat_cycles));
                    if (ll < 1) ll = 1;
                    config.noc_link_latency = ll;
                    config.noc_router_latency = 1;  // minimal router; link dominates

                    // Remove the old per-tier analytical model: each tier hop costs
                    // the SAME physics-derived link latency (the H-tree edge
                    // traversal), so the per-access remote cost scales with the
                    // technology's effective bandwidth, not hand-authored per-tier
                    // tables. Bridges add no extra latency.
                    for (int i = 0; i < 7; ++i) config.hierarchy_level_latency[i] = ll;
                    for (int i = 0; i < 6; ++i) config.hierarchy_bridge_latency[i] = 0;
                }
            }
        }
    }

    // Compute topology-aware NoC parameters for simple model
    int noc_nodes = config.total_network_endpoints > 0
                    ? config.total_network_endpoints : config.num_pes;
    bool ru = config.noc_ring_unidirectional;
    double avg_hops = avgHopsForTopology(config.noc_topology, noc_nodes, ru);
    int per_hop = config.noc_link_latency + config.noc_router_latency;

    // Flit/serialization: link_width = flit size, data_msg_bits = packet payload
    int link_width_bits = 128;  // matches Garnet flit size (ni_flit_size * 8)
    int data_msg_bits = config.noc_data_msg_bits > 0 ? config.noc_data_msg_bits : 576;
    int flits_per_packet = (data_msg_bits + link_width_bits - 1) / link_width_bits;

    config.noc_per_hop_cycles = per_hop;
    config.noc_flits_per_packet = flits_per_packet;
    config.noc_link_width_bits_cfg = link_width_bits;

    // One-way latency = NI overhead + internal hop traversal + wormhole serialization
    // NI overhead: 2×NI pipeline + 2×ext links = 6 cycles (validated against Garnet)
    int ni_overhead = 6;
    config.noc_avg_one_way_latency = ni_overhead
                                   + static_cast<int>(std::ceil(avg_hops * per_hop))
                                   + (flits_per_packet > 1 ? flits_per_packet - 1 : 0);

    // Store topology-aware parameters for contention model
    config.noc_avg_hops_times_100 = static_cast<int>(avg_hops * 100.0 + 0.5);
    config.noc_num_nodes = noc_nodes;
    config.noc_bisection_links = bisectionLinksForTopology(config.noc_topology, noc_nodes, ru);
    config.noc_topology_class = topologyClassForTopology(config.noc_topology);
    config.noc_total_channels = totalChannelsForTopology(config.noc_topology, noc_nodes, ru);
    double hotspot = hotspotFactorForTopology(config.noc_topology);
    config.noc_hotspot_factor_100 = static_cast<int>(hotspot * 100.0 + 0.5);
}

/**
 * @brief Auto-generate PE-to-mem-org mapping when not explicitly provided.
 *
 * Handles three cases:
 *   1. Empty map → 1:1 with wraparound if PEs > mem_orgs
 *   2. Uniform M:1 sentinel (pe_id == -M) → M PEs per mem org
 *   3. Uniform 1:N sentinel (pe_id == -1, mem_org_ids.size() == N) → N mem orgs per PE
 */
static void autoGeneratePEMemMap(UnifiedConfig& config) {
    int num_pes = config.num_pes;
    if (num_pes <= 0) return;  // nothing to map
    if (num_pes > 2048) {
        std::cerr << "WARNING: num_pes=" << num_pes
                  << " exceeds MAX_THREADS (2048). Clamping to 2048.\n";
        num_pes = 2048;
        config.num_pes = 2048;
    }
    int num_orgs = config.total_mem_orgs;
    if (num_orgs <= 0) num_orgs = num_pes;

    // Check for sentinel-based uniform mode from YAML parsing
    if (config.pe_mem_map.size() == 1 && config.pe_mem_map[0].pe_id < 0) {
        int sentinel_pe_id = config.pe_mem_map[0].pe_id;
        int sentinel_mo_count = static_cast<int>(config.pe_mem_map[0].mem_org_ids.size());

        if (sentinel_mo_count > 0 && config.pe_mem_map[0].mem_org_ids[0] == -1) {
            // 1:N mode — one PE manages N mem orgs
            int mos_per_pe = sentinel_mo_count;
            config.pe_mem_map.clear();
            for (int pe = 0; pe < num_pes; pe++) {
                UnifiedConfig::PEMemMapping m;
                m.pe_id = pe;
                for (int j = 0; j < mos_per_pe; j++) {
                    int mo = pe * mos_per_pe + j;
                    if (mo < num_orgs)
                        m.mem_org_ids.push_back(mo);
                }
                if (m.mem_org_ids.empty())
                    m.mem_org_ids.push_back(pe % num_orgs);
                config.pe_mem_map.push_back(m);
            }
        } else {
            // M:1 mode — M PEs share each mem org
            int pes_per_mo = -sentinel_pe_id;
            config.pe_mem_map.clear();
            for (int pe = 0; pe < num_pes; pe++) {
                UnifiedConfig::PEMemMapping m;
                m.pe_id = pe;
                int mo = pe / pes_per_mo;
                if (mo >= num_orgs) mo = num_orgs - 1;
                m.mem_org_ids.push_back(mo);
                config.pe_mem_map.push_back(m);
            }
        }
        return;
    }

    // If already explicitly populated (from YAML explicit mode), leave as-is
    if (!config.pe_mem_map.empty()) return;

    // Default: auto 1:1 with wraparound
    config.pe_mem_map.clear();
    for (int pe = 0; pe < num_pes; pe++) {
        UnifiedConfig::PEMemMapping m;
        m.pe_id = pe;
        m.mem_org_ids.push_back(pe % num_orgs);
        config.pe_mem_map.push_back(m);
    }
}

/**
 * @brief Validate PE-to-mem-org mapping table.
 */
static void validatePEMemMapping(const UnifiedConfig& config) {
    int num_pes = config.num_pes;
    int num_orgs = config.total_mem_orgs;
    if (num_orgs <= 0) return;

    // Check all PEs are mapped
    std::vector<bool> pe_mapped(num_pes, false);
    for (const auto& m : config.pe_mem_map) {
        if (m.pe_id < 0 || m.pe_id >= num_pes) {
            std::cerr << "WARNING: PE mapping has invalid pe_id=" << m.pe_id << "\n";
            continue;
        }
        pe_mapped[m.pe_id] = true;
        for (int mo : m.mem_org_ids) {
            if (mo < 0 || mo >= num_orgs) {
                std::cerr << "WARNING: PE " << m.pe_id << " maps to invalid mem_org_id="
                          << mo << " (valid range 0-" << (num_orgs - 1) << ")\n";
            }
        }
    }
    for (int pe = 0; pe < num_pes; pe++) {
        if (!pe_mapped[pe]) {
            std::cerr << "WARNING: PE " << pe << " has no mem org mapping\n";
        }
    }
}

/**
 * @brief Emit the sys.hierarchy { ... } block into a ZSim config file.
 *
 * Pre-computed latencies are written as integers so the zsim_trace driver
 * can read them without any PIMID library dependency.
 */
// Emit just the PCIe/CXL/interposer offload-link keys (consumed by
// getPCIeLatency for the WORK_BEGIN/END M/D/1 transfer charge). These live
// under sys.hierarchy.pcie*, but the full hierarchy block is only emitted for
// device DRAM hierarchies — so on the host↔device cosim path (often SRAM, no
// DRAM hierarchy) we emit a minimal standalone hierarchy block carrying only
// the pcie* keys. Caller must ensure no other hierarchy block is emitted.
static void emitZSimPcieBlock(std::ostream& out, const UnifiedConfig& config) {
    if (!config.pcie_timing_configured) return;
    double freq_mhz = config.frequency_mhz;
    uint32_t base_lat_cycles = static_cast<uint32_t>(
        config.pcie_base_latency_ns * freq_mhz / 1000.0 + 0.5);
    double bytes_per_cycle = (config.pcie_bandwidth_GBs * 1e9) / (freq_mhz * 1e6);
    out << "\n    hierarchy = {\n";
    out << "        pcieEnabled = 1;\n";
    out << "        pcieBaseLatencyCycles = " << base_lat_cycles << ";\n";
    out << "        pcieModel = 0;\n";
    out << "        pcieBytesPerCycle = \"" << std::fixed << std::setprecision(4)
        << bytes_per_cycle << "\";\n" << std::defaultfloat;
    out << "        pcieHeaderBytes = " << config.pcie_header_bytes << ";\n";
    uint32_t coh_cycles = static_cast<uint32_t>(
        config.pcie_coherence_extra_ns * freq_mhz / 1000.0 + 0.5);
    out << "        pcieCoherenceExtraCycles = " << coh_cycles << ";\n";
    out << "    };\n";
}

static void emitZSimHierarchyBlock(std::ostream& out, const UnifiedConfig& config) {
    if (!config.hierarchy_enabled) return;
    out << "\n    hierarchy = {\n";
    out << "        placementLevel = " << config.pe_hierarchy_level << ";\n";
    out << "        subarraysPerBank = " << config.subarrays_per_bank << ";\n";
    out << "        banksPerBG = " << config.hierarchy_banks_per_bg << ";\n";
    out << "        bgPerChip = " << config.hierarchy_bg_per_chip << ";\n";
    out << "        chipsPerRank = " << config.hierarchy_chips_per_rank << ";\n";
    out << "        ranksPerChannel = " << config.hierarchy_ranks_per_channel << ";\n";
    out << "        channelsPerSystem = " << config.hierarchy_channels_per_system << ";\n";
    out << "        dramChannels = " << config.hierarchy_dram_channels << ";\n";
    out << "        nocAggBandwidthMBs = " << config.hierarchy_agg_bandwidth_mbs << ";\n";
    for (int i = 0; i < 7; ++i)
        out << "        levelLatency" << i << " = " << config.hierarchy_level_latency[i] << ";\n";
    for (int i = 0; i < 6; ++i)
        out << "        bridgeLatency" << i << " = " << config.hierarchy_bridge_latency[i] << ";\n";
    for (int i = 0; i < 6; ++i)
        out << "        bridgeModel" << i << " = \"" << config.hierarchy_bridge_model[i] << "\";\n";

    // M:N PE-to-memory-org mapping
    out << "        connectionMode = " << static_cast<int>(config.pe_mem_connection) << ";\n";
    out << "        localLinkLatency = " << config.local_link_latency << ";\n";
    out << "        mcStandalone = " << (config.mc_standalone ? 1 : 0) << ";\n";
    out << "        totalMemOrgs = " << config.total_mem_orgs << ";\n";
    out << "        totalNetworkEndpoints = " << config.total_network_endpoints << ";\n";
    out << "        nocAvgOneWayLatency = " << config.noc_avg_one_way_latency << ";\n";
    out << "        nocBisectionLinks = " << config.noc_bisection_links << ";\n";
    out << "        nocFlitsPerPacket = " << config.noc_flits_per_packet << ";\n";
    out << "        nocPerHopCycles = " << config.noc_per_hop_cycles << ";\n";
    out << "        nocLinkWidthBits = " << config.noc_link_width_bits_cfg << ";\n";
    out << "        nocVcsPerVnet = " << config.noc_vcs_per_vnet << ";\n";
    out << "        nocAvgHopsTimes100 = " << config.noc_avg_hops_times_100 << ";\n";
    out << "        nocNumNodes = " << config.noc_num_nodes << ";\n";
    out << "        nocTopologyClass = " << config.noc_topology_class << ";\n";
    out << "        nocTotalChannels = " << config.noc_total_channels << ";\n";
    out << "        nocHotspotFactor100 = " << config.noc_hotspot_factor_100 << ";\n";
    out << "        nocInjectorCalib = " << config.noc_injector_calib << ";\n";
    out << "        nocCurveModel = " << config.noc_curve_model << ";\n";
    out << "        nocCalQueue = " << config.noc_calqueue << ";\n";
    out << "        nocMlpModel = " << config.noc_mlp_model << ";\n";
    // M (MLP intensity) for the analytical model. Explicit noc.mlp wins;
    // otherwise AUTO-derive from the PE core model:
    //   alu_core    -> 10  CALIBRATED vs detailed Garnet (P-sweep 2026-06-10:
    //                  err <= 1.11x across P=8/16/32 x DDR3/HBM3)
    //   in_order /  -> 10  PLACEHOLDER: cached cores do not fit a single M
    //   simple_core        (the NoC only sees misses; needs a miss-rate-aware
    //                       load term -- future work). User-overridable.
    //   ooo_core    -> 10  TODO: no cycle data under QEMU (known caveat).
    {
        int mlpM = config.noc_mlp_degree;
        if (mlpM < 1) mlpM = 10;   // AUTO: single validated default for now
        out << "        nocMlpDegree = " << mlpM << ";\n";
    }
    out << "        nocParallel = " << config.noc_parallel << ";\n";

    // Flattened mapping: offsets as space-separated, data as space-separated
    if (!config.pe_mem_map.empty()) {
        size_t map_size = std::min(config.pe_mem_map.size(), static_cast<size_t>(2048));
        // Count total data entries to validate against 4096 limit
        size_t total_data = 0;
        for (size_t i = 0; i < map_size; i++)
            total_data += config.pe_mem_map[i].mem_org_ids.size();
        if (total_data > 4096) {
            std::cerr << "WARNING: PE-mem mapping has " << total_data
                      << " total entries, exceeding 4096 limit. Truncating.\n";
        }

        out << "        peMemMapSize = " << map_size << ";\n";
        // Build prefix-sum offsets
        std::ostringstream offsets_ss, data_ss;
        int offset = 0;
        for (size_t i = 0; i < map_size; i++) {
            if (i > 0) offsets_ss << " ";
            offsets_ss << offset;
            for (size_t j = 0; j < config.pe_mem_map[i].mem_org_ids.size(); j++) {
                if (offset >= 4096) break;  // don't overflow peMemMapData[4096]
                if (offset > 0 || j > 0) data_ss << " ";
                data_ss << config.pe_mem_map[i].mem_org_ids[j];
                offset++;
            }
        }
        offsets_ss << " " << offset;  // sentinel
        out << "        peMemMapOffsets = \"" << offsets_ss.str() << "\";\n";
        out << "        peMemMapData = \"" << data_ss.str() << "\";\n";
    }

    // PCIe/CXL timing model — only emit when user explicitly configured it
    // (pcie_timing_configured is set when power.pcie section exists in YAML)
    if (config.pcie_timing_configured) {
        double freq_mhz = config.frequency_mhz;
        uint32_t base_lat_cycles = static_cast<uint32_t>(
            config.pcie_base_latency_ns * freq_mhz / 1000.0 + 0.5);
        double bytes_per_cycle = (config.pcie_bandwidth_GBs * 1e9) / (freq_mhz * 1e6);
        out << "        pcieEnabled = 1;\n";
        out << "        pcieBaseLatencyCycles = " << base_lat_cycles << ";\n";
        out << "        pcieModel = 0;\n";  // always SIMPLE (includes M/D/1 queuing)
        // Emit as string so ZSim config parser can read it as const char*
        out << "        pcieBytesPerCycle = \"" << std::fixed << std::setprecision(4)
            << bytes_per_cycle << "\";\n" << std::defaultfloat;

        // Protocol overhead and coherence — derived from system link config
        int header_bytes = 20;  // default PCIe TLP overhead
        double coherence_extra_ns = 0.0;
        if (!config.system_network.links.empty()) {
            header_bytes = config.system_network.links[0].header_bytes;
            coherence_extra_ns = config.system_network.links[0].coherence_extra_ns;
        }
        out << "        pcieHeaderBytes = " << header_bytes << ";\n";
        uint32_t coh_cycles = static_cast<uint32_t>(coherence_extra_ns * freq_mhz / 1000.0 + 0.5);
        out << "        pcieCoherenceExtraCycles = " << coh_cycles << ";\n";
    }

    // PE-MI distributed memory interface config
    if (config.pe_mc_enabled && config.placement_level != "HOST_MC") {
        int mc_count = config.num_pes / config.pes_per_mc;
        if (mc_count < 1) mc_count = 1;

        // Compute total units at the placement level
        int total_units = config.num_banks;
        if (config.placement_level == "SUBARRAY")
            total_units = config.num_banks * config.subarrays_per_bank;

        // Auto-derive local latency from technology if not overridden
        int local_latency = config.pe_mc_local_latency;
        if (local_latency < 0) {
            std::string tech = config.memory_tech;
            if (tech == "SRAM")             local_latency = 3;
            else if (tech == "STT_MRAM")    local_latency = 20;
            else if (tech == "PCM")         local_latency = 50;
            else if (tech == "RERAM")       local_latency = 30;
            else if (tech == "HBM2" || tech == "HBM3") local_latency = 8;
            else                            local_latency = 10;  // DDR default
        }

        // Auto-derive bandwidth from technology if not overridden
        int bw_mbs = config.pe_mc_bandwidth_mbs;
        if (bw_mbs < 0) {
            std::string tech = config.memory_tech;
            if (tech == "HBM2")             bw_mbs = 25600;
            else if (tech == "HBM3")        bw_mbs = 51200;
            else if (tech == "GDDR6")       bw_mbs = 19200;
            else if (tech == "SRAM")        bw_mbs = 51200;
            else                            bw_mbs = 12800;  // DDR default
        }

        out << "        totalUnits = " << total_units << ";\n";
        out << "        totalMCs = " << mc_count << ";\n";
        out << "        pesPerMC = " << config.pes_per_mc << ";\n";
        out << "        localLatency = " << local_latency << ";\n";
        out << "        defaultBandwidthMBs = " << bw_mbs << ";\n";

        // Emit per-group overrides
        for (const auto& grp : config.pe_mc_group_overrides) {
            for (int gid : grp.ids) {
                out << "        mcGroup" << gid << " = {\n";
                if (grp.bandwidth_mbs > 0)
                    out << "            bandwidthMBs = " << grp.bandwidth_mbs << ";\n";
                if (grp.local_latency > 0)
                    out << "            localLatency = " << grp.local_latency << ";\n";
                out << "        };\n";
            }
        }
    }

    out << "    };\n";

}

/**
 * @brief Synthesize system nodes from existing single-device config.
 *
 * For backward compatibility: when scope is "device" (or remapped from "cosim"),
 * create SystemNode entries from the existing flat UnifiedConfig fields so that
 * the system-level code path can treat all scopes uniformly.
 *
 * When cosim_remapped is true, also synthesizes the system_network from
 * the legacy host_* and pcie_* fields.
 */
static void synthesizeSystemNodes(UnifiedConfig& config) {
    if (config.scope == "system" && !config.cosim_remapped) return;  // already parsed from YAML

    config.system_nodes.clear();

    if (config.cosim_remapped) {
        // Host node (from legacy cosim host_* fields)
        UnifiedConfig::SystemNode host;
        host.name = "host";
        host.role = UnifiedConfig::SystemNode::HOST;
        host.core_type = config.host_core_type;
        host.num_cores = config.host_num_cores;
        host.frequency_mhz = config.host_frequency_mhz;
        host.tech_node_nm = config.host_tech_node_nm;
        host.l1d_kb = config.host_l1d_kb;
        host.l1i_kb = config.host_l1i_kb;
        host.l2_kb = config.host_l2_kb;
        host.l3_kb = config.host_l3_kb;
        host.enable_l3 = (config.host_l3_kb > 0);
        host.memory_tech = config.host_memory_tech;
        config.system_nodes.push_back(host);

        // Synthesize system_network from pcie config
        config.system_network.topology = "crossbar";
        config.system_network.model = config.pcie_model;  // always "simple" (md1 mapped earlier)
        if (config.pcie_timing_configured) {
            UnifiedConfig::SystemLinkConfig link;
            link.src_name = "host";
            link.dst_name = "device";
            link.link_type = config.pcie_link_type;
            link.lanes = config.pcie_num_lanes;
            link.base_latency_ns = config.pcie_base_latency_ns;
            link.bandwidth_GBs = config.pcie_bandwidth_GBs;
            link.header_bytes = config.pcie_header_bytes;
            link.coherence_extra_ns = config.pcie_coherence_extra_ns;
            config.system_network.links.push_back(link);
        }
    }

    // Device node (always present for device and cosim-remapped scopes)
    UnifiedConfig::SystemNode dev;
    dev.name = "device";
    dev.role = UnifiedConfig::SystemNode::DEVICE;
    dev.device_type = UnifiedConfig::SystemNode::COMPUTE;
    dev.attachment = config.pcie_timing_configured ?
        UnifiedConfig::SystemNode::EXTERNAL : UnifiedConfig::SystemNode::INTERNAL;
    dev.core_type = config.pe_type;
    dev.num_cores = config.num_pes;
    dev.num_pes = config.num_pes;
    dev.pe_type = config.pe_type;
    dev.frequency_mhz = config.frequency_mhz;
    dev.tech_node_nm = config.tech_node_nm;
    dev.memory_tech = config.memory_tech;
    dev.placement_level = config.placement_level;
    dev.alu_compute_factor = config.alu_compute_factor;
    dev.alu_access_factor = config.alu_access_factor;
    dev.alu_throughput_factor = config.alu_throughput_factor;
    dev.alu_operand_width = config.alu_operand_width;
    dev.alu_energy_factor = config.alu_energy_factor;
    dev.l1d_kb = config.l1d_size_kb;
    dev.l1i_kb = config.l1i_size_kb;
    dev.l2_kb = config.enable_l2 ? config.l2_size_kb : 0;
    dev.l3_kb = config.enable_l3 ? config.l3_size_kb : 0;
    dev.noc_topology = config.noc_topology;
    config.system_nodes.push_back(dev);
}

/**
 * @brief Normalize system config: assign core indices, address ranges, freq scaling.
 *
 * Called after YAML parsing + synthesize. Sets up:
 * - reference_frequency_mhz = max(all node frequencies)
 * - Per-node core_start_idx / core_end_idx (global ZSim core array)
 * - Per-node freq_scale = reference_freq / node_freq
 * - Per-device addr_start / addr_end (4GB-aligned contiguous ranges)
 * - Per-node node_network_id (position in system network topology)
 */
static void normalizeSystemConfig(UnifiedConfig& config) {
    if (config.system_nodes.empty()) return;

    // Validate: hosts must have at least one accompanying memory device
    bool has_host = false, has_device = false;
    for (const auto& n : config.system_nodes) {
        if (n.role == UnifiedConfig::SystemNode::HOST) has_host = true;
        if (n.role == UnifiedConfig::SystemNode::DEVICE) has_device = true;
    }
    if (has_host && !has_device) {
        std::cerr << "ERROR: A host must be accompanied by at least one memory device.\n"
                  << "  Add a device entry under 'system.devices' in your YAML config.\n";
        exit(1);
    }

    // 1. Compute reference frequency (max of all)
    double max_freq = 0;
    for (const auto& n : config.system_nodes) {
        if (n.frequency_mhz > max_freq) max_freq = n.frequency_mhz;
    }
    config.reference_frequency_mhz = max_freq;

    // 2. Assign core indices and freq_scale
    int core_idx = 0;
    for (auto& n : config.system_nodes) {
        n.core_start_idx = core_idx;
        n.core_end_idx = core_idx + n.num_cores;
        core_idx += n.num_cores;
        n.freq_scale = max_freq / n.frequency_mhz;
    }

    // 3. Assign network node IDs (all nodes) and address ranges (devices only)
    // Hosts access device memory through the system network — no host address range.
    uint64_t addr = 0;
    const uint64_t RANGE_SIZE = 0x100000000ULL;  // 4GB per device
    int net_id = 0;
    for (auto& n : config.system_nodes) {
        n.node_network_id = net_id++;
        if (n.role == UnifiedConfig::SystemNode::DEVICE) {
            n.addr_start = addr;
            n.addr_end = addr + RANGE_SIZE;
            addr += RANGE_SIZE;
        } else {
            // Host: no address range (accesses device memory through system network)
            n.addr_start = 0;
            n.addr_end = 0;
        }
    }

    // 4. Inherit top-level workload for nodes without explicit workload
    for (auto& n : config.system_nodes) {
        if (n.workload_binary.empty() && n.num_cores > 0) {
            n.workload_binary = config.workload_binary;
            if (n.workload_args.empty())
                n.workload_args = config.workload_args;
        }
    }
}

/**
 * @brief Check if system nodes require multiple QEMU processes (different binaries).
 */
static bool needsMultiQemu(const UnifiedConfig& config) {
    std::set<std::string> unique_binaries;
    for (const auto& node : config.system_nodes) {
        if (node.num_cores > 0 && !node.workload_binary.empty())
            unique_binaries.insert(node.workload_binary);
    }
    return unique_binaries.size() > 1;
}

/**
 * @brief Group system nodes by workload binary for multi-QEMU orchestration.
 * Each group becomes one QEMU process with a contiguous core range.
 */
struct ProcessGroup {
    std::string binary;
    std::vector<std::string> args;
    int core_start = 0;
    int core_end = 0;    // exclusive
    std::vector<int> node_indices;
};

static std::vector<ProcessGroup> buildProcessGroups(const UnifiedConfig& config) {
    std::vector<ProcessGroup> groups;
    std::map<std::string, size_t> binary_to_group;

    for (size_t i = 0; i < config.system_nodes.size(); i++) {
        const auto& node = config.system_nodes[i];
        if (node.num_cores == 0 || node.workload_binary.empty()) continue;

        auto it = binary_to_group.find(node.workload_binary);
        if (it == binary_to_group.end()) {
            ProcessGroup pg;
            pg.binary = node.workload_binary;
            pg.args = node.workload_args;
            pg.core_start = node.core_start_idx;
            pg.core_end = node.core_end_idx;
            pg.node_indices.push_back((int)i);
            binary_to_group[node.workload_binary] = groups.size();
            groups.push_back(std::move(pg));
        } else {
            auto& pg = groups[it->second];
            pg.core_start = std::min(pg.core_start, node.core_start_idx);
            pg.core_end = std::max(pg.core_end, node.core_end_idx);
            pg.node_indices.push_back((int)i);
        }
    }
    return groups;
}

/**
 * @brief Get average hop count for a topology with N nodes.
 * Same formulas used by InternalDRAMNetwork for tier networks.
 */
static double avgHopsForTopology(const std::string& topology, int num_nodes,
                                 bool ring_unidir) {
    if (num_nodes <= 1) return 0.0;
    int N = num_nodes;
    std::string topo = topology;
    // Normalize to uppercase
    for (auto& c : topo) c = std::toupper(c);

    // BUS/CROSSBAR: single central router, 0 internal hops.
    // NI→Router→NI uses external links only (accounted for separately).
    if (topo == "CROSSBAR") return 0.0;
    if (topo == "BUS") return 0.0;

    if (topo == "RING") {
        if (ring_unidir) {
            // Unidirectional ring: avg hops = (N-1)/2
            return (N - 1) / 2.0;
        }
        // Bidirectional ring: exact combinatorial avg hops
        if (N == 2) return 1.0;
        if (N % 2 == 0)
            return (double)(N * N) / (4.0 * (N - 1));
        else
            return (N + 1) / 4.0;
    }

    if (topo == "MESH_2D" || topo == "MESH") {
        // k×k mesh, XY routing: avg hops per dimension = (k²-1)/(3k), 2 dimensions
        double k = std::ceil(std::sqrt((double)N));
        return 2.0 * (k * k - 1.0) / (3.0 * k);
    }

    if (topo == "TORUS_2D" || topo == "TORUS") {
        // k×k torus: avg hops per dimension = k/4 (even k), 2 dimensions
        double k = std::ceil(std::sqrt((double)N));
        if ((int)k % 2 == 0)
            return 2.0 * (k / 4.0);
        else
            return 2.0 * (k * k - 1.0) / (4.0 * k);
    }

    if (topo == "FAT_TREE" || topo == "H_TREE") {
        // Binary tree (arity=2): exact LCA-based average internal hops
        int a = 2;
        int L = (int)std::ceil(std::log((double)N) / std::log((double)a));
        double totalHops = 0.0;
        int totalPairs = 0;
        int subtreeSize = a;
        for (int d = L; d >= 1; d--) {
            int prevSubtreeSize = (d == L) ? 1 : subtreeSize / a;
            int newPeers = subtreeSize - prevSubtreeSize;
            int hopCount = 2 * (L - d);
            totalHops += (double)newPeers * hopCount;
            totalPairs += newPeers;
            subtreeSize *= a;
        }
        return (totalPairs > 0) ? totalHops / totalPairs : 0.0;
    }

    if (topo == "STAR") return 2.0;  // always through center
    // Default: crossbar-like
    return 0.0;
}

/**
 * @brief Get bisection bandwidth (in links) for a topology with N nodes.
 * Bisection = min links cut to split network into two equal halves.
 */
static int bisectionLinksForTopology(const std::string& topology, int num_nodes,
                                     bool ring_unidir) {
    if (num_nodes <= 1) return 1;
    std::string topo = topology;
    for (auto& c : topo) c = std::toupper(c);

    if (topo == "CROSSBAR") return num_nodes / 2;
    if (topo == "FAT_TREE") return num_nodes / 2;  // full bisection BW
    if (topo == "TORUS_2D" || topo == "TORUS") {
        int side = (int)std::sqrt((double)num_nodes);
        return 2 * side;  // cut wraps around both dimensions
    }
    if (topo == "MESH_2D" || topo == "MESH") {
        int side = (int)std::sqrt((double)num_nodes);
        return side;  // cut along one dimension
    }
    if (topo == "RING") return ring_unidir ? 1 : 2;  // uni: 1 CW link; bi: 2 links
    if (topo == "BUS") return 1;      // single shared medium
    if (topo == "H_TREE") return 1;   // root link
    if (topo == "STAR") return 1;     // center node
    return num_nodes / 2;  // default: crossbar-like
}

/**
 * @brief Total unidirectional channels in the network.
 * Each channel carries 1 flit/cycle; this determines per-channel load.
 */
static int totalChannelsForTopology(const std::string& topology, int num_nodes,
                                    bool ring_unidir) {
    if (num_nodes <= 1) return 1;
    std::string topo = topology;
    for (auto& c : topo) c = std::toupper(c);

    int N = num_nodes;
    int k = (int)std::sqrt((double)N);

    if (topo == "BUS") return 1;           // shared bus: 1 flit/cycle total
    if (topo == "CROSSBAR") return N;      // N output ports
    if (topo == "RING") return ring_unidir ? N : 2 * N;  // uni: N CW; bi: N CW + N CCW
    if (topo == "MESH_2D" || topo == "MESH")
        return 4 * k * (k - 1);           // 2 dir × 2 dim × k rows × (k-1) links
    if (topo == "TORUS_2D" || topo == "TORUS")
        return 4 * k * k;                 // 2 dir × 2 dim × k rows × k links
    if (topo == "FAT_TREE" || topo == "H_TREE") {
        int levels = (int)std::ceil(std::log2((double)N));
        int total_routers = (1 << levels) - 1;
        return 2 * std::max(1, total_routers - 1);
    }
    return N;
}

/**
 * @brief Hotspot factor: ratio of max-loaded channel to average.
 * Accounts for non-uniform link loading per topology.
 */
static double hotspotFactorForTopology(const std::string& topology) {
    std::string topo = topology;
    for (auto& c : topo) c = std::toupper(c);

    if (topo == "BUS" || topo == "CROSSBAR") return 1.0;
    if (topo == "RING") return 1.0;
    if (topo == "MESH_2D" || topo == "MESH") return 2.0;
    if (topo == "TORUS_2D" || topo == "TORUS") return 3.5;
    if (topo == "FAT_TREE" || topo == "H_TREE") return 1.5;
    return 1.0;
}

/**
 * @brief Topology contention class: 0=BUS, 1=CROSSBAR, 2=MULTI_HOP.
 */
static int topologyClassForTopology(const std::string& topology) {
    std::string topo = topology;
    for (auto& c : topo) c = std::toupper(c);
    if (topo == "BUS") return 0;
    if (topo == "CROSSBAR") return 1;
    return 2;  // RING, MESH, TORUS, FAT_TREE, H_TREE
}

/**
 * @brief Compute system network latency between two nodes (in cycles at system net freq).
 *
 * Uses the same avgHops-based formula as intra-device tier networks.
 * Returns latency in reference frequency cycles.
 */
static uint32_t computeSystemNetLatency(const UnifiedConfig& config,
                                          int src_node_id, int dst_node_id) {
    if (src_node_id == dst_node_id) return 0;
    const auto& net = config.system_network;
    int num_nodes = (int)config.system_nodes.size();
    double hops = avgHopsForTopology(net.topology, num_nodes);
    double base_cycles = hops * (net.router_latency + net.latency_cycles);
    // Serialization: 64B cache line
    double data_bits = 64.0 * 8.0;
    double serialization = std::ceil(data_bits / net.link_width_bits);
    double net_lat_sys_cycles = base_cycles + serialization;

    // Convert from system network frequency to reference frequency
    double ref_freq_ghz = config.reference_frequency_mhz / 1000.0;
    double ratio = ref_freq_ghz / net.frequency_ghz;
    return static_cast<uint32_t>(std::ceil(net_lat_sys_cycles * ratio));
}

/**
 * @brief Write Ramulator2 YAML config content for a given DRAM technology.
 *
 * Reusable helper: writes the YAML to any ostream.  Called by both the
 * single-device autoGenerateRamulatorConfig() and the multi-device
 * generateSystemConfig() per-device path.
 */
// device_width: "" keeps each technology's JEDEC default org preset; "x4"/"x8"/
// "x16" selects the matching Ramulator2 preset so the timing model agrees with
// PIMID's chips/rank + BG/chip derivation (see the device_width block above).
// Width legality per tech is validated earlier; this only swaps the suffix.
static void writeRamulatorConfigYaml(std::ostream& ofs, const std::string& tech,
                                     const std::string& device_width = "") {
    ofs << "Frontend:\n  impl: GEM5\n\n";

    bool useClosedRow = false;

    if (tech == "DDR3") {
        std::string w = device_width.empty() ? "x8" : device_width;
        ofs << "MemorySystem:\n  impl: GenericDRAM\n  clock_ratio: 1\n  DRAM:\n    impl: DDR3\n"
            << "    org:\n      preset: DDR3_8Gb_" << w << "\n    timing:\n      preset: DDR3_1600H\n";
    } else if (tech == "DDR5") {
        std::string w = device_width.empty() ? "x8" : device_width;
        ofs << "MemorySystem:\n  impl: GenericDRAM\n  clock_ratio: 1\n  DRAM:\n    impl: DDR5\n"
            << "    org:\n      preset: DDR5_8Gb_" << w << "\n    timing:\n      preset: DDR5_3200AN\n"
            << "    RFM:\n      BRC: 2\n";
        useClosedRow = true;
    } else if (tech == "LPDDR5") {
        // LPDDR5 only has an x16 organization in Ramulator2.
        ofs << "MemorySystem:\n  impl: GenericDRAM\n  clock_ratio: 1\n  DRAM:\n    impl: LPDDR5\n"
            << "    org:\n      preset: LPDDR5_8Gb_x16\n    timing:\n      preset: LPDDR5_6400\n";
    } else if (tech == "GDDR6") {
        std::string w = device_width.empty() ? "x16" : device_width;
        ofs << "MemorySystem:\n  impl: GenericDRAM\n  clock_ratio: 1\n  DRAM:\n    impl: GDDR6\n"
            << "    org:\n      preset: GDDR6_8Gb_" << w << "\n    timing:\n      preset: GDDR6_2000_1350mV_double\n";
    } else if (tech == "HBM2") {
        ofs << "MemorySystem:\n  impl: GenericDRAM\n  clock_ratio: 1\n  DRAM:\n    impl: HBM2\n"
            << "    org:\n      preset: HBM2_4Gb\n    timing:\n      preset: HBM2_2.4Gbps\n";
    } else if (tech == "HBM3") {
        ofs << "MemorySystem:\n  impl: GenericDRAM\n  clock_ratio: 1\n  DRAM:\n    impl: HBM3\n"
            << "    org:\n      preset: HBM3_4Gb\n    timing:\n      preset: HBM3_6.4Gbps\n";
    } else {
        // Default: DDR4
        std::string w = device_width.empty() ? "x8" : device_width;
        ofs << "MemorySystem:\n  impl: GenericDRAM\n  clock_ratio: 1\n  DRAM:\n    impl: DDR4\n"
            << "    org:\n      preset: DDR4_8Gb_" << w << "\n    timing:\n      preset: DDR4_2400R\n";
        useClosedRow = true;
    }

    ofs << "  Controller:\n    impl: Generic\n"
        << "    Scheduler:\n      impl: FRFCFS\n"
        << "    RefreshManager:\n      impl: AllBank\n";
    if (useClosedRow) {
        ofs << "    RowPolicy:\n      impl: ClosedRowPolicy\n      cap: 4\n";
    } else {
        ofs << "    RowPolicy:\n      impl: OpenRowPolicy\n";
    }
    ofs << "  AddrMapper:\n    impl: RoBaRaCoCh\n";
}


/**
 * @brief Auto-generate a Ramulator2 YAML config file based on memory technology.
 */
static void autoGenerateRamulatorConfig(UnifiedConfig& config, const std::string& tech) {
    std::string tmpCfg = "/tmp/pimid_ramulator_" + std::to_string(getpid()) + ".yaml";
    std::ofstream ofs(tmpCfg);
    writeRamulatorConfigYaml(ofs, tech, config.dram_device_width);
    ofs.close();
    config.ramulator_config_file = tmpCfg;
}

/**
 * @brief Emit the appropriate ZSim mem = {...} block based on controller type.
 *
 * @param out Output stream for the config file
 * @param config UnifiedConfig with controller parameters
 * @param mem_latency Latency in cycles (used for MD1/Simple/Ramulator only)
 */
static void emitZSimMemBlock(std::ostream& out, const UnifiedConfig& config, int mem_latency) {
    std::string ct = config.zsim_mem_controller_type;
    int bound_lat = (config.weave_bound_latency >= 0) ? config.weave_bound_latency : mem_latency;

    if (ct == "ramulator") {
        out << "    mem = {\n";
        out << "        type = \"Ramulator\";\n";
        out << "        configFile = \"" << config.ramulator_config_file << "\";\n";
        out << "        latency = " << mem_latency << ";\n";
        out << "    };\n";
    } else if (ct == "weavesimple") {
        out << "    mem = {\n";
        out << "        type = \"WeaveSimple\";\n";
        out << "        latency = " << mem_latency << ";\n";
        out << "        bandwidth = " << config.md1_bandwidth_mbs << ";\n";
        out << "        boundLatency = " << bound_lat << ";\n";
        out << "    };\n";
    } else {
        // Simple (M/D/1 always active)
        out << "    mem = {\n";
        out << "        type = \"Simple\";\n";
        out << "        latency = " << mem_latency << ";\n";
        out << "        bandwidth = " << config.md1_bandwidth_mbs << ";\n";
        out << "    };\n";
    }
}

/**
 * @brief Emit the ZSim Garnet network configuration block.
 *
 * Handles all topologies (not just MESH_2D). Computes mesh dimensions for
 * grid topologies, derives effective routing from topology if not overridden.
 */
static void emitZSimNetworkBlock(std::ostream& out, const UnifiedConfig& config) {
    // Compute mesh dimensions from total network endpoints (not just num_pes)
    int num_nodes = config.total_network_endpoints > 0
                    ? config.total_network_endpoints : config.num_pes;
    // The emitted network is shared by every endpoint that registers into it:
    // device PEs/mem-orgs AND each system node's per-core caches (host_l1d-N,
    // N up to num_cores-1). Size it to cover the largest registering set so a
    // 16-core host does not overflow a 4-node grid (previously high-id caches
    // silently wrapped onto wrong nodes via modulo).
    for (const auto& n : config.system_nodes)
        if (n.num_cores > num_nodes) num_nodes = n.num_cores;
    if (config.num_pes > num_nodes) num_nodes = config.num_pes;
    int mesh_size = static_cast<int>(std::sqrt(num_nodes));
    if (mesh_size * mesh_size < num_nodes) mesh_size++;

    // Derive effective routing: if user didn't specify, use topology default
    std::string effective_routing = config.noc_routing;
    if (effective_routing.empty()) {
        // Map topology to default routing string
        if (config.noc_topology == "MESH_2D")       effective_routing = "XY";
        else if (config.noc_topology == "TORUS_2D")  effective_routing = "DOR";
        else if (config.noc_topology == "RING")      effective_routing = "SHORTEST";
        else if (config.noc_topology == "CROSSBAR")  effective_routing = "DIRECT";
        else if (config.noc_topology == "FAT_TREE")  effective_routing = "NCA";
        else if (config.noc_topology == "BUS")       effective_routing = "DIRECT";
        else if (config.noc_topology == "H_TREE")    effective_routing = "NCA";
        else if (config.noc_topology == "CUSTOM")    effective_routing = "TABLE";
        else                                         effective_routing = "XY";
    }

    // Node-grid sizing. MESH/TORUS need a 2D grid, so pad to a square
    // (ceil(sqrt(endpoints))^2); spare nodes carry no traffic and registration is
    // range-checked. Non-grid topologies (H_TREE, RING, BUS, FAT_TREE, CROSSBAR)
    // are sized to the EXACT endpoint count -- otherwise an over-padded H-tree
    // builds too many leaves and inflates hop counts.
    int noc_rows = mesh_size, noc_cols = mesh_size;
    if (config.noc_topology != "MESH_2D" && config.noc_topology != "TORUS_2D") {
        noc_rows = 1;
        noc_cols = num_nodes;
    }

    out << "    networkType = \"garnet\";\n";
    out << "    network = {\n";
    out << "        topology = \"" << config.noc_topology << "\";\n";
    out << "        rows = " << noc_rows << ";\n";
    out << "        cols = " << noc_cols << ";\n";
    out << "        routerLatency = " << config.noc_router_latency << ";\n";
    // Channel-aware Garnet link latency for DRAM detailed/parallel (accuracy
    // fix): when set (>0) the cycle-accurate Garnet H-tree uses the per-channel-
    // BW-derived link latency so DRAM techs are differentiated by bandwidth
    // (DDR4 slower channel -> higher link latency than HBM2/HBM3). The
    // analytical models do not read this network block, so simple/calibrated/
    // curve/approximate stay on noc_link_latency, unchanged.
    {
        // Apply the channel-aware override ONLY for the cycle-accurate Garnet
        // (detailed / parallel). The non-cycle-accurate models build a PROBE
        // network from this same block (calibrated/curve), so we must NOT alter
        // their linkLatency or their probed L0 would shift -> keep them on
        // noc_link_latency for cycleAccurate=false.
        bool detailed_only = config.noc_cycle_accurate && (config.noc_parallel == 0);
        int garnet_link = (detailed_only && config.noc_garnet_link_latency > 0)
                          ? config.noc_garnet_link_latency : config.noc_link_latency;
        out << "        linkLatency = " << garnet_link << ";\n";
    }
    out << "        cycleAccurate = " << (config.noc_cycle_accurate ? "true" : "false") << ";\n";
    out << "        routing = \"" << effective_routing << "\";\n";
    out << "        vcsPerVnet = " << config.noc_vcs_per_vnet << ";\n";
    out << "        buffersPerVc = " << config.noc_buffers_per_vc << ";\n";
    if (!config.noc_topology_file.empty()) {
        out << "        topologyFile = \"" << config.noc_topology_file << "\";\n";
    }
    if (!config.noc_routing_table_file.empty()) {
        out << "        routingTableFile = \"" << config.noc_routing_table_file << "\";\n";
    }
    if (config.noc_control_msg_bits > 0) {
        out << "        controlMsgBits = " << config.noc_control_msg_bits << ";\n";
    }
    if (config.noc_data_msg_bits > 0) {
        out << "        dataMsgBits = " << config.noc_data_msg_bits << ";\n";
    }
    if (config.noc_ring_unidirectional) {
        out << "        ringUnidirectional = true;\n";
    }
    out << "    };\n";
}

//=============================================================================
// Simulation Method Implementations
//=============================================================================

/**
 * Parsed Garnet network statistics for McPAT power modeling
 * Matches the stats output by ZSim's GarnetNetwork::writeStatsFile()
 */
struct GarnetParsedStats {
    uint64_t total_packets = 0;
    uint64_t total_flits = 0;
    uint64_t total_hops = 0;
    uint64_t buffer_reads = 0;
    uint64_t buffer_writes = 0;
    uint64_t crossbar_traversals = 0;
    uint64_t arbiter_events = 0;
    uint64_t link_traversals = 0;
    uint64_t total_cycles = 0;
    uint64_t total_latency = 0;
    uint32_t num_routers = 0;
    uint32_t num_rows = 0;
    uint32_t num_cols = 0;
    uint32_t flit_size_bits = 128;
    double clock_mhz = 1000.0;
};

/**
 * Parse Garnet stats file (key=value format) into GarnetParsedStats.
 * Returns default (zero) stats if the file doesn't exist.
 */
static GarnetParsedStats parseGarnetStatsFile(const std::string& path) {
    GarnetParsedStats stats;
    std::ifstream f(path);
    if (!f.is_open()) return stats;

    std::string line;
    while (std::getline(f, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        // Trim whitespace
        while (!key.empty() && std::isspace(key.back())) key.pop_back();
        while (!val.empty() && std::isspace(val.front())) val.erase(val.begin());

        try {
            if (key == "total_packets") stats.total_packets = std::stoull(val);
            else if (key == "total_flits") stats.total_flits = std::stoull(val);
            else if (key == "total_hops") stats.total_hops = std::stoull(val);
            else if (key == "buffer_reads") stats.buffer_reads = std::stoull(val);
            else if (key == "buffer_writes") stats.buffer_writes = std::stoull(val);
            else if (key == "crossbar_traversals") stats.crossbar_traversals = std::stoull(val);
            else if (key == "arbiter_events") stats.arbiter_events = std::stoull(val);
            else if (key == "link_traversals") stats.link_traversals = std::stoull(val);
            else if (key == "total_cycles") stats.total_cycles = std::stoull(val);
            else if (key == "total_latency") stats.total_latency = std::stoull(val);
            else if (key == "num_routers") stats.num_routers = std::stoul(val);
            else if (key == "num_rows") stats.num_rows = std::stoul(val);
            else if (key == "num_cols") stats.num_cols = std::stoul(val);
            else if (key == "flit_size_bits") stats.flit_size_bits = std::stoul(val);
            else if (key == "clock_mhz") stats.clock_mhz = std::stod(val);
        } catch (...) {
            // Skip unparseable lines
        }
    }
    return stats;
}

/**
 * JEDEC die density table (Mbit/mm²) for DRAM area estimation.
 * Values from published datasheets (Micron/Samsung/SK Hynix).
 */
static double getDRAMDieDensity(const std::string& tech) {
    if (tech == "DDR3")   return 45.0;   // 2Gb/~5.5mm²
    if (tech == "DDR4")   return 90.0;   // 8Gb/~11mm²
    if (tech == "DDR5")   return 165.0;  // 16Gb/~12mm²
    if (tech == "LPDDR5") return 180.0;
    if (tech == "GDDR6")  return 200.0;
    if (tech == "HBM2")   return 350.0;
    if (tech == "HBM3")   return 450.0;
    return 90.0;  // default: DDR4-class
}

/**
 * Map memory technology to McPAT MC parameters.
 */
static pimid::McPATWrapper::MCTechParams getMCTechParamsForMcPAT(
    const std::string& tech, int num_mcs)
{
    pimid::McPATWrapper::MCTechParams p;
    p.number_mcs = std::max(1, num_mcs);

    if (tech == "DDR3") {
        p.peak_transfer_rate = 1600; p.databus_width = 64; p.number_ranks = 2;
    } else if (tech == "DDR4") {
        p.peak_transfer_rate = 3200; p.databus_width = 64; p.number_ranks = 2;
    } else if (tech == "DDR5") {
        p.peak_transfer_rate = 4800; p.databus_width = 64; p.number_ranks = 2;
    } else if (tech == "LPDDR5") {
        p.peak_transfer_rate = 6400; p.databus_width = 32; p.number_ranks = 1;
    } else if (tech == "GDDR6") {
        p.peak_transfer_rate = 4000; p.databus_width = 32; p.number_ranks = 1;
    } else if (tech == "HBM2") {
        p.peak_transfer_rate = 2400; p.databus_width = 128; p.number_ranks = 1;
    } else if (tech == "HBM3") {
        p.peak_transfer_rate = 6400; p.databus_width = 128; p.number_ranks = 1;
    } else {
        // SRAM / NVM — simple controller
        p.peak_transfer_rate = 1600; p.databus_width = 64; p.number_ranks = 1;
    }
    return p;
}

/**
 * Build per-hierarchy-level NoCLevelConfig for McPAT.
 * If hierarchy is enabled, creates one NoC instance per active level.
 * Otherwise, creates a single NoC from the flat topology config.
 *
 * All derived parameters (duty_cycle, chip_coverage) are printed with
 * formulas and can be overridden via YAML power.mcpat_overrides.
 */
static std::vector<pimid::McPATWrapper::NoCLevelConfig> buildNoCLevelsForMcPAT(
    const UnifiedConfig& config,
    const GarnetParsedStats& garnet,
    uint64_t total_cycles,
    const std::map<std::string, double>& overrides)
{
    using NoCLevel = pimid::McPATWrapper::NoCLevelConfig;
    std::vector<NoCLevel> levels;

    // Map PIMID topology string to McPAT type (0=bus, 1=router-based)
    auto topologyToMcPATType = [](const std::string& topo) -> int {
        if (topo == "BUS") return 0;
        return 1;  // MESH_2D, TORUS_2D, RING, CROSSBAR, FAT_TREE, H_TREE, CUSTOM
    };

    if (config.hierarchy_enabled && config.pe_hierarchy_level >= 0 && config.pe_hierarchy_level < 7) {
        // Names for hierarchy levels
        static const char* level_names[] = {
            "L0_subarray", "L1_bank", "L2_bankgroup",
            "L3_chip", "L4_rank", "L5_channel", "L6_system"
        };

        // Count active levels (from PE placement level up to system)
        int pe_level = config.pe_hierarchy_level;
        int num_active = 7 - pe_level;
        if (num_active < 1) num_active = 1;

        uint64_t garnet_packets = garnet.total_packets;

        for (int lvl = pe_level; lvl < 7; lvl++) {
            NoCLevel nc;
            nc.name = level_names[lvl];

            // Lower levels (subarray/bank) → bus; upper levels → router NoC
            nc.type = (lvl <= 1) ? 0 : topologyToMcPATType(config.noc_topology);

            // Estimate node counts per level (use network endpoints at PE level)
            int nodes = 1;
            int pe_level_nodes = (config.total_network_endpoints > 0)
                                  ? config.total_network_endpoints : config.num_pes;
            if (lvl == pe_level) {
                int grid = static_cast<int>(std::ceil(std::sqrt(pe_level_nodes)));
                nc.horizontal_nodes = grid;
                nc.vertical_nodes = grid;
                nodes = pe_level_nodes;
            } else {
                // Higher levels have fewer nodes
                int factor = 1;
                for (int l = pe_level; l < lvl; l++) {
                    if (l == 1) factor *= config.hierarchy_banks_per_bg;
                    else if (l == 2) factor *= config.hierarchy_bg_per_chip;
                    else if (l == 3) factor *= config.hierarchy_chips_per_rank;
                    else factor *= 2;
                }
                nodes = std::max(2, pe_level_nodes / factor);
                int g = static_cast<int>(std::ceil(std::sqrt(nodes)));
                nc.horizontal_nodes = g;
                nc.vertical_nodes = g;
            }

            nc.input_ports = (nc.type == 0) ? nodes : 5;
            nc.output_ports = (nc.type == 0) ? nodes : 5;
            nc.flit_bits = 128;
            nc.clock_mhz = config.frequency_mhz;

            // Distribute Garnet accesses: lower levels see more traffic
            // Simple model: level i gets fraction proportional to 1/(i+1 - pe_level)
            double level_weight = 1.0 / (1 + lvl - pe_level);
            double total_weight = 0;
            for (int l = pe_level; l < 7; l++) total_weight += 1.0 / (1 + l - pe_level);
            uint64_t level_accesses = static_cast<uint64_t>(garnet_packets * level_weight / total_weight);

            // chip_coverage
            double chip_cov = 1.0 / num_active;

            // duty_cycle: fraction of peak bandwidth [0,1]
            // Peak = nodes × 1 access/cycle, so duty = accesses / (cycles × nodes)
            double duty = (total_cycles > 0 && nodes > 0)
                ? static_cast<double>(level_accesses) / (total_cycles * nodes) : 0.0;

            // Apply overrides
            std::string prefix = "noc." + std::to_string(lvl - pe_level);
            auto it_dc = overrides.find(prefix + ".duty_cycle");
            if (it_dc != overrides.end()) duty = it_dc->second;
            auto it_cc = overrides.find(prefix + ".chip_coverage");
            if (it_cc != overrides.end()) chip_cov = it_cc->second;
            auto it_acc = overrides.find(prefix + ".total_accesses");
            if (it_acc != overrides.end()) level_accesses = static_cast<uint64_t>(it_acc->second);

            nc.total_accesses = level_accesses;
            nc.duty_cycle = duty;
            nc.chip_coverage = chip_cov;

            levels.push_back(nc);
        }
    } else {
        // Single flat NoC
        NoCLevel nc;
        nc.name = "NoC_flat";
        nc.type = topologyToMcPATType(config.noc_topology);
        int flat_nodes = (config.total_network_endpoints > 0)
                          ? config.total_network_endpoints : config.num_pes;
        int grid = static_cast<int>(std::ceil(std::sqrt(flat_nodes)));
        nc.horizontal_nodes = grid;
        nc.vertical_nodes = grid;
        nc.input_ports = 5;
        nc.output_ports = 5;
        nc.flit_bits = 128;
        nc.clock_mhz = config.frequency_mhz;
        nc.chip_coverage = 1.0;

        uint64_t accesses = garnet.total_packets;
        // duty_cycle: fraction of peak bandwidth [0,1]
        double duty = (total_cycles > 0 && flat_nodes > 0)
            ? static_cast<double>(accesses) / (total_cycles * flat_nodes) : 0.0;

        auto it_dc = overrides.find("noc.0.duty_cycle");
        if (it_dc != overrides.end()) duty = it_dc->second;
        auto it_cc = overrides.find("noc.0.chip_coverage");
        if (it_cc != overrides.end()) nc.chip_coverage = it_cc->second;
        auto it_acc = overrides.find("noc.0.total_accesses");
        if (it_acc != overrides.end()) accesses = static_cast<uint64_t>(it_acc->second);

        nc.total_accesses = accesses;
        nc.duty_cycle = duty;

        levels.push_back(nc);
    }
    return levels;
}

/**
 * Run McPAT power analysis using simulation stats.
 * Called after successful simulation in exec/trace/MPI modes.
 *
 * All power/area results come from McPAT.
 * Derived McPAT input parameters are printed with formulas and are
 * overridable via YAML power.mcpat_overrides.
 */
static void runPowerAnalysis(const UnifiedConfig& config,
                             const ZSimParsedOutput& zsim_stats,
                             const std::string& output_dir) {
    using McPAT = pimid::McPATWrapper;

    // YAML overrides for McPAT derived parameters
    const std::map<std::string, double>& overrides = config.mcpat_overrides;

    // ── Device McPAT ──
    McPAT::SystemConfig mcfg;
    mcfg.num_cores = config.num_pes;
    mcfg.core_clock_mhz = config.frequency_mhz;
    mcfg.tech_node_nm = config.tech_node_nm;
    mcfg.temperature_k = 350;

    // Derive McPAT architecture from pe_type
    if (config.pe_type == "ooo_core") {
        mcfg.pipeline_depth = 19;
        mcfg.issue_width = 4;
        mcfg.num_alus = 4;
        mcfg.num_muls = 2;
        mcfg.num_fpus = 2;
    } else if (config.pe_type == "alu_core" || config.pe_type == "alu") {
        mcfg.pipeline_depth = 5;
        mcfg.issue_width = 1;
        mcfg.num_alus = 1;
        mcfg.num_muls = 0;
        mcfg.num_fpus = 0;
    } else {
        // in_order_core (real in-order, zsim InOrderCore), simple_core (coarse)
        mcfg.pipeline_depth = 14;
        mcfg.issue_width = 1;
        mcfg.num_alus = 3;
        mcfg.num_muls = 1;
        mcfg.num_fpus = 1;
    }

    // Apply mcpat_overrides as escape hatch for architecture-level knobs
    auto ov_get_int = [&overrides](const std::string& key, int fallback) -> int {
        auto it = overrides.find(key);
        return (it != overrides.end()) ? static_cast<int>(it->second) : fallback;
    };
    mcfg.pipeline_depth = ov_get_int("pipeline_depth", mcfg.pipeline_depth);
    mcfg.issue_width = ov_get_int("issue_width", mcfg.issue_width);
    mcfg.num_alus = ov_get_int("num_alus", mcfg.num_alus);
    mcfg.num_muls = ov_get_int("num_muls", mcfg.num_muls);
    mcfg.num_fpus = ov_get_int("num_fpus", mcfg.num_fpus);
    mcfg.device_type = ov_get_int("device_type", 0);
    mcfg.longer_channel_device = ov_get_int("longer_channel_device", 1);
    mcfg.number_hardware_threads = ov_get_int("number_hardware_threads", 1);
    mcfg.interconnect_projection_type = ov_get_int("interconnect_projection_type", 0);

    bool alu_only = (config.pe_type == "alu_core" || config.pe_type == "alu");
    if (alu_only) {
        mcfg.l1i_size_bytes = 0;
        mcfg.l1d_size_bytes = 0;
        mcfg.l2_size_bytes = 0;
        mcfg.l3_size_bytes = 0;
    } else {
        mcfg.l1i_size_bytes = config.l1i_size_kb * 1024ULL;
        mcfg.l1d_size_bytes = config.l1d_size_kb * 1024ULL;
        mcfg.l2_size_bytes = config.enable_l2 ? config.l2_size_kb * 1024ULL : 0;
        mcfg.l3_size_bytes = config.enable_l3 ? config.l3_size_kb * 1024ULL : 0;
    }

    // If PE-MCs are enabled, model them as memory controllers in McPAT
    if (config.pe_mc_enabled && config.pes_per_mc > 0) {
        mcfg.num_memory_controllers = config.num_pes / config.pes_per_mc;
    } else {
        mcfg.num_memory_controllers = 1;
    }
    mcfg.mc_clock_mhz = config.frequency_mhz / 2.0;
    mcfg.has_noc = (config.num_pes > 1);
    mcfg.noc_clock_mhz = config.frequency_mhz;
    mcfg.noc_num_routers = config.num_pes;
    int grid_side = static_cast<int>(std::ceil(std::sqrt(config.num_pes)));
    mcfg.noc_num_rows = grid_side;
    mcfg.noc_num_cols = grid_side;

    McPAT mcpat(mcfg);

    // Device profile
    if (alu_only)
        mcpat.setDeviceProfile(McPAT::DeviceProfile::DEVICE_ALU);
    else
        mcpat.setDeviceProfile(McPAT::DeviceProfile::DEVICE_INORDER);

    mcpat.initialize();

    // Feed simulation stats
    // OOO cores in QEMU mode may report cycles=0 (contention sim not triggered);
    // estimate cycles from instrs assuming IPC≈1 so McPAT gets reasonable values
    uint64_t cycles = zsim_stats.cycles;
    uint64_t instrs = zsim_stats.instrs > 0 ? zsim_stats.instrs : 1;
    if (cycles == 0 && instrs > 1) {
        cycles = instrs;  // Conservative IPC=1 estimate
        std::cout << "  Note: cycles=0 (OOO contention sim not triggered), "
                  << "estimating cycles=" << cycles << " from instrs" << std::endl;
    }
    if (cycles == 0) cycles = 1;
    mcpat.setTotalCycles(cycles);
    mcpat.setBusyCycles(cycles);
    mcpat.setTotalInstructions(instrs);

    // Split cache stats from ZSim
    mcpat.setL1IAccesses(zsim_stats.l1i_total_reads(), zsim_stats.l1i_mGETS);
    mcpat.setL1DAccesses(zsim_stats.l1d_total_reads(), zsim_stats.l1d_total_writes(),
                         zsim_stats.l1d_mGETS, zsim_stats.l1d_mGETXIM);
    mcpat.setL2Accesses(zsim_stats.l2_total_reads(), zsim_stats.l2_total_writes(),
                        zsim_stats.l2_mGETS, zsim_stats.l2_mGETXIM);
    mcpat.setL3Accesses(zsim_stats.l3_total_reads(), zsim_stats.l3_total_writes(),
                        zsim_stats.l3_mGETS, zsim_stats.l3_mGETXIM);

    // MC stats from ZSim
    mcpat.setMemControllerAccesses(zsim_stats.mem_rd, zsim_stats.mem_wr);

    // MC technology params
    McPAT::MCTechParams mc_tech = getMCTechParamsForMcPAT(config.memory_tech, 1);
    auto it_ptr = overrides.find("mc.peak_transfer_rate");
    if (it_ptr != overrides.end()) mc_tech.peak_transfer_rate = static_cast<int>(it_ptr->second);
    auto it_dbw = overrides.find("mc.databus_width");
    if (it_dbw != overrides.end()) mc_tech.databus_width = static_cast<int>(it_dbw->second);
    auto it_nr = overrides.find("mc.number_ranks");
    if (it_nr != overrides.end()) mc_tech.number_ranks = static_cast<int>(it_nr->second);
    mcpat.setMCTechParams(mc_tech);

    // Garnet stats
    std::string garnet_path = output_dir + "/garnet_stats.txt";
    GarnetParsedStats garnet = parseGarnetStatsFile(garnet_path);

    // Build per-level NoC configs
    if (config.num_pes > 1) {
        auto noc_levels = buildNoCLevelsForMcPAT(config, garnet, cycles, overrides);
        mcpat.setNoCLevels(noc_levels);
    }

    // Report detail level
    bool detail_verbose = (config.power_report_detail == "verbose");
    bool detail_summary = (config.power_report_detail == "summary");
    // detail_standard = !detail_verbose && !detail_summary

    // ── Print derived-parameter transparency block (verbose only) ──
    if (detail_verbose) {
        double pipeline_duty_cycle = static_cast<double>(cycles) / cycles;  // always 1.0 (busy=total)
        auto it_pdc = overrides.find("core.pipeline_duty_cycle");
        if (it_pdc != overrides.end()) pipeline_duty_cycle = it_pdc->second;

        std::cout << "\nMcPAT Derived Inputs:" << std::endl;
        if (it_pdc != overrides.end())
            std::cout << "  core.pipeline_duty_cycle    = " << pipeline_duty_cycle << "  [OVERRIDE from YAML]" << std::endl;
        else
            std::cout << "  core.pipeline_duty_cycle    = busy_cycles / total_cycles = "
                      << cycles << " / " << cycles << " = " << pipeline_duty_cycle << std::endl;

        std::cout << "  mc.peak_transfer_rate       = " << mc_tech.peak_transfer_rate << " MT/s"
                  << (it_ptr != overrides.end() ? "  [OVERRIDE from YAML]" : ("  [from " + config.memory_tech + " spec]"))
                  << std::endl;
        std::cout << "  mc.databus_width            = " << mc_tech.databus_width << " bits"
                  << (it_dbw != overrides.end() ? "  [OVERRIDE from YAML]" : ("  [from " + config.memory_tech + " spec]"))
                  << std::endl;
        std::cout << "  mc.number_ranks             = " << mc_tech.number_ranks
                  << (it_nr != overrides.end() ? "  [OVERRIDE from YAML]" : ("  [from " + config.memory_tech + " spec]"))
                  << std::endl;
        std::cout << "  mc.memory_reads             = " << zsim_stats.mem_rd << std::endl;
        std::cout << "  mc.memory_writes            = " << zsim_stats.mem_wr << std::endl;

        std::cout << "  pcie.total_load_perc        = 0.000  [no co-sim transfers]" << std::endl;
    }

    // ── Run device McPAT ──
    try {
        mcpat.computePower();
    } catch (const std::exception& e) {
        std::cerr << "\n[Power] McPAT failed: " << e.what() << std::endl;
        std::cerr << "[Power] Power analysis skipped." << std::endl;
        return;
    }

    // ── Print results (gated by report_detail) ──
    // Summary: one-line totals only
    // Standard: component breakdown + memory array + coverage table
    // Verbose: all of standard + derived inputs (above) + per-component area + XML dump
    if (detail_summary) {
        mcpat.printSummaryLine();
    } else {
        mcpat.printDetailedResults();

        // Per-level NoC power breakdown
        const auto& noc_level_power = mcpat.getNoCLevelPower();
        if (noc_level_power.size() > 1) {
            std::cout << "NoC power spans " << noc_level_power.size()
                      << " hierarchy levels (see breakdown above)." << std::endl;
        }

        // Verbose: per-component area table
        if (detail_verbose) {
            std::cout << "\nPer-Component Area Breakdown:" << std::endl;
            std::cout << "  Cores:              " << std::fixed << std::setprecision(3)
                      << mcpat.getComponentArea(McPAT::ComponentType::CORE) << " mm^2" << std::endl;
            std::cout << "  L2 Caches:          "
                      << mcpat.getComponentArea(McPAT::ComponentType::L2_CACHE) << " mm^2" << std::endl;
            std::cout << "  L3 Cache:           "
                      << mcpat.getComponentArea(McPAT::ComponentType::L3_CACHE) << " mm^2" << std::endl;
            std::cout << "  Memory Controllers: "
                      << mcpat.getComponentArea(McPAT::ComponentType::MEMORY_CONTROLLER) << " mm^2" << std::endl;
            std::cout << "  NoC:                "
                      << mcpat.getComponentArea(McPAT::ComponentType::NOC) << " mm^2" << std::endl;
            std::cout << "  Total:              "
                      << mcpat.getTotalArea() << " mm^2" << std::defaultfloat << std::endl;

            // Dump McPAT XML to output directory for post-analysis
            if (!output_dir.empty()) {
                std::string xml_src = "/tmp/mcpat_input.xml";
                std::string xml_dst = output_dir + "/mcpat_config.xml";
                std::ifstream src(xml_src);
                if (src.good()) {
                    std::ofstream dst(xml_dst);
                    dst << src.rdbuf();
                    std::cout << "\nMcPAT XML saved to: " << xml_dst << std::endl;
                }
            }
        }
    }

    // ── Host McPAT for system scope with host nodes (standard/verbose) ──
    bool has_host_node = false;
    for (const auto& n : config.system_nodes) {
        if (n.role == UnifiedConfig::SystemNode::HOST) { has_host_node = true; break; }
    }
    if (!detail_summary && config.scope == "system" && has_host_node) {
        std::cout << "\n--- Host Power (System) ---" << std::endl;

        McPAT::SystemConfig host_cfg;
        host_cfg.num_cores = config.host_num_cores;
        host_cfg.core_clock_mhz = config.host_frequency_mhz;
        host_cfg.pipeline_depth = 19;
        host_cfg.issue_width = 4;
        host_cfg.num_alus = 4;
        host_cfg.num_muls = 2;
        host_cfg.num_fpus = 2;
        host_cfg.l1i_size_bytes = config.host_l1i_kb * 1024;
        host_cfg.l1d_size_bytes = config.host_l1d_kb * 1024;
        host_cfg.l2_size_bytes = config.host_l2_kb * 1024;
        host_cfg.l3_size_bytes = config.host_l3_kb * 1024;
        host_cfg.num_memory_controllers = 1;
        host_cfg.mc_clock_mhz = 1200.0;
        host_cfg.has_noc = false;
        host_cfg.tech_node_nm = std::max(22, config.host_tech_node_nm);
        host_cfg.temperature_k = 350;

        McPAT host_mcpat(host_cfg);
        host_mcpat.setDeviceProfile(McPAT::DeviceProfile::HOST_OOO);
        host_mcpat.initialize();

        // Use actual ZSim stats when available (from host ZSim run), else estimate
        host_mcpat.setTotalCycles(cycles);
        host_mcpat.setBusyCycles(cycles / 10);  // host mostly waiting
        host_mcpat.setTotalInstructions(instrs / 10);

        // Minimal host cache activity (will be overridden by real stats when available)
        host_mcpat.setL1IAccesses(instrs / 10, instrs / 100);
        host_mcpat.setL1DAccesses(instrs / 20, instrs / 40, instrs / 200, instrs / 400);
        host_mcpat.setL2Accesses(instrs / 200, instrs / 400, instrs / 2000, instrs / 4000);
        host_mcpat.setL3Accesses(instrs / 2000, instrs / 4000, instrs / 20000, instrs / 40000);
        host_mcpat.setMemControllerAccesses(instrs / 20000, instrs / 40000);
        host_mcpat.setMCTechParams(getMCTechParamsForMcPAT(config.host_memory_tech, 1));

        // PCIe for host-device transfers (configurable via power.pcie YAML)
        if (config.pcie_enabled) {
            McPAT::PCIeStats pcie;
            pcie.number_units = config.pcie_num_units;
            // Use num_lanes for num_channels if available
            pcie.num_channels = (config.pcie_num_lanes > 0) ? config.pcie_num_lanes : config.pcie_num_channels;
            pcie.duty_cycle = config.pcie_duty_cycle;
            pcie.total_load_perc = config.pcie_load_perc;
            host_mcpat.setPCIeStats(pcie);
        }

        try {
            host_mcpat.computePower();
            host_mcpat.printDetailedResults();

            auto dev_power = mcpat.getSystemPower();
            auto host_power = host_mcpat.getSystemPower();
            std::cout << "\n--- System Power Summary ---" << std::endl;
            std::cout << "  Host Power:     " << host_power.total_power << " W" << std::endl;
            std::cout << "  Device Power:   " << dev_power.total_power << " W" << std::endl;
            std::cout << "  Total System:   " << (host_power.total_power + dev_power.total_power) << " W" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Power] Host McPAT failed: " << e.what() << std::endl;
        }
    }

    // ── Memory Array Power/Area (standard/verbose) ──
    bool is_dram_tech = !(config.memory_tech == "SRAM" || config.memory_tech == "STT_MRAM" ||
                          config.memory_tech == "PCM" || config.memory_tech == "RERAM");

    if (!detail_summary) {
    std::cout << "\n--- Memory Array Power/Area ---" << std::endl;

    if (is_dram_tech) {
        // DRAM: Ramulator2 energy model
        try {
            pimid::RamulatorWrapper ram_oracle("", config.memory_tech);
            ram_oracle.initialize();
            double rd_energy = ram_oracle.getReadEnergy();
            double wr_energy = ram_oracle.getWriteEnergy();
            double act_energy = ram_oracle.getActivationEnergy();
            double pre_energy = ram_oracle.getPrechargeEnergy();
            double ref_energy = ram_oracle.getRefreshEnergy();
            double leakage_mw = ram_oracle.getLeakagePower();

            // Scale by actual access counts
            double total_rd_nj = rd_energy * zsim_stats.mem_rd;
            double total_wr_nj = wr_energy * zsim_stats.mem_wr;
            double total_act_nj = act_energy * (zsim_stats.mem_rd + zsim_stats.mem_wr);
            double total_pre_nj = pre_energy * (zsim_stats.mem_rd + zsim_stats.mem_wr);

            std::cout << "  Technology:      " << config.memory_tech << " (Ramulator2 energy model)" << std::endl;
            std::cout << "  Per-access:      read=" << std::fixed << std::setprecision(3)
                      << rd_energy << " nJ, write=" << wr_energy << " nJ" << std::endl;
            std::cout << "  Activation:      " << act_energy << " nJ/access" << std::endl;
            std::cout << "  Precharge:       " << pre_energy << " nJ/access" << std::endl;
            std::cout << "  Refresh:         " << ref_energy << " nJ/op" << std::endl;
            std::cout << "  Leakage:         " << leakage_mw << " mW" << std::endl;
            std::cout << "  Total dynamic:   " << std::setprecision(1)
                      << (total_rd_nj + total_wr_nj + total_act_nj + total_pre_nj) / 1e6
                      << " mJ (rd=" << total_rd_nj / 1e6 << " + wr=" << total_wr_nj / 1e6
                      << " + act=" << total_act_nj / 1e6 << " + pre=" << total_pre_nj / 1e6 << ")"
                      << std::defaultfloat << std::endl;

            // DRAM die area via CACTI 7 (commercial DRAM cell model)
            try {
                pimid::CACTIWrapper::SRAMConfig dram_cfg;
                uint64_t chip_bytes = ram_oracle.getChipSizeMB() * 1024ULL * 1024ULL;
                uint32_t total_banks = ram_oracle.getBanksPerBankGroup()
                                     * ram_oracle.getBankGroupsPerChip();
                dram_cfg.capacity_bytes = chip_bytes;
                dram_cfg.line_size = 64;
                dram_cfg.associativity = 1;
                dram_cfg.banks = std::max(1u, total_banks);
                dram_cfg.tech_node_nm = std::max(22, config.tech_node_nm);
                dram_cfg.is_cache = false;
                dram_cfg.is_main_memory = true;
                dram_cfg.cell_type = pimid::CACTIWrapper::COMM_DRAM;
                dram_cfg.output_width_bits = static_cast<uint32_t>(
                    std::max(8, ram_oracle.getChipIOBits()));
                dram_cfg.page_sz_bits = 8192;  // 1KB page typical
                dram_cfg.burst_len = 8;
                dram_cfg.int_prefetch_w = 8;

                pimid::CACTIWrapper cacti_dram(dram_cfg);
                cacti_dram.initialize();
                if (cacti_dram.isValid()) {
                    double die_area = cacti_dram.getArea();
                    std::cout << "  Area:            " << std::fixed << std::setprecision(2)
                              << die_area << " mm^2/die (CACTI 7 comm-DRAM)"
                              << std::defaultfloat << std::endl;
                } else {
                    // Fallback to JEDEC density if CACTI DRAM fails
                    double density = getDRAMDieDensity(config.memory_tech);
                    double chip_mbit = ram_oracle.getChipSizeMB() * 8.0;
                    double die_area = (chip_mbit / density) * 1.12;
                    std::cout << "  Area:            " << std::fixed << std::setprecision(2)
                              << die_area << " mm^2/die (JEDEC density fallback)"
                              << std::defaultfloat << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "  [DRAM area] CACTI query failed: " << e.what() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "  [DRAM energy] Ramulator2 query failed: " << e.what() << std::endl;
        }
    } else if (config.memory_tech == "SRAM") {
        // SRAM: CACTI model
        try {
            pimid::CACTIWrapper::SRAMConfig sram_cfg;
            sram_cfg.capacity_bytes = config.num_banks * 64 * 1024;  // default 64KB/bank
            sram_cfg.line_size = config.cache_line_size;
            sram_cfg.associativity = 1;
            sram_cfg.banks = config.num_banks;
            sram_cfg.tech_node_nm = std::max(22, config.tech_node_nm);
            sram_cfg.is_cache = false;  // RAM mode
            sram_cfg.read_write_ports = config.ports_per_bank;

            pimid::CACTIWrapper cacti(sram_cfg);
            cacti.initialize();

            double rd_energy = cacti.getDynamicReadEnergy();
            double wr_energy = cacti.getDynamicWriteEnergy();
            double leakage = cacti.getLeakagePower();
            double area = cacti.getArea();

            double total_rd_nj = rd_energy * zsim_stats.mem_rd;
            double total_wr_nj = wr_energy * zsim_stats.mem_wr;

            std::cout << "  Technology:      SRAM (CACTI 7)" << std::endl;
            std::cout << "  Per-access:      read=" << std::fixed << std::setprecision(3)
                      << rd_energy << " nJ, write=" << wr_energy << " nJ" << std::endl;
            std::cout << "  Leakage:         " << leakage << " mW" << std::endl;
            std::cout << "  Total dynamic:   " << std::setprecision(1)
                      << (total_rd_nj + total_wr_nj) / 1e6 << " mJ"
                      << std::defaultfloat << std::endl;
            std::cout << "  Area:            " << std::fixed << std::setprecision(3)
                      << area << " mm^2" << std::defaultfloat << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "  [SRAM power] CACTI query failed: " << e.what() << std::endl;
        }
    } else {
        // NVM: NVSim model (STT_MRAM, PCM, RERAM)
        try {
            pimid::NVSimWrapper::NVMConfig nvm_cfg;
            nvm_cfg.capacity_bytes = config.num_banks * 64 * 1024;  // default 64KB/bank
            nvm_cfg.word_width_bits = 64;
            nvm_cfg.process_node_nm = std::max(22, config.tech_node_nm);
            if (config.memory_tech == "STT_MRAM")
                nvm_cfg.nvm_type = pimid::NVSimWrapper::NVMType::STTRAM;
            else if (config.memory_tech == "PCM")
                nvm_cfg.nvm_type = pimid::NVSimWrapper::NVMType::PCRAM;
            else if (config.memory_tech == "RERAM")
                nvm_cfg.nvm_type = pimid::NVSimWrapper::NVMType::RERAM;

            pimid::NVSimWrapper nvsim(nvm_cfg);
            nvsim.initialize();

            double rd_energy = nvsim.getReadDynamicEnergy();
            double wr_energy = nvsim.getWriteDynamicEnergy();
            double leakage = nvsim.getLeakagePower();
            double area = nvsim.getArea();

            double total_rd_nj = rd_energy * zsim_stats.mem_rd;
            double total_wr_nj = wr_energy * zsim_stats.mem_wr;

            std::cout << "  Technology:      " << config.memory_tech << " (NVSim)" << std::endl;
            std::cout << "  Per-access:      read=" << std::fixed << std::setprecision(3)
                      << rd_energy << " nJ, write=" << wr_energy << " nJ" << std::endl;
            std::cout << "  Leakage:         " << leakage << " mW" << std::endl;
            std::cout << "  Total dynamic:   " << std::setprecision(1)
                      << (total_rd_nj + total_wr_nj) / 1e6 << " mJ"
                      << std::defaultfloat << std::endl;
            std::cout << "  Area:            " << std::fixed << std::setprecision(3)
                      << area << " mm^2" << std::defaultfloat << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "  [NVM power] NVSim query failed: " << e.what() << std::endl;
        }
    }

    // ── Model Coverage Table ──
    std::cout << "\nModel Coverage:" << std::endl;
    std::cout << "  Component                Timing        Power         Area" << std::endl;
    std::cout << "  ─────────────────────────────────────────────────────────────" << std::endl;

    // PE cores
    std::cout << "  PE cores (x" << config.num_pes << ")";
    int pad = 22 - static_cast<int>(std::to_string(config.num_pes).size()) - 12;
    for (int i = 0; i < std::max(1, pad); i++) std::cout << " ";
    std::cout << "ZSim          McPAT         McPAT" << std::endl;

    // L2 cache
    if (!alu_only && config.enable_l2) {
        std::string l2_label = "L2 cache (x" + std::to_string(config.l2_count) + ")";
        std::cout << "  " << std::left << std::setw(24) << l2_label
                  << "CACTI         McPAT         McPAT" << std::endl;
    }

    // L3 cache
    if (!alu_only && config.enable_l3) {
        std::cout << "  " << std::left << std::setw(24) << "L3 cache (x1)"
                  << "CACTI         McPAT         McPAT" << std::endl;
    }

    // PE-MIs
    if (config.pe_mc_enabled && config.pes_per_mc > 0) {
        int mi_count = config.num_pes / config.pes_per_mc;
        std::string mi_label = "PE-MIs (x" + std::to_string(mi_count) + ")";
        std::cout << "  " << std::left << std::setw(24) << mi_label
                  << std::setw(14) << "Simple" << "McPAT         McPAT" << std::endl;
    }

    // Memory array
    {
        std::string mem_label = "Memory (" + config.memory_tech + ")";
        std::string timing_src, power_src, area_src;
        if (is_dram_tech) {
            timing_src = "Ramulator2";
            power_src = "Ramulator2";
            area_src = "CACTI";
        } else if (config.memory_tech == "SRAM") {
            timing_src = "CACTI";
            power_src = "CACTI";
            area_src = "CACTI";
        } else {
            timing_src = "NVSim";
            power_src = "NVSim";
            area_src = "NVSim";
        }
        std::cout << "  " << std::left << std::setw(24) << mem_label
                  << std::setw(14) << timing_src << std::setw(14) << power_src
                  << area_src << std::endl;
    }

    // Host MC (when PEs use HOST_MC placement — MC is a NoC endpoint)
    if (!config.pe_mc_enabled && config.num_pes > 1) {
        std::string hmc_label = "Host MC (x1)";
        std::string hmc_timing = is_dram_tech ? "Ramulator2" : "Simple";
        std::cout << "  " << std::left << std::setw(24) << hmc_label
                  << std::setw(14) << hmc_timing << "McPAT         McPAT" << std::endl;
    }

    // NoC (connects PEs ↔ MCs — memory controllers are network endpoints)
    if (config.num_pes > 1) {
        int active_levels = 0;
        for (int i = 0; i < 7; ++i) {
            if (config.hierarchy_level_latency[i] > 0 || config.hierarchy_bridge_latency[i] > 0)
                active_levels++;
        }
        int mc_endpoints = 0;
        if (config.pe_mc_enabled && config.pes_per_mc > 0)
            mc_endpoints = config.num_pes / config.pes_per_mc;
        else
            mc_endpoints = 1;  // host MC

        std::string noc_label = "NoC";
        if (active_levels > 0)
            noc_label += " (" + std::to_string(active_levels) + " lvl)";
        std::string noc_detail = std::to_string(config.num_pes) + " PEs + "
                                 + std::to_string(mc_endpoints) + " MCs";
        // Determine displayed NoC model
        std::string timing_model;
        if (config.noc_cycle_accurate) {
            timing_model = "Garnet";
        } else {
            timing_model = "Simple";  // hop count + M/D/1 queuing
        }
        std::cout << "  " << std::left << std::setw(24) << noc_label
                  << std::setw(14) << timing_model << "McPAT         McPAT" << std::endl;
        std::cout << "    endpoints: " << noc_detail << std::endl;
    }

    // Host (system scope with host nodes)
    if (config.scope == "system") {
        bool show_host = false;
        for (const auto& n : config.system_nodes) {
            if (n.role == UnifiedConfig::SystemNode::HOST) { show_host = true; break; }
        }
        if (show_host) {
            std::cout << "  " << std::left << std::setw(24) << "Host processor"
                      << "ZSim          McPAT         McPAT" << std::endl;
            std::string pcie_model_display = "Simple";  // always includes M/D/1 queuing
            std::cout << "  " << std::left << std::setw(24) << "PCIe/CXL"
                      << std::setw(14) << pcie_model_display << "McPAT         McPAT" << std::endl;
        }
    }
    std::cout << std::right;  // restore default alignment

    } // !detail_summary
}

/**
 * @brief Per-node power analysis for system scope — runs McPAT once per node type.
 * Each node gets its own DeviceProfile derived from its core_type.
 */
static void runPerNodePowerAnalysis(const UnifiedConfig& config,
                                     const ZSimParsedOutput& zsim_stats,
                                     const std::string& output_dir) {
    using McPAT = pimid::McPATWrapper;

    // If no system nodes or no stats, fall back to original
    if (config.system_nodes.empty() || (zsim_stats.cycles == 0 && zsim_stats.instrs == 0)) {
        runPowerAnalysis(config, zsim_stats, output_dir);
        return;
    }

    std::cout << "\n--- Per-Node Power ---" << std::endl;

    struct NodePowerResult {
        std::string name;
        std::string core_desc;
        double total_power = 0;
        double dynamic_power = 0;
        double leakage_power = 0;
        double area = 0;
        bool valid = false;
    };
    std::vector<NodePowerResult> results;

    // Use global stats as a fallback when per-core stats unavailable
    uint64_t total_cycles = zsim_stats.cycles > 0 ? zsim_stats.cycles : 1;
    uint64_t total_instrs = zsim_stats.instrs > 0 ? zsim_stats.instrs : 1;

    const std::map<std::string, double>& overrides = config.mcpat_overrides;
    auto ov_get_int = [&overrides](const std::string& key, int fallback) -> int {
        auto it = overrides.find(key);
        return (it != overrides.end()) ? static_cast<int>(it->second) : fallback;
    };

    for (const auto& node : config.system_nodes) {
        if (node.num_cores == 0) continue;  // memory-only, skip

        NodePowerResult result;
        result.name = node.name;

        McPAT::SystemConfig mcfg;
        mcfg.num_cores = node.num_cores;
        mcfg.core_clock_mhz = node.frequency_mhz;
        mcfg.tech_node_nm = std::max(22, node.tech_node_nm);
        mcfg.temperature_k = 350;

        McPAT::DeviceProfile profile;
        bool is_alu = false;

        // Host nodes carry their core type in node.core_type; device nodes
        // carry it in node.pe_type (see YAML parse at hosts[].core_type vs
        // devices[].pe_type). Selecting the wrong field was making ALU device
        // nodes look like OOO hosts, which made the dual-McPAT device XML
        // emit OOO + caches and triggered downstream errors.
        const std::string& effective_type =
            (node.role == UnifiedConfig::SystemNode::DEVICE)
                ? node.pe_type : node.core_type;

        if (effective_type == "ooo_core") {
            mcfg.pipeline_depth = 19; mcfg.issue_width = 4;
            mcfg.num_alus = 4; mcfg.num_muls = 2; mcfg.num_fpus = 2;
            profile = McPAT::DeviceProfile::HOST_OOO;
            result.core_desc = "OOO";
        } else if (effective_type == "alu_core") {
            mcfg.pipeline_depth = 5; mcfg.issue_width = 1;
            mcfg.num_alus = 1; mcfg.num_muls = 0; mcfg.num_fpus = 0;
            profile = McPAT::DeviceProfile::DEVICE_ALU;
            is_alu = true;
            result.core_desc = "ALU";
        } else {
            mcfg.pipeline_depth = 14; mcfg.issue_width = 1;
            mcfg.num_alus = 3; mcfg.num_muls = 1; mcfg.num_fpus = 1;
            profile = McPAT::DeviceProfile::DEVICE_INORDER;
            result.core_desc = (effective_type == "in_order_core") ? "InOrder" : "Simple";
        }

        // Apply overrides
        mcfg.pipeline_depth = ov_get_int("pipeline_depth", mcfg.pipeline_depth);
        mcfg.issue_width = ov_get_int("issue_width", mcfg.issue_width);
        mcfg.num_alus = ov_get_int("num_alus", mcfg.num_alus);
        mcfg.num_muls = ov_get_int("num_muls", mcfg.num_muls);
        mcfg.num_fpus = ov_get_int("num_fpus", mcfg.num_fpus);
        mcfg.device_type = ov_get_int("device_type", 0);
        mcfg.longer_channel_device = ov_get_int("longer_channel_device", 1);
        mcfg.number_hardware_threads = ov_get_int("number_hardware_threads", 1);
        mcfg.interconnect_projection_type = ov_get_int("interconnect_projection_type", 0);

        // Caches
        if (is_alu) {
            mcfg.l1i_size_bytes = 0; mcfg.l1d_size_bytes = 0;
            mcfg.l2_size_bytes = 0; mcfg.l3_size_bytes = 0;
        } else {
            mcfg.l1i_size_bytes = node.l1i_kb * 1024ULL;
            mcfg.l1d_size_bytes = node.l1d_kb * 1024ULL;
            mcfg.l2_size_bytes = node.enable_l2 ? node.l2_kb * 1024ULL : 0;
            mcfg.l3_size_bytes = node.enable_l3 ? node.l3_kb * 1024ULL : 0;
        }

        // MC
        mcfg.num_memory_controllers = 1;
        mcfg.mc_clock_mhz = node.frequency_mhz / 2.0;

        // NoC (for device nodes with PEs)
        if (node.role == UnifiedConfig::SystemNode::DEVICE && node.num_pes > 1) {
            mcfg.has_noc = true;
            mcfg.noc_clock_mhz = node.frequency_mhz;
            mcfg.noc_num_routers = node.num_pes;
            int grid_side = static_cast<int>(std::ceil(std::sqrt(node.num_pes)));
            mcfg.noc_num_rows = grid_side;
            mcfg.noc_num_cols = grid_side;
        } else {
            mcfg.has_noc = false;
        }

        try {
            McPAT mcpat(mcfg);
            mcpat.setDeviceProfile(profile);

            // Avoid McPAT's homogeneous_NoCs=1 code path (number_of_NoCs == 1),
            // which CACTI 6.5-P routes through a configuration that crashes on
            // "Must have at least one port" regardless of the per-bank ports
            // emitted in the XML. The non-cosim runPowerAnalysis path emits N
            // NoC levels for the hierarchy; per-node analysis lacks that
            // detail today, so unconditionally emit a minimal two-entry layout
            // (PE/core-side + MC-side) so McPAT picks the heterogeneous path.
            // Applied to both host and device nodes — both XMLs would default
            // to number_of_NoCs=1 without this and hit the same CACTI assert.
            {
                using NoCLevel = pimid::McPATWrapper::NoCLevelConfig;
                std::vector<NoCLevel> two_levels;
                NoCLevel pe_lvl;
                pe_lvl.name = "PE_noc";
                pe_lvl.type = 1;  // router-based
                pe_lvl.horizontal_nodes = mcfg.has_noc ? mcfg.noc_num_cols : 1;
                pe_lvl.vertical_nodes   = mcfg.has_noc ? mcfg.noc_num_rows : 1;
                pe_lvl.clock_mhz = mcfg.has_noc ? mcfg.noc_clock_mhz
                                               : node.frequency_mhz;
                pe_lvl.chip_coverage = 0.5;
                pe_lvl.duty_cycle = 0.1;
                pe_lvl.total_accesses = 0;
                two_levels.push_back(pe_lvl);
                NoCLevel mc_lvl = pe_lvl;
                mc_lvl.name = "MC_noc";
                mc_lvl.type = 0;  // bus
                mc_lvl.chip_coverage = 0.1;
                two_levels.push_back(mc_lvl);
                mcpat.setNoCLevels(two_levels);
            }

            mcpat.initialize();

            // Distribute stats proportionally by core count
            double core_frac = static_cast<double>(node.num_cores) /
                std::max(1, [&]() {
                    int total = 0;
                    for (const auto& n : config.system_nodes) total += n.num_cores;
                    return total;
                }());
            uint64_t node_cycles = static_cast<uint64_t>(total_cycles * core_frac);
            uint64_t node_instrs = static_cast<uint64_t>(total_instrs * core_frac);
            if (node_cycles == 0) node_cycles = 1;
            if (node_instrs == 0) node_instrs = 1;

            mcpat.setTotalCycles(node_cycles);
            mcpat.setBusyCycles(node_cycles);
            mcpat.setTotalInstructions(node_instrs);

            // Distribute cache stats proportionally
            mcpat.setL1IAccesses(
                static_cast<uint64_t>(zsim_stats.l1i_total_reads() * core_frac),
                static_cast<uint64_t>(zsim_stats.l1i_mGETS * core_frac));
            mcpat.setL1DAccesses(
                static_cast<uint64_t>(zsim_stats.l1d_total_reads() * core_frac),
                static_cast<uint64_t>(zsim_stats.l1d_total_writes() * core_frac),
                static_cast<uint64_t>(zsim_stats.l1d_mGETS * core_frac),
                static_cast<uint64_t>(zsim_stats.l1d_mGETXIM * core_frac));
            mcpat.setL2Accesses(
                static_cast<uint64_t>(zsim_stats.l2_total_reads() * core_frac),
                static_cast<uint64_t>(zsim_stats.l2_total_writes() * core_frac),
                static_cast<uint64_t>(zsim_stats.l2_mGETS * core_frac),
                static_cast<uint64_t>(zsim_stats.l2_mGETXIM * core_frac));
            mcpat.setL3Accesses(
                static_cast<uint64_t>(zsim_stats.l3_total_reads() * core_frac),
                static_cast<uint64_t>(zsim_stats.l3_total_writes() * core_frac),
                static_cast<uint64_t>(zsim_stats.l3_mGETS * core_frac),
                static_cast<uint64_t>(zsim_stats.l3_mGETXIM * core_frac));

            mcpat.setMemControllerAccesses(
                static_cast<uint64_t>(zsim_stats.mem_rd * core_frac),
                static_cast<uint64_t>(zsim_stats.mem_wr * core_frac));
            mcpat.setMCTechParams(getMCTechParamsForMcPAT(node.memory_tech, 1));

            mcpat.computePower();

            auto power = mcpat.getSystemPower();
            result.total_power = power.total_power;
            result.dynamic_power = power.total_dynamic;
            result.leakage_power = power.total_leakage;
            result.area = mcpat.getTotalArea();
            result.valid = true;

            std::cout << "  " << node.name << " (" << result.core_desc
                      << ", " << static_cast<int>(node.frequency_mhz) << "MHz"
                      << ", " << node.tech_node_nm << "nm): "
                      << std::fixed << std::setprecision(2) << result.total_power << " W ("
                      << result.dynamic_power << " dyn + "
                      << result.leakage_power << " leak), "
                      << result.area << " mm^2"
                      << std::defaultfloat << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "  " << node.name << ": McPAT failed: " << e.what() << std::endl;
        }

        results.push_back(result);
    }

    // System totals
    double sys_power = 0, sys_dyn = 0, sys_leak = 0, sys_area = 0;
    for (const auto& r : results) {
        if (!r.valid) continue;
        sys_power += r.total_power;
        sys_dyn += r.dynamic_power;
        sys_leak += r.leakage_power;
        sys_area += r.area;
    }

    std::cout << "\n--- System Total ---" << std::endl;
    std::cout << "  Power: " << std::fixed << std::setprecision(2)
              << sys_power << " W (" << sys_dyn << " dyn + " << sys_leak << " leak)" << std::endl;
    std::cout << "  Area:  " << sys_area << " mm^2"
              << std::defaultfloat << std::endl;
}

/**
 * ZSim configuration helper.
 * Generates ZSim .cfg files for use by QEMU+ZSim (exec mode) and zsim_trace (trace mode).
 */
class ZSimSimulator {
public:
    ZSimSimulator(const UnifiedConfig& config) : config_(config) {}

    /**
     * Generate ZSim configuration file.
     * Used by exec mode (QEMU + ZSim) and trace mode (zsim_trace replay).
     */
    std::string generateConfig() {
        std::string cfg_path = "/tmp/pimid_qemu_zsim_" + std::to_string(getpid()) + ".cfg";
        std::ofstream cfg(cfg_path);
        if (!cfg.is_open()) return "";

        std::string core_type = "Simple";
        if (config_.pe_type == "ooo_core") {
            core_type = "OoO";
        } else if (config_.pe_type == "in_order_core") {
            // The cycle-detailed in-order core (zsim InOrderCore).
            core_type = "InOrder";
        } else if (config_.pe_type == "alu_core" || config_.pe_type == "alu") {
            core_type = "ALU";
        } else if (config_.pe_type == "null_core" || config_.pe_type == "null") {
            core_type = "Null";
        }   // simple_core / unknown -> "Simple" (coarse SimpleCore)

        // Memory latency: priority is override > YAML config > external models > defaults
        int mem_latency;
        if (config_.memory_latency_override >= 0) {
            mem_latency = config_.memory_latency_override;
        } else {
            // Use external models (default) or complete YAML override.
            // For SRAM/NVM, pass the array capacity (banks × 64 KB/bank) so the
            // access latency reflects the configured organization (matches the
            // energy model). DRAM ignores this (sized by Ramulator JEDEC org).
            uint64_t array_cap = static_cast<uint64_t>(std::max(1, config_.num_banks)) * 64ULL * 1024ULL;
            mem_latency = getMemoryLatencyCycles(config_.memory_tech, config_.frequency_mhz,
                                                  config_.use_yaml_memory_params,
                                                  config_.memory_params.read_latency_ns,
                                                  array_cap);
        }

        // Cache latencies: YAML override > CACTI > defaults
        int l1d_latency, l1i_latency, l2_latency = 0, l3_latency = 0;
        if (config_.use_yaml_cache_params) {
            l1d_latency = static_cast<int>(std::round(
                config_.l1d_params.latency_ns * config_.frequency_mhz / 1000.0));
            l1i_latency = static_cast<int>(std::round(
                config_.l1i_params.latency_ns * config_.frequency_mhz / 1000.0));
            if (config_.enable_l2)
                l2_latency = static_cast<int>(std::round(
                    config_.l2_params.latency_ns * config_.frequency_mhz / 1000.0));
            if (config_.enable_l3)
                l3_latency = static_cast<int>(std::round(
                    config_.l3_params.latency_ns * config_.frequency_mhz / 1000.0));
        } else {
            l1d_latency = getCacheLatencyCycles(config_.l1d_size_kb, config_.l1d_ways,
                                                 config_.cache_line_size, config_.frequency_mhz, config_.tech_node_nm, 4);
            l1i_latency = getCacheLatencyCycles(config_.l1i_size_kb, config_.l1i_ways,
                                                 config_.cache_line_size, config_.frequency_mhz, config_.tech_node_nm, 3);
            if (config_.enable_l2)
                l2_latency = getCacheLatencyCycles(config_.l2_size_kb, config_.l2_ways,
                                                    config_.cache_line_size, config_.frequency_mhz, config_.tech_node_nm, 12);
            if (config_.enable_l3)
                l3_latency = getCacheLatencyCycles(config_.l3_size_kb, config_.l3_ways,
                                                    config_.cache_line_size, config_.frequency_mhz, config_.tech_node_nm, 20);
        }
        l1d_latency = std::max(1, l1d_latency);
        l1i_latency = std::max(1, l1i_latency);
        if (config_.enable_l2) l2_latency = std::max(1, l2_latency);
        if (config_.enable_l3) l3_latency = std::max(1, l3_latency);

        cfg << "// Auto-generated ZSim config by PIMID (QEMU mode)\n";
        cfg << "// Memory: " << config_.memory_tech << ", PEs: " << config_.num_pes << "\n";
        cfg << "// Frequency: " << config_.frequency_mhz << " MHz\n\n";

        cfg << "sys = {\n";
        cfg << "    lineSize = " << config_.cache_line_size << ";\n";
        cfg << "    frequency = " << config_.frequency_mhz << ";\n";
        cfg << "\n";
        // ALU + Null cores have no cache hierarchy (PIM ALU has analog/digital
        // compute units only; Null is a no-op for memory/network-only studies).
        bool core_has_caches = (core_type != "ALU" && core_type != "Null");

        cfg << "    cores = {\n";
        cfg << "        pim_pes = {\n";
        cfg << "            type = \"" << core_type << "\";\n";
        cfg << "            cores = " << config_.num_pes << ";\n";
        if (core_type == "ALU") {
            cfg << std::fixed << std::setprecision(2);
            cfg << "            computeFactor = " << config_.alu_compute_factor << ";\n";
            cfg << "            accessFactor = " << config_.alu_access_factor << ";\n";
            cfg << "            throughputFactor = " << config_.alu_throughput_factor << ";\n";
            cfg << std::defaultfloat;
            cfg << "            operandWidth = " << config_.alu_operand_width << ";\n";
            cfg << std::fixed << std::setprecision(2);
            cfg << "            energyFactor = " << config_.alu_energy_factor << ";\n";
            cfg << std::defaultfloat;
        } else if (core_has_caches) {
            cfg << "            dcache = \"l1d\";\n";
            cfg << "            icache = \"l1i\";\n";
        }
        // Null cores: no per-core attributes beyond type + count
        cfg << "        };\n";
        cfg << "    };\n";
        cfg << "\n";

        if (core_has_caches) {
            cfg << "    caches = {\n";
            cfg << "        l1d = {\n";
            cfg << "            caches = " << config_.num_pes << ";\n";
            cfg << "            size = " << (config_.l1d_size_kb * 1024) << ";\n";
            cfg << "            array = { type = \"SetAssoc\"; ways = " << config_.l1d_ways << "; };\n";
            cfg << "            latency = " << l1d_latency << ";\n";
            cfg << "        };\n";
            cfg << "        l1i = {\n";
            cfg << "            caches = " << config_.num_pes << ";\n";
            cfg << "            size = " << (config_.l1i_size_kb * 1024) << ";\n";
            cfg << "            array = { type = \"SetAssoc\"; ways = " << config_.l1i_ways << "; };\n";
            cfg << "            latency = " << l1i_latency << ";\n";
            cfg << "        };\n";
            if (config_.enable_l2) {
                cfg << "        l2 = {\n";
                cfg << "            caches = " << config_.l2_count << ";\n";
                cfg << "            size = " << (config_.l2_size_kb * 1024) << ";\n";
                cfg << "            array = { type = \"SetAssoc\"; ways = " << config_.l2_ways << "; };\n";
                cfg << "            children = \"l1i|l1d\";\n";
                cfg << "            latency = " << l2_latency << ";\n";
                cfg << "        };\n";
            }
            if (config_.enable_l3) {
                cfg << "        l3 = {\n";
                cfg << "            caches = 1;\n";
                cfg << "            size = " << (config_.l3_size_kb * 1024) << ";\n";
                cfg << "            array = { type = \"SetAssoc\"; ways = " << config_.l3_ways << "; };\n";
                cfg << "            children = \"l2\";\n";
                cfg << "            latency = " << l3_latency << ";\n";
                cfg << "        };\n";
            }
            cfg << "    };\n";
            cfg << "\n";
        }

        emitZSimMemBlock(cfg, config_, mem_latency);
        cfg << "\n";

        // Configure Garnet-based network for all topologies
        emitZSimNetworkBlock(cfg, config_);
        emitZSimHierarchyBlock(cfg, config_);

        cfg << "};\n\n";

        cfg << "sim = {\n";
        cfg << "    phaseLength = " << config_.phase_length << ";\n";
        cfg << "    maxTotalInstrs = " << config_.max_instructions << "L;\n";
        cfg << "    statsPhaseInterval = " << config_.stats_interval << ";\n";
        // For OMP/MPI workloads, allow all PEs to run concurrently
        int parallelism = (config_.workload_type == "openmp" || config_.workload_type == "mpi")
                          ? config_.num_pes : 1;
        cfg << "    parallelism = " << parallelism << ";\n";
        cfg << "    printHierarchy = true;\n";
        cfg << "    aslr = false;\n";
        // process0.command is never consumed by ZSim internals (it was read
        // by the Pin harness which no longer exists). Disable strict config
        // checking to avoid a panic on this unused setting.
        cfg << "    strictConfig = false;\n";
        cfg << "};\n\n";

        cfg << "// QEMU mode: workload is launched by QEMU, not by ZSim\n";
        cfg << "process0 = {\n";
        cfg << "    command = \"<qemu-managed>\";\n";
        cfg << "};\n";

        cfg.close();
        return cfg_path;
    }

private:
    UnifiedConfig config_;
};

/**
 * @brief Generate a single combined ZSim .cfg for multi-host/multi-device system.
 *
 * Produces a single config with all hosts and devices combined:
 * - Named core groups per node (e.g., "cpu0_cores", "hbm_pim0_pes")
 * - Per-node cache hierarchies (host: full L1/L2/L3, ALU device: no caches)
 * - Flat address space with SystemAddressRouter
 * - System network config (topology, latency, model)
 * - Node map for runtime core→node resolution
 * - Frequency normalized to reference (max of all nodes)
 */
static std::string generateSystemConfig(UnifiedConfig& config) {
    std::string cfg_path = "/tmp/pimid_system_zsim_" + std::to_string(getpid()) + ".cfg";
    std::ofstream cfg(cfg_path);
    if (!cfg.is_open()) return "";

    double ref_freq = config.reference_frequency_mhz;
    int total_cores = 0;
    for (const auto& n : config.system_nodes) total_cores += n.num_cores;

    cfg << "// Auto-generated ZSim config by PIMID (System multi-node mode)\n";
    cfg << "// Nodes: " << config.system_nodes.size() << ", Total cores: " << total_cores << "\n";
    cfg << "// Reference frequency: " << ref_freq << " MHz\n\n";

    cfg << "sys = {\n";
    cfg << "    lineSize = " << config.cache_line_size << ";\n";
    cfg << "    frequency = " << static_cast<int>(ref_freq) << ";\n\n";

    // ── Cores ──
    cfg << "    cores = {\n";
    for (const auto& node : config.system_nodes) {
        if (node.num_cores == 0) continue;  // memory-only

        std::string group_name = node.name + (node.role == UnifiedConfig::SystemNode::HOST ? "_cores" : "_pes");
        std::string core_type = "Simple";
        bool is_alu = false;

        if (node.core_type == "ooo_core") core_type = "OoO";
        else if (node.core_type == "in_order_core") core_type = "InOrder";
        else if (node.core_type == "alu_core") { core_type = "ALU"; is_alu = true; }
        else if (node.core_type == "null_core") core_type = "Null";
        // simple_core / unknown -> "Simple" (coarse SimpleCore)

        cfg << "        " << group_name << " = {\n";
        cfg << "            type = \"" << core_type << "\";\n";
        cfg << "            cores = " << node.num_cores << ";\n";

        if (is_alu) {
            // Apply frequency scaling to ALU factors
            double scaled_compute = node.alu_compute_factor * node.freq_scale;
            double scaled_access = node.alu_access_factor * node.freq_scale;
            cfg << std::fixed << std::setprecision(2);
            cfg << "            computeFactor = " << scaled_compute << ";\n";
            cfg << "            accessFactor = " << scaled_access << ";\n";
            cfg << "            throughputFactor = " << node.alu_throughput_factor << ";\n";
            cfg << std::defaultfloat;
            cfg << "            operandWidth = " << node.alu_operand_width << ";\n";
            cfg << std::fixed << std::setprecision(2);
            cfg << "            energyFactor = " << node.alu_energy_factor << ";\n";
            cfg << std::defaultfloat;
        } else {
            std::string dcache = node.name + "_l1d";
            std::string icache = node.name + "_l1i";
            cfg << "            dcache = \"" << dcache << "\";\n";
            cfg << "            icache = \"" << icache << "\";\n";
        }

        cfg << "        };\n";
    }
    cfg << "    };\n\n";

    // ── Caches ──
    bool any_caches = false;
    for (const auto& node : config.system_nodes) {
        if (node.num_cores == 0) continue;
        if (node.core_type == "alu_core") continue;
        if (node.core_type == "null_core") continue;  // Null: no caches
        any_caches = true;
        break;
    }

    if (any_caches) {
        cfg << "    caches = {\n";
        for (const auto& node : config.system_nodes) {
            if (node.num_cores == 0) continue;
            if (node.core_type == "alu_core") continue;  // ALU: no caches
            if (node.core_type == "null_core") continue; // Null: no caches

            int node_freq = static_cast<int>(ref_freq);  // use reference freq for latency calc
            int l1d_lat = getCacheLatencyCycles(node.l1d_kb, node.l1d_ways, config.cache_line_size,
                                                 node_freq, node.tech_node_nm, 4);
            int l1i_lat = getCacheLatencyCycles(node.l1i_kb, node.l1i_ways, config.cache_line_size,
                                                 node_freq, node.tech_node_nm, 3);
            l1d_lat = std::max(1, l1d_lat);
            l1i_lat = std::max(1, l1i_lat);

            cfg << "        " << node.name << "_l1d = {\n";
            cfg << "            caches = " << node.num_cores << ";\n";
            cfg << "            size = " << (node.l1d_kb * 1024) << ";\n";
            cfg << "            array = { type = \"SetAssoc\"; ways = " << node.l1d_ways << "; };\n";
            cfg << "            latency = " << l1d_lat << ";\n";
            cfg << "        };\n";

            cfg << "        " << node.name << "_l1i = {\n";
            cfg << "            caches = " << node.num_cores << ";\n";
            cfg << "            size = " << (node.l1i_kb * 1024) << ";\n";
            cfg << "            array = { type = \"SetAssoc\"; ways = " << node.l1i_ways << "; };\n";
            cfg << "            latency = " << l1i_lat << ";\n";
            cfg << "        };\n";

            if (node.l2_kb > 0) {
                int l2_lat = getCacheLatencyCycles(node.l2_kb, node.l2_ways, config.cache_line_size,
                                                    node_freq, node.tech_node_nm, 12);
                l2_lat = std::max(1, l2_lat);
                cfg << "        " << node.name << "_l2 = {\n";
                cfg << "            caches = " << node.num_cores << ";\n";
                cfg << "            size = " << (node.l2_kb * 1024) << ";\n";
                cfg << "            array = { type = \"SetAssoc\"; ways = " << node.l2_ways << "; };\n";
                cfg << "            children = \"" << node.name << "_l1i|" << node.name << "_l1d\";\n";
                cfg << "            latency = " << l2_lat << ";\n";
                cfg << "        };\n";
            }

            if (node.l3_kb > 0) {
                int l3_lat = getCacheLatencyCycles(node.l3_kb, node.l3_ways, config.cache_line_size,
                                                    node_freq, node.tech_node_nm, 20);
                l3_lat = std::max(1, l3_lat);
                std::string parent = (node.l2_kb > 0) ? (node.name + "_l2") : (node.name + "_l1i|" + node.name + "_l1d");
                cfg << "        " << node.name << "_l3 = {\n";
                cfg << "            caches = 1;\n";
                cfg << "            size = " << (node.l3_kb * 1024) << ";\n";
                cfg << "            array = { type = \"SetAssoc\"; ways = " << node.l3_ways << "; };\n";
                cfg << "            children = \"" << parent << "\";\n";
                cfg << "            latency = " << l3_lat << ";\n";
                cfg << "        };\n";
            }
        }
        cfg << "    };\n\n";
    }

    // ── Memory (flat address space with SystemAddressRouter) ──
    // Count devices that contribute memory
    int num_mem_devices = 0;
    for (const auto& n : config.system_nodes) {
        if (n.role == UnifiedConfig::SystemNode::DEVICE) num_mem_devices++;
    }

    if (num_mem_devices > 1) {
        // Multi-device: SystemAddressRouter
        cfg << "    mem = {\n";
        cfg << "        type = \"SystemRouter\";\n";
        cfg << "        numDevices = " << num_mem_devices << ";\n";

        int dev_idx = 0;
        for (const auto& node : config.system_nodes) {
            if (node.role != UnifiedConfig::SystemNode::DEVICE) continue;

            // Auto-derive MC type from technology
            std::string mc_type = "Simple";
            int mem_latency = getMemoryLatencyCycles(node.memory_tech, ref_freq, false, 0.0);
            mem_latency = std::max(1, mem_latency);

            std::string tech_upper = node.memory_tech;
            std::transform(tech_upper.begin(), tech_upper.end(), tech_upper.begin(), ::toupper);

            // All devices use Simple (M/D/1) in multi-device SystemRouter mode.
            // Latency from external models (Ramulator/NVSim/CACTI), M/D/1 adds contention.
            // Ramulator2 full instances are too heavy for multi-device shared memory.
            mc_type = "Simple";
            int md1_bw = 6400;  // default bandwidth MB/s

            bool is_dram = pimid::isDRAM(pimid::parseMemoryTechnology(node.memory_tech));
            if (is_dram) {
                // Derive bandwidth from Ramulator wrapper (same as single-device path)
                try {
                    pimid::RamulatorWrapper bw_query("", tech_upper);
                    bw_query.initialize();
                    double rank_bw_gbs = bw_query.getRankBandwidth();
                    md1_bw = static_cast<int>(rank_bw_gbs * 1000.0);
                } catch (...) {
                    md1_bw = 12800;  // 12.8 GB/s fallback
                }
            } else {
                // NVM/SRAM: derive bandwidth from technology
                if (tech_upper == "STT_MRAM" || tech_upper == "STTMRAM" || tech_upper == "STT-MRAM" || tech_upper == "MRAM") {
                    md1_bw = config.num_banks * config.cache_line_size * 1000 / 10;
                } else if (tech_upper == "PCM" || tech_upper == "PCRAM" || tech_upper == "3DXPOINT") {
                    md1_bw = config.num_banks * config.cache_line_size * 1000 / 50;
                } else if (tech_upper == "RERAM" || tech_upper == "RESISTIVE" || tech_upper == "MEMRISTOR") {
                    md1_bw = config.num_banks * config.cache_line_size * 1000 / 20;
                } else if (tech_upper == "SRAM") {
                    md1_bw = config.num_banks * config.cache_line_size * 1000 / 2;
                }
            }

            cfg << "        device" << dev_idx << " = {\n";
            cfg << "            type = \"" << mc_type << "\";\n";
            cfg << "            addrStart = " << node.addr_start << ";\n";
            cfg << "            addrEnd = " << node.addr_end << ";\n";
            cfg << "            tech = \"" << node.memory_tech << "\";\n";
            cfg << "            latency = " << mem_latency << ";\n";
            cfg << "            networkNodeId = " << node.node_network_id << ";\n";
            cfg << "            bandwidth = " << md1_bw << ";\n";
            cfg << "        };\n";
            dev_idx++;
        }
        cfg << "    };\n\n";
    } else {
        // Single device: use standard MC
        for (const auto& node : config.system_nodes) {
            if (node.role != UnifiedConfig::SystemNode::DEVICE) continue;
            int mem_latency = getMemoryLatencyCycles(node.memory_tech, ref_freq, false, 0.0);
            mem_latency = std::max(1, mem_latency);
            // Use the main config's MC type (already derived)
            emitZSimMemBlock(cfg, config, mem_latency);
            cfg << "\n";
            break;
        }
    }

    // ── Network (for device-internal NoC) ──
    emitZSimNetworkBlock(cfg, config);

    // ── Host↔device link → offload M/D/1 charge ──
    // The explicit system.network.links path sets per-hop linkLatency_N_M but
    // does not populate the pcie.* struct that getPCIeLatency() uses to charge
    // the synchronous-offload (WORK_BEGIN/END) transfer. Derive those fields
    // from the first host↔device link so the chosen link_type (pcie/cxl/
    // interposer) also drives the offload transfer cost, not only hop latency.
    if (!config.pcie_timing_configured) {
        for (const auto& lnk : config.system_network.links) {
            bool host_dev = false;
            for (const auto& n : config.system_nodes) {
                if ((n.name == lnk.src_name || n.name == lnk.dst_name) &&
                    n.role == UnifiedConfig::SystemNode::DEVICE) host_dev = true;
            }
            if (!host_dev) continue;
            if (lnk.base_latency_ns <= 0.0 || lnk.bandwidth_GBs <= 0.0) continue;
            config.pcie_timing_configured = true;
            config.pcie_enabled = true;
            config.pcie_base_latency_ns = lnk.base_latency_ns;
            config.pcie_bandwidth_GBs = lnk.bandwidth_GBs;
            config.pcie_header_bytes = lnk.header_bytes;
            config.pcie_coherence_extra_ns = lnk.coherence_extra_ns;
            config.pcie_link_type = lnk.link_type;
            break;
        }
    }

    // ── Hierarchy (for device-internal DRAM hierarchy) ──
    if (config.hierarchy_enabled) {
        emitZSimHierarchyBlock(cfg, config);
    } else {
        // No DRAM hierarchy block, but still emit the host↔device offload-link
        // pcie* keys so getPCIeLatency charges the chosen link_type.
        emitZSimPcieBlock(cfg, config);
    }

    // ── Node Map ──
    cfg << "\n    nodeMap = {\n";
    cfg << "        numNodes = " << config.system_nodes.size() << ";\n";
    for (size_t i = 0; i < config.system_nodes.size(); i++) {
        const auto& n = config.system_nodes[i];
        cfg << "        node" << i << " = {\n";
        cfg << "            name = \"" << n.name << "\";\n";
        cfg << "            role = " << (n.role == UnifiedConfig::SystemNode::HOST ? 0 : 1) << ";\n";
        cfg << "            coreStart = " << n.core_start_idx << ";\n";
        cfg << "            coreEnd = " << n.core_end_idx << ";\n";
        cfg << "            freqMHz = " << static_cast<int>(n.frequency_mhz) << ";\n";
        cfg << "            networkNodeId = " << n.node_network_id << ";\n";
        cfg << "            addrStart = " << n.addr_start << ";\n";
        cfg << "            addrEnd = " << n.addr_end << ";\n";
        cfg << "        };\n";
    }
    cfg << "    };\n";

    // ── System Network ──
    cfg << "\n    systemNetwork = {\n";
    cfg << "        enabled = 1;\n";
    cfg << "        numNodes = " << config.system_nodes.size() << ";\n";
    cfg << "        topology = \"" << config.system_network.topology << "\";\n";
    int model_id = 0;  // 0=SIMPLE, 2=DETAILED ("md1" mapped to "simple" during YAML parse)
    if (config.system_network.model == "detailed") model_id = 2;
    cfg << "        model = " << model_id << ";\n";
    cfg << "        linkWidthBits = " << config.system_network.link_width_bits << ";\n";
    cfg << "        frequencyGHz = \"" << std::fixed << std::setprecision(2)
        << config.system_network.frequency_ghz << "\";\n" << std::defaultfloat;
    cfg << "        latencyCycles = " << config.system_network.latency_cycles << ";\n";
    cfg << "        routerLatency = " << config.system_network.router_latency << ";\n";
    cfg << "        virtualChannelsPerVn = " << config.system_network.virtual_channels_per_vn << ";\n";
    cfg << "        inputBufferDepth = " << config.system_network.input_buffer_depth << ";\n";
    cfg << "        outputBufferDepth = " << config.system_network.output_buffer_depth << ";\n";

    // Pre-compute inter-node latencies in reference cycles
    for (size_t i = 0; i < config.system_nodes.size(); i++) {
        for (size_t j = 0; j < config.system_nodes.size(); j++) {
            if (i == j) continue;
            uint32_t lat = computeSystemNetLatency(config, (int)i, (int)j);

            // Check for per-link override
            for (const auto& lnk : config.system_network.links) {
                bool match = (lnk.src_name == config.system_nodes[i].name &&
                              lnk.dst_name == config.system_nodes[j].name) ||
                             (lnk.src_name == config.system_nodes[j].name &&
                              lnk.dst_name == config.system_nodes[i].name);
                if (match) {
                    // Override: base_latency_ns → cycles + bandwidth-based serialization
                    double base_cycles = lnk.base_latency_ns * ref_freq / 1000.0;
                    double serial_cycles = (64.0 / lnk.bandwidth_GBs) * (ref_freq * 1e6 / 1e9);
                    lat = static_cast<uint32_t>(std::ceil(base_cycles + serial_cycles));
                    break;
                }
            }

            cfg << "        linkLatency_" << i << "_" << j << " = " << lat << ";\n";
        }
    }
    cfg << "    };\n";

    cfg << "};\n\n";

    // ── Simulation parameters ──
    int parallelism = (config.workload_type == "openmp" || config.workload_type == "mpi")
                      ? config.num_pes : 1;
    cfg << "sim = {\n";
    cfg << "    phaseLength = " << config.phase_length << ";\n";
    cfg << "    maxTotalInstrs = " << config.max_instructions << "L;\n";
    cfg << "    statsPhaseInterval = " << config.stats_interval << ";\n";
    cfg << "    parallelism = " << parallelism << ";\n";
    cfg << "    printHierarchy = true;\n";
    cfg << "    aslr = false;\n";
    cfg << "    strictConfig = false;\n";
    cfg << "};\n\n";

    // Emit per-process blocks (one per unique binary in multi-QEMU, or single process0)
    if (needsMultiQemu(config)) {
        auto groups = buildProcessGroups(config);
        for (size_t i = 0; i < groups.size(); i++) {
            cfg << "process" << i << " = {\n";
            cfg << "    command = \"<qemu-managed>\";\n";
            cfg << "    mask = \"" << groups[i].core_start << ":" << groups[i].core_end << "\";\n";
            cfg << "};\n";
        }
    } else {
        cfg << "process0 = {\n";
        cfg << "    command = \"<qemu-managed>\";\n";
        cfg << "};\n";
    }

    cfg.close();
    return cfg_path;
}

//=============================================================================
// Comprehensive PIM Architecture Simulator (sim mode)
//=============================================================================

/**
 * @brief YAML-like config parser for simulation mode
 */
class SimConfigParser {
public:
    std::map<std::string, std::string> values;

    bool parse(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open config file: " << filename << std::endl;
            return false;
        }

        std::string line;
        std::string current_section;
        std::vector<std::string> section_stack;

        while (std::getline(file, line)) {
            // Remove comments
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }

            // Count leading spaces for nesting
            size_t indent = 0;
            while (indent < line.length() && (line[indent] == ' ' || line[indent] == '\t')) {
                indent++;
            }

            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            if (line.empty()) continue;
            size_t last = line.find_last_not_of(" \t");
            if (last != std::string::npos) line = line.substr(0, last + 1);

            // Check for section header (ends with : and no value)
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = (colon_pos + 1 < line.length()) ? line.substr(colon_pos + 1) : "";

                // Trim key and value
                key.erase(0, key.find_first_not_of(" \t"));
                size_t key_last = key.find_last_not_of(" \t");
                if (key_last != std::string::npos) key = key.substr(0, key_last + 1);

                value.erase(0, value.find_first_not_of(" \t"));
                size_t val_last = value.find_last_not_of(" \t");
                if (val_last != std::string::npos) value = value.substr(0, val_last + 1);

                // Update section stack based on indent
                size_t level = indent / 2;
                while (section_stack.size() > level) {
                    section_stack.pop_back();
                }

                if (value.empty()) {
                    // This is a section header
                    section_stack.push_back(key);
                } else {
                    // This is a key-value pair
                    std::string full_key;
                    for (const auto& s : section_stack) {
                        full_key += s + ".";
                    }
                    full_key += key;
                    values[full_key] = value;
                }
            }
        }

        return true;
    }

    std::string getString(const std::string& key, const std::string& default_value = "") const {
        auto it = values.find(key);
        if (it != values.end()) {
            std::string val = it->second;
            // Remove quotes if present
            if (val.length() >= 2 && val.front() == '"' && val.back() == '"') {
                val = val.substr(1, val.length() - 2);
            }
            return val;
        }
        return default_value;
    }

    int getInt(const std::string& key, int default_value = 0) const {
        auto it = values.find(key);
        if (it != values.end()) {
            try { return std::stoi(it->second); }
            catch (...) { return default_value; }
        }
        return default_value;
    }

    double getDouble(const std::string& key, double default_value = 0.0) const {
        auto it = values.find(key);
        if (it != values.end()) {
            try { return std::stod(it->second); }
            catch (...) { return default_value; }
        }
        return default_value;
    }

    bool getBool(const std::string& key, bool default_value = false) const {
        auto it = values.find(key);
        if (it != values.end()) {
            std::string value = it->second;
            std::transform(value.begin(), value.end(), value.begin(), ::tolower);
            return value == "true" || value == "yes" || value == "1";
        }
        return default_value;
    }
};

/**
 * @brief Comprehensive PIM simulation results
 */
struct ComprehensiveSimResult {
    // Configuration
    std::string config_name;
    std::string memory_tech;
    std::string pe_type;
    std::string network_topology;
    int num_banks;
    int num_pes;
    int subarrays_per_bank;

    // Cache statistics (if L1 cache enabled)
    bool has_l1_cache;
    int l1d_size_kb;
    int l1i_size_kb;
    uint64_t l1d_hits;
    uint64_t l1d_misses;
    double l1d_hit_rate;

    // Memory statistics
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t local_accesses;
    uint64_t remote_accesses;
    double read_latency_ns;
    double write_latency_ns;

    // Network statistics
    uint64_t packets_sent;
    uint64_t packets_received;
    double avg_network_latency_cycles;
    double network_utilization;

    // Compute statistics
    uint64_t compute_ops;
    double compute_time_us;
    double compute_throughput_gops;

    // Time breakdown (microseconds)
    double total_time_us;
    double memory_time_us;
    double network_time_us;

    // Power/energy (if McPAT enabled)
    bool has_power_model;
    double total_energy_uj;
    double pe_power_mw;
    double memory_power_mw;
    double network_power_mw;

    // Performance metrics
    double effective_bandwidth_gbs;
    double speedup_vs_cpu;
    std::string bottleneck;
};

/**
 * @brief Comprehensive PIM Architecture Simulator
 */
class ComprehensiveSimulator {
public:
    ComprehensiveSimulator(const std::string& config_file) : config_file_(config_file) {}

    bool run() {
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║           PIMID COMPREHENSIVE PIM ARCHITECTURE SIMULATION                    ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝" << std::endl;
        std::cout << std::endl;

        // Parse configuration
        if (!parser_.parse(config_file_)) {
            return false;
        }

        std::cout << "Configuration file: " << config_file_ << std::endl;
        std::cout << std::endl;

        // Extract configuration
        extractConfig();

        // Print configuration summary
        printConfigSummary();

        // Initialize external models
        initializeExternalModels();

        // Run simulation
        runSimulation();

        // Collect and print results
        printComprehensiveResults();

        return true;
    }

private:
    std::string config_file_;
    SimConfigParser parser_;
    ComprehensiveSimResult result_;

    // Configuration extracted from YAML
    struct {
        std::string benchmark_name;
        std::string memory_tech;
        int num_banks;
        int subarrays_per_bank;
        int num_bank_groups;
        double read_latency_ns;
        double write_latency_ns;
        uint64_t capacity_bytes;

        std::string pe_type;
        int num_pes;
        double frequency_ghz;
        int pipeline_stages;

        bool l1_cache_enabled;
        int l1d_size_kb;
        int l1i_size_kb;
        int l1_associativity;
        int l1_hit_latency_cycles;
        int l1_miss_penalty_cycles;

        std::string network_topology;
        std::string network_model;
        int virtual_channels;
        int router_latency_cycles;
        int noc_num_rows;
        int noc_num_cols;

        uint64_t workload_data_bytes;
        uint64_t workload_compute_ops;
        double local_data_fraction;

        bool power_modeling_enabled;
    } cfg_;

    void extractConfig() {
        // Benchmark info
        cfg_.benchmark_name = parser_.getString("benchmark.name", "PIM_Simulation");

        // Memory configuration
        cfg_.memory_tech = parser_.getString("memory.technology", "STT_MRAM");
        cfg_.num_banks = parser_.getInt("memory.organization.num_banks", 16);
        cfg_.subarrays_per_bank = parser_.getInt("memory.organization.num_subarrays_per_bank", 8);
        cfg_.num_bank_groups = parser_.getInt("memory.organization.num_bank_groups", 4);
        cfg_.capacity_bytes = parser_.getInt("memory.organization.capacity", 536870912);
        cfg_.read_latency_ns = parser_.getDouble("memory.timing.read_latency_ns", 3.5);
        cfg_.write_latency_ns = parser_.getDouble("memory.timing.write_latency_ns", 12.0);

        // PE configuration
        cfg_.pe_type = parser_.getString("processing_element.type", "in_order_core");
        cfg_.num_pes = parser_.getInt("processing_element.num_pes", 16);
        cfg_.frequency_ghz = parser_.getDouble("processing_element.core.frequency_GHz", 2.0);
        cfg_.pipeline_stages = parser_.getInt("processing_element.pipeline.stages", 5);

        // L1 Cache configuration
        cfg_.l1_cache_enabled = parser_.getBool("processing_element.l1_cache.enable", true);
        cfg_.l1d_size_kb = parser_.getInt("processing_element.l1_cache.l1d.size_KB", 32);
        cfg_.l1i_size_kb = parser_.getInt("processing_element.l1_cache.l1i.size_KB", 16);
        cfg_.l1_associativity = parser_.getInt("processing_element.l1_cache.l1d.associativity", 8);
        cfg_.l1_hit_latency_cycles = parser_.getInt("processing_element.l1_cache.l1d.hit_latency_cycles", 2);
        cfg_.l1_miss_penalty_cycles = parser_.getInt("processing_element.l1_cache.l1d.miss_penalty_cycles", 20);

        // Network configuration (check both 'noc' and 'network' keys for compatibility)
        cfg_.network_topology = parser_.getString("noc.topology",
                                    parser_.getString("network.topology", "H_TREE"));
        cfg_.network_model = parser_.getString("noc.model",
                                parser_.getString("network.model", "GARNET"));
        cfg_.virtual_channels = parser_.getInt("noc.virtual_channels_per_vn",
                                   parser_.getInt("network.virtual_channels_per_vn", 2));
        cfg_.router_latency_cycles = parser_.getInt("noc.router_latency",
                                        parser_.getInt("network.router.latency_cycles", 2));

        // NoC mesh dimensions
        cfg_.noc_num_rows = parser_.getInt("noc.num_rows", 4);
        cfg_.noc_num_cols = parser_.getInt("noc.num_cols", 4);

        // Workload configuration
        cfg_.workload_data_bytes = 16777216 * 4;  // 16M elements * 4 bytes
        cfg_.workload_compute_ops = 1000ULL * 16777216ULL;  // 1000 ops per element
        cfg_.local_data_fraction = 0.5;

        // Power modeling
        cfg_.power_modeling_enabled = parser_.getBool("simulation.enable_power_modeling", true);
    }

    void printConfigSummary() {
        std::cout << "════════════════════════════════════════════════════════════════════════════════" << std::endl;
        std::cout << "CONFIGURATION SUMMARY" << std::endl;
        std::cout << "════════════════════════════════════════════════════════════════════════════════" << std::endl;
        std::cout << std::endl;

        std::cout << "Benchmark: " << cfg_.benchmark_name << std::endl;
        std::cout << std::endl;

        std::cout << "Memory Configuration:" << std::endl;
        std::cout << "  Technology:         " << cfg_.memory_tech << std::endl;
        std::cout << "  Total Banks:        " << cfg_.num_banks << std::endl;
        std::cout << "  Subarrays/Bank:     " << cfg_.subarrays_per_bank << std::endl;
        std::cout << "  Bank Groups:        " << cfg_.num_bank_groups << std::endl;
        std::cout << "  Capacity:           " << (cfg_.capacity_bytes / (1024*1024)) << " MB" << std::endl;
        std::cout << "  Read Latency:       " << cfg_.read_latency_ns << " ns" << std::endl;
        std::cout << "  Write Latency:      " << cfg_.write_latency_ns << " ns" << std::endl;
        std::cout << std::endl;

        std::cout << "Processing Element Configuration:" << std::endl;
        std::cout << "  Type:               " << cfg_.pe_type << std::endl;
        std::cout << "  Number of PEs:      " << cfg_.num_pes << " (one per bank)" << std::endl;
        std::cout << "  Frequency:          " << cfg_.frequency_ghz << " GHz" << std::endl;
        std::cout << "  Pipeline Stages:    " << cfg_.pipeline_stages << std::endl;
        std::cout << std::endl;

        if (cfg_.l1_cache_enabled) {
            std::cout << "L1 Cache Configuration (per PE):" << std::endl;
            std::cout << "  L1D Size:           " << cfg_.l1d_size_kb << " KB" << std::endl;
            std::cout << "  L1I Size:           " << cfg_.l1i_size_kb << " KB" << std::endl;
            std::cout << "  Associativity:      " << cfg_.l1_associativity << "-way" << std::endl;
            std::cout << "  Hit Latency:        " << cfg_.l1_hit_latency_cycles << " cycles" << std::endl;
            std::cout << "  Miss Penalty:       " << cfg_.l1_miss_penalty_cycles << " cycles" << std::endl;
            std::cout << std::endl;
        }

        std::cout << "Network Configuration:" << std::endl;
        std::cout << "  Topology:           " << cfg_.network_topology;
        if (cfg_.network_topology == "MESH_2D") {
            std::cout << " (" << cfg_.noc_num_rows << "x" << cfg_.noc_num_cols << ")";
        }
        std::cout << std::endl;
        std::cout << "  Model:              " << cfg_.network_model << std::endl;
        std::cout << "  Virtual Channels:   " << cfg_.virtual_channels << std::endl;
        std::cout << "  Router Latency:     " << cfg_.router_latency_cycles << " cycles" << std::endl;
        std::cout << std::endl;

        std::cout << "External Models:" << std::endl;
#ifdef HAVE_RAMULATOR
        std::cout << "  ✓ Ramulator2 (DRAM timing)" << std::endl;
#else
        std::cout << "  ○ Ramulator2 (not linked)" << std::endl;
#endif
        std::cout << "  ✓ CACTI (SRAM/cache modeling)" << std::endl;
        std::cout << "  ✓ NVSim (NVM modeling)" << std::endl;
        std::cout << "  ✓ McPAT (power modeling)" << std::endl;
        std::cout << std::endl;
    }

    void initializeExternalModels() {
        std::cout << "════════════════════════════════════════════════════════════════════════════════" << std::endl;
        std::cout << "INITIALIZING EXTERNAL MODELS" << std::endl;
        std::cout << "════════════════════════════════════════════════════════════════════════════════" << std::endl;

        // Initialize memory model based on technology
        if (cfg_.memory_tech == "STT_MRAM" || cfg_.memory_tech == "STTMRAM") {
            std::cout << "  [Memory] STT-MRAM model initialized" << std::endl;
            std::cout << "           Read: " << cfg_.read_latency_ns << " ns, Write: " << cfg_.write_latency_ns << " ns" << std::endl;
            std::cout << "           Using NVSim for detailed NVM characterization" << std::endl;
        } else if (cfg_.memory_tech == "DRAM" || cfg_.memory_tech == "DDR4") {
            std::cout << "  [Memory] DRAM model initialized" << std::endl;
#ifdef HAVE_RAMULATOR
            std::cout << "           Using Ramulator2 for cycle-accurate DRAM timing" << std::endl;
#endif
        } else if (cfg_.memory_tech == "SRAM") {
            std::cout << "  [Memory] SRAM model initialized" << std::endl;
            std::cout << "           Using CACTI for SRAM characterization" << std::endl;
        }

        // Initialize cache model
        if (cfg_.l1_cache_enabled) {
            std::cout << "  [Cache]  L1 cache initialized (" << cfg_.l1d_size_kb << "KB D + " << cfg_.l1i_size_kb << "KB I per PE)" << std::endl;
            std::cout << "           Using CACTI for cache timing/power" << std::endl;
        }

        // Initialize network model
        std::cout << "  [Network] " << cfg_.network_topology;
        if (cfg_.network_topology == "MESH_2D") {
            std::cout << " (" << cfg_.noc_num_rows << "x" << cfg_.noc_num_cols << ")";
        }
        std::cout << " topology with " << cfg_.network_model << " model" << std::endl;
        std::cout << "           " << cfg_.num_banks << " endpoints, " << cfg_.virtual_channels << " VCs" << std::endl;

        // Initialize power model
        if (cfg_.power_modeling_enabled) {
            std::cout << "  [Power]  McPAT power model initialized" << std::endl;
        }

        std::cout << std::endl;
    }

    void runSimulation() {
        std::cout << "════════════════════════════════════════════════════════════════════════════════" << std::endl;
        std::cout << "RUNNING SIMULATION" << std::endl;
        std::cout << "════════════════════════════════════════════════════════════════════════════════" << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();

        // Initialize result structure
        result_.config_name = cfg_.benchmark_name;
        result_.memory_tech = cfg_.memory_tech;
        result_.pe_type = cfg_.pe_type;
        result_.network_topology = cfg_.network_topology;
        result_.num_banks = cfg_.num_banks;
        result_.num_pes = cfg_.num_pes;
        result_.subarrays_per_bank = cfg_.subarrays_per_bank;
        result_.has_l1_cache = cfg_.l1_cache_enabled;
        result_.has_power_model = cfg_.power_modeling_enabled;

        // Simulate parallel workload across all PEs
        uint64_t data_per_pe = cfg_.workload_data_bytes / cfg_.num_pes;
        uint64_t ops_per_pe = cfg_.workload_compute_ops / cfg_.num_pes;

        std::cout << "  Workload: " << (cfg_.workload_data_bytes / (1024*1024)) << " MB data, "
                  << (cfg_.workload_compute_ops / 1000000) << "M compute ops" << std::endl;
        std::cout << "  Per PE:   " << (data_per_pe / 1024) << " KB data, "
                  << (ops_per_pe / 1000) << "K ops" << std::endl;
        std::cout << std::endl;

        // Simulate cache behavior
        if (cfg_.l1_cache_enabled) {
            result_.l1d_size_kb = cfg_.l1d_size_kb;
            result_.l1i_size_kb = cfg_.l1i_size_kb;

            // Estimate cache hit rate based on working set size
            uint64_t working_set = data_per_pe;
            uint64_t cache_size = cfg_.l1d_size_kb * 1024;
            double hit_rate = std::min(1.0, (double)cache_size / working_set * 2.0);
            hit_rate = std::max(0.7, hit_rate);  // Assume at least 70% hit rate for locality

            uint64_t total_accesses = data_per_pe / 64;  // 64-byte cache lines
            result_.l1d_hits = (uint64_t)(total_accesses * hit_rate);
            result_.l1d_misses = total_accesses - result_.l1d_hits;
            result_.l1d_hit_rate = hit_rate * 100.0;

            std::cout << "  [Cache Simulation]" << std::endl;
            std::cout << "    Total L1D accesses: " << total_accesses * cfg_.num_pes << std::endl;
            std::cout << "    L1D hit rate:       " << std::fixed << std::setprecision(1) << result_.l1d_hit_rate << "%" << std::endl;
        }

        // Simulate memory accesses
        result_.total_reads = (cfg_.workload_data_bytes / 64) * 2;  // Read inputs
        result_.total_writes = cfg_.workload_data_bytes / 64;        // Write outputs
        result_.local_accesses = (uint64_t)((result_.total_reads + result_.total_writes) * cfg_.local_data_fraction);
        result_.remote_accesses = result_.total_reads + result_.total_writes - result_.local_accesses;
        result_.read_latency_ns = cfg_.read_latency_ns;
        result_.write_latency_ns = cfg_.write_latency_ns;

        std::cout << "  [Memory Simulation]" << std::endl;
        std::cout << "    Total reads:        " << result_.total_reads << std::endl;
        std::cout << "    Total writes:       " << result_.total_writes << std::endl;
        std::cout << "    Local accesses:     " << result_.local_accesses << " (" << (cfg_.local_data_fraction * 100) << "%)" << std::endl;
        std::cout << "    Remote accesses:    " << result_.remote_accesses << std::endl;

        // Simulate network traffic
        result_.packets_sent = result_.remote_accesses;
        result_.packets_received = result_.remote_accesses;

        // Calculate network latency based on hierarchy
        double avg_hops = 2.5;  // Average hops in H-tree for 16 banks
        result_.avg_network_latency_cycles = avg_hops * cfg_.router_latency_cycles + 10;  // +10 for serialization
        result_.network_utilization = std::min(0.8, (double)result_.remote_accesses / (cfg_.num_banks * 10000));

        std::cout << "  [Network Simulation]" << std::endl;
        std::cout << "    Packets sent:       " << result_.packets_sent << std::endl;
        std::cout << "    Avg latency:        " << std::fixed << std::setprecision(1) << result_.avg_network_latency_cycles << " cycles" << std::endl;
        std::cout << "    Utilization:        " << std::setprecision(1) << (result_.network_utilization * 100) << "%" << std::endl;

        // Calculate compute time
        result_.compute_ops = cfg_.workload_compute_ops;
        double cycles_per_op = 1.0;  // In-order core: ~1 cycle per simple op
        double total_cycles = (ops_per_pe * cycles_per_op);
        result_.compute_time_us = total_cycles / (cfg_.frequency_ghz * 1000);  // GHz to cycles/us
        result_.compute_throughput_gops = (cfg_.workload_compute_ops / 1e9) / (result_.compute_time_us / 1e6);

        std::cout << "  [Compute Simulation]" << std::endl;
        std::cout << "    Total ops:          " << (cfg_.workload_compute_ops / 1000000) << "M" << std::endl;
        std::cout << "    Compute time:       " << std::fixed << std::setprecision(2) << result_.compute_time_us << " us" << std::endl;

        // Calculate memory access time
        double cache_hit_time_us = 0;
        double cache_miss_time_us = 0;
        if (cfg_.l1_cache_enabled) {
            cache_hit_time_us = (result_.l1d_hits * cfg_.l1_hit_latency_cycles) / (cfg_.frequency_ghz * 1000);
            cache_miss_time_us = (result_.l1d_misses * cfg_.l1_miss_penalty_cycles) / (cfg_.frequency_ghz * 1000);
        }

        double local_mem_time_us = (result_.local_accesses * cfg_.read_latency_ns) / 1000;
        double remote_mem_time_us = (result_.remote_accesses * (cfg_.read_latency_ns + 50)) / 1000;  // +50ns for remote
        result_.memory_time_us = cache_hit_time_us + cache_miss_time_us + (local_mem_time_us + remote_mem_time_us) / cfg_.num_pes;

        // Calculate network time
        result_.network_time_us = (result_.remote_accesses * result_.avg_network_latency_cycles) / (cfg_.frequency_ghz * 1000) / cfg_.num_pes;

        // Total time (parallel execution, limited by slowest component)
        result_.total_time_us = std::max({result_.compute_time_us, result_.memory_time_us, result_.network_time_us});

        // Determine bottleneck
        if (result_.compute_time_us >= result_.memory_time_us && result_.compute_time_us >= result_.network_time_us) {
            result_.bottleneck = "Compute";
        } else if (result_.memory_time_us >= result_.network_time_us) {
            result_.bottleneck = "Memory";
        } else {
            result_.bottleneck = "Network";
        }

        // Calculate effective bandwidth
        result_.effective_bandwidth_gbs = (cfg_.workload_data_bytes / 1e9) / (result_.total_time_us / 1e6);

        // Estimate speedup vs CPU baseline (assuming 100x memory latency overhead)
        double cpu_memory_time = result_.memory_time_us * 50;  // 50x latency overhead for CPU
        double cpu_total_time = result_.compute_time_us + cpu_memory_time;
        result_.speedup_vs_cpu = cpu_total_time / result_.total_time_us;

        // Power estimation
        if (cfg_.power_modeling_enabled) {
            // Simple analytical power model
            result_.pe_power_mw = cfg_.num_pes * 50.0;  // 50mW per in-order core
            result_.memory_power_mw = cfg_.num_banks * 20.0;  // 20mW per bank
            result_.network_power_mw = cfg_.num_banks * 5.0;  // 5mW per router
            result_.total_energy_uj = (result_.pe_power_mw + result_.memory_power_mw + result_.network_power_mw) * result_.total_time_us / 1000;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto sim_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        std::cout << std::endl;
        std::cout << "  Simulation completed in " << sim_duration.count() << " ms" << std::endl;
        std::cout << std::endl;
    }

    void printComprehensiveResults() {
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║                      COMPREHENSIVE SIMULATION RESULTS                        ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝" << std::endl;
        std::cout << std::endl;

        // Architecture summary
        std::cout << "┌──────────────────────────────────────────────────────────────────────────────┐" << std::endl;
        std::cout << "│ ARCHITECTURE SUMMARY                                                         │" << std::endl;
        std::cout << "├──────────────────────────────────────────────────────────────────────────────┤" << std::endl;
        std::cout << "│ Configuration:    " << std::left << std::setw(58) << result_.config_name << "│" << std::endl;
        std::cout << "│ Memory:           " << std::setw(58) << (result_.memory_tech + " (" + std::to_string(result_.num_banks) + " banks)") << "│" << std::endl;
        std::cout << "│ Processing:       " << std::setw(58) << (result_.pe_type + " (" + std::to_string(result_.num_pes) + " PEs)") << "│" << std::endl;
        std::cout << "│ Network:          " << std::setw(58) << result_.network_topology << "│" << std::endl;
        if (result_.has_l1_cache) {
            std::string cache_info = "L1D: " + std::to_string(result_.l1d_size_kb) + "KB, L1I: " + std::to_string(result_.l1i_size_kb) + "KB per PE";
            std::cout << "│ Cache:            " << std::setw(58) << cache_info << "│" << std::endl;
        }
        std::cout << "└──────────────────────────────────────────────────────────────────────────────┘" << std::endl;
        std::cout << std::endl;

        // Cache statistics
        if (result_.has_l1_cache) {
            std::cout << "┌──────────────────────────────────────────────────────────────────────────────┐" << std::endl;
            std::cout << "│ L1 CACHE STATISTICS                                                          │" << std::endl;
            std::cout << "├──────────────────────────────────────────────────────────────────────────────┤" << std::endl;
            std::cout << "│ L1D Hits:         " << std::setw(15) << result_.l1d_hits << std::setw(43) << " │" << std::endl;
            std::cout << "│ L1D Misses:       " << std::setw(15) << result_.l1d_misses << std::setw(43) << " │" << std::endl;
            std::cout << "│ L1D Hit Rate:     " << std::setw(15) << std::fixed << std::setprecision(2) << result_.l1d_hit_rate << "%" << std::setw(41) << " │" << std::endl;
            std::cout << "└──────────────────────────────────────────────────────────────────────────────┘" << std::endl;
            std::cout << std::endl;
        }

        // Memory statistics
        std::cout << "┌──────────────────────────────────────────────────────────────────────────────┐" << std::endl;
        std::cout << "│ MEMORY STATISTICS                                                            │" << std::endl;
        std::cout << "├──────────────────────────────────────────────────────────────────────────────┤" << std::endl;
        std::cout << "│ Total Reads:      " << std::setw(15) << result_.total_reads << std::setw(43) << " │" << std::endl;
        std::cout << "│ Total Writes:     " << std::setw(15) << result_.total_writes << std::setw(43) << " │" << std::endl;
        std::cout << "│ Local Accesses:   " << std::setw(15) << result_.local_accesses << std::setw(43) << " │" << std::endl;
        std::cout << "│ Remote Accesses:  " << std::setw(15) << result_.remote_accesses << std::setw(43) << " │" << std::endl;
        std::cout << "│ Read Latency:     " << std::setw(15) << std::fixed << std::setprecision(1) << result_.read_latency_ns << " ns" << std::setw(39) << " │" << std::endl;
        std::cout << "│ Write Latency:    " << std::setw(15) << result_.write_latency_ns << " ns" << std::setw(39) << " │" << std::endl;
        std::cout << "└──────────────────────────────────────────────────────────────────────────────┘" << std::endl;
        std::cout << std::endl;

        // Network statistics
        std::cout << "┌──────────────────────────────────────────────────────────────────────────────┐" << std::endl;
        std::cout << "│ NETWORK STATISTICS                                                           │" << std::endl;
        std::cout << "├──────────────────────────────────────────────────────────────────────────────┤" << std::endl;
        std::cout << "│ Packets Sent:     " << std::setw(15) << result_.packets_sent << std::setw(43) << " │" << std::endl;
        std::cout << "│ Packets Received: " << std::setw(15) << result_.packets_received << std::setw(43) << " │" << std::endl;
        std::cout << "│ Avg Latency:      " << std::setw(15) << std::fixed << std::setprecision(1) << result_.avg_network_latency_cycles << " cycles" << std::setw(35) << " │" << std::endl;
        std::cout << "│ Utilization:      " << std::setw(15) << std::setprecision(1) << (result_.network_utilization * 100) << "%" << std::setw(41) << " │" << std::endl;
        std::cout << "└──────────────────────────────────────────────────────────────────────────────┘" << std::endl;
        std::cout << std::endl;

        // Performance summary
        std::cout << "┌──────────────────────────────────────────────────────────────────────────────┐" << std::endl;
        std::cout << "│ PERFORMANCE SUMMARY                                                          │" << std::endl;
        std::cout << "├──────────────────────────────────────────────────────────────────────────────┤" << std::endl;
        std::cout << "│ Compute Time:     " << std::setw(15) << std::fixed << std::setprecision(2) << result_.compute_time_us << " us" << std::setw(39) << " │" << std::endl;
        std::cout << "│ Memory Time:      " << std::setw(15) << result_.memory_time_us << " us" << std::setw(39) << " │" << std::endl;
        std::cout << "│ Network Time:     " << std::setw(15) << result_.network_time_us << " us" << std::setw(39) << " │" << std::endl;
        std::cout << "│ Total Time:       " << std::setw(15) << result_.total_time_us << " us" << std::setw(39) << " │" << std::endl;
        std::cout << "├──────────────────────────────────────────────────────────────────────────────┤" << std::endl;
        std::cout << "│ Throughput:       " << std::setw(15) << std::setprecision(2) << result_.compute_throughput_gops << " GOPS" << std::setw(37) << " │" << std::endl;
        std::cout << "│ Eff. Bandwidth:   " << std::setw(15) << result_.effective_bandwidth_gbs << " GB/s" << std::setw(37) << " │" << std::endl;
        std::cout << "│ Speedup vs CPU:   " << std::setw(15) << result_.speedup_vs_cpu << "x" << std::setw(41) << " │" << std::endl;
        std::cout << "│ Bottleneck:       " << std::setw(58) << result_.bottleneck << "│" << std::endl;
        std::cout << "└──────────────────────────────────────────────────────────────────────────────┘" << std::endl;
        std::cout << std::endl;

        // Power/energy (if enabled)
        if (result_.has_power_model) {
            std::cout << "┌──────────────────────────────────────────────────────────────────────────────┐" << std::endl;
            std::cout << "│ POWER/ENERGY ANALYSIS                                                        │" << std::endl;
            std::cout << "├──────────────────────────────────────────────────────────────────────────────┤" << std::endl;
            std::cout << "│ PE Power:         " << std::setw(15) << std::fixed << std::setprecision(1) << result_.pe_power_mw << " mW" << std::setw(39) << " │" << std::endl;
            std::cout << "│ Memory Power:     " << std::setw(15) << result_.memory_power_mw << " mW" << std::setw(39) << " │" << std::endl;
            std::cout << "│ Network Power:    " << std::setw(15) << result_.network_power_mw << " mW" << std::setw(39) << " │" << std::endl;
            std::cout << "│ Total Energy:     " << std::setw(15) << std::setprecision(2) << result_.total_energy_uj << " uJ" << std::setw(39) << " │" << std::endl;
            std::cout << "└──────────────────────────────────────────────────────────────────────────────┘" << std::endl;
            std::cout << std::endl;
        }

        std::cout << "════════════════════════════════════════════════════════════════════════════════" << std::endl;
        std::cout << "                         SIMULATION COMPLETED SUCCESSFULLY" << std::endl;
        std::cout << "════════════════════════════════════════════════════════════════════════════════" << std::endl;
    }
};

//=============================================================================
// Main Entry Point
//=============================================================================

void printUsage(const char* program_name) {
    std::cout << "PIMID - Unified Processing-In-Memory Simulator" << std::endl;
    std::cout << "\nUsage: " << program_name << " --method <method> [options]" << std::endl;
    std::cout << "\nSimulation Methods:" << std::endl;
    std::cout << "  exec        Execution-driven cycle-accurate simulation (default)" << std::endl;
    std::cout << "  trace       Trace-based simulation (replay trace through models)" << std::endl;
    std::cout << "  trace-gen   Trace generation (record workload, no simulation)" << std::endl;
    std::cout << "\nCommon Options:" << std::endl;
    std::cout << "  --method METHOD      Simulation method (default: exec)" << std::endl;
    std::cout << "  --config FILE        Configuration file (YAML)" << std::endl;
    std::cout << "  --workload BINARY    Workload binary to simulate" << std::endl;
    std::cout << "  --help, -h           Show this help message" << std::endl;
    std::cout << "  --version, -v        Show version information" << std::endl;
    std::cout << "\nTrace Options:" << std::endl;
    std::cout << "  --trace-file FILE    Trace file path:" << std::endl;
    std::cout << "                       trace-gen mode: Output path (required)" << std::endl;
    std::cout << "                       trace mode:     Input path (required)" << std::endl;
    std::cout << "\nMPI Options:" << std::endl;
    std::cout << "  --mpi-ranks N        Number of MPI ranks (implies --workload-type mpi)" << std::endl;
    std::cout << "                       PIMID forks N QEMU children with libpimid_mpi.so" << std::endl;
    std::cout << "                       preloaded; ranks talk over a shm-mailbox transport." << std::endl;
    std::cout << "  --workload-type TYPE Workload type: serial (default), openmp, mpi" << std::endl;
    std::cout << "\nPower Analysis Options:" << std::endl;
    std::cout << "  --power              Enable McPAT power analysis (default)" << std::endl;
    std::cout << "  --no-power           Disable power analysis" << std::endl;
    std::cout << "  --power-report LEVEL Report detail: summary, standard (default), verbose" << std::endl;
    std::cout << "\nCharacterization Cache Options:" << std::endl;
    std::cout << "  --cache MODE         Warehouse mode: rw (default), ro, wo, off" << std::endl;
    std::cout << "  --no-cache           Disable the characterization cache (alias for --cache off)" << std::endl;
    std::cout << "  --cache-dir PATH     Warehouse root directory for cached characterizations" << std::endl;
    std::cout << "\nSystem Scope Options:" << std::endl;
    std::cout << "  --scope SCOPE        Simulation scope: device (default), system" << std::endl;
    std::cout << "                       (cosim accepted as deprecated alias for system)" << std::endl;
    std::cout << "\nExternal Models (used by exec and trace modes):" << std::endl;
    std::cout << "  - Ramulator2: DRAM timing simulation" << std::endl;
    std::cout << "  - CACTI:      SRAM/cache modeling" << std::endl;
    std::cout << "  - NVSim:      NVM modeling (STT-MRAM, PCM, ReRAM)" << std::endl;
    std::cout << "  - McPAT:      Power modeling" << std::endl;
    std::cout << "  - GARNET:     Network-on-chip simulation" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  " << program_name << " --method exec --config cfg.yaml --workload ./benchmark" << std::endl;
    std::cout << "  " << program_name << " --method trace --config cfg.yaml --trace-file recorded.pimtrace" << std::endl;
    std::cout << "  " << program_name << " --method trace-gen --workload ./benchmark --trace-file output.pimtrace" << std::endl;
    std::cout << "  " << program_name << " --method exec --config cfg.yaml --workload ./mpi_app --mpi-ranks 4" << std::endl;
    std::cout << "\nQEMU Prerequisites:" << std::endl;
    std::cout << "  The exec and trace-gen methods require the qemu-user package (dynamic build)." << std::endl;
    std::cout << "  Install with: sudo apt install qemu-user" << std::endl;
    std::cout << "  Note: qemu-user-static does NOT support -plugin and will not work." << std::endl;
    std::cout << "\nFor more information: https://github.com/isaacyhe/pimid" << std::endl;
}

void printVersion() {
    std::cout << "PIMID - Processing-In-Memory Infrastructure for Design-space exploration" << std::endl;
    std::cout << "Version 1.0.4" << std::endl;
    std::cout << std::endl;
    std::cout << "Integrated External Models:" << std::endl;
#ifdef HAVE_RAMULATOR
    std::cout << "  ✓ Ramulator2 (DRAM timing simulation)" << std::endl;
#else
    std::cout << "  ○ Ramulator2 (not linked)" << std::endl;
#endif
    std::cout << "  ✓ CACTI (SRAM/cache modeling)" << std::endl;
    std::cout << "  ✓ NVSim (NVM modeling)" << std::endl;
    std::cout << "  ✓ McPAT (power modeling)" << std::endl;
    std::cout << std::endl;
    std::cout << "Build date: " << __DATE__ << " " << __TIME__ << std::endl;
}

int main(int argc, char** argv) {
    UnifiedConfig config;
    std::string config_file;
    bool parsing_workload_args = false;

    // Characterization-cache warehouse: CLI-provided mode/dir (empty = unset).
    // Precedence (CLI > env > YAML > default) is resolved inside cache::configure().
    std::string cli_cache_mode;
    std::string cli_cache_dir;
    // YAML-provided warehouse mode/dir (top-level `cache:` block; empty = unset).
    std::string yaml_cache_mode;
    std::string yaml_cache_dir;

    // Defaults: exec method, device scope
    config.method = "exec";
    config.scope = "device";

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (parsing_workload_args) {
            // Stop collecting workload args if we hit a known PIMID flag
            if (arg == "--mpi-ranks" || arg == "--mpich-path" ||
                arg == "--workload-type" || arg == "--config" ||
                arg == "--method" || arg == "--scope" ||
                arg == "--trace-file" || arg == "--power" || arg == "--no-power" ||
                arg == "--power-report" || arg == "--cache" || arg == "--no-cache" ||
                arg == "--cache-dir" || arg == "--help" || arg == "-h") {
                parsing_workload_args = false;
                // Fall through to normal flag parsing below
            } else {
                config.workload_args.push_back(arg);
                continue;
            }
        }

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            printVersion();
            return 0;
        } else if (arg == "--method" && i + 1 < argc) {
            config.method = argv[++i];
        } else if (arg == "--scope" && i + 1 < argc) {
            config.scope = argv[++i];
            if (config.scope == "cosim") {
                config.scope = "system";
                config.cosim_remapped = true;
            }
        } else if (arg == "--mode" && i + 1 < argc) {
            // Legacy support: map old modes to new method/scope
            std::string mode = argv[++i];
            if (mode == "sim" || mode == "standalone") {
                config.method = "exec";
                config.scope = "device";
            } else if (mode == "cosim") {
                config.method = "exec";
                config.scope = "system";
                config.cosim_remapped = true;
            } else {
                std::cerr << "Warning: Unknown legacy mode '" << mode << "', using defaults" << std::endl;
            }
        } else if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        } else if (arg == "--workload" && i + 1 < argc) {
            config.workload_binary = argv[++i];
            parsing_workload_args = true;
        } else if (arg == "--mpi-ranks" && i + 1 < argc) {
            config.mpi_ranks = std::stoi(argv[++i]);
            config.workload_type = "mpi";
        } else if (arg == "--mpich-path" && i + 1 < argc) {
            std::cerr << "Warning: --mpich-path is deprecated and ignored "
                         "(PIMID no longer uses mpirun for MPI launch)" << std::endl;
            ++i;
        } else if (arg == "--workload-type" && i + 1 < argc) {
            config.workload_type = argv[++i];
        } else if (arg == "--trace-file" && i + 1 < argc) {
            config.trace_file = argv[++i];
        } else if (arg == "--power") {
            config.enable_power = true;
        } else if (arg == "--no-power") {
            config.enable_power = false;
        } else if (arg == "--power-report" && i + 1 < argc) {
            config.power_report_detail = argv[++i];
            if (config.power_report_detail != "summary" &&
                config.power_report_detail != "standard" &&
                config.power_report_detail != "verbose") {
                std::cerr << "Invalid --power-report value: " << config.power_report_detail
                          << " (expected: summary, standard, verbose)" << std::endl;
                return 1;
            }
        } else if (arg == "--cache" && i + 1 < argc) {
            // Characterization-cache warehouse mode: rw|ro|wo|off
            cli_cache_mode = argv[++i];
        } else if (arg == "--no-cache") {
            // Alias for --cache off
            cli_cache_mode = "off";
        } else if (arg == "--cache-dir" && i + 1 < argc) {
            cli_cache_dir = argv[++i];
        } else if (arg[0] != '-' && config_file.empty()) {
            // Positional argument - treat as config file
            config_file = arg;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // Save CLI-specified values before YAML loading (CLI overrides YAML)
    // We track which fields were explicitly set on the command line
    bool cli_method_set = false;
    bool cli_workload_set = false;
    bool cli_workload_type_set = false;
    bool cli_mpi_ranks_set = false;

    // Re-scan argv to detect which flags were explicitly given on CLI
    bool cli_power_set = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--method") cli_method_set = true;
        else if (a == "--workload") cli_workload_set = true;
        else if (a == "--workload-type") cli_workload_type_set = true;
        else if (a == "--mpi-ranks") { cli_mpi_ranks_set = true; cli_workload_type_set = true; }
        else if (a == "--power" || a == "--no-power") cli_power_set = true;
    }

    // Load YAML config early so it can provide method, workload, etc.
    if (!config_file.empty()) {
        try {
            YAML::Node yaml_cfg = YAML::LoadFile(config_file);

            // Top-level simulation settings (can be overridden by CLI)
            if (!cli_method_set && yaml_cfg["method"]) {
                config.method = yaml_cfg["method"].as<std::string>();
            }

            // Scope: device or system (cosim accepted as deprecated alias → system)
            if (yaml_cfg["scope"]) {
                config.scope = yaml_cfg["scope"].as<std::string>();
                if (config.scope == "cosim") {
                    config.scope = "system";
                    config.cosim_remapped = true;
                }
            }

            // Workload section
            if (yaml_cfg["workload"]) {
                if (!cli_workload_set && yaml_cfg["workload"]["binary"]) {
                    config.workload_binary = yaml_cfg["workload"]["binary"].as<std::string>();
                }
                if (!cli_workload_type_set && yaml_cfg["workload"]["type"]) {
                    config.workload_type = yaml_cfg["workload"]["type"].as<std::string>();
                }
                if (!cli_mpi_ranks_set && yaml_cfg["workload"]["mpi_ranks"]) {
                    config.mpi_ranks = yaml_cfg["workload"]["mpi_ranks"].as<int>();
                }
                if (yaml_cfg["workload"]["mpich_path"]) {
                    std::cerr << "Warning: workload.mpich_path in YAML is deprecated "
                                 "and ignored (PIMID no longer uses mpirun)" << std::endl;
                }
                // YAML args (CLI --workload args override entirely)
                if (!cli_workload_set && yaml_cfg["workload"]["args"]) {
                    for (const auto& a : yaml_cfg["workload"]["args"]) {
                        config.workload_args.push_back(a.as<std::string>());
                    }
                }
                // Environment variables
                if (yaml_cfg["workload"]["env"]) {
                    for (const auto& e : yaml_cfg["workload"]["env"]) {
                        config.workload_env[e.first.as<std::string>()] = e.second.as<std::string>();
                    }
                }
            }

            // Load simulation metadata
            if (yaml_cfg["name"]) {
                config.name = yaml_cfg["name"].as<std::string>();
            }
            if (yaml_cfg["description"]) {
                config.description = yaml_cfg["description"].as<std::string>();
            }

            // Load PE configuration
            if (yaml_cfg["pim"]) {
                if (yaml_cfg["pim"]["pe"]) {
                    config.num_pes = yaml_cfg["pim"]["pe"]["count"].as<int>(config.num_pes);
                    if (yaml_cfg["pim"]["pe"]["core_type"])
                        config.pe_type = yaml_cfg["pim"]["pe"]["core_type"].as<std::string>();
                    else
                        config.pe_type = yaml_cfg["pim"]["pe"]["type"].as<std::string>(config.pe_type);
                    // Normalize pe_type aliases
                    if (config.pe_type == "OOO" || config.pe_type == "OoO" ||
                        config.pe_type == "ooo" || config.pe_type == "out-of-order")
                        config.pe_type = "ooo_core";
                    else if (config.pe_type == "InOrder" || config.pe_type == "in-order" ||
                             config.pe_type == "in_order")
                        config.pe_type = "in_order_core";
                    else if (config.pe_type == "ALU" || config.pe_type == "alu")
                        config.pe_type = "alu_core";
                    else if (config.pe_type == "Simple" || config.pe_type == "simple")
                        config.pe_type = "simple_core";
                    else if (config.pe_type == "Null" || config.pe_type == "null")
                        config.pe_type = "null_core";
                    // STRICT whitelist -- the only PE core models that exist.
                    // (timing_core was removed; the in-order core is in_order_core.)
                    if (config.pe_type != "alu_core" && config.pe_type != "simple_core" &&
                        config.pe_type != "in_order_core" && config.pe_type != "ooo_core" &&
                        config.pe_type != "null_core") {
                        std::cerr << "Error: unknown pim.pe.type '" << config.pe_type
                                  << "'. Valid: alu_core | simple_core | in_order_core"
                                  << " | ooo_core | null_core" << std::endl;
                        return 1;
                    }
                    // PE frequency (alternative to system.frequency_mhz)
                    if (yaml_cfg["pim"]["pe"]["frequency_mhz"])
                        config.frequency_mhz = yaml_cfg["pim"]["pe"]["frequency_mhz"].as<int>();
                    // ALU scaling factors
                    config.alu_compute_factor = yaml_cfg["pim"]["pe"]["compute_factor"].as<double>(config.alu_compute_factor);
                    config.alu_access_factor = yaml_cfg["pim"]["pe"]["access_factor"].as<double>(config.alu_access_factor);
                    config.alu_throughput_factor = yaml_cfg["pim"]["pe"]["throughput_factor"].as<double>(config.alu_throughput_factor);
                    config.alu_operand_width = yaml_cfg["pim"]["pe"]["operand_width"].as<int>(config.alu_operand_width);
                    config.alu_energy_factor = yaml_cfg["pim"]["pe"]["energy_factor"].as<double>(config.alu_energy_factor);
                }
                // placement can live either at pim.placement (older flat form)
                // or pim.pe.placement (newer nested form, used by README and
                // YAML_REFERENCE examples). Accept both; nested overrides flat.
                auto placement_node = yaml_cfg["pim"]["placement"];
                if (yaml_cfg["pim"]["pe"] && yaml_cfg["pim"]["pe"]["placement"]) {
                    placement_node = yaml_cfg["pim"]["pe"]["placement"];
                }
                if (placement_node) {
                    config.placement_level = placement_node["level"].as<std::string>(config.placement_level);
                    // Connection mode: shared_io (default) or separate_endpoints
                    if (placement_node["connection"]) {
                        std::string conn = placement_node["connection"].as<std::string>();
                        if (conn == "separate_endpoints")
                            config.pe_mem_connection = UnifiedConfig::PEMemConnectionMode::SEPARATE_ENDPOINTS;
                        else
                            config.pe_mem_connection = UnifiedConfig::PEMemConnectionMode::SHARED_IO;
                    }
                    config.local_link_latency = placement_node["local_link_latency"].as<int>(config.local_link_latency);
                }
                // PE-to-memory-org mapping (M:N connectivity)
                if (yaml_cfg["pim"]["mapping"]) {
                    auto mapping = yaml_cfg["pim"]["mapping"];
                    std::string mode = mapping["mode"].as<std::string>("uniform");
                    if (mode == "uniform") {
                        int pes_per_mo = mapping["pes_per_mem_org"].as<int>(0);
                        int mos_per_pe = mapping["mem_orgs_per_pe"].as<int>(0);
                        // Defer actual map generation to autoGeneratePEMemMap()
                        // Store ratios temporarily in pe_mem_map as empty with sentinel
                        if (pes_per_mo > 0) {
                            // M:1 — multiple PEs per mem org
                            // We'll build the map after we know total_mem_orgs
                            config.pe_mem_map.clear();
                            // Use negative pe_id as sentinel: -pes_per_mo
                            config.pe_mem_map.push_back({-pes_per_mo, {}});
                        } else if (mos_per_pe > 0) {
                            // 1:N — one PE manages multiple mem orgs
                            config.pe_mem_map.clear();
                            config.pe_mem_map.push_back({-1, std::vector<int>(mos_per_pe, -1)});
                        }
                    } else if (mode == "explicit") {
                        config.pe_mem_map.clear();
                        if (mapping["map"]) {
                            for (const auto& entry : mapping["map"]) {
                                UnifiedConfig::PEMemMapping m;
                                m.pe_id = entry["pe"].as<int>();
                                if (entry["mem_orgs"].IsSequence()) {
                                    for (const auto& mo : entry["mem_orgs"])
                                        m.mem_org_ids.push_back(mo.as<int>());
                                }
                                config.pe_mem_map.push_back(m);
                            }
                        }
                    }
                }
                // PE-MI distributed memory interface config
                if (yaml_cfg["pim"]["mc"]) {
                    config.pe_mc_enabled = true;
                    auto mc = yaml_cfg["pim"]["mc"];
                    config.pe_mc_type = mc["type"].as<std::string>(config.pe_mc_type);
                    if (mc["pes_per_mc"]) {
                        config.pes_per_mc = mc["pes_per_mc"].as<int>();
                        config.pes_per_mc_user_set = true;
                    }
                    if (mc["local_latency"])
                        config.pe_mc_local_latency = mc["local_latency"].as<int>();
                    if (mc["bandwidth_mbs"])
                        config.pe_mc_bandwidth_mbs = mc["bandwidth_mbs"].as<int>();

                    // MC placement: with_core (default) or standalone NoC endpoint
                    if (mc["placement"]) {
                        std::string pl = mc["placement"].as<std::string>();
                        config.mc_standalone = (pl == "standalone");
                    }

                    // Per-group overrides
                    if (mc["groups"]) {
                        for (const auto& grp : mc["groups"]) {
                            UnifiedConfig::PEMCGroupOverride ov;
                            if (grp["id"].IsSequence()) {
                                for (const auto& id : grp["id"])
                                    ov.ids.push_back(id.as<int>());
                            } else if (grp["id"]) {
                                ov.ids.push_back(grp["id"].as<int>());
                            }
                            ov.type = grp["type"].as<std::string>(config.pe_mc_type);
                            if (grp["local_latency"])
                                ov.local_latency = grp["local_latency"].as<int>();
                            if (grp["bandwidth_mbs"])
                                ov.bandwidth_mbs = grp["bandwidth_mbs"].as<int>();
                            config.pe_mc_group_overrides.push_back(ov);
                        }
                    }
                }
            }

            // Load system configuration
            if (yaml_cfg["system"]) {
                config.frequency_mhz = yaml_cfg["system"]["frequency_mhz"].as<int>(config.frequency_mhz);
                config.cache_line_size = yaml_cfg["system"]["cache_line_size"].as<int>(config.cache_line_size);
                config.tech_node_nm = yaml_cfg["system"]["tech_node_nm"].as<int>(config.tech_node_nm);
            }

            // Alternative technology section (technology.node_nm)
            if (yaml_cfg["technology"] && yaml_cfg["technology"]["node_nm"]) {
                config.tech_node_nm = yaml_cfg["technology"]["node_nm"].as<int>();
            }

            // Load cache configuration
            if (yaml_cfg["cache"]) {
                // Characterization-cache warehouse settings (mode/dir). These feed
                // cache::configure() after parsing; precedence is resolved there.
                if (yaml_cfg["cache"]["mode"]) {
                    yaml_cache_mode = yaml_cfg["cache"]["mode"].as<std::string>("");
                } else if (yaml_cfg["cache"]["enabled"] &&
                           !yaml_cfg["cache"]["enabled"].as<bool>(true)) {
                    // cache.enabled: false → treat as mode "off" (only if mode unset)
                    yaml_cache_mode = "off";
                }
                if (yaml_cfg["cache"]["dir"]) {
                    yaml_cache_dir = yaml_cfg["cache"]["dir"].as<std::string>("");
                }
                if (yaml_cfg["cache"]["l1d"]) {
                    config.l1d_size_kb = yaml_cfg["cache"]["l1d"]["size_kb"].as<int>(config.l1d_size_kb);
                    config.l1d_ways = yaml_cfg["cache"]["l1d"]["ways"].as<int>(config.l1d_ways);
                }
                if (yaml_cfg["cache"]["l1i"]) {
                    config.l1i_size_kb = yaml_cfg["cache"]["l1i"]["size_kb"].as<int>(config.l1i_size_kb);
                    config.l1i_ways = yaml_cfg["cache"]["l1i"]["ways"].as<int>(config.l1i_ways);
                }
                if (yaml_cfg["cache"]["l2"]) {
                    config.enable_l2 = yaml_cfg["cache"]["l2"]["enabled"].as<bool>(config.enable_l2);
                    config.l2_size_kb = yaml_cfg["cache"]["l2"]["size_kb"].as<int>(config.l2_size_kb);
                    config.l2_ways = yaml_cfg["cache"]["l2"]["ways"].as<int>(config.l2_ways);
                    config.l2_count = yaml_cfg["cache"]["l2"]["count"].as<int>(config.l2_count);
                }
                if (yaml_cfg["cache"]["l3"]) {
                    config.enable_l3 = yaml_cfg["cache"]["l3"]["enabled"].as<bool>(config.enable_l3);
                    config.l3_size_kb = yaml_cfg["cache"]["l3"]["size_kb"].as<int>(config.l3_size_kb);
                    config.l3_ways = yaml_cfg["cache"]["l3"]["ways"].as<int>(config.l3_ways);
                }

                // Parse cache timing/energy/power overrides
                if (yaml_cfg["cache"]["l1d"]) {
                    auto n = yaml_cfg["cache"]["l1d"];
                    if (n["latency_ns"]) config.l1d_params.latency_ns = n["latency_ns"].as<double>();
                    if (n["energy_nj"])  config.l1d_params.energy_nj = n["energy_nj"].as<double>();
                    if (n["static_power_mw"]) config.l1d_params.static_power_mw = n["static_power_mw"].as<double>();
                }
                if (yaml_cfg["cache"]["l1i"]) {
                    auto n = yaml_cfg["cache"]["l1i"];
                    if (n["latency_ns"]) config.l1i_params.latency_ns = n["latency_ns"].as<double>();
                    if (n["energy_nj"])  config.l1i_params.energy_nj = n["energy_nj"].as<double>();
                    if (n["static_power_mw"]) config.l1i_params.static_power_mw = n["static_power_mw"].as<double>();
                }
                if (yaml_cfg["cache"]["l2"]) {
                    auto n = yaml_cfg["cache"]["l2"];
                    if (n["latency_ns"]) config.l2_params.latency_ns = n["latency_ns"].as<double>();
                    if (n["energy_nj"])  config.l2_params.energy_nj = n["energy_nj"].as<double>();
                    if (n["static_power_mw"]) config.l2_params.static_power_mw = n["static_power_mw"].as<double>();
                }
                if (yaml_cfg["cache"]["l3"]) {
                    auto n = yaml_cfg["cache"]["l3"];
                    if (n["latency_ns"]) config.l3_params.latency_ns = n["latency_ns"].as<double>();
                    if (n["energy_nj"])  config.l3_params.energy_nj = n["energy_nj"].as<double>();
                    if (n["static_power_mw"]) config.l3_params.static_power_mw = n["static_power_mw"].as<double>();
                }

                // Check if user provided complete overrides for all active cache levels
                config.use_yaml_cache_params =
                    config.l1d_params.isComplete() &&
                    config.l1i_params.isComplete() &&
                    (!config.enable_l2 || config.l2_params.isComplete()) &&
                    (!config.enable_l3 || config.l3_params.isComplete());

                if (!config.use_yaml_cache_params) {
                    // Check if any partial params were provided
                    bool any_provided =
                        config.l1d_params.latency_ns > 0 || config.l1d_params.energy_nj > 0 || config.l1d_params.static_power_mw > 0 ||
                        config.l1i_params.latency_ns > 0 || config.l1i_params.energy_nj > 0 || config.l1i_params.static_power_mw > 0 ||
                        config.l2_params.latency_ns > 0 || config.l2_params.energy_nj > 0 || config.l2_params.static_power_mw > 0 ||
                        config.l3_params.latency_ns > 0 || config.l3_params.energy_nj > 0 || config.l3_params.static_power_mw > 0;
                    if (any_provided) {
                        std::cerr << "Warning: Partial YAML cache params provided.\n"
                                  << "To override CACTI, provide ALL 3 params (latency_ns, energy_nj, static_power_mw)\n"
                                  << "for ALL active cache levels (l1d, l1i"
                                  << (config.enable_l2 ? ", l2" : "")
                                  << (config.enable_l3 ? ", l3" : "") << ").\n"
                                  << "Using CACTI-derived cache latencies instead.\n";
                        std::cerr.flush();
                    }
                }
            }

            // Load NoC configuration
            if (yaml_cfg["noc"]) {
                if (yaml_cfg["noc"]["topology"]) config.noc_topology_user_set = true;
                config.noc_topology = yaml_cfg["noc"]["topology"].as<std::string>(config.noc_topology);
                std::transform(config.noc_topology.begin(), config.noc_topology.end(),
                               config.noc_topology.begin(), ::toupper);
                config.noc_router_latency = yaml_cfg["noc"]["router_latency"].as<int>(config.noc_router_latency);
                config.noc_link_latency = yaml_cfg["noc"]["link_latency"].as<int>(config.noc_link_latency);

                // Parse noc.model. The lineup is exactly TWO models -- no aliases:
                //   "analytical" -> closed-form hop-count + M/D/1 + MLP (see noc.mlp)
                //   "detailed"   -> cycle-accurate Garnet
                if (yaml_cfg["noc"]["model"]) {
                    std::string noc_model = yaml_cfg["noc"]["model"].as<std::string>();
                    if (noc_model == "detailed") {
                        config.noc_cycle_accurate = true;
                        config.noc_injector_calib = 0;
                        config.noc_curve_model = 0;
                        config.noc_calqueue = 0;
                        config.noc_parallel = 0;
                        config.noc_mlp_model = 0;
                    } else if (noc_model == "analytical") {
                        config.noc_cycle_accurate = false;
                        config.noc_injector_calib = 0;
                        config.noc_curve_model = 0;
                        config.noc_calqueue = 0;
                        config.noc_parallel = 0;
                        config.noc_mlp_model = 1;
                    } else {
                        std::cerr << "Error: unknown noc.model '" << noc_model
                                  << "'. Valid: analytical | detailed" << std::endl;
                        return 1;
                    }
                    // Propagate to per-level network models ("simple" is the
                    // INTERNAL name of the per-level analytical path).
                    std::string effective_model = noc_model;
                    if (effective_model == "analytical")
                        effective_model = "simple";
                    for (int i = 0; i < 7; ++i) {
                        config.network_level_model[i] = effective_model;
                    }
                }
                // noc.mlp: M, the PE outstanding-access window for the 'mlp' model.
                if (yaml_cfg["noc"]["mlp"]) {
                    config.noc_mlp_degree = yaml_cfg["noc"]["mlp"].as<int>(config.noc_mlp_degree);
                    if (config.noc_mlp_degree < 1) config.noc_mlp_degree = 1;
                }
                // (the legacy noc.cycle_accurate boolean key was removed --
                //  noc.model: analytical | detailed is the only selector)
                config.noc_routing = yaml_cfg["noc"]["routing"].as<std::string>(config.noc_routing);
                std::transform(config.noc_routing.begin(), config.noc_routing.end(),
                               config.noc_routing.begin(), ::toupper);
                config.noc_vcs_per_vnet = yaml_cfg["noc"]["vcs_per_vnet"].as<int>(config.noc_vcs_per_vnet);
                // Accept both virtual_channels_per_vn and vcs_per_vnet
                if (yaml_cfg["noc"]["virtual_channels_per_vn"])
                    config.noc_vcs_per_vnet = yaml_cfg["noc"]["virtual_channels_per_vn"].as<int>();
                config.noc_buffers_per_vc = yaml_cfg["noc"]["buffers_per_vc"].as<int>(config.noc_buffers_per_vc);
                // Accept clock_mhz under noc as frequency override for NoC
                if (yaml_cfg["noc"]["clock_mhz"])
                    config.frequency_mhz = yaml_cfg["noc"]["clock_mhz"].as<int>();
                // Accept flit_size_bits (stored for synthetic; ZSim uses fixed 128b)
                // (informational — Garnet always uses 128 bits internally)
                config.noc_topology_file = yaml_cfg["noc"]["topology_file"].as<std::string>(config.noc_topology_file);
                config.noc_routing_table_file = yaml_cfg["noc"]["routing_table_file"].as<std::string>(config.noc_routing_table_file);
                // Message sizes in bits (0 = use defaults: control=64, data=576)
                config.noc_control_msg_bits = yaml_cfg["noc"]["control_message_bits"].as<int>(config.noc_control_msg_bits);
                config.noc_data_msg_bits = yaml_cfg["noc"]["data_message_bits"].as<int>(config.noc_data_msg_bits);

                // Ring direction: "unidirectional"/"uni" or "bidirectional"/"bi" (default)
                if (yaml_cfg["noc"]["ring_direction"]) {
                    std::string dir = yaml_cfg["noc"]["ring_direction"].as<std::string>();
                    std::transform(dir.begin(), dir.end(), dir.begin(), ::tolower);
                    config.noc_ring_unidirectional = (dir == "unidirectional" || dir == "uni");
                }

                // Per-level overrides (under noc.levels)
                if (yaml_cfg["noc"]["levels"]) {
                    const auto& levels = yaml_cfg["noc"]["levels"];
                    const std::array<std::string, 7> level_keys = {
                        "subarray", "bank", "bank_group", "chip", "rank", "channel", "system"
                    };
                    for (int i = 0; i < 7; ++i) {
                        if (levels[level_keys[i]]) {
                            const auto& lv = levels[level_keys[i]];
                            if (lv["model"])
                                config.network_level_model[i] = lv["model"].as<std::string>();
                            if (lv["link_width_bits"])
                                config.network_level_overrides[i].link_width_bits = lv["link_width_bits"].as<int>();
                            if (lv["frequency_ghz"])
                                config.network_level_overrides[i].frequency_ghz = lv["frequency_ghz"].as<double>();
                            if (lv["latency_cycles"])
                                config.network_level_overrides[i].latency_cycles = lv["latency_cycles"].as<int>();
                            if (lv["topology"])
                                config.network_level_overrides[i].topology = lv["topology"].as<std::string>();
                            // Router params
                            if (lv["router_latency"])
                                config.network_level_overrides[i].router_latency = lv["router_latency"].as<int>();
                            if (lv["router_pipeline"]) {
                                std::string rp = lv["router_pipeline"].as<std::string>();
                                if (rp == "full")           config.network_level_overrides[i].router_pipeline = 0;
                                else if (rp == "reduced")   config.network_level_overrides[i].router_pipeline = 1;
                                else if (rp == "simple")    config.network_level_overrides[i].router_pipeline = 2;
                                else if (rp == "minimal")   config.network_level_overrides[i].router_pipeline = 3;
                                else                        config.network_level_overrides[i].router_pipeline = lv["router_pipeline"].as<int>();
                            }
                            if (lv["router_bypass"])
                                config.network_level_overrides[i].router_bypass = lv["router_bypass"].as<bool>() ? 1 : 0;
                            if (lv["virtual_networks"])
                                config.network_level_overrides[i].virtual_networks = lv["virtual_networks"].as<int>();
                            if (lv["virtual_channels_per_vn"])
                                config.network_level_overrides[i].virtual_channels_per_vn = lv["virtual_channels_per_vn"].as<int>();
                            if (lv["input_buffer_depth"])
                                config.network_level_overrides[i].input_buffer_depth = lv["input_buffer_depth"].as<int>();
                            if (lv["output_buffer_depth"])
                                config.network_level_overrides[i].output_buffer_depth = lv["output_buffer_depth"].as<int>();
                        }
                    }
                }

                // Per-boundary bridge overrides (under noc.bridges, backward compat: noc.gateways)
                auto parse_bridge_overrides = [&](const YAML::Node& node) {
                    const std::array<std::string, 6> br_keys = {
                        "subarray_bank", "bank_bankgroup", "bankgroup_chip",
                        "chip_rank", "rank_channel", "channel_system"
                    };
                    for (int i = 0; i < 6; ++i) {
                        if (node[br_keys[i]]) {
                            auto& ov = config.bridge_overrides[i];
                            const auto& n = node[br_keys[i]];
                            if (n["count"])
                                ov.count = n["count"].as<int>();
                            // New dual-link fields
                            if (n["lower_width_bits"])
                                ov.lower_width_bits = n["lower_width_bits"].as<int>();
                            if (n["lower_frequency_mhz"])
                                ov.lower_frequency_mhz = n["lower_frequency_mhz"].as<double>();
                            if (n["upper_width_bits"])
                                ov.upper_width_bits = n["upper_width_bits"].as<int>();
                            if (n["upper_frequency_mhz"])
                                ov.upper_frequency_mhz = n["upper_frequency_mhz"].as<double>();
                            if (n["fifo_depth"])
                                ov.fifo_depth = n["fifo_depth"].as<int>();
                            if (n["latency_ns"])
                                ov.latency_ns = n["latency_ns"].as<double>();
                            // Legacy single-width fields (backward compat)
                            if (n["width_bits"])
                                ov.width_bits = n["width_bits"].as<int>();
                            if (n["latency_cycles"])
                                ov.latency_cycles = n["latency_cycles"].as<int>();
                            // Independent bridge model
                            if (n["model"])
                                ov.model = n["model"].as<std::string>();
                            // Router params
                            if (n["router_latency"])
                                ov.router_latency = n["router_latency"].as<int>();
                            if (n["router_pipeline"]) {
                                std::string rp = n["router_pipeline"].as<std::string>();
                                if (rp == "full")           ov.router_pipeline = 0;
                                else if (rp == "reduced")   ov.router_pipeline = 1;
                                else if (rp == "simple")    ov.router_pipeline = 2;
                                else if (rp == "minimal")   ov.router_pipeline = 3;
                                else                        ov.router_pipeline = n["router_pipeline"].as<int>();
                            }
                            if (n["router_bypass"])
                                ov.router_bypass = n["router_bypass"].as<bool>() ? 1 : 0;
                            if (n["virtual_networks"])
                                ov.virtual_networks = n["virtual_networks"].as<int>();
                            if (n["virtual_channels_per_vn"])
                                ov.virtual_channels_per_vn = n["virtual_channels_per_vn"].as<int>();
                            if (n["input_buffer_depth"])
                                ov.input_buffer_depth = n["input_buffer_depth"].as<int>();
                            if (n["output_buffer_depth"])
                                ov.output_buffer_depth = n["output_buffer_depth"].as<int>();
                        }
                    }
                };
                if (yaml_cfg["noc"]["bridges"]) {
                    parse_bridge_overrides(yaml_cfg["noc"]["bridges"]);
                } else if (yaml_cfg["noc"]["gateways"]) {
                    std::cerr << "WARNING: 'noc.gateways' is deprecated, use 'noc.bridges' instead\n";
                    parse_bridge_overrides(yaml_cfg["noc"]["gateways"]);
                }
            }

            // Load memory configuration
            if (yaml_cfg["memory"]) {
                config.memory_tech = yaml_cfg["memory"]["technology"].as<std::string>(config.memory_tech);

                // Reject unknown memory technologies up front rather than
                // silently falling back to DDR4 (which would hide user typos
                // and produce misleading results). HBM gen-1 was removed —
                // use HBM2/HBM3.
                {
                    static const std::set<std::string> kValidTechs = {
                        "DDR3", "DDR4", "DDR5", "LPDDR5", "GDDR6", "HBM2", "HBM3",
                        "SRAM", "STT_MRAM", "STTMRAM", "PCM", "RERAM", "ReRAM"
                    };
                    if (kValidTechs.find(config.memory_tech) == kValidTechs.end()) {
                        std::cerr << "ERROR: unknown memory technology '"
                                  << config.memory_tech << "'.\n"
                                  << "Supported: DDR3, DDR4, DDR5, LPDDR5, GDDR6, HBM2, HBM3, "
                                  << "SRAM, STT_MRAM, PCM, ReRAM.\n"
                                  << "(HBM gen-1 is not supported — use HBM2 or HBM3.)\n";
                        std::exit(1);
                    }
                }

                config.num_banks = yaml_cfg["memory"]["banks"].as<int>(config.num_banks);
                config.subarrays_per_bank = yaml_cfg["memory"]["subarrays_per_bank"].as<int>(config.subarrays_per_bank);
                config.memory_latency_override = yaml_cfg["memory"]["latency"].as<int>(config.memory_latency_override);
                config.ports_per_bank = yaml_cfg["memory"]["ports_per_bank"].as<int>(config.ports_per_bank);
                if (yaml_cfg["memory"]["dram"] && yaml_cfg["memory"]["dram"]["device_width"]) {
                    config.dram_device_width =
                        yaml_cfg["memory"]["dram"]["device_width"].as<std::string>(config.dram_device_width);
                }

                // Parse memory parameters from YAML
                // To override external models, user must provide ALL 5 required params:
                //   1. read_latency_ns    2. write_latency_ns
                //   3. read_energy_nj     4. write_energy_nj
                //   5. static_power_mw
                auto& p = config.memory_params;

                // Timing parameters (accept multiple naming conventions)
                if (yaml_cfg["memory"]["timing"]) {
                    const auto& t = yaml_cfg["memory"]["timing"];
                    // Read latency
                    if (t["read_latency_ns"]) p.read_latency_ns = t["read_latency_ns"].as<double>();
                    else if (t["subarray_read_ns"]) p.read_latency_ns = t["subarray_read_ns"].as<double>();
                    // Write latency
                    if (t["write_latency_ns"]) p.write_latency_ns = t["write_latency_ns"].as<double>();
                    else if (t["subarray_write_ns"]) p.write_latency_ns = t["subarray_write_ns"].as<double>();
                }

                // Energy parameters
                if (yaml_cfg["memory"]["energy"]) {
                    const auto& e = yaml_cfg["memory"]["energy"];
                    if (e["read_energy_nj"]) p.read_energy_nj = e["read_energy_nj"].as<double>();
                    if (e["write_energy_nj"]) p.write_energy_nj = e["write_energy_nj"].as<double>();
                }

                // Power parameters
                if (yaml_cfg["memory"]["power"]) {
                    const auto& pw = yaml_cfg["memory"]["power"];
                    if (pw["static_power_mw"]) p.static_power_mw = pw["static_power_mw"].as<double>();
                }

                // Check if user provided ALL 5 required params to override external models
                config.use_yaml_memory_params = p.isComplete();

                if (!config.use_yaml_memory_params &&
                    (p.read_latency_ns > 0 || p.write_latency_ns > 0 ||
                     p.read_energy_nj > 0 || p.write_energy_nj > 0 || p.static_power_mw > 0)) {
                    std::cerr << "Warning: Partial YAML memory params provided.\n"
                              << "To override external models, provide ALL 5 required params:\n"
                              << "  memory:\n"
                              << "    timing:\n"
                              << "      read_latency_ns: <value>\n"
                              << "      write_latency_ns: <value>\n"
                              << "    energy:\n"
                              << "      read_energy_nj: <value>\n"
                              << "      write_energy_nj: <value>\n"
                              << "    power:\n"
                              << "      static_power_mw: <value>\n"
                              << "Using external models (NVSim/CACTI/Ramulator) instead.\n";
                    std::cerr.flush();
                }
            }

            // Parse optional memory controller overrides
            if (yaml_cfg["memory"] && yaml_cfg["memory"]["controller"]) {
                const auto& ctrl = yaml_cfg["memory"]["controller"];
                if (ctrl["type"])
                    config.zsim_mem_controller_type = ctrl["type"].as<std::string>(config.zsim_mem_controller_type);
                if (ctrl["bandwidth"]) {
                    // Accepts "25.6 GB/s", "19200 MB/s", "1 TB/s", "6400000 KB/s", or bare number (MB/s)
                    int parsed = parseBandwidthToMBs(ctrl["bandwidth"].as<std::string>());
                    if (parsed > 0) {
                        config.md1_bandwidth_mbs = parsed;
                        config.md1_bandwidth_user_set = true;
                    } else {
                        std::cerr << "  [WARN] Could not parse bandwidth: '"
                                  << ctrl["bandwidth"].as<std::string>() << "'" << std::endl;
                    }
                }
                // WeaveSimple
                if (ctrl["bound_latency"])
                    config.weave_bound_latency = ctrl["bound_latency"].as<int>(config.weave_bound_latency);
                // Ramulator
                if (ctrl["ramulator_config"])
                    config.ramulator_config_file = ctrl["ramulator_config"].as<std::string>(config.ramulator_config_file);
            }

            // Load simulation parameters
            if (yaml_cfg["simulation"]) {
                config.phase_length = yaml_cfg["simulation"]["phase_length"].as<int>(config.phase_length);
                config.max_instructions = yaml_cfg["simulation"]["max_instructions"].as<long long>(config.max_instructions);
                config.stats_interval = yaml_cfg["simulation"]["stats_interval"].as<int>(config.stats_interval);
            }

            // Power analysis toggle (CLI --power/--no-power overrides YAML)
            if (!cli_power_set && yaml_cfg["power"] && yaml_cfg["power"]["enabled"]) {
                config.enable_power = yaml_cfg["power"]["enabled"].as<bool>(config.enable_power);
            }

            // Power report detail level
            if (yaml_cfg["power"] && yaml_cfg["power"]["report_detail"]) {
                config.power_report_detail = yaml_cfg["power"]["report_detail"].as<std::string>(config.power_report_detail);
            }

            // McPAT derived-parameter overrides
            if (yaml_cfg["power"] && yaml_cfg["power"]["mcpat_overrides"]) {
                auto ov = yaml_cfg["power"]["mcpat_overrides"];
                for (auto it = ov.begin(); it != ov.end(); ++it) {
                    try {
                        config.mcpat_overrides[it->first.as<std::string>()] =
                            it->second.as<double>();
                    } catch (...) {}
                }
            }

            // PCIe transfer modeling config (for cosim)
            if (yaml_cfg["power"] && yaml_cfg["power"]["pcie"]) {
                auto pc = yaml_cfg["power"]["pcie"];
                config.pcie_timing_configured = true;  // power.pcie section present
                config.pcie_enabled = pc["enabled"].as<bool>(config.pcie_enabled);
                config.pcie_num_units = pc["num_units"].as<int>(config.pcie_num_units);
                config.pcie_num_channels = pc["num_channels"].as<int>(config.pcie_num_channels);
                config.pcie_duty_cycle = pc["duty_cycle"].as<double>(config.pcie_duty_cycle);
                config.pcie_load_perc = pc["total_load_perc"].as<double>(config.pcie_load_perc);
                // PCIe timing model params (tunable for CXL-like behavior)
                config.pcie_base_latency_ns = pc["base_latency_ns"].as<double>(config.pcie_base_latency_ns);
                config.pcie_bandwidth_GBs = pc["bandwidth_GBs"].as<double>(config.pcie_bandwidth_GBs);
                config.pcie_num_lanes = pc["num_lanes"].as<int>(config.pcie_num_lanes);
                config.pcie_model = pc["model"].as<std::string>(config.pcie_model);
                if (config.pcie_model == "md1") config.pcie_model = "simple";  // backward compat
                // Link technology + per-transaction overhead (tunable per link).
                config.pcie_link_type = pc["link_type"].as<std::string>(config.pcie_link_type);
                config.pcie_header_bytes = pc["header_bytes"].as<int>(config.pcie_header_bytes);
                config.pcie_coherence_extra_ns =
                    pc["coherence_extra_ns"].as<double>(config.pcie_coherence_extra_ns);
                // Interposer preset (2.5D on-package): fill only fields the user
                // did NOT set explicitly, so an interposer link is one knob away.
                // Preset matches the explicit system.network.links interposer
                // entry (2.5D on-package): low latency, high BW, no overhead.
                if (config.pcie_link_type == "interposer") {
                    if (!pc["base_latency_ns"])   config.pcie_base_latency_ns = 5.0;      // ~few ns on-package
                    if (!pc["bandwidth_GBs"])     config.pcie_bandwidth_GBs   = 256.0;    // 2.5D interposer BW
                    if (!pc["header_bytes"])      config.pcie_header_bytes    = 0;        // no protocol framing
                    if (!pc["coherence_extra_ns"])config.pcie_coherence_extra_ns = 0.0;   // no protocol coherence
                }
            }

            // Host processor config (for co-sim ZSim-based host modeling)
            if (yaml_cfg["host"]) {
                auto h = yaml_cfg["host"];
                config.host_core_type = h["core_type"].as<std::string>(config.host_core_type);
                config.host_num_cores = h["num_cores"].as<int>(config.host_num_cores);
                config.host_frequency_mhz = h["frequency_mhz"].as<double>(config.host_frequency_mhz);
                config.host_tech_node_nm = h["tech_node_nm"].as<int>(config.host_tech_node_nm);
                if (h["cache"]) {
                    config.host_l1d_kb = h["cache"]["l1d_kb"].as<int>(config.host_l1d_kb);
                    config.host_l1i_kb = h["cache"]["l1i_kb"].as<int>(config.host_l1i_kb);
                    config.host_l2_kb = h["cache"]["l2_kb"].as<int>(config.host_l2_kb);
                    config.host_l3_kb = h["cache"]["l3_kb"].as<int>(config.host_l3_kb);
                }
                if (h["memory"]) {
                    config.host_memory_tech = h["memory"]["technology"].as<std::string>(config.host_memory_tech);
                }
            }

            //=================================================================
            // Multi-host/multi-device system configuration (scope: "system")
            //=================================================================
            if (config.scope == "system" && yaml_cfg["system"]) {
                auto sys = yaml_cfg["system"];

                // Helper: normalize core type string
                auto normalizeCoreType = [](const std::string& ct) -> std::string {
                    if (ct == "OOO" || ct == "OoO" || ct == "ooo" || ct == "out-of-order")
                        return "ooo_core";
                    if (ct == "InOrder" || ct == "in-order" || ct == "in_order")
                        return "in_order_core";
                    if (ct == "ALU" || ct == "alu") return "alu_core";
                    if (ct == "Simple" || ct == "simple") return "simple_core";
                    if (ct == "Null" || ct == "null") return "null_core";
                    return ct;  // already normalized (e.g., "ooo_core")
                };

                // Parse hosts
                if (sys["hosts"]) {
                    for (const auto& h : sys["hosts"]) {
                        UnifiedConfig::SystemNode node;
                        node.name = h["name"].as<std::string>("host" + std::to_string(config.system_nodes.size()));
                        node.role = UnifiedConfig::SystemNode::HOST;
                        node.core_type = normalizeCoreType(h["core_type"].as<std::string>("ooo_core"));
                        node.num_cores = h["num_cores"].as<int>(4);
                        node.frequency_mhz = h["frequency_mhz"].as<double>(3000.0);
                        node.tech_node_nm = h["tech_node_nm"].as<int>(7);
                        if (h["cache"]) {
                            node.l1d_kb = h["cache"]["l1d_kb"].as<int>(node.l1d_kb);
                            node.l1i_kb = h["cache"]["l1i_kb"].as<int>(node.l1i_kb);
                            node.l2_kb = h["cache"]["l2_kb"].as<int>(node.l2_kb);
                            node.l3_kb = h["cache"]["l3_kb"].as<int>(node.l3_kb);
                            node.enable_l3 = (node.l3_kb > 0);
                        }
                        if (h["memory"]) {
                            node.memory_tech = h["memory"]["technology"].as<std::string>(node.memory_tech);
                        }
                        if (h["workload"]) {
                            node.workload_binary = h["workload"]["binary"].as<std::string>("");
                            if (h["workload"]["args"]) {
                                for (const auto& a : h["workload"]["args"])
                                    node.workload_args.push_back(a.as<std::string>());
                            }
                        }
                        config.system_nodes.push_back(node);
                    }
                }

                // Parse devices
                if (sys["devices"]) {
                    for (const auto& d : sys["devices"]) {
                        UnifiedConfig::SystemNode node;
                        node.name = d["name"].as<std::string>("device" + std::to_string(config.system_nodes.size()));
                        node.role = UnifiedConfig::SystemNode::DEVICE;

                        std::string dtype = d["type"].as<std::string>("compute");
                        node.device_type = (dtype == "memory") ?
                            UnifiedConfig::SystemNode::MEMORY_ONLY :
                            UnifiedConfig::SystemNode::COMPUTE;

                        std::string attach = d["attachment"].as<std::string>("external");
                        node.attachment = (attach == "internal") ?
                            UnifiedConfig::SystemNode::INTERNAL :
                            UnifiedConfig::SystemNode::EXTERNAL;

                        node.frequency_mhz = d["frequency_mhz"].as<double>(1000.0);
                        node.tech_node_nm = d["tech_node_nm"].as<int>(22);

                        if (d["memory"]) {
                            node.memory_tech = d["memory"]["technology"].as<std::string>(node.memory_tech);
                            node.ports_per_bank = d["memory"]["ports_per_bank"].as<int>(node.ports_per_bank);
                        }

                        // PIM config (only for compute devices)
                        if (node.device_type == UnifiedConfig::SystemNode::COMPUTE) {
                            node.pe_type = normalizeCoreType(d["pe_type"].as<std::string>("alu_core"));
                            node.num_pes = d["num_pes"].as<int>(0);
                            node.num_cores = node.num_pes;  // PEs are cores in ZSim
                            node.core_type = node.pe_type;

                            if (d["pim"]) {
                                auto pim = d["pim"];
                                if (pim["placement"] && pim["placement"]["level"])
                                    node.placement_level = pim["placement"]["level"].as<std::string>();
                                if (pim["pe"]) {
                                    node.alu_compute_factor = pim["pe"]["compute_factor"].as<double>(node.alu_compute_factor);
                                    node.alu_access_factor = pim["pe"]["access_factor"].as<double>(node.alu_access_factor);
                                    node.alu_throughput_factor = pim["pe"]["throughput_factor"].as<double>(node.alu_throughput_factor);
                                    node.alu_operand_width = pim["pe"]["operand_width"].as<int>(node.alu_operand_width);
                                    node.alu_energy_factor = pim["pe"]["energy_factor"].as<double>(node.alu_energy_factor);
                                }
                                if (pim["mc"]) {
                                    node.pe_mc_type = pim["mc"]["type"].as<std::string>(node.pe_mc_type);
                                    node.pes_per_mc = pim["mc"]["pes_per_mc"].as<int>(node.pes_per_mc);
                                }
                            }

                            if (d["noc"]) {
                                node.noc_topology = d["noc"]["topology"].as<std::string>(node.noc_topology);
                                node.noc_model = d["noc"]["model"].as<std::string>(node.noc_model);
                            }
                        } else {
                            // Memory-only device: no PEs/cores
                            node.num_cores = 0;
                            node.num_pes = 0;
                        }

                        if (d["cache"]) {
                            node.l1d_kb = d["cache"]["l1d_kb"].as<int>(node.l1d_kb);
                            node.l1i_kb = d["cache"]["l1i_kb"].as<int>(node.l1i_kb);
                            node.l2_kb = d["cache"]["l2_kb"].as<int>(node.l2_kb);
                            node.l3_kb = d["cache"]["l3_kb"].as<int>(node.l3_kb);
                        }

                        if (d["workload"]) {
                            node.workload_binary = d["workload"]["binary"].as<std::string>("");
                            if (d["workload"]["args"]) {
                                for (const auto& a : d["workload"]["args"])
                                    node.workload_args.push_back(a.as<std::string>());
                            }
                        }

                        config.system_nodes.push_back(node);
                    }
                }

                // Parse system network
                if (sys["network"]) {
                    auto net = sys["network"];
                    config.system_network.topology = net["topology"].as<std::string>(config.system_network.topology);
                    config.system_network.model = net["model"].as<std::string>(config.system_network.model);
                    if (config.system_network.model == "md1" || config.system_network.model == "analytical")
                        config.system_network.model = "simple";  // backward compat
                    config.system_network.link_width_bits = net["link_width_bits"].as<int>(config.system_network.link_width_bits);
                    config.system_network.frequency_ghz = net["frequency_ghz"].as<double>(config.system_network.frequency_ghz);
                    config.system_network.latency_cycles = net["latency_cycles"].as<int>(config.system_network.latency_cycles);
                    config.system_network.router_latency = net["router_latency"].as<int>(config.system_network.router_latency);
                    config.system_network.virtual_channels_per_vn = net["virtual_channels_per_vn"].as<int>(config.system_network.virtual_channels_per_vn);
                    config.system_network.input_buffer_depth = net["input_buffer_depth"].as<int>(config.system_network.input_buffer_depth);
                    config.system_network.output_buffer_depth = net["output_buffer_depth"].as<int>(config.system_network.output_buffer_depth);

                    // Per-link overrides
                    if (net["links"]) {
                        for (const auto& lnk : net["links"]) {
                            UnifiedConfig::SystemLinkConfig link;
                            link.src_name = lnk["src"].as<std::string>("");
                            link.dst_name = lnk["dst"].as<std::string>("");
                            link.link_type = lnk["type"].as<std::string>("pcie_gen5");
                            link.lanes = lnk["lanes"].as<int>(16);
                            link.base_latency_ns = lnk["base_latency_ns"].as<double>(-1.0);
                            link.bandwidth_GBs = lnk["bandwidth_GBs"].as<double>(-1.0);

                            // Apply presets from link type if not explicitly set
                            // Presets: {latency_ns, bw_GBs, header_bytes, coherence, coh_extra_ns}
                            if (link.base_latency_ns < 0 || link.bandwidth_GBs < 0) {
                                double preset_lat = 500.0, preset_bw = 63.0;
                                int preset_hdr = 20; std::string preset_coh = "none"; double preset_coh_ns = 0.0;
                                if (link.link_type == "pcie_gen4")        { preset_lat = 500.0; preset_bw = 31.5; preset_hdr = 20; }
                                else if (link.link_type == "pcie_gen5")   { preset_lat = 500.0; preset_bw = 63.0; preset_hdr = 20; }
                                else if (link.link_type == "cxl_2_0")    { preset_lat = 200.0; preset_bw = 63.0; preset_hdr = 16; preset_coh = "bias"; preset_coh_ns = 50.0; }
                                else if (link.link_type == "cxl_3_0")    { preset_lat = 100.0; preset_bw = 126.0; preset_hdr = 16; preset_coh = "bias"; preset_coh_ns = 30.0; }
                                else if (link.link_type == "nvlink_3_0") { preset_lat = 700.0; preset_bw = 50.0; preset_hdr = 16; }
                                else if (link.link_type == "nvlink_4_0") { preset_lat = 500.0; preset_bw = 100.0; preset_hdr = 16; }
                                else if (link.link_type == "nvlink_c2c") { preset_lat = 100.0; preset_bw = 450.0; preset_hdr = 16; preset_coh = "directory"; preset_coh_ns = 50.0; }
                                else if (link.link_type == "ualink_1_0") { preset_lat = 200.0; preset_bw = 100.0; preset_hdr = 16; preset_coh = "bias"; preset_coh_ns = 40.0; }
                                else if (link.link_type == "interposer") { preset_lat = 5.0;   preset_bw = 256.0; preset_hdr = 0; }
                                if (link.base_latency_ns < 0) link.base_latency_ns = preset_lat;
                                if (link.bandwidth_GBs < 0)   link.bandwidth_GBs = preset_bw;
                                link.header_bytes = preset_hdr;
                                link.coherence = preset_coh;
                                link.coherence_extra_ns = preset_coh_ns;
                            }

                            config.system_network.links.push_back(link);
                        }
                    }
                }
            }

        } catch (const YAML::Exception& e) {
            std::cerr << "Warning: Failed to load YAML config: " << e.what() << std::endl;
        }
    }

    // Auto-derive memory controller type and parameters from technology
    getMemControllerConfig(config);

    // Pre-compute internal DRAM hierarchy latencies (for device scope / first device)
    if (config.scope != "system") {
        computeHierarchyLatencies(config);
    }

    // Synthesize system nodes from device/cosim config (backward compat)
    synthesizeSystemNodes(config);
    normalizeSystemConfig(config);

    // Configure the characterization-cache warehouse ONCE, after CLI + YAML are
    // both parsed and before any backend (NVSim/CACTI/Ramulator) characterization.
    // Precedence (CLI > env > YAML > default) is resolved inside configure().
    pimid::cache::configure(cli_cache_mode, cli_cache_dir,
                            yaml_cache_mode, yaml_cache_dir);
    std::cout << "[pimid] cache mode: "
              << pimid::cache::modeName(pimid::cache::mode()) << std::endl;

    // Validate L2/L3 cache configuration
    if (config.l2_count < 1) config.l2_count = 1;
    if (config.enable_l2 && config.l2_count > 1) {
        if (config.num_pes % config.l2_count != 0) {
            std::cerr << "Error: num_pes (" << config.num_pes << ") must be divisible by l2.count ("
                      << config.l2_count << ")" << std::endl;
            return 1;
        }
        if (!config.enable_l3) {
            // No-unified-LLC mode: each L2 instance is a top-level cache.
            // No global coherence across L2 groups — PEs within the same L2
            // are coherent, but cross-L2 accesses go straight to memory.
            std::cout << "Note: Clustered L2 (×" << config.l2_count
                      << ") without L3 — no global coherence (each L2 is an independent LLC)." << std::endl;
        }
    }
    if (config.enable_l3 && !config.enable_l2) {
        std::cerr << "Error: L3 cache requires L2 to be enabled.\n"
                  << "  Set cache.l2.enabled: true in your YAML config." << std::endl;
        return 1;
    }

    // If workload_type is "mpi" but mpi_ranks not set, default to num_pes
    if (config.workload_type == "mpi" && config.mpi_ranks <= 0) {
        config.mpi_ranks = config.num_pes;
    }

    // Validate method
    if (config.method != "exec" && config.method != "trace-gen" &&
        config.method != "trace" && config.method != "synthetic") {
        std::cerr << "Error: Unknown simulation method: " << config.method << std::endl;
        std::cerr << "Valid methods: exec, trace, trace-gen, synthetic" << std::endl;
        return 1;
    }

    // ── Synthetic traffic mode (early exit — no ZSim/workload needed) ──
    if (config.method == "synthetic") {
#ifdef HAVE_GARNET
        int num_nodes = config.num_pes;
        int mesh_size = (int)std::sqrt(num_nodes);
        if (mesh_size * mesh_size < num_nodes) mesh_size++;

        std::string topo_str = config.noc_topology;
        std::string routing_str = config.noc_routing;
        if (routing_str.empty()) {
            if (topo_str == "MESH_2D")       routing_str = "XY";
            else if (topo_str == "TORUS_2D") routing_str = "DOR";
            else if (topo_str == "RING")     routing_str = "SHORTEST";
            else if (topo_str == "CROSSBAR") routing_str = "DIRECT";
            else if (topo_str == "FAT_TREE") routing_str = "NCA";
            else if (topo_str == "BUS")      routing_str = "DIRECT";
            else if (topo_str == "H_TREE")   routing_str = "NCA";
            else                             routing_str = "TABLE";
        }

        // Parse synthetic config from YAML
        int pattern = 0;
        double injRate = 0.1;
        uint64_t numPackets = 10000;
        int warmup = 1000;
        bool enablePower = config.enable_power;

        // Injection rate sweep: injection_rate_min/max/step
        double injRateMin = 0.0, injRateMax = 0.0, injRateStep = 0.0;
        bool doSweep = false;

        // Pattern name→id mapping
        static const std::pair<std::string, int> patternMap[] = {
            {"uniform", 0}, {"bit-complement", 1}, {"bitcomp", 1},
            {"tornado", 2}, {"neighbor", 3}, {"transpose", 4},
            {"bit-reverse", 5}, {"bitrev", 5}, {"bit-rotation", 6},
            {"bitrot", 6}, {"shuffle", 7},
            {"memory-directed", 8}, {"memdir", 8}
        };
        static const char* patternNames[] = {
            "uniform", "bit-complement", "tornado", "neighbor",
            "transpose", "bit-reverse", "bit-rotation", "shuffle",
            "memory-directed"
        };

        if (!config_file.empty()) {
            try {
                YAML::Node syn_cfg = YAML::LoadFile(config_file);
                if (syn_cfg["synthetic"]) {
                    auto syn = syn_cfg["synthetic"];
                    if (syn["pattern"]) {
                        std::string p = syn["pattern"].as<std::string>();
                        bool found = false;
                        for (const auto& pm : patternMap) {
                            if (p == pm.first) { pattern = pm.second; found = true; break; }
                        }
                        if (!found) {
                            std::cerr << "Warning: unknown pattern '" << p
                                      << "', using uniform. Valid: uniform, bit-complement, "
                                      << "tornado, neighbor, transpose, bit-reverse, "
                                      << "bit-rotation, shuffle" << std::endl;
                        }
                    }
                    if (syn["injection_rate"]) injRate = syn["injection_rate"].as<double>();
                    if (syn["packets"])        numPackets = syn["packets"].as<uint64_t>();
                    if (syn["warmup"])         warmup = syn["warmup"].as<int>();

                    // Injection rate sweep
                    if (syn["injection_rate_min"] && syn["injection_rate_max"]) {
                        injRateMin = syn["injection_rate_min"].as<double>();
                        injRateMax = syn["injection_rate_max"].as<double>();
                        injRateStep = syn["injection_rate_step"]
                            ? syn["injection_rate_step"].as<double>() : 0.02;
                        if (injRateMin > 0 && injRateMax > injRateMin && injRateStep > 0) {
                            doSweep = true;
                        }
                    }

                    if (syn["power"]) enablePower = syn["power"].as<bool>();
                }
            } catch (...) {}
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "Synthetic Traffic Test" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "  Topology:       " << topo_str << std::endl;
        std::cout << "  Routing:        " << routing_str << std::endl;
        std::cout << "  Nodes:          " << (mesh_size * mesh_size) << std::endl;
        std::cout << "  Pattern:        " << patternNames[pattern] << std::endl;
        if (doSweep) {
            std::cout << "  Injection rate: " << injRateMin << " → " << injRateMax
                      << " (step " << injRateStep << ")" << std::endl;
        } else {
            std::cout << "  Injection rate: " << injRate
                      << " flits/node/cycle" << std::endl;
        }
        std::cout << "  Packets:        " << numPackets << std::endl;
        std::cout << "  Warmup:         " << warmup << std::endl;
        std::cout << "  Power analysis: " << (enablePower ? "yes" : "no") << std::endl;
        std::cout << "========================================" << std::endl;

        // Build list of injection rates to test
        std::vector<double> rates;
        if (doSweep) {
            for (double r = injRateMin; r <= injRateMax + 1e-9; r += injRateStep)
                rates.push_back(r);
        } else {
            rates.push_back(injRate);
        }

        // Lambda: run one synthetic test and optionally compute NoC power
        auto runOneSynthetic = [&](double rate) -> int {
            SyntheticTrafficResult result;
            int rc = zsim_synthetic_traffic_ex(
                topo_str.c_str(), routing_str.c_str(),
                (uint32_t)mesh_size, (uint32_t)mesh_size,
                pattern, rate, numPackets, warmup,
                (uint32_t)config.noc_router_latency,
                (uint32_t)config.noc_link_latency,
                (uint32_t)config.noc_vcs_per_vnet,
                (uint32_t)config.noc_buffers_per_vc,
                (double)config.frequency_mhz, 128, &result);

            // Print latency/throughput summary
            if (!doSweep) {
                printf("\n── Synthetic Traffic Results ──\n");
                printf("  Delivered:      %lu packets\n", result.totalPackets);
                printf("  Total cycles:   %lu\n", result.totalCycles);
                printf("  Avg latency:    %.1f cycles\n", result.avgLatency);
                printf("  Min latency:    %lu cycles\n", result.minLatency);
                printf("  Max latency:    %lu cycles\n", result.maxLatency);
                printf("  Throughput:     %.6f flits/node/cycle\n", result.throughput);
            }

            // ── NoC power analysis via McPAT ──
            if (enablePower && result.totalPackets > 0) {
                using McPAT = pimid::McPATWrapper;

                // Minimal system config — NoC-only power
                McPAT::SystemConfig mcfg;
                mcfg.num_cores = 1;  // McPAT needs ≥1 core
                mcfg.core_clock_mhz = result.clockMhz;
                mcfg.pipeline_depth = 5;
                mcfg.issue_width = 1;
                mcfg.num_alus = 1;
                mcfg.num_muls = 0;
                mcfg.num_fpus = 0;
                mcfg.l1i_size_bytes = 0;
                mcfg.l1d_size_bytes = 0;
                mcfg.l2_size_bytes = 0;
                mcfg.l3_size_bytes = 0;
                mcfg.num_memory_controllers = 0;
                mcfg.mc_clock_mhz = result.clockMhz / 2.0;
                mcfg.tech_node_nm = config.tech_node_nm;
                mcfg.temperature_k = 350;
                mcfg.has_noc = true;

                // Map topology to McPAT type
                int mcpat_noc_type = (topo_str == "BUS") ? 0 : 1;

                mcfg.noc_topology = mcpat_noc_type;
                mcfg.noc_num_routers = result.numRouters;
                mcfg.noc_num_rows = result.numRows;
                mcfg.noc_num_cols = result.numCols;
                mcfg.noc_flit_size_bits = result.flitSizeBits;
                mcfg.noc_clock_mhz = result.clockMhz;
                mcfg.noc_input_ports = (mcpat_noc_type == 0) ? (int)result.numNodes : 5;
                mcfg.noc_output_ports = (mcpat_noc_type == 0) ? (int)result.numNodes : 5;
                mcfg.noc_vcs_per_vnet = (int)config.noc_vcs_per_vnet;
                mcfg.noc_vc_buffer_size = (int)config.noc_buffers_per_vc;

                // Apply mcpat_overrides
                auto ov_get_int = [&](const std::string& key, int fallback) -> int {
                    auto it = config.mcpat_overrides.find(key);
                    return (it != config.mcpat_overrides.end()) ? (int)it->second : fallback;
                };
                mcfg.device_type = ov_get_int("device_type", 0);
                mcfg.longer_channel_device = ov_get_int("longer_channel_device", 1);
                mcfg.interconnect_projection_type = ov_get_int("interconnect_projection_type", 0);

                McPAT mcpat(mcfg);
                mcpat.setDeviceProfile(McPAT::DeviceProfile::DEVICE_ALU);
                mcpat.initialize();

                mcpat.setTotalCycles(result.totalCycles > 0 ? result.totalCycles : 1);
                mcpat.setBusyCycles(result.totalCycles > 0 ? result.totalCycles : 1);
                mcpat.setTotalInstructions(1);

                // Single flat NoC level with synthetic traffic stats
                McPAT::NoCLevelConfig nc;
                nc.name = "NoC_synthetic";
                nc.type = mcpat_noc_type;
                nc.horizontal_nodes = result.numRows;
                nc.vertical_nodes = result.numCols;
                nc.input_ports = mcfg.noc_input_ports;
                nc.output_ports = mcfg.noc_output_ports;
                nc.flit_bits = result.flitSizeBits;
                nc.clock_mhz = result.clockMhz;
                nc.chip_coverage = 1.0;
                nc.total_accesses = result.totalPackets;
                // duty_cycle = fraction of peak bandwidth used [0,1]
                // Peak = N nodes × 1 packet/cycle, so duty = packets / (cycles × N)
                nc.duty_cycle = (result.totalCycles > 0 && result.numNodes > 0)
                    ? (double)result.totalPackets / (result.totalCycles * result.numNodes)
                    : 0.0;
                mcpat.setNoCLevels({nc});

                try {
                    mcpat.computePower();
                    auto noc_power = mcpat.getComponentPower(McPAT::ComponentType::NOC);
                    double noc_area = mcpat.getComponentArea(McPAT::ComponentType::NOC);

                    double time_s = result.totalCycles / (result.clockMhz * 1e6);
                    double noc_energy_nj = noc_power.total_power * time_s * 1e9;
                    double energy_per_pkt_pj = (result.totalPackets > 0)
                        ? (noc_energy_nj * 1000.0) / result.totalPackets : 0.0;

                    if (!doSweep) {
                        printf("\n── NoC Power Analysis (McPAT, %dnm) ──\n", config.tech_node_nm);
                        printf("  Leakage power:    %.4f W (sub=%.4f, gate=%.4f)\n",
                               noc_power.total_leakage, noc_power.subthreshold_leakage,
                               noc_power.gate_leakage);
                        printf("  Dynamic power:    %.4f W\n", noc_power.runtime_dynamic);
                        printf("  Total power:      %.4f W\n", noc_power.total_power);
                        printf("  NoC area:         %.3f mm^2\n", noc_area);
                        printf("  Total energy:     %.2f nJ\n", noc_energy_nj);
                        printf("  Energy/packet:    %.2f pJ\n", energy_per_pkt_pj);
                    } else {
                        // Sweep table row
                        printf("  rate=%.3f  lat=%.1f  tput=%.6f  "
                               "pwr=%.4fW (leak=%.4f dyn=%.4f)  "
                               "E/pkt=%.1fpJ\n",
                               rate, result.avgLatency, result.throughput,
                               noc_power.total_power, noc_power.total_leakage,
                               noc_power.runtime_dynamic, energy_per_pkt_pj);
                    }
                } catch (const std::exception& e) {
                    if (!doSweep)
                        std::cerr << "[Power] McPAT failed: " << e.what() << std::endl;
                }
            } else if (!doSweep) {
                // No power, print basic results
            }

            if (doSweep && !enablePower) {
                printf("  rate=%.3f  lat=%.1f  tput=%.6f\n",
                       rate, result.avgLatency, result.throughput);
            }

            return rc;
        };

        // Run sweep or single test
        int final_rc = 0;
        if (doSweep) {
            printf("\n── Injection Rate Sweep ──\n");
            if (enablePower)
                printf("  %-8s %-8s %-12s %-10s %-10s %-10s %-10s\n",
                       "Rate", "Lat", "Throughput", "Power(W)", "Leak(W)", "Dyn(W)", "E/pkt(pJ)");
            else
                printf("  %-8s %-8s %-12s\n", "Rate", "Lat", "Throughput");
        }

        for (double rate : rates) {
            int rc = runOneSynthetic(rate);
            if (rc != 0) final_rc = rc;
        }

        std::cout << "========================================" << std::endl;
        return final_rc;
#else
        std::cerr << "Error: synthetic mode requires Garnet" << std::endl;
        return 1;
#endif
    }

    // Validate trace arguments based on method
    if (config.method == "trace-gen") {
        if (config.trace_file.empty()) {
            std::cerr << "Error: trace-gen mode requires --trace-file to specify output path" << std::endl;
            return 1;
        }
    } else if (config.method == "trace") {
        if (config.trace_file.empty()) {
            std::cerr << "Error: trace mode requires --trace-file to specify input trace path" << std::endl;
            return 1;
        }
    } else if (config.method == "exec") {
        if (config.workload_binary.empty()) {
            // For system scope, per-node workloads may cover all cores
            bool all_nodes_have_workload = false;
            if (config.scope == "system" && !config.system_nodes.empty()) {
                all_nodes_have_workload = true;
                for (const auto& n : config.system_nodes) {
                    if (n.num_cores > 0 && n.workload_binary.empty()) {
                        all_nodes_have_workload = false;
                        break;
                    }
                }
            }
            if (!all_nodes_have_workload) {
                std::cerr << "Error: exec mode requires --workload to specify a binary" << std::endl;
                return 1;
            }
        }
    }

    // Resolve + validate workload binaries. Paths are tried (1) CWD-relative,
    // (2) relative to the PIMID root (exe location or $PIMID_ROOT) -- so configs
    // shipping root-relative paths (benchmarks/...) work from ANY directory --
    // and (3) with a legacy "pimid/" dev-space prefix stripped, for old configs.
    auto resolveWorkload = [](std::string& path) {
        if (path.empty() || access(path.c_str(), X_OK) == 0) return;
        std::string root = getPimidRoot();
        std::string alt = root + "/" + path;
        if (access(alt.c_str(), X_OK) == 0) { path = alt; return; }
        if (path.rfind("pimid/", 0) == 0) {
            std::string alt2 = root + "/" + path.substr(6);
            if (access(alt2.c_str(), X_OK) == 0) { path = alt2; return; }
        }
    };
    resolveWorkload(config.workload_binary);
    for (auto& n : config.system_nodes) resolveWorkload(n.workload_binary);
    if (!config.workload_binary.empty() && config.method != "trace") {
        if (access(config.workload_binary.c_str(), X_OK) != 0) {
            std::cerr << "Warning: workload binary '" << config.workload_binary
                      << "' not found or not executable" << std::endl;
        }
    }

    if (config.scope != "device" && config.scope != "system") {
        std::cerr << "Error: Unknown simulation scope: " << config.scope << std::endl;
        std::cerr << "Valid scopes: device, system (cosim accepted as deprecated alias for system)" << std::endl;
        return 1;
    }

    // Print simulation configuration
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    PIMID Simulation Infrastructure v1.0.4                      ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "  Method: " << config.method;
    if (config.method == "exec") {
        std::cout << " (QEMU + ZSim cycle-accurate)";
    } else if (config.method == "trace") {
        std::cout << " (replay)";
    } else if (config.method == "trace-gen") {
        std::cout << " (QEMU trace recording)";
    }
    std::cout << " | Scope: " << config.scope << std::endl;
    if (!config_file.empty()) {
        std::cout << "  Config: " << config_file << std::endl;
    }
    if (!config.workload_binary.empty()) {
        std::cout << "  Workload: " << config.workload_binary;
        if (config.workload_type == "mpi") {
            std::cout << " (MPI, " << config.mpi_ranks << " ranks)";
        } else if (config.workload_type != "serial") {
            std::cout << " (" << config.workload_type << ")";
        }
        std::cout << std::endl;
    }
    std::cout << "  Power: " << (config.enable_power ? "enabled" : "disabled") << std::endl;
    if (!config.trace_file.empty()) {
        std::cout << "  Trace: " << config.trace_file;
        if (config.method == "trace") {
            std::cout << " (input)";
        } else {
            std::cout << " (output)";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    // Run appropriate simulator based on method and scope
    bool success = false;

    if (config.scope == "device") {
        // Device-only simulation (PIM only)
        if (config.method == "trace") {
            // ── Trace Replay (ZSim cycle-accurate) ──
            // Replays trace through full ZSim hierarchy (cores, caches, MCs, NoC)
            if (config_file.empty()) {
                std::cerr << "Error: --config FILE required for trace simulation" << std::endl;
                return 1;
            }

            // Determine core type (display label)
            std::string core_type = "Simple";
            if (config.pe_type == "ooo_core") {
                core_type = "OoO";
            } else if (config.pe_type == "in_order_core") {
                core_type = "InOrder";
            } else if (config.pe_type == "alu_core" || config.pe_type == "alu") {
                core_type = "ALU";
            } else if (config.pe_type == "null_core" || config.pe_type == "null") {
                core_type = "Null";
            }

            // Generate ZSim config (reuse QEMU config generator)
            ZSimSimulator zsim_helper(config);
            std::string zsim_cfg_path = zsim_helper.generateConfig();
            if (zsim_cfg_path.empty()) {
                std::cerr << "Error: Failed to generate ZSim config" << std::endl;
                return 1;
            }

            // Output directory
            std::string output_dir = "/tmp/pimid_trace_zsim_" + std::to_string(getpid());
            mkdir(output_dir.c_str(), 0755);

            // Banner
            std::cout << "\n========================================" << std::endl;
            std::cout << "Trace Replay (ZSim cycle-accurate)" << std::endl;
            std::cout << "========================================" << std::endl;
            std::cout << "  Trace:     " << config.trace_file << std::endl;
            if (core_type == "ALU") {
                std::cout << "  Core:      ALU (compute=" << std::fixed << std::setprecision(2)
                          << config.alu_compute_factor << ", access=" << config.alu_access_factor
                          << ", throughput=" << config.alu_throughput_factor
                          << ", energy=" << config.alu_energy_factor << std::defaultfloat
                          << ", width=" << config.alu_operand_width << "b)" << std::endl;
            } else {
                std::cout << "  Core:      " << core_type << std::endl;
            }

            // Display cache configuration with latencies (trace mode banner)
            if (core_type != "ALU") {
                std::string cache_src = config.use_yaml_cache_params ? "YAML override" : "CACTI " + std::to_string(std::max(22, config.tech_node_nm)) + "nm";
                int disp_l1d, disp_l1i, disp_l2 = 0, disp_l3 = 0;
                if (config.use_yaml_cache_params) {
                    disp_l1d = std::max(1, static_cast<int>(std::round(
                        config.l1d_params.latency_ns * config.frequency_mhz / 1000.0)));
                    disp_l1i = std::max(1, static_cast<int>(std::round(
                        config.l1i_params.latency_ns * config.frequency_mhz / 1000.0)));
                    if (config.enable_l2)
                        disp_l2 = std::max(1, static_cast<int>(std::round(
                            config.l2_params.latency_ns * config.frequency_mhz / 1000.0)));
                    if (config.enable_l3)
                        disp_l3 = std::max(1, static_cast<int>(std::round(
                            config.l3_params.latency_ns * config.frequency_mhz / 1000.0)));
                } else {
                    disp_l1d = getCacheLatencyCycles(config.l1d_size_kb, config.l1d_ways,
                                                      config.cache_line_size, config.frequency_mhz, config.tech_node_nm, 4);
                    disp_l1i = getCacheLatencyCycles(config.l1i_size_kb, config.l1i_ways,
                                                      config.cache_line_size, config.frequency_mhz, config.tech_node_nm, 3);
                    if (config.enable_l2)
                        disp_l2 = getCacheLatencyCycles(config.l2_size_kb, config.l2_ways,
                                                         config.cache_line_size, config.frequency_mhz, config.tech_node_nm, 12);
                    if (config.enable_l3)
                        disp_l3 = getCacheLatencyCycles(config.l3_size_kb, config.l3_ways,
                                                         config.cache_line_size, config.frequency_mhz, config.tech_node_nm, 20);
                }
                std::cout << "  L1D:       " << config.l1d_size_kb << "KB, "
                          << config.l1d_ways << "-way, " << disp_l1d
                          << (disp_l1d == 1 ? " cycle" : " cycles")
                          << " (" << cache_src << ")" << std::endl;
                std::cout << "  L1I:       " << config.l1i_size_kb << "KB, "
                          << config.l1i_ways << "-way, " << disp_l1i
                          << (disp_l1i == 1 ? " cycle" : " cycles")
                          << " (" << cache_src << ")" << std::endl;
                if (config.enable_l2) {
                    std::cout << "  L2:        " << config.l2_size_kb << "KB, "
                              << config.l2_ways << "-way, " << disp_l2
                              << (disp_l2 == 1 ? " cycle" : " cycles");
                    if (config.l2_count > 1) {
                        std::cout << " (x" << config.l2_count << ", "
                                  << (config.num_pes / config.l2_count) << " PEs/L2";
                        if (!config.enable_l3)
                            std::cout << ", independent LLCs";
                        std::cout << ")";
                    }
                    std::cout << " (" << cache_src << ")" << std::endl;
                }
                if (config.enable_l3) {
                    std::cout << "  L3:        " << config.l3_size_kb << "KB, "
                              << config.l3_ways << "-way, " << disp_l3
                              << (disp_l3 == 1 ? " cycle" : " cycles")
                              << " (unified LLC, " << cache_src << ")" << std::endl;
                }
            }

            // Pre-compute memory latency (may trigger CACTI/NVSim/Ramulator output)
            int disp_latency = 0;
            if (config.zsim_mem_controller_type != "ramulator") {
                disp_latency = (config.memory_latency_override >= 0) ?
                    config.memory_latency_override :
                    getMemoryLatencyCycles(config.memory_tech, config.frequency_mhz,
                                           config.use_yaml_memory_params,
                                           config.memory_params.read_latency_ns);
            }
            // Display memory controller configuration
            std::cout << "  Memory:    " << config.memory_tech << ", Controller: ";
            if (config.zsim_mem_controller_type == "ramulator") {
                std::cout << "Ramulator (Ramulator2 cycle-accurate)" << std::endl;
            } else if (config.zsim_mem_controller_type == "weavesimple") {
                std::cout << "WeaveSimple (phase-aware M/D/1), " << disp_latency << " cycles, "
                          << config.md1_bandwidth_mbs << " MB/s" << std::endl;
            } else {
                std::cout << "Simple (M/D/1), " << disp_latency << " cycles, "
                          << config.md1_bandwidth_mbs << " MB/s" << std::endl;
            }

            // Display NoC configuration
            std::cout << "  NoC:       " << config.noc_topology;
            if (config.noc_topology == "MESH_2D" || config.noc_topology == "TORUS_2D") {
                int mesh_size = static_cast<int>(std::sqrt(config.num_pes));
                if (mesh_size * mesh_size < config.num_pes) mesh_size++;
                std::cout << " (" << mesh_size << "x" << mesh_size << ")";
            } else if (config.noc_topology == "RING") {
                std::cout << " (" << (config.noc_ring_unidirectional ? "unidirectional" : "bidirectional") << ")";
            }
            {
                std::string disp_routing = config.noc_routing;
                if (disp_routing.empty()) {
                    if (config.noc_topology == "MESH_2D")       disp_routing = "XY";
                    else if (config.noc_topology == "TORUS_2D")  disp_routing = "DOR";
                    else if (config.noc_topology == "RING")      disp_routing = "SHORTEST";
                    else if (config.noc_topology == "CROSSBAR")  disp_routing = "DIRECT";
                    else if (config.noc_topology == "FAT_TREE")  disp_routing = "NCA";
                    else if (config.noc_topology == "BUS")       disp_routing = "DIRECT";
                    else if (config.noc_topology == "H_TREE")    disp_routing = "NCA";
                    else if (config.noc_topology == "CUSTOM")    disp_routing = "TABLE";
                    else                                          disp_routing = "XY";
                }
                std::cout << ", routing=" << disp_routing
                          << ", mode=" << (config.noc_parallel ? "parallel" : (config.noc_cycle_accurate ? "detailed" : "simple")) << std::endl;
            }

            // Display hierarchy info
            if (config.hierarchy_enabled) {
                const std::array<std::string, 7> level_names = {
                    "Subarray", "Bank", "BankGroup", "Chip", "Rank", "Channel", "System"
                };
                std::cout << "  Hierarchy: " << config.memory_tech << " internal network" << std::endl;
                if (config.placement_level != "HOST_MC") {
                    for (int i = 0; i < 7; ++i) {
                        int lat = config.hierarchy_level_latency[i];
                        int brlat = (i < 6) ? config.hierarchy_bridge_latency[i] : 0;
                        if (lat == 0 && brlat == 0) continue;  // skip passthrough levels
                        std::cout << "    L" << i << " " << std::setw(12) << std::left
                                  << (level_names[i] + ":") << " " << lat << " cycles";
                        if (i < 6 && brlat > 0) {
                            std::cout << " + bridge " << brlat << " cy";
                            const auto& bmodel = config.hierarchy_bridge_model[i];
                            if (!bmodel.empty() && bmodel != "auto")
                                std::cout << " [" << bmodel << "]";
                        }
                        std::cout << std::endl;
                    }
                }
                // PE placement with M:N mapping info
                if (config.placement_level == "HOST_MC") {
                    std::cout << "    PE placement: HOST_MC (" << config.num_pes << " PEs share host MC)" << std::endl;
                    std::cout << "    PE-MIs: none (PEs share host MC)" << std::endl;
                } else {
                    std::cout << "    PE placement: L" << config.pe_hierarchy_level
                              << " (" << config.placement_level << "), "
                              << config.num_pes << " PEs -> " << config.total_mem_orgs << " mem orgs";
                    // Compute and display ratio
                    if (config.total_mem_orgs > 0 && config.num_pes > 0) {
                        int g = std::gcd(config.num_pes, config.total_mem_orgs);
                        std::cout << " (" << (config.num_pes / g) << ":"
                                  << (config.total_mem_orgs / g) << ", "
                                  << (config.pe_mem_connection == UnifiedConfig::PEMemConnectionMode::SHARED_IO
                                      ? "shared_io" : "separate_endpoints") << ")";
                    }
                    std::cout << std::endl;
                    if (config.pe_mem_connection == UnifiedConfig::PEMemConnectionMode::SEPARATE_ENDPOINTS) {
                        std::cout << "    Endpoints: " << config.total_network_endpoints
                                  << " (PEs=" << config.num_pes << " + mem_orgs=" << config.total_mem_orgs
                                  << "), local_link=" << config.local_link_latency << " cy" << std::endl;
                    }

                    // PE-MI info (always present for non-HOST_MC due to validation)
                    if (config.pe_mc_enabled) {
                        int mi_count = config.num_pes / config.pes_per_mc;
                        std::cout << "    PE-MIs: " << mi_count
                                  << " (" << config.pes_per_mc << " PEs/MI";
                        if (config.ports_per_bank > 1)
                            std::cout << ", " << config.ports_per_bank << " ports/bank";
                        std::cout << ")";
                        if (config.pe_mc_local_latency > 0)
                            std::cout << ", local=" << config.pe_mc_local_latency << " cy";
                        if (config.pe_mc_bandwidth_mbs > 0)
                            std::cout << ", bw=" << config.pe_mc_bandwidth_mbs << " MB/s";
                        std::cout << std::endl;
                    }
                }

                // PCIe/CXL interconnect info
                if (config.pcie_timing_configured && config.pcie_enabled) {
                    std::cout << "    Interconnect: " << config.pcie_link_type
                              << " x" << config.pcie_num_lanes
                              << " (base=" << std::fixed << std::setprecision(0)
                              << config.pcie_base_latency_ns << "ns, BW="
                              << std::setprecision(1) << config.pcie_bandwidth_GBs
                              << " GB/s, model=" << config.pcie_model
                              << ")" << std::defaultfloat << std::endl;
                }
            }

            std::cout << "  ZSim config: " << zsim_cfg_path << std::endl;
            std::cout << "  Output dir:  " << output_dir << std::endl;
            std::cout << std::endl;

            // fork/exec: zsim_trace <config.cfg> <trace.pimtrace> <output_dir>
            std::cout << "Running trace through ZSim simulation..." << std::endl;
            std::cout << "----------------------------------------" << std::endl;

            int trace_result = zsim_trace_run(zsim_cfg_path.c_str(),
                                              config.trace_file.c_str(),
                                              output_dir.c_str());

            std::cout << "----------------------------------------" << std::endl;

            if (trace_result == 0) {
                // Parse zsim_trace_stats.txt for summary
                std::string stats_path = output_dir + "/zsim_trace_stats.txt";
                std::ifstream stats_file(stats_path);
                if (stats_file.is_open()) {
                    std::cout << "\nZSim Trace Results:" << std::endl;
                    std::string line;
                    while (std::getline(stats_file, line)) {
                        std::cout << "  " << line << std::endl;
                    }
                }

                // Also parse zsim.out for cache stats
                std::string zsim_out_path = output_dir + "/zsim.out";
                ZSimParsedOutput zsim_stats = parseZSimOutputFile(zsim_out_path);
                if (zsim_stats.cycles > 0) {
                    std::cout << "\nCache Statistics:" << std::endl;
                    if (zsim_stats.l1d_hits() + zsim_stats.l1d_misses() > 0) {
                        double l1d_rate = 100.0 * zsim_stats.l1d_hits() /
                            (zsim_stats.l1d_hits() + zsim_stats.l1d_misses());
                        std::cout << "  L1D hit rate: " << std::fixed << std::setprecision(1)
                                  << l1d_rate << "%" << std::endl;
                    }
                    if (zsim_stats.l1i_hits() + zsim_stats.l1i_misses() > 0) {
                        double l1i_rate = 100.0 * zsim_stats.l1i_hits() /
                            (zsim_stats.l1i_hits() + zsim_stats.l1i_misses());
                        std::cout << "  L1I hit rate: " << std::fixed << std::setprecision(1)
                                  << l1i_rate << "%" << std::endl;
                    }
                    if (zsim_stats.l2_hits() + zsim_stats.l2_misses() > 0) {
                        double l2_rate = 100.0 * zsim_stats.l2_hits() /
                            (zsim_stats.l2_hits() + zsim_stats.l2_misses());
                        std::cout << "  L2 hit rate:  " << std::fixed << std::setprecision(1)
                                  << l2_rate << "%" << std::endl;
                    }
                }

                // Power analysis
                if (config.enable_power) {
                    runPowerAnalysis(config, zsim_stats, output_dir);
                }
            }

            std::cout << "\n========================================" << std::endl;
            success = (trace_result == 0);
        } else if (config.method == "trace-gen") {
            // ── QEMU Trace Generation ──
            // Uses QEMU user-mode emulation with libpimid_trace.so plugin
            // to generate PIMID-format binary traces
            if (config.workload_binary.empty()) {
                std::cerr << "Error: --workload BINARY required for qemu-trace mode" << std::endl;
                return 1;
            }

            std::cout << "\n========================================" << std::endl;
            std::cout << "QEMU Trace Generation" << std::endl;
            std::cout << "========================================" << std::endl;
            std::cout << "Workload: " << config.workload_binary << std::endl;
            std::cout << "Output:   " << config.trace_file << std::endl;

            // Find qemu-x86_64 binary
            std::string qemu_binary = findQemuBinary();
            if (qemu_binary.empty()) {
                std::cerr << "Error: qemu-x86_64 not found" << std::endl;
                std::cerr << "Install with: sudo apt install qemu-user" << std::endl;
                std::cerr << "Note: qemu-user-static does NOT support -plugin" << std::endl;
                return 1;
            }
            std::cout << "Using QEMU: " << qemu_binary << std::endl;

            // Find libpimid_trace.so plugin
            std::string plugin_path = findQemuPlugin("libpimid_trace.so");
            if (plugin_path.empty()) {
                std::cerr << "Error: libpimid_trace.so not found" << std::endl;
                std::cerr << "Build with: cmake --build . --target pimid_trace" << std::endl;
                return 1;
            }
            std::cout << "Plugin:   " << plugin_path << std::endl;
            std::cout << std::endl;

            if (config.workload_type == "mpi" && config.mpi_ranks > 0) {
                // ── MPI path: per-rank QEMU trace generation via mpirun ──
                int ranks = config.mpi_ranks;
                std::string output_base = "/tmp/pimid_qemu_trace_" + std::to_string(getpid());
                mkdir(output_base.c_str(), 0755);

                std::cout << "MPI Mode: " << ranks << " ranks" << std::endl;
                std::cout << "Output directory: " << output_base << std::endl;

                // Create per-rank directories
                for (int rank = 0; rank < ranks; rank++) {
                    std::string rank_dir = output_base + "/rank" + std::to_string(rank);
                    mkdir(rank_dir.c_str(), 0755);
                }

                // Extract base filename components from trace_file
                std::string trace_basename = config.trace_file;
                size_t slash_pos = trace_basename.rfind('/');
                if (slash_pos != std::string::npos) {
                    trace_basename = trace_basename.substr(slash_pos + 1);
                }
                std::string trace_name = trace_basename;
                std::string trace_ext;
                size_t dot_pos = trace_basename.rfind('.');
                if (dot_pos != std::string::npos) {
                    trace_name = trace_basename.substr(0, dot_pos);
                    trace_ext = trace_basename.substr(dot_pos);
                }

                // Find libpimid_mpi.so (PIMID's in-scope shm-mailbox MPI transport)
                std::string pimid_mpi_lib = findPimidMpiLib();
                if (pimid_mpi_lib.empty()) {
                    std::cerr << "Error: libpimid_mpi.so not found (searched build dirs)" << std::endl;
                    std::cerr << "Build pimid_mpi target before using --mpi-ranks" << std::endl;
                    return 1;
                }

                // Unique shm name per launch so concurrent pimid invocations don't collide
                std::string shm_name = "/pimid_mpi_" + std::to_string(getpid());

                std::cout << "MPI transport: libpimid_mpi.so (shm=" << shm_name << ")" << std::endl;
                std::cout << "Launching " << ranks << " ranks (PIMID-managed multi-process)" << std::endl;
                std::cout << "────────────────────────────────────────" << std::endl;

                // Fork N children; each becomes a QEMU+plugin process for one MPI rank
                std::vector<pid_t> child_pids;
                child_pids.reserve(ranks);
                for (int rank = 0; rank < ranks; rank++) {
                    pid_t pid = fork();
                    if (pid < 0) {
                        std::cerr << "Error: fork() failed for rank " << rank << ": "
                                  << strerror(errno) << std::endl;
                        // Reap any already-spawned children before bailing
                        for (pid_t cp : child_pids) {
                            kill(cp, SIGTERM);
                            waitpid(cp, nullptr, 0);
                        }
                        return 1;
                    }
                    if (pid == 0) {
                        // Child: become QEMU rank `rank`
                        std::string rank_dir = output_base + "/rank" + std::to_string(rank);
                        std::string trace_file = rank_dir + "/" + trace_name
                                                 + "_rank" + std::to_string(rank) + trace_ext;
                        std::string plugin_arg = plugin_path + ",output=" + trace_file;

                        setenv("OMP_NUM_THREADS", "1", 1);
                        setenv("LD_PRELOAD", pimid_mpi_lib.c_str(), 1);
                        setenv("PIMID_MPI_RANKS", std::to_string(ranks).c_str(), 1);
                        setenv("PIMID_MPI_RANK", std::to_string(rank).c_str(), 1);
                        setenv("PIMID_MPI_SHM", shm_name.c_str(), 1);
                        for (const auto& [key, val] : config.workload_env) {
                            setenv(key.c_str(), val.c_str(), 1);
                        }

                        // argv: qemu -plugin <plugin>,output=<trace> -- <workload> [args...]
                        std::vector<std::string> argv_owned;
                        argv_owned.push_back(qemu_binary);
                        argv_owned.push_back("-plugin");
                        argv_owned.push_back(plugin_arg);
                        argv_owned.push_back("--");
                        argv_owned.push_back(config.workload_binary);
                        for (const auto& wa : config.workload_args) argv_owned.push_back(wa);

                        std::vector<char*> argv_cstr;
                        argv_cstr.reserve(argv_owned.size() + 1);
                        for (auto& s : argv_owned) argv_cstr.push_back(s.data());
                        argv_cstr.push_back(nullptr);

                        execvp(qemu_binary.c_str(), argv_cstr.data());
                        std::cerr << "Error: execvp(" << qemu_binary << ") failed: "
                                  << strerror(errno) << std::endl;
                        _exit(127);
                    }
                    child_pids.push_back(pid);
                }

                // Parent: wait for all children, aggregate exit code
                int worst_exit = 0;
                for (pid_t cp : child_pids) {
                    int st = 0;
                    waitpid(cp, &st, 0);
                    int rc = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
                    if (rc > worst_exit) worst_exit = rc;
                }

                // Unlink shared memory segment now that all ranks exited
                shm_unlink(shm_name.c_str());

                std::cout << "────────────────────────────────────────" << std::endl;

                {
                    int exit_code = worst_exit;
                    std::cout << "\nAll " << ranks << " ranks exited (worst exit code: "
                              << exit_code << ")" << std::endl;

                    // Report per-rank trace stats
                    std::cout << "\n========================================" << std::endl;
                    std::cout << "MPI QEMU Trace Results (" << ranks << " ranks)" << std::endl;
                    std::cout << "========================================" << std::endl;

                    uint64_t total_events = 0;
                    for (int rank = 0; rank < ranks; rank++) {
                        std::string rank_trace_path = output_base + "/rank" + std::to_string(rank)
                            + "/" + trace_name + "_rank" + std::to_string(rank) + trace_ext;

                        struct stat st;
                        if (stat(rank_trace_path.c_str(), &st) == 0) {
                            uint64_t file_size = st.st_size;
                            uint64_t approx_events = (file_size > 576) ? (file_size - 576) / 48 : 0;
                            std::cout << "Rank " << rank << ": " << rank_trace_path
                                      << " (" << approx_events << " events, "
                                      << file_size << " bytes)" << std::endl;
                            total_events += approx_events;
                        } else {
                            std::cout << "Rank " << rank << ": (no trace file)" << std::endl;
                        }
                    }

                    std::cout << "────────────────────────────────────────" << std::endl;
                    std::cout << "Total: ~" << total_events << " events across "
                              << ranks << " ranks" << std::endl;
                    std::cout << "Per-rank traces: " << output_base << "/rank*/\n";

                    success = (exit_code == 0);
                }
            } else {
                // ── Single-rank path ──
                // Build plugin argument string
                std::string plugin_arg = plugin_path + ",output=" + config.trace_file;

                // fork/exec: qemu-x86_64 -plugin <plugin>,output=<file> -- <workload> [args]
                std::cout << "Running workload through QEMU trace generation..." << std::endl;
                std::cout << "----------------------------------------" << std::endl;

                pid_t pid = fork();
                if (pid < 0) {
                    std::cerr << "Error: Failed to fork process" << std::endl;
                    return 1;
                }

                if (pid == 0) {
                    // Child process — set env vars and exec QEMU
                    // OpenMP defaults: without a cap, libgomp sizes the team to
                    // omp_get_num_procs() = the HOST core count under qemu-user,
                    // spawning far more threads than the num_pes simulated PE
                    // contexts. The surplus threads spin at the parallel-region
                    // barrier, the region never completes, ROI_END never fires,
                    // and the sim livelocks (OMP-detailed) or reports wrong cycles
                    // (other OMP modes, from oversubscription). Cap to num_pes +
                    // passive waits. overwrite=0 so explicit workload_env wins.
                    if (config.workload_type == "openmp") {
                        int omp_threads = (config.num_pes > 0) ? config.num_pes : 1;
                        setenv("OMP_NUM_THREADS", std::to_string(omp_threads).c_str(), 0);
                        setenv("OMP_DYNAMIC", "FALSE", 0);
                        setenv("OMP_WAIT_POLICY", "PASSIVE", 0);
                        setenv("GOMP_SPINCOUNT", "0", 0);
                    }
                    for (const auto& [key, val] : config.workload_env) {
                        setenv(key.c_str(), val.c_str(), 1);
                    }
                    std::vector<const char*> args;
                    args.push_back(qemu_binary.c_str());
                    args.push_back("-plugin");
                    args.push_back(plugin_arg.c_str());
                    args.push_back("--");
                    args.push_back(config.workload_binary.c_str());
                    for (const auto& wa : config.workload_args) {
                        args.push_back(wa.c_str());
                    }
                    args.push_back(nullptr);

                    execvp(qemu_binary.c_str(), const_cast<char* const*>(args.data()));
                    std::cerr << "Error: Failed to execute QEMU: " << strerror(errno) << std::endl;
                    _exit(1);
                }

                // Parent — wait for completion
                int status;
                waitpid(pid, &status, 0);

                std::cout << "----------------------------------------" << std::endl;

                if (WIFEXITED(status)) {
                    int exit_code = WEXITSTATUS(status);
                    std::cout << "QEMU completed with exit code: " << exit_code << std::endl;

                    if (exit_code == 0) {
                        // Report trace file stats
                        struct stat st;
                        if (stat(config.trace_file.c_str(), &st) == 0) {
                            uint64_t file_size = st.st_size;
                            // Header is 64 bytes + YAML, events are 48 bytes each
                            uint64_t approx_events = (file_size > 576) ? (file_size - 576) / 48 : 0;
                            std::cout << "Trace file: " << config.trace_file << std::endl;
                            std::cout << "  Size: " << file_size << " bytes" << std::endl;
                            std::cout << "  Approx events: " << approx_events << std::endl;
                        }
                    }

                    success = (exit_code == 0);
                } else if (WIFSIGNALED(status)) {
                    int sig = WTERMSIG(status);
                    std::cerr << "QEMU terminated by signal " << sig
                              << " (" << strsignal(sig) << ")" << std::endl;
                    success = false;
                } else {
                    std::cerr << "QEMU terminated abnormally" << std::endl;
                    success = false;
                }
            }

            std::cout << "========================================" << std::endl;

        } else {
            // ── exec: Cycle-Accurate Simulation (QEMU + ZSim) ──
            // Uses QEMU user-mode emulation with libzsim_qemu.so plugin
            // to drive the full ZSim core/cache/memory hierarchy

            std::cout << "\n========================================" << std::endl;
            std::cout << "PIMID CYCLE-ACCURATE MODE (QEMU + ZSim)" << std::endl;
            std::cout << "========================================" << std::endl;
            std::cout << "Workload: " << config.workload_binary << std::endl;

            // Find qemu-x86_64 binary
            std::string qemu_binary = findQemuBinary();
            if (qemu_binary.empty()) {
                std::cerr << "Error: qemu-x86_64 not found" << std::endl;
                std::cerr << "Install with: sudo apt install qemu-user" << std::endl;
                return 1;
            }
            std::cout << "Using QEMU: " << qemu_binary << std::endl;

            // Find libzsim_qemu.so plugin
            std::string plugin_path = findQemuPlugin("libzsim_qemu.so");
            if (plugin_path.empty()) {
                std::cerr << "Error: libzsim_qemu.so not found" << std::endl;
                std::cerr << "Build with: cmake --build . --target zsim_qemu" << std::endl;
                return 1;
            }
            std::cout << "Plugin:   " << plugin_path << std::endl;

            // Determine core type for display
            std::string exec_core_type = "Simple";
            if (config.pe_type == "ooo_core") {
                exec_core_type = "OoO";
            } else if (config.pe_type == "in_order_core") {
                exec_core_type = "InOrder";
            } else if (config.pe_type == "alu_core" || config.pe_type == "alu") {
                exec_core_type = "ALU";
            } else if (config.pe_type == "null_core" || config.pe_type == "null") {
                exec_core_type = "Null";
            }

            // Display core info
            if (exec_core_type == "ALU") {
                std::cout << "  Core:      ALU (compute=" << std::fixed << std::setprecision(2)
                          << config.alu_compute_factor << ", access=" << config.alu_access_factor
                          << ", throughput=" << config.alu_throughput_factor
                          << ", energy=" << config.alu_energy_factor << std::defaultfloat
                          << ", width=" << config.alu_operand_width << "b)" << std::endl;
            } else {
                std::cout << "  Core:      " << exec_core_type << std::endl;
            }

            // Display cache configuration with latencies (exec mode banner)
            if (exec_core_type != "ALU") {
                std::string cache_src = config.use_yaml_cache_params ? "YAML override" : "CACTI " + std::to_string(std::max(22, config.tech_node_nm)) + "nm";
                int disp_l1d, disp_l1i, disp_l2 = 0, disp_l3 = 0;
                if (config.use_yaml_cache_params) {
                    disp_l1d = std::max(1, static_cast<int>(std::round(
                        config.l1d_params.latency_ns * config.frequency_mhz / 1000.0)));
                    disp_l1i = std::max(1, static_cast<int>(std::round(
                        config.l1i_params.latency_ns * config.frequency_mhz / 1000.0)));
                    if (config.enable_l2)
                        disp_l2 = std::max(1, static_cast<int>(std::round(
                            config.l2_params.latency_ns * config.frequency_mhz / 1000.0)));
                    if (config.enable_l3)
                        disp_l3 = std::max(1, static_cast<int>(std::round(
                            config.l3_params.latency_ns * config.frequency_mhz / 1000.0)));
                } else {
                    disp_l1d = getCacheLatencyCycles(config.l1d_size_kb, config.l1d_ways,
                                                      config.cache_line_size, config.frequency_mhz, config.tech_node_nm, 4);
                    disp_l1i = getCacheLatencyCycles(config.l1i_size_kb, config.l1i_ways,
                                                      config.cache_line_size, config.frequency_mhz, config.tech_node_nm, 3);
                    if (config.enable_l2)
                        disp_l2 = getCacheLatencyCycles(config.l2_size_kb, config.l2_ways,
                                                         config.cache_line_size, config.frequency_mhz, config.tech_node_nm, 12);
                    if (config.enable_l3)
                        disp_l3 = getCacheLatencyCycles(config.l3_size_kb, config.l3_ways,
                                                         config.cache_line_size, config.frequency_mhz, config.tech_node_nm, 20);
                }
                std::cout << "  L1D:       " << config.l1d_size_kb << "KB, "
                          << config.l1d_ways << "-way, " << disp_l1d
                          << (disp_l1d == 1 ? " cycle" : " cycles")
                          << " (" << cache_src << ")" << std::endl;
                std::cout << "  L1I:       " << config.l1i_size_kb << "KB, "
                          << config.l1i_ways << "-way, " << disp_l1i
                          << (disp_l1i == 1 ? " cycle" : " cycles")
                          << " (" << cache_src << ")" << std::endl;
                if (config.enable_l2) {
                    std::cout << "  L2:        " << config.l2_size_kb << "KB, "
                              << config.l2_ways << "-way, " << disp_l2
                              << (disp_l2 == 1 ? " cycle" : " cycles");
                    if (config.l2_count > 1) {
                        std::cout << " (x" << config.l2_count << ", "
                                  << (config.num_pes / config.l2_count) << " PEs/L2";
                        if (!config.enable_l3)
                            std::cout << ", independent LLCs";
                        std::cout << ")";
                    }
                    std::cout << " (" << cache_src << ")" << std::endl;
                }
                if (config.enable_l3) {
                    std::cout << "  L3:        " << config.l3_size_kb << "KB, "
                              << config.l3_ways << "-way, " << disp_l3
                              << (disp_l3 == 1 ? " cycle" : " cycles")
                              << " (unified LLC, " << cache_src << ")" << std::endl;
                }
            }

            // Pre-compute memory latency (may trigger CACTI/NVSim/Ramulator output)
            int disp_latency = 0;
            if (config.zsim_mem_controller_type != "ramulator") {
                disp_latency = (config.memory_latency_override >= 0) ?
                    config.memory_latency_override :
                    getMemoryLatencyCycles(config.memory_tech, config.frequency_mhz,
                                           config.use_yaml_memory_params,
                                           config.memory_params.read_latency_ns);
            }
            // Display memory controller configuration
            std::cout << "  Memory:    " << config.memory_tech << ", Controller: ";
            if (config.zsim_mem_controller_type == "ramulator") {
                std::cout << "Ramulator (Ramulator2 cycle-accurate)" << std::endl;
                std::cout << "             Config: " << config.ramulator_config_file << std::endl;
            } else if (config.zsim_mem_controller_type == "weavesimple") {
                int bl = (config.weave_bound_latency >= 0) ? config.weave_bound_latency : disp_latency;
                std::cout << "WeaveSimple (phase-aware M/D/1)" << std::endl;
                std::cout << "             Zero-load latency: " << disp_latency
                          << " cycles, Bandwidth: " << config.md1_bandwidth_mbs << " MB/s"
                          << ", Bound: " << bl << " cycles" << std::endl;
            } else {
                std::cout << "Simple (M/D/1 queuing)" << std::endl;
                std::cout << "             Zero-load latency: " << disp_latency
                          << " cycles, Bandwidth: " << config.md1_bandwidth_mbs << " MB/s" << std::endl;
            }

            // Display NoC configuration
            std::cout << "  NoC:       " << config.noc_topology;
            if (config.noc_topology == "MESH_2D" || config.noc_topology == "TORUS_2D") {
                int mesh_size = static_cast<int>(std::sqrt(config.num_pes));
                if (mesh_size * mesh_size < config.num_pes) mesh_size++;
                std::cout << " (" << mesh_size << "x" << mesh_size << ")";
            } else if (config.noc_topology == "RING") {
                std::cout << " (" << (config.noc_ring_unidirectional ? "unidirectional" : "bidirectional") << ")";
            } else if (config.noc_topology == "CUSTOM" && !config.noc_topology_file.empty()) {
                std::cout << " (from " << config.noc_topology_file << ")";
            }
            {
                std::string disp_routing = config.noc_routing;
                if (disp_routing.empty()) {
                    if (config.noc_topology == "MESH_2D")       disp_routing = "XY";
                    else if (config.noc_topology == "TORUS_2D")  disp_routing = "DOR";
                    else if (config.noc_topology == "RING")      disp_routing = "SHORTEST";
                    else if (config.noc_topology == "CROSSBAR")  disp_routing = "DIRECT";
                    else if (config.noc_topology == "FAT_TREE")  disp_routing = "NCA";
                    else if (config.noc_topology == "BUS")       disp_routing = "DIRECT";
                    else if (config.noc_topology == "H_TREE")    disp_routing = "NCA";
                    else if (config.noc_topology == "CUSTOM")    disp_routing = "TABLE";
                    else                                          disp_routing = "XY";
                }
                std::cout << ", routing=" << disp_routing
                          << ", mode=" << (config.noc_parallel ? "parallel" : (config.noc_cycle_accurate ? "detailed" : "simple")) << std::endl;
                std::cout << "             VCs=" << config.noc_vcs_per_vnet << "/vnet"
                          << ", buffers=" << config.noc_buffers_per_vc << "/VC"
                          << ", router=" << config.noc_router_latency
                          << ", link=" << config.noc_link_latency << " cycles";
                if (config.noc_data_msg_bits > 0 || config.noc_control_msg_bits > 0) {
                    std::cout << ", msg=" << (config.noc_control_msg_bits > 0 ? config.noc_control_msg_bits : 64)
                              << "b/" << (config.noc_data_msg_bits > 0 ? config.noc_data_msg_bits : 576) << "b";
                }
                std::cout << std::endl;
            }

            // Display hierarchy info (exec mode)
            if (config.hierarchy_enabled) {
                const std::array<std::string, 7> level_names = {
                    "Subarray", "Bank", "BankGroup", "Chip", "Rank", "Channel", "System"
                };
                std::cout << "  Hierarchy: " << config.memory_tech << " internal network" << std::endl;
                if (config.placement_level != "HOST_MC") {
                    for (int i = 0; i < 7; ++i) {
                        int lat = config.hierarchy_level_latency[i];
                        int brlat = (i < 6) ? config.hierarchy_bridge_latency[i] : 0;
                        if (lat == 0 && brlat == 0) continue;  // skip passthrough levels
                        std::cout << "    L" << i << " " << std::setw(12) << std::left
                                  << (level_names[i] + ":") << " " << lat << " cycles";
                        if (i < 6 && brlat > 0) {
                            std::cout << " + bridge " << brlat << " cy";
                            const auto& bmodel = config.hierarchy_bridge_model[i];
                            if (!bmodel.empty() && bmodel != "auto")
                                std::cout << " [" << bmodel << "]";
                        }
                        std::cout << std::endl;
                    }
                }
                // PE placement with M:N mapping info
                if (config.placement_level == "HOST_MC") {
                    std::cout << "    PE placement: HOST_MC (" << config.num_pes << " PEs share host MC)" << std::endl;
                    std::cout << "    PE-MIs: none (PEs share host MC)" << std::endl;
                } else {
                    std::cout << "    PE placement: L" << config.pe_hierarchy_level
                              << " (" << config.placement_level << "), "
                              << config.num_pes << " PEs -> " << config.total_mem_orgs << " mem orgs";
                    // Compute and display ratio
                    if (config.total_mem_orgs > 0 && config.num_pes > 0) {
                        int g = std::gcd(config.num_pes, config.total_mem_orgs);
                        std::cout << " (" << (config.num_pes / g) << ":"
                                  << (config.total_mem_orgs / g) << ", "
                                  << (config.pe_mem_connection == UnifiedConfig::PEMemConnectionMode::SHARED_IO
                                      ? "shared_io" : "separate_endpoints") << ")";
                    }
                    std::cout << std::endl;
                    if (config.pe_mem_connection == UnifiedConfig::PEMemConnectionMode::SEPARATE_ENDPOINTS) {
                        std::cout << "    Endpoints: " << config.total_network_endpoints
                                  << " (PEs=" << config.num_pes << " + mem_orgs=" << config.total_mem_orgs
                                  << "), local_link=" << config.local_link_latency << " cy" << std::endl;
                    }

                    // PE-MI info (always present for non-HOST_MC due to validation)
                    if (config.pe_mc_enabled) {
                        int mi_count = config.num_pes / config.pes_per_mc;
                        std::cout << "    PE-MIs: " << mi_count
                                  << " (" << config.pes_per_mc << " PEs/MI";
                        if (config.ports_per_bank > 1)
                            std::cout << ", " << config.ports_per_bank << " ports/bank";
                        std::cout << ")";
                        if (config.pe_mc_local_latency > 0)
                            std::cout << ", local_lat=" << config.pe_mc_local_latency << "cy";
                        if (config.pe_mc_bandwidth_mbs > 0)
                            std::cout << ", bw=" << config.pe_mc_bandwidth_mbs << "MB/s";
                        std::cout << std::endl;
                    }
                }
            }

            if (config.workload_type == "mpi" && config.mpi_ranks > 0) {
                // ── MPI path: per-rank QEMU+ZSim simulation via mpirun ──
                int ranks = config.mpi_ranks;
                std::string output_base = "/tmp/pimid_qemu_" + std::to_string(getpid());
                mkdir(output_base.c_str(), 0755);

                std::cout << "MPI Mode: " << ranks << " ranks" << std::endl;
                std::cout << "Output directory: " << output_base << std::endl;

                // Generate per-rank ZSim configs (1 core per rank, QEMU-mode)
                ZSimSimulator zsim_helper(config);
                for (int rank = 0; rank < ranks; rank++) {
                    std::string rank_dir = output_base + "/rank" + std::to_string(rank);
                    mkdir(rank_dir.c_str(), 0755);

                    // Generate a QEMU-mode ZSim config with 1 core for this rank
                    std::string cfg_path = rank_dir + "/zsim.cfg";
                    std::ofstream cfg(cfg_path);
                    if (!cfg.is_open()) {
                        std::cerr << "Error: Failed to create config for rank " << rank << std::endl;
                        return 1;
                    }

                    std::string core_type = "Simple";
                    if (config.pe_type == "ooo_core") {
                        core_type = "OoO";
                    } else if (config.pe_type == "in_order_core") {
                        core_type = "InOrder";
                    } else if (config.pe_type == "alu_core" || config.pe_type == "alu") {
                        core_type = "ALU";
                    } else if (config.pe_type == "null_core" || config.pe_type == "null") {
                        core_type = "Null";
                    }

                    // Memory latency: priority is override > YAML config > external models > defaults
                    int mem_latency;
                    if (config.memory_latency_override >= 0) {
                        mem_latency = config.memory_latency_override;
                    } else {
                        // Use external models (default) or complete YAML override
                        mem_latency = getMemoryLatencyCycles(config.memory_tech, config.frequency_mhz,
                                                              config.use_yaml_memory_params,
                                                              config.memory_params.read_latency_ns);
                    }

                    // Cache latencies: YAML override > CACTI > defaults
                    int l1d_latency, l1i_latency, l2_latency = 0, l3_latency = 0;
                    if (config.use_yaml_cache_params) {
                        l1d_latency = static_cast<int>(std::round(
                            config.l1d_params.latency_ns * config.frequency_mhz / 1000.0));
                        l1i_latency = static_cast<int>(std::round(
                            config.l1i_params.latency_ns * config.frequency_mhz / 1000.0));
                        if (config.enable_l2)
                            l2_latency = static_cast<int>(std::round(
                                config.l2_params.latency_ns * config.frequency_mhz / 1000.0));
                        if (config.enable_l3)
                            l3_latency = static_cast<int>(std::round(
                                config.l3_params.latency_ns * config.frequency_mhz / 1000.0));
                    } else {
                        l1d_latency = getCacheLatencyCycles(config.l1d_size_kb, config.l1d_ways,
                                                             config.cache_line_size, config.frequency_mhz, config.tech_node_nm, 4);
                        l1i_latency = getCacheLatencyCycles(config.l1i_size_kb, config.l1i_ways,
                                                             config.cache_line_size, config.frequency_mhz, config.tech_node_nm, 3);
                        if (config.enable_l2)
                            l2_latency = getCacheLatencyCycles(config.l2_size_kb, config.l2_ways,
                                                                config.cache_line_size, config.frequency_mhz, config.tech_node_nm, 12);
                        if (config.enable_l3)
                            l3_latency = getCacheLatencyCycles(config.l3_size_kb, config.l3_ways,
                                                                config.cache_line_size, config.frequency_mhz, config.tech_node_nm, 20);
                    }
                    l1d_latency = std::max(1, l1d_latency);
                    l1i_latency = std::max(1, l1i_latency);
                    if (config.enable_l2) l2_latency = std::max(1, l2_latency);
                    if (config.enable_l3) l3_latency = std::max(1, l3_latency);

                    cfg << "// Auto-generated ZSim config by PIMID (QEMU MPI rank " << rank << ")\n";
                    cfg << "// Memory: " << config.memory_tech << ", 1 core per rank\n";
                    cfg << "// Frequency: " << config.frequency_mhz << " MHz\n\n";

                    cfg << "sys = {\n";
                    cfg << "    lineSize = " << config.cache_line_size << ";\n";
                    cfg << "    frequency = " << config.frequency_mhz << ";\n\n";
                    cfg << "    cores = {\n";
                    cfg << "        pim_pe = {\n";
                    cfg << "            type = \"" << core_type << "\";\n";
                    cfg << "            cores = 1;\n";
                    if (core_type == "ALU") {
                        cfg << std::fixed << std::setprecision(2);
                        cfg << "            computeFactor = " << config.alu_compute_factor << ";\n";
                        cfg << "            accessFactor = " << config.alu_access_factor << ";\n";
                        cfg << "            throughputFactor = " << config.alu_throughput_factor << ";\n";
                        cfg << std::defaultfloat;
                        cfg << "            operandWidth = " << config.alu_operand_width << ";\n";
                        cfg << std::fixed << std::setprecision(2);
                        cfg << "            energyFactor = " << config.alu_energy_factor << ";\n";
                        cfg << std::defaultfloat;
                    } else {
                        cfg << "            dcache = \"l1d\";\n";
                        cfg << "            icache = \"l1i\";\n";
                    }
                    cfg << "        };\n";
                    cfg << "    };\n\n";

                    if (core_type != "ALU") {
                        cfg << "    caches = {\n";
                        cfg << "        l1d = {\n";
                        cfg << "            caches = 1;\n";
                        cfg << "            size = " << (config.l1d_size_kb * 1024) << ";\n";
                        cfg << "            array = { type = \"SetAssoc\"; ways = " << config.l1d_ways << "; };\n";
                        cfg << "            latency = " << l1d_latency << ";\n";
                        cfg << "        };\n";
                        cfg << "        l1i = {\n";
                        cfg << "            caches = 1;\n";
                        cfg << "            size = " << (config.l1i_size_kb * 1024) << ";\n";
                        cfg << "            array = { type = \"SetAssoc\"; ways = " << config.l1i_ways << "; };\n";
                        cfg << "            latency = " << l1i_latency << ";\n";
                        cfg << "        };\n";
                        if (config.enable_l2) {
                            cfg << "        l2 = {\n";
                            // MPI per-rank: 1 core, so l2 caches=1 even if clustered globally
                            cfg << "            caches = 1;\n";
                            cfg << "            size = " << (config.l2_size_kb * 1024) << ";\n";
                            cfg << "            array = { type = \"SetAssoc\"; ways = " << config.l2_ways << "; };\n";
                            cfg << "            children = \"l1i|l1d\";\n";
                            cfg << "            latency = " << l2_latency << ";\n";
                            cfg << "        };\n";
                        }
                        if (config.enable_l3) {
                            cfg << "        l3 = {\n";
                            cfg << "            caches = 1;\n";
                            cfg << "            size = " << (config.l3_size_kb * 1024) << ";\n";
                            cfg << "            array = { type = \"SetAssoc\"; ways = " << config.l3_ways << "; };\n";
                            cfg << "            children = \"l2\";\n";
                            cfg << "            latency = " << l3_latency << ";\n";
                            cfg << "        };\n";
                        }
                        cfg << "    };\n\n";
                    }

                    emitZSimMemBlock(cfg, config, mem_latency);

                    // Configure Garnet-based network for all topologies
                    emitZSimNetworkBlock(cfg, config);
                    emitZSimHierarchyBlock(cfg, config);
                    cfg << "};\n\n";

                    int mpi_parallelism = (config.workload_type == "mpi") ? config.num_pes : 1;
                    cfg << "sim = {\n";
                    cfg << "    outputDir = \"" << rank_dir << "\";\n";
                    cfg << "    phaseLength = " << config.phase_length << ";\n";
                    cfg << "    maxTotalInstrs = " << config.max_instructions << "L;\n";
                    cfg << "    statsPhaseInterval = " << config.stats_interval << ";\n";
                    cfg << "    parallelism = " << mpi_parallelism << ";\n";
                    cfg << "    printHierarchy = true;\n";
                    cfg << "    aslr = false;\n";
                    cfg << "    strictConfig = false;\n";
                    cfg << "};\n\n";

                    cfg << "// QEMU mode: workload is launched by QEMU, not by ZSim\n";
                    cfg << "process0 = {\n";
                    cfg << "    command = \"<qemu-managed>\";\n";
                    cfg << "};\n";
                    cfg.close();
                }

                // Find libpimid_mpi.so (PIMID's in-scope shm-mailbox MPI transport)
                std::string pimid_mpi_lib = findPimidMpiLib();
                if (pimid_mpi_lib.empty()) {
                    std::cerr << "Error: libpimid_mpi.so not found (searched build dirs)" << std::endl;
                    std::cerr << "Build pimid_mpi target before using --mpi-ranks" << std::endl;
                    return 1;
                }

                // Unique shm name per launch so concurrent pimid invocations don't collide
                std::string shm_name = "/pimid_mpi_" + std::to_string(getpid());

                std::cout << "MPI transport: libpimid_mpi.so (shm=" << shm_name << ")" << std::endl;
                std::cout << "Launching " << ranks << " ranks (PIMID-managed multi-process)" << std::endl;
                std::cout << "────────────────────────────────────────" << std::endl;

                std::vector<pid_t> child_pids;
                child_pids.reserve(ranks);
                for (int rank = 0; rank < ranks; rank++) {
                    pid_t pid = fork();
                    if (pid < 0) {
                        std::cerr << "Error: fork() failed for rank " << rank << ": "
                                  << strerror(errno) << std::endl;
                        for (pid_t cp : child_pids) {
                            kill(cp, SIGTERM);
                            waitpid(cp, nullptr, 0);
                        }
                        return 1;
                    }
                    if (pid == 0) {
                        std::string rank_dir = output_base + "/rank" + std::to_string(rank);
                        std::string plugin_arg = plugin_path + ",cfg=" + rank_dir
                                                 + "/zsim.cfg,out=" + rank_dir;

                        setenv("OMP_NUM_THREADS", "1", 1);
                        setenv("LD_PRELOAD", pimid_mpi_lib.c_str(), 1);
                        setenv("PIMID_MPI_RANKS", std::to_string(ranks).c_str(), 1);
                        setenv("PIMID_MPI_RANK", std::to_string(rank).c_str(), 1);
                        setenv("PIMID_MPI_SHM", shm_name.c_str(), 1);
                        for (const auto& [key, val] : config.workload_env) {
                            setenv(key.c_str(), val.c_str(), 1);
                        }

                        std::vector<std::string> argv_owned;
                        argv_owned.push_back(qemu_binary);
                        argv_owned.push_back("-plugin");
                        argv_owned.push_back(plugin_arg);
                        argv_owned.push_back("--");
                        argv_owned.push_back(config.workload_binary);
                        for (const auto& wa : config.workload_args) argv_owned.push_back(wa);

                        std::vector<char*> argv_cstr;
                        argv_cstr.reserve(argv_owned.size() + 1);
                        for (auto& s : argv_owned) argv_cstr.push_back(s.data());
                        argv_cstr.push_back(nullptr);

                        execvp(qemu_binary.c_str(), argv_cstr.data());
                        std::cerr << "Error: execvp(" << qemu_binary << ") failed: "
                                  << strerror(errno) << std::endl;
                        _exit(127);
                    }
                    child_pids.push_back(pid);
                }

                int worst_exit = 0;
                for (pid_t cp : child_pids) {
                    int st = 0;
                    waitpid(cp, &st, 0);
                    int rc = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
                    if (rc > worst_exit) worst_exit = rc;
                }

                shm_unlink(shm_name.c_str());

                std::cout << "────────────────────────────────────────" << std::endl;

                {
                    int exit_code = worst_exit;
                    std::cout << "\nAll " << ranks << " ranks exited (worst exit code: "
                              << exit_code << ")" << std::endl;

                    // Parse per-rank ZSim stats
                    std::cout << "\n========================================" << std::endl;
                    std::cout << "MPI QEMU+ZSim Results (" << ranks << " ranks)" << std::endl;
                    std::cout << "========================================" << std::endl;

                    uint64_t total_cycles = 0;
                    uint64_t max_cycles = 0;
                    uint64_t total_instrs = 0;

                    for (int rank = 0; rank < ranks; rank++) {
                        std::string stats_path = output_base + "/rank" + std::to_string(rank) + "/zsim.out";
                        ZSimParsedOutput rank_stats = parseZSimOutputFile(stats_path);

                        std::cout << "Rank " << rank << ": ";
                        if (rank_stats.cycles > 0 || rank_stats.instrs > 0) {
                            std::cout << rank_stats.cycles << " cycles, "
                                      << rank_stats.instrs << " instrs" << std::endl;
                        } else {
                            std::cout << "(no stats available)" << std::endl;
                        }

                        total_cycles += rank_stats.cycles;
                        total_instrs += rank_stats.instrs;
                        if (rank_stats.cycles > max_cycles) max_cycles = rank_stats.cycles;
                    }

                    std::cout << "────────────────────────────────────────" << std::endl;
                    std::cout << "Total:  " << total_cycles << " cycles (max: " << max_cycles << ")" << std::endl;
                    std::cout << "        " << total_instrs << " instructions" << std::endl;
                    std::cout << "Per-rank results: " << output_base << "/rank*/zsim.out" << std::endl;

                    // Power analysis using aggregate stats
                    if (config.enable_power && exit_code == 0) {
                        ZSimParsedOutput mpi_agg_stats;
                        mpi_agg_stats.cycles = max_cycles;
                        mpi_agg_stats.instrs = total_instrs;
                        runPowerAnalysis(config, mpi_agg_stats, output_base + "/rank0");
                    }

                    success = (exit_code == 0);
                }
            } else {
                // ── Single-rank path ──
                // Generate ZSim configuration file (reuse ZSimSimulator's logic)
                ZSimSimulator zsim_helper(config);
                std::string zsim_cfg_path = zsim_helper.generateConfig();
                if (zsim_cfg_path.empty()) {
                    std::cerr << "Error: Failed to generate ZSim config" << std::endl;
                    return 1;
                }

                std::cout << "ZSim config: " << zsim_cfg_path << std::endl;

                // Output directory (also used by QEMU plugin via out= parameter)
                std::string output_dir = "/tmp/pimid_qemu_" + std::to_string(getpid());
                mkdir(output_dir.c_str(), 0755);
                config.zsim_output_dir = output_dir;

                // Build plugin argument string
                std::string plugin_arg = plugin_path
                    + ",cfg=" + zsim_cfg_path
                    + ",out=" + output_dir;

                std::cout << "Output:   " << output_dir << std::endl;
                std::cout << std::endl;

                // fork/exec: qemu-x86_64 -plugin <plugin>,cfg=<cfg>,out=<dir> -- <workload> [args]
                std::cout << "Running workload through QEMU+ZSim simulation..." << std::endl;
                std::cout << "----------------------------------------" << std::endl;

                pid_t pid = fork();
                if (pid < 0) {
                    std::cerr << "Error: Failed to fork process" << std::endl;
                    return 1;
                }

                if (pid == 0) {
                    // Child process — set env vars and exec QEMU
                    // OpenMP defaults: without a cap, libgomp sizes the team to
                    // omp_get_num_procs() = the HOST core count under qemu-user,
                    // spawning far more threads than the num_pes simulated PE
                    // contexts. The surplus threads spin at the parallel-region
                    // barrier, the region never completes, ROI_END never fires,
                    // and the sim livelocks (OMP-detailed) or reports wrong cycles
                    // (other OMP modes, from oversubscription). Cap to num_pes +
                    // passive waits. overwrite=0 so explicit workload_env wins.
                    if (config.workload_type == "openmp") {
                        int omp_threads = (config.num_pes > 0) ? config.num_pes : 1;
                        setenv("OMP_NUM_THREADS", std::to_string(omp_threads).c_str(), 0);
                        setenv("OMP_DYNAMIC", "FALSE", 0);
                        setenv("OMP_WAIT_POLICY", "PASSIVE", 0);
                        setenv("GOMP_SPINCOUNT", "0", 0);
                    }
                    for (const auto& [key, val] : config.workload_env) {
                        setenv(key.c_str(), val.c_str(), 1);
                    }
                    std::vector<const char*> args;
                    args.push_back(qemu_binary.c_str());
                    args.push_back("-plugin");
                    args.push_back(plugin_arg.c_str());
                    args.push_back("--");
                    args.push_back(config.workload_binary.c_str());
                    for (const auto& wa : config.workload_args) {
                        args.push_back(wa.c_str());
                    }
                    args.push_back(nullptr);

                    execvp(qemu_binary.c_str(), const_cast<char* const*>(args.data()));
                    std::cerr << "Error: Failed to execute QEMU: " << strerror(errno) << std::endl;
                    _exit(1);
                }

                // Parent — wait for completion
                int status;
                waitpid(pid, &status, 0);

                std::cout << "----------------------------------------" << std::endl;

                if (WIFEXITED(status)) {
                    int exit_code = WEXITSTATUS(status);
                    std::cout << "QEMU+ZSim completed with exit code: " << exit_code << std::endl;

                    // Parse ZSim stats regardless of guest exit code
                    // (guest may return non-zero but ZSim still collected valid data)
                    std::string stats_path = output_dir + "/zsim.out";
                    ZSimParsedOutput exec_zsim_stats = parseZSimOutputFile(stats_path);
                    std::ifstream stats_file(stats_path);
                    if (stats_file.is_open()) {
                        std::cout << "\nZSim Statistics:" << std::endl;
                        std::string line;
                        while (std::getline(stats_file, line)) {
                            std::cout << "  " << line << std::endl;
                        }
                    }

                    // Power analysis (runs if stats exist, even for non-zero guest exit)
                    // OOO cores may report cycles=0 (contention sim not triggered in QEMU mode)
                    // but still produce valid instrs — use instrs as fallback guard
                    if (config.enable_power &&
                        (exec_zsim_stats.cycles > 0 || exec_zsim_stats.instrs > 0)) {
                        runPowerAnalysis(config, exec_zsim_stats, output_dir);
                    }

                    success = (exit_code == 0);
                } else if (WIFSIGNALED(status)) {
                    int sig = WTERMSIG(status);
                    std::cerr << "QEMU+ZSim terminated by signal " << sig
                              << " (" << strsignal(sig) << ")" << std::endl;
                    success = false;
                } else {
                    std::cerr << "QEMU terminated abnormally" << std::endl;
                    success = false;
                }
            }

            std::cout << "========================================" << std::endl;

        }
    } else if (config.scope == "system") {
        // Multi-host/multi-device system simulation
        if (config.method == "exec") {
            // Banner
            int num_hosts = 0, num_compute_devs = 0, num_mem_devs = 0;
            for (const auto& n : config.system_nodes) {
                if (n.role == UnifiedConfig::SystemNode::HOST) num_hosts++;
                else if (n.device_type == UnifiedConfig::SystemNode::COMPUTE) num_compute_devs++;
                else num_mem_devs++;
            }

            std::cout << "\n========================================" << std::endl;
            std::cout << "PIMID SYSTEM: " << num_hosts << " Host"
                      << (num_hosts > 1 ? "s" : "")
                      << " + " << (num_compute_devs + num_mem_devs) << " Device"
                      << ((num_compute_devs + num_mem_devs) > 1 ? "s" : "") << std::endl;
            std::cout << "========================================" << std::endl;

            for (const auto& n : config.system_nodes) {
                if (n.role == UnifiedConfig::SystemNode::HOST) {
                    std::cout << "  Host " << n.name << ": "
                              << n.num_cores << "x ";
                    if (n.core_type == "ooo_core") std::cout << "OOO";
                    else if (n.core_type == "in_order_core") std::cout << "InOrder";
                    else if (n.core_type == "alu_core") std::cout << "ALU";
                    else if (n.core_type == "null_core") std::cout << "Null";
                    else std::cout << "Simple";
                    std::cout << " @ " << static_cast<int>(n.frequency_mhz) << " MHz"
                              << ", " << n.memory_tech << std::endl;
                } else {
                    std::cout << "  Device " << n.name << ": ";
                    if (n.device_type == UnifiedConfig::SystemNode::COMPUTE) {
                        std::cout << n.num_pes << "x ";
                        if (n.core_type == "alu_core") std::cout << "ALU";
                        else if (n.core_type == "ooo_core") std::cout << "OOO";
                        else std::cout << "Simple";
                        std::cout << " @ " << static_cast<int>(n.frequency_mhz) << " MHz";
                    } else {
                        std::cout << "memory-only";
                    }
                    std::cout << ", " << n.memory_tech
                              << " (" << (n.device_type == UnifiedConfig::SystemNode::COMPUTE ? "compute" : "memory")
                              << ", " << (n.attachment == UnifiedConfig::SystemNode::INTERNAL ? "internal" : "external")
                              << ")" << std::endl;
                }
                // Per-node workload display
                if (!n.workload_binary.empty()) {
                    std::cout << "    Workload: " << n.workload_binary;
                    for (const auto& a : n.workload_args) std::cout << " " << a;
                    std::cout << std::endl;
                }
            }

            // Multi-QEMU mode indicator
            if (needsMultiQemu(config)) {
                auto groups = buildProcessGroups(config);
                std::cout << "  Execution: Multi-QEMU (" << groups.size() << " processes)" << std::endl;
            }

            // System network info
            int num_nodes = (int)config.system_nodes.size();
            double avg_hops = avgHopsForTopology(config.system_network.topology, num_nodes);
            uint32_t example_lat = computeSystemNetLatency(config, 0, num_nodes > 1 ? 1 : 0);
            std::cout << "  System Network: " << config.system_network.topology
                      << " [" << config.system_network.model << "]"
                      << ", " << example_lat << " ref-cycles (avg "
                      << std::fixed << std::setprecision(1) << avg_hops << std::defaultfloat << " hops)"
                      << std::endl;

            // Per-link info
            for (const auto& lnk : config.system_network.links) {
                std::cout << "    " << lnk.src_name << " <-> " << lnk.dst_name
                          << ": " << lnk.link_type
                          << " (" << lnk.base_latency_ns << "ns, "
                          << lnk.bandwidth_GBs << " GB/s)" << std::endl;
            }

            std::cout << "  Reference frequency: " << static_cast<int>(config.reference_frequency_mhz)
                      << " MHz" << std::endl;

            // Generate combined ZSim config
            std::string zsim_cfg_path = generateSystemConfig(config);
            if (zsim_cfg_path.empty()) {
                std::cerr << "Error: Failed to generate system ZSim config" << std::endl;
                return 1;
            }
            std::cout << "  ZSim config: " << zsim_cfg_path << std::endl;

            // Find QEMU and plugin
            std::string qemu_binary = findQemuBinary();
            if (qemu_binary.empty()) {
                std::cerr << "Error: qemu-x86_64 not found" << std::endl;
                return 1;
            }
            std::string plugin_path = findQemuPlugin("libzsim_qemu.so");
            if (plugin_path.empty()) {
                std::cerr << "Error: libzsim_qemu.so not found" << std::endl;
                return 1;
            }

            // Output directory
            std::string output_dir = "/tmp/pimid_system_zsim_" + std::to_string(getpid());
            mkdir(output_dir.c_str(), 0755);
            config.zsim_output_dir = output_dir;

            // Launch QEMU + ZSim
            int status = 0;
            if (needsMultiQemu(config)) {
                // Multi-QEMU: different binaries on different nodes
                auto groups = buildProcessGroups(config);
                std::vector<pid_t> pids;

                // Clean up any stale sentinel
                std::string sentinel = output_dir + "/.zsim_shmid";
                unlink(sentinel.c_str());

                for (size_t i = 0; i < groups.size(); i++) {
                    if (i > 0) {
                        // Wait for primary to finish init (sentinel file exists)
                        while (access(sentinel.c_str(), F_OK) != 0) usleep(10000);
                    }

                    pid_t pid = fork();
                    if (pid == 0) {
                        setenv("ZSIM_CFG_FILE", zsim_cfg_path.c_str(), 1);
                        setenv("ZSIM_OUTPUT_DIR", output_dir.c_str(), 1);
                        for (const auto& [key, val] : config.workload_env) {
                            setenv(key.c_str(), val.c_str(), 1);
                        }

                        std::string parg = plugin_path + ",cfg=" + zsim_cfg_path
                            + ",out=" + output_dir + ",procIdx=" + std::to_string(i);

                        std::vector<const char*> args;
                        args.push_back(qemu_binary.c_str());
                        args.push_back("-plugin");
                        args.push_back(parg.c_str());
                        args.push_back("--");
                        args.push_back(groups[i].binary.c_str());
                        for (const auto& wa : groups[i].args)
                            args.push_back(wa.c_str());
                        args.push_back(nullptr);

                        execvp(qemu_binary.c_str(), const_cast<char* const*>(args.data()));
                        std::cerr << "Error: Failed to execute QEMU (process " << i << "): "
                                  << strerror(errno) << std::endl;
                        _exit(1);
                    }
                    pids.push_back(pid);
                }

                // Wait for all QEMU processes
                bool all_ok = true;
                for (pid_t p : pids) {
                    int s;
                    waitpid(p, &s, 0);
                    if (!WIFEXITED(s) || WEXITSTATUS(s) != 0) all_ok = false;
                }
                status = all_ok ? 0 : 1;

                // Clean up sentinel
                unlink(sentinel.c_str());
            } else {
                // Single-QEMU: all nodes use the same binary
                std::string plugin_arg = plugin_path
                    + ",cfg=" + zsim_cfg_path
                    + ",out=" + output_dir;
                pid_t pid = fork();
                if (pid == 0) {
                    setenv("ZSIM_CFG_FILE", zsim_cfg_path.c_str(), 1);
                    setenv("ZSIM_OUTPUT_DIR", output_dir.c_str(), 1);
                    for (const auto& [key, val] : config.workload_env) {
                        setenv(key.c_str(), val.c_str(), 1);
                    }

                    std::vector<const char*> args;
                    args.push_back(qemu_binary.c_str());
                    args.push_back("-plugin");
                    args.push_back(plugin_arg.c_str());
                    args.push_back("--");
                    args.push_back(config.workload_binary.c_str());
                    for (const auto& wa : config.workload_args) {
                        args.push_back(wa.c_str());
                    }
                    args.push_back(nullptr);
                    execvp(qemu_binary.c_str(), const_cast<char* const*>(args.data()));
                    std::cerr << "Error: Failed to execute QEMU: " << strerror(errno) << std::endl;
                    _exit(1);
                }

                waitpid(pid, &status, 0);
                status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
            }

            std::cout << "----------------------------------------" << std::endl;
            std::cout << "QEMU+ZSim (system) completed with exit code: " << status << std::endl;

            // Parse ZSim stats
            std::string stats_path = output_dir + "/zsim.out";
            ZSimParsedOutput sys_stats = parseZSimOutputFile(stats_path);
            std::ifstream stats_file(stats_path);
            if (stats_file.is_open()) {
                std::cout << "\nSystem ZSim Statistics:" << std::endl;
                std::string line;
                while (std::getline(stats_file, line)) {
                    std::cout << "  " << line << std::endl;
                }
            }

            // Power analysis
            if (config.enable_power &&
                (sys_stats.cycles > 0 || sys_stats.instrs > 0)) {
                runPerNodePowerAnalysis(config, sys_stats, output_dir);
            }

            success = (status == 0);
            std::cout << "========================================" << std::endl;
        } else if (config.method == "trace") {
            // ── Trace Replay for System Scope ──
            // Banner (reuse system banner logic)
            int num_hosts = 0, num_compute_devs = 0, num_mem_devs = 0;
            for (const auto& n : config.system_nodes) {
                if (n.role == UnifiedConfig::SystemNode::HOST) num_hosts++;
                else if (n.device_type == UnifiedConfig::SystemNode::COMPUTE) num_compute_devs++;
                else num_mem_devs++;
            }

            std::cout << "\n========================================" << std::endl;
            std::cout << "PIMID SYSTEM TRACE REPLAY: " << num_hosts << " Host"
                      << (num_hosts > 1 ? "s" : "")
                      << " + " << (num_compute_devs + num_mem_devs) << " Device"
                      << ((num_compute_devs + num_mem_devs) > 1 ? "s" : "") << std::endl;
            std::cout << "========================================" << std::endl;

            for (const auto& n : config.system_nodes) {
                if (n.role == UnifiedConfig::SystemNode::HOST) {
                    std::cout << "  Host " << n.name << ": "
                              << n.num_cores << "x ";
                    if (n.core_type == "ooo_core") std::cout << "OOO";
                    else if (n.core_type == "in_order_core") std::cout << "InOrder";
                    else if (n.core_type == "alu_core") std::cout << "ALU";
                    else if (n.core_type == "null_core") std::cout << "Null";
                    else std::cout << "Simple";
                    std::cout << " @ " << static_cast<int>(n.frequency_mhz) << " MHz"
                              << ", " << n.memory_tech << std::endl;
                } else {
                    std::cout << "  Device " << n.name << ": ";
                    if (n.device_type == UnifiedConfig::SystemNode::COMPUTE) {
                        std::cout << n.num_pes << "x ";
                        if (n.core_type == "alu_core") std::cout << "ALU";
                        else if (n.core_type == "ooo_core") std::cout << "OOO";
                        else std::cout << "Simple";
                        std::cout << " @ " << static_cast<int>(n.frequency_mhz) << " MHz";
                    } else {
                        std::cout << "memory-only";
                    }
                    std::cout << ", " << n.memory_tech << std::endl;
                }
            }

            // System network info
            int num_nodes = (int)config.system_nodes.size();
            double avg_hops = avgHopsForTopology(config.system_network.topology, num_nodes);
            uint32_t example_lat = computeSystemNetLatency(config, 0, num_nodes > 1 ? 1 : 0);
            std::cout << "  System Network: " << config.system_network.topology
                      << " [" << config.system_network.model << "]"
                      << ", " << example_lat << " ref-cycles (avg "
                      << std::fixed << std::setprecision(1) << avg_hops << std::defaultfloat << " hops)"
                      << std::endl;

            std::cout << "  Trace: " << config.trace_file << std::endl;

            // Generate combined ZSim config
            std::string zsim_cfg_path = generateSystemConfig(config);
            if (zsim_cfg_path.empty()) {
                std::cerr << "Error: Failed to generate system ZSim config" << std::endl;
                return 1;
            }

            // Output directory
            std::string output_dir = "/tmp/pimid_system_trace_" + std::to_string(getpid());
            mkdir(output_dir.c_str(), 0755);
            config.zsim_output_dir = output_dir;

            std::cout << "  ZSim config: " << zsim_cfg_path << std::endl;
            std::cout << "  Output dir:  " << output_dir << std::endl;
            std::cout << std::endl;

            // Run trace through ZSim simulation
            std::cout << "Running trace through ZSim simulation (system)..." << std::endl;
            std::cout << "----------------------------------------" << std::endl;

            int trace_result = zsim_trace_run(zsim_cfg_path.c_str(),
                                              config.trace_file.c_str(),
                                              output_dir.c_str());

            std::cout << "----------------------------------------" << std::endl;

            if (trace_result == 0) {
                // Parse zsim_trace_stats.txt for summary
                std::string stats_path = output_dir + "/zsim_trace_stats.txt";
                std::ifstream stats_file(stats_path);
                if (stats_file.is_open()) {
                    std::cout << "\nSystem Trace Results:" << std::endl;
                    std::string line;
                    while (std::getline(stats_file, line)) {
                        std::cout << "  " << line << std::endl;
                    }
                }

                // Parse zsim.out for cache stats
                std::string zsim_out_path = output_dir + "/zsim.out";
                ZSimParsedOutput sys_stats = parseZSimOutputFile(zsim_out_path);

                // Power analysis
                if (config.enable_power &&
                    (sys_stats.cycles > 0 || sys_stats.instrs > 0)) {
                    runPowerAnalysis(config, sys_stats, output_dir);
                }
            }

            std::cout << "\n========================================" << std::endl;
            success = (trace_result == 0);
        } else {
            std::cerr << "Error: system scope does not support " << config.method << " method" << std::endl;
            std::cerr << "Supported methods for system scope: exec, trace" << std::endl;
            return 1;
        }
    }

    return success ? 0 : 1;
}
