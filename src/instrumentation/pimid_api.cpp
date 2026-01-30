/**
 * @file pimid_api.cpp
 * @brief Implementation of PIMID public API
 *
 * This file provides the implementation of the API defined in pimid/api.h.
 * When not running under PIMID, most functions are no-ops or return sensible defaults.
 */

#include "pimid/api.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <chrono>

//=============================================================================
// Internal State
//=============================================================================

namespace {

// Global flag indicating if PIMID is active
bool g_pimid_active = false;

// Current PIM region (nullptr if not in PIM region)
const char* g_current_region = nullptr;

// Statistics
struct Stats {
    uint64_t cycles = 0;
    double time_ns = 0.0;
    double energy_pj = 0.0;
    size_t pim_regions = 0;
};

Stats g_stats;

// Region-specific statistics
std::map<std::string, Stats> g_region_stats;

// Simulation start time
std::chrono::high_resolution_clock::time_point g_start_time;

} // anonymous namespace

//=============================================================================
// API Implementation
//=============================================================================

extern "C" {

pimid_context_t pimid_init(void) {
    // Check if running under PIMID (environment variable set by simulator)
    const char* pimid_env = std::getenv("PIMID_ACTIVE");
    g_pimid_active = (pimid_env != nullptr && std::strcmp(pimid_env, "1") == 0);

    if (g_pimid_active) {
        std::cout << "[PIMID] Workload running under PIMID simulator" << std::endl;
        g_start_time = std::chrono::high_resolution_clock::now();
    }

    // Return a dummy context (in full implementation, this would be a real context)
    return g_pimid_active ? reinterpret_cast<pimid_context_t>(1) : nullptr;
}

void pimid_finalize(pimid_context_t ctx) {
    if (!g_pimid_active || !ctx) return;

    std::cout << "\n[PIMID] Simulation Summary:" << std::endl;
    std::cout << "  Total cycles:        " << g_stats.cycles << std::endl;
    std::cout << "  Total time:          " << g_stats.time_ns / 1e6 << " ms" << std::endl;
    std::cout << "  Total energy:        " << g_stats.energy_pj / 1e6 << " μJ" << std::endl;
    std::cout << "  PIM regions entered: " << g_stats.pim_regions << std::endl;
}

int pimid_is_active(void) {
    return g_pimid_active ? 1 : 0;
}

void pimid_begin_pim_region(const char* region_name) {
    if (!g_pimid_active) return;

    g_current_region = region_name;
    g_stats.pim_regions++;

    std::cout << "[PIMID] Entering PIM region: " << region_name << std::endl;
}

void pimid_end_pim_region(void) {
    if (!g_pimid_active || !g_current_region) return;

    std::cout << "[PIMID] Exiting PIM region: " << g_current_region << std::endl;
    g_current_region = nullptr;
}

void* pimid_malloc(size_t size) {
    // In full implementation, this would register the allocation
    // and track accesses to this memory region
    void* ptr = std::malloc(size);

    if (g_pimid_active && ptr) {
        std::cout << "[PIMID] Allocated " << size << " bytes at " << ptr << std::endl;
    }

    return ptr;
}

void* pimid_aligned_alloc(size_t alignment, size_t size) {
    void* ptr = std::aligned_alloc(alignment, size);

    if (g_pimid_active && ptr) {
        std::cout << "[PIMID] Allocated " << size << " bytes (aligned to " << alignment << ") at " << ptr << std::endl;
    }

    return ptr;
}

void pimid_free(void* ptr) {
    if (g_pimid_active && ptr) {
        std::cout << "[PIMID] Freed memory at " << ptr << std::endl;
    }

    std::free(ptr);
}

void pimid_hint_access_pattern(void* ptr, size_t size, pimid_access_pattern_t pattern) {
    if (!g_pimid_active) return;

    const char* pattern_str = "UNKNOWN";
    switch (pattern) {
        case PIMID_ACCESS_RANDOM: pattern_str = "RANDOM"; break;
        case PIMID_ACCESS_SEQUENTIAL: pattern_str = "SEQUENTIAL"; break;
        case PIMID_ACCESS_STRIDED: pattern_str = "STRIDED"; break;
        case PIMID_ACCESS_GATHER_SCATTER: pattern_str = "GATHER_SCATTER"; break;
    }

    std::cout << "[PIMID] Hint: " << size << " bytes at " << ptr
              << " will be accessed with " << pattern_str << " pattern" << std::endl;
}

void pimid_mark_readonly(void* ptr, size_t size) {
    if (!g_pimid_active) return;
    std::cout << "[PIMID] Marked " << size << " bytes at " << ptr << " as read-only" << std::endl;
}

void pimid_mark_writeonly(void* ptr, size_t size) {
    if (!g_pimid_active) return;
    std::cout << "[PIMID] Marked " << size << " bytes at " << ptr << " as write-only" << std::endl;
}

void pimid_offload(const char* kernel_name, void* data, size_t size,
                   void (*kernel_func)(void*), void* args) {
    if (!g_pimid_active) {
        // If not under PIMID, just execute the kernel normally
        if (kernel_func) {
            kernel_func(args);
        }
        return;
    }

    std::cout << "[PIMID] Offloading kernel '" << kernel_name << "' with " << size << " bytes of data" << std::endl;

    // In full implementation, this would simulate PIM execution
    if (kernel_func) {
        kernel_func(args);
    }
}

uint64_t pimid_get_cycles(void) {
    return g_stats.cycles;
}

double pimid_get_time_ns(void) {
    if (!g_pimid_active) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - g_start_time);
        return static_cast<double>(elapsed.count());
    }
    return g_stats.time_ns;
}

double pimid_get_energy_pj(void) {
    return g_stats.energy_pj;
}

void pimid_print_region_stats(const char* region_name) {
    if (!g_pimid_active) return;

    auto it = g_region_stats.find(region_name);
    if (it == g_region_stats.end()) {
        std::cout << "[PIMID] No statistics for region: " << region_name << std::endl;
        return;
    }

    const Stats& stats = it->second;
    std::cout << "\n[PIMID] Statistics for region '" << region_name << "':" << std::endl;
    std::cout << "  Cycles: " << stats.cycles << std::endl;
    std::cout << "  Time:   " << stats.time_ns / 1e6 << " ms" << std::endl;
    std::cout << "  Energy: " << stats.energy_pj / 1e6 << " μJ" << std::endl;
}

void pimid_reset_stats(void) {
    g_stats = Stats();
    g_region_stats.clear();
}

int pimid_get_num_pes(void) {
    // In full implementation, read from configuration
    const char* num_pes_env = std::getenv("PIMID_NUM_PES");
    if (num_pes_env) {
        return std::atoi(num_pes_env);
    }
    return 4; // Default
}

const char* pimid_get_memory_tech(void) {
    const char* tech = std::getenv("PIMID_MEMORY_TECH");
    return tech ? tech : "UNKNOWN";
}

const char* pimid_get_placement_level(void) {
    const char* placement = std::getenv("PIMID_PLACEMENT_LEVEL");
    return placement ? placement : "UNKNOWN";
}

} // extern "C"
