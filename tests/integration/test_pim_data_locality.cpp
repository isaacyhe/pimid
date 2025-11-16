/**
 * @file test_pim_data_locality.cpp
 * @brief Comprehensive test of PIM Data Locality and Reach
 *
 * This test demonstrates the CRITICAL importance of data locality in PIM:
 * 1. Different PIM granularities have different DATA REACH
 * 2. Data beyond a PE's local reach requires INTERNAL NETWORK transfers
 * 3. Network transfers add LATENCY and consume BANDWIDTH
 * 4. Global data access patterns can KILL Bank/BG/Chip-PIM performance
 *
 * Key Comparisons:
 * - Host CPU: Can access ALL data via memory controller (but slow)
 * - MC-PIM/Rank-PIM: Can access ALL data in rank (full DIMM bandwidth)
 * - Chip-PIM: Can only access 1 chip locally (~1 GB), needs network for rest
 * - BG-PIM: Can only access 1 BG locally (~2 GB), needs network for rest
 * - Bank-PIM: Can only access 1 bank locally (~512 MB), needs network for rest
 *
 * Workload Scenarios:
 * A. LOCAL data (fits in PE's local capacity) - BEST CASE for fine-grained PIM
 * B. GLOBAL data (spread across all banks) - WORST CASE for fine-grained PIM
 * C. PARTIAL data (some local, some remote) - REALISTIC scenario
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <cmath>

#include "pim_request_payload.h"
#include "pim_bandwidth_tracker.h"
#include "internal_dram_network.h"
#include "pimid/memory/dram_architecture_v2.h"

using namespace pimid;
using namespace pimid::memory;

struct WorkloadPattern {
    std::string name;
    std::string description;
    double local_data_fraction;    // Fraction of data that's local to PE
    uint64_t total_data_bytes;     // Total data needed
    uint64_t compute_ops;          // Compute operations
};

struct LocalityResult {
    std::string config_name;
    std::string workload_name;

    // Data reach
    uint64_t local_capacity_bytes;
    uint64_t local_data_bytes;
    uint64_t remote_data_bytes;
    double local_fraction;

    // Performance breakdown
    double compute_time_us;
    double local_access_time_us;
    double remote_access_time_us;
    double network_time_us;
    double total_time_us;

    // Bottleneck
    std::string bottleneck;
};

void printHeader(const std::string& title) {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << std::left << std::setw(64) << title << " ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";
}

/**
 * @brief Get access latency overhead for each PIM granularity
 *
 * Host CPU has HIGHEST latency (cache hierarchy, coherence, OS overhead)
 * MC-PIM has LOWER latency (at memory controller, no cache overhead)
 * Rank-PIM has EVEN LOWER latency (closer to DRAM)
 * Fine-grained PIM has LOWEST latency (directly at data)
 */
double getAccessLatencyOverhead(
    PIMGranularity granularity,
    std::shared_ptr<DRAMArchitectureV2> dram_arch) {

    // Base DRAM access latency (tRCD + tCAS)
    double base_dram_latency_ns = dram_arch->timing.tRCD_ns + dram_arch->timing.tCAS_ns;

    switch (granularity) {
        case PIMGranularity::CPU:
            // Host CPU: Cache hierarchy + coherence + OS + distance from MC
            // Additional overhead: ~100-200ns
            return base_dram_latency_ns + 150.0;

        case PIMGranularity::MEMORY_CONTROLLER:
            // MC-PIM: At memory controller, no cache/coherence overhead
            // But still has MC logic overhead: ~30-50ns
            return base_dram_latency_ns + 40.0;

        case PIMGranularity::RANK:
            // Rank-PIM: Even closer to DRAM, minimal MC overhead
            // Very low overhead: ~10-20ns
            return base_dram_latency_ns + 15.0;

        case PIMGranularity::CHIP:
            // Chip-PIM: At chip level, very close to banks
            // Minimal overhead: ~5-10ns
            return base_dram_latency_ns + 7.0;

        case PIMGranularity::BANK_GROUP:
            // BG-PIM: At bank group, very close to data
            // Tiny overhead: ~2-5ns
            return base_dram_latency_ns + 3.0;

        case PIMGranularity::BANK:
        case PIMGranularity::SUBARRAY:
            // Bank/Subarray-PIM: Directly at the data
            // Minimal overhead: just DRAM access
            return base_dram_latency_ns;

        default:
            return base_dram_latency_ns;
    }
}

LocalityResult simulateWithLocality(
    const std::string& config_name,
    PIMGranularity granularity,
    const WorkloadPattern& workload,
    std::shared_ptr<DRAMArchitectureV2> dram_arch,
    std::shared_ptr<PIMBandwidthTracker> bw_tracker,
    std::shared_ptr<InternalDRAMNetwork> network,
    int num_pes,
    double compute_gflops) {

    LocalityResult result;
    result.config_name = config_name;
    result.workload_name = workload.name;

    // Calculate local capacity for this granularity
    result.local_capacity_bytes = PIMRequestPayload::getTypicalLocalCapacity(granularity);

    // Determine local vs remote data
    result.local_data_bytes = std::min(
        workload.total_data_bytes,
        static_cast<uint64_t>(workload.total_data_bytes * workload.local_data_fraction));
    result.remote_data_bytes = workload.total_data_bytes - result.local_data_bytes;
    result.local_fraction = workload.local_data_fraction;

    // Compute time (same for all)
    result.compute_time_us = (workload.compute_ops / compute_gflops) / 1e3; // us

    // Calculate access latency overhead for this granularity
    double access_latency_ns = getAccessLatencyOverhead(granularity, dram_arch);

    // Calculate number of accesses (assume cache line size = 64 bytes)
    const uint64_t CACHE_LINE_SIZE = 64;
    uint64_t num_local_accesses = (result.local_data_bytes + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;

    // Local data access time = latency overhead × num_accesses + bandwidth time
    double local_latency_time_us = (num_local_accesses * access_latency_ns) / 1e3; // us

    double local_bw_GBs;
    if (granularity == PIMGranularity::CPU ||
        granularity == PIMGranularity::MEMORY_CONTROLLER ||
        granularity == PIMGranularity::RANK) {
        // Access via rank interface
        local_bw_GBs = bw_tracker->getBandwidthLimit(PIMGranularity::RANK);
    } else {
        // Access via local port (bank/BG/chip)
        local_bw_GBs = bw_tracker->getBandwidthLimit(granularity);
        // If multiple PEs per bank, divide bandwidth
        if (granularity == PIMGranularity::BANK && num_pes > 16) {
            int pes_per_bank = num_pes / 16; // 16 banks
            local_bw_GBs /= pes_per_bank;
        }
    }

    double local_bw_time_us = (result.local_data_bytes / local_bw_GBs) / 1e3; // us

    // Total local access time = MAX(latency-limited, bandwidth-limited)
    result.local_access_time_us = std::max(local_latency_time_us, local_bw_time_us);

    // Remote data access time (if any)
    if (result.remote_data_bytes > 0) {
        uint64_t num_remote_accesses = (result.remote_data_bytes + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;
        double remote_latency_time_us = (num_remote_accesses * access_latency_ns) / 1e3; // us

        // Remote data needs network transfer
        if (granularity == PIMGranularity::CPU ||
            granularity == PIMGranularity::MEMORY_CONTROLLER ||
            granularity == PIMGranularity::RANK) {
            // Can access all data via rank interface, no internal network needed
            double remote_bw_time_us = (result.remote_data_bytes / local_bw_GBs) / 1e3;
            result.remote_access_time_us = std::max(remote_latency_time_us, remote_bw_time_us);
            result.network_time_us = 0.0;
        } else {
            // Need internal network for remote data
            // Simulate gathering data from other banks
            int num_remote_banks = 15; // Assume data spread across other banks
            std::vector<int> remote_banks;
            for (int i = 1; i <= num_remote_banks; i++) {
                remote_banks.push_back(i);
            }

            uint64_t bytes_per_bank = result.remote_data_bytes / num_remote_banks;
            uint64_t network_latency_cycles = network->executeGather(
                0, remote_banks, bytes_per_bank);

            // Convert to time
            double cycle_time_ns = 1000.0 / dram_arch->timing.clock_freq_mhz;
            result.network_time_us = (network_latency_cycles * cycle_time_ns) / 1e3;

            // Remote access also limited by network bandwidth
            NetworkLevel level = NetworkLevel::BANK_NETWORK;
            double network_bw = network->getAvailableBandwidth(level);
            double remote_bw_time_us = (result.remote_data_bytes / network_bw) / 1e3;
            result.remote_access_time_us = std::max(remote_latency_time_us, remote_bw_time_us);
        }
    } else {
        result.remote_access_time_us = 0.0;
        result.network_time_us = 0.0;
    }

    // Total time
    result.total_time_us = result.compute_time_us +
                          result.local_access_time_us +
                          std::max(result.remote_access_time_us, result.network_time_us);

    // Determine bottleneck
    if (result.compute_time_us > result.local_access_time_us &&
        result.compute_time_us > result.remote_access_time_us) {
        result.bottleneck = "Compute";
    } else if (result.network_time_us > result.local_access_time_us) {
        result.bottleneck = "Network";
    } else if (result.remote_access_time_us > result.local_access_time_us) {
        result.bottleneck = "Remote BW";
    } else {
        result.bottleneck = "Local BW";
    }

    return result;
}

void printResults(const std::vector<LocalityResult>& results, const std::string& workload_name) {
    printHeader("Results for: " + workload_name);

    std::cout << std::left;
    std::cout << std::setw(15) << "Config"
              << std::setw(14) << "Local Cap"
              << std::setw(12) << "Local%"
              << std::setw(14) << "Compute(μs)"
              << std::setw(14) << "Local(μs)"
              << std::setw(14) << "Remote(μs)"
              << std::setw(14) << "Network(μs)"
              << std::setw(14) << "Total(μs)"
              << std::setw(12) << "Bottleneck"
              << "\n";
    std::cout << std::string(132, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::setw(15) << r.config_name
                  << std::setw(14) << (r.local_capacity_bytes / (1024*1024))
                  << " MB"
                  << std::setw(9) << std::fixed << std::setprecision(1) << (r.local_fraction * 100)
                  << "%"
                  << std::setw(11) << std::setprecision(2) << r.compute_time_us
                  << std::setw(14) << r.local_access_time_us
                  << std::setw(14) << r.remote_access_time_us
                  << std::setw(14) << r.network_time_us
                  << std::setw(14) << r.total_time_us
                  << std::setw(12) << r.bottleneck
                  << "\n";
    }
    std::cout << "\n";
}

void compareLocalVsGlobalWorkloads(
    std::shared_ptr<DRAMArchitectureV2> dram_arch,
    std::shared_ptr<PIMBandwidthTracker> bw_tracker,
    std::shared_ptr<InternalDRAMNetwork> network) {

    printHeader("Workload Patterns Impact on Different PIM Granularities");

    // Define workload patterns
    WorkloadPattern local_workload = {
        "LOCAL",
        "All data fits in PE's local capacity - BEST CASE",
        1.0,  // 100% local
        64ULL * 1024 * 1024,   // 64 MB (fits in Bank-PIM local capacity)
        128ULL * 1024 * 1024 * 1024  // 128 GFLOPs
    };

    WorkloadPattern global_workload = {
        "GLOBAL",
        "Data spread across all banks - WORST CASE",
        0.0625,  // 1/16 banks local (6.25%)
        1024ULL * 1024 * 1024,   // 1 GB (spread across 16 banks)
        128ULL * 1024 * 1024 * 1024  // 128 GFLOPs
    };

    WorkloadPattern partial_workload = {
        "PARTIAL",
        "50% local, 50% remote - REALISTIC",
        0.5,  // 50% local
        512ULL * 1024 * 1024,   // 512 MB
        128ULL * 1024 * 1024 * 1024  // 128 GFLOPs
    };

    std::vector<WorkloadPattern> workloads = {local_workload, global_workload, partial_workload};

    double compute_gflops = 128.0;  // Same compute for all
    int num_pes = 64;

    for (const auto& workload : workloads) {
        std::vector<LocalityResult> results;

        results.push_back(simulateWithLocality(
            "Host CPU", PIMGranularity::CPU, workload,
            dram_arch, bw_tracker, network, 1, compute_gflops));

        results.push_back(simulateWithLocality(
            "MC-PIM", PIMGranularity::MEMORY_CONTROLLER, workload,
            dram_arch, bw_tracker, network, 64, compute_gflops));

        results.push_back(simulateWithLocality(
            "Rank-PIM", PIMGranularity::RANK, workload,
            dram_arch, bw_tracker, network, 64, compute_gflops));

        results.push_back(simulateWithLocality(
            "Chip-PIM", PIMGranularity::CHIP, workload,
            dram_arch, bw_tracker, network, 64, compute_gflops));

        results.push_back(simulateWithLocality(
            "BG-PIM", PIMGranularity::BANK_GROUP, workload,
            dram_arch, bw_tracker, network, 64, compute_gflops));

        results.push_back(simulateWithLocality(
            "Bank-PIM", PIMGranularity::BANK, workload,
            dram_arch, bw_tracker, network, 64, compute_gflops));

        printResults(results, workload.name + ": " + workload.description);
    }
}

void analyzeDataReach() {
    printHeader("Data Reach and Latency Analysis");

    std::cout << "PIM Granularity      Local Capacity    Access Latency    Can Access Without Network\n";
    std::cout << "──────────────────────────────────────────────────────────────────────────────────\n";
    std::cout << "CPU                  ALL               HIGHEST (~180ns)  Entire system memory\n";
    std::cout << "MC-PIM               ~16 GB            LOWER (~70ns)     Entire rank/DIMM\n";
    std::cout << "Rank-PIM             ~16 GB            EVEN LOWER (~42ns) Entire rank/DIMM\n";
    std::cout << "Chip-PIM             ~1 GB             LOW (~34ns)       Single chip (1/8 of rank)\n";
    std::cout << "Bank Group-PIM       ~2 GB             VERY LOW (~30ns)  Single BG (4 banks)\n";
    std::cout << "Bank-PIM             ~512 MB           LOWEST (~27ns)    Single bank (1/16 of rank)\n";
    std::cout << "Subarray-PIM         ~32 MB            LOWEST (~27ns)    Single subarray\n\n";

    std::cout << "LATENCY HIERARCHY:\n";
    std::cout << "─────────────────\n";
    std::cout << "Host CPU      > MC-PIM > Rank-PIM > Chip-PIM > BG-PIM > Bank-PIM (LOWEST)\n";
    std::cout << "~180ns          ~70ns     ~42ns      ~34ns      ~30ns     ~27ns\n\n";
    std::cout << "WHY:\n";
    std::cout << "  - Host CPU: Cache hierarchy + coherence + OS overhead + distance from MC\n";
    std::cout << "  - MC-PIM: At memory controller, no cache overhead, but MC logic delay\n";
    std::cout << "  - Rank-PIM: Closer to DRAM, minimal MC overhead\n";
    std::cout << "  - Fine-grained PIM: Directly at the data, minimal overhead\n\n";

    std::cout << "DATA REACH vs NETWORK:\n";
    std::cout << "──────────────────────\n";
    std::cout << "When PIM unit needs data beyond its local capacity:\n";
    std::cout << "  1. Must use internal DRAM network\n";
    std::cout << "  2. Network has LIMITED bandwidth (8-16 bits for DDR4 bank network)\n";
    std::cout << "  3. Network adds LATENCY (10-50+ cycles per transfer)\n";
    std::cout << "  4. Multiple PEs competing for network creates CONGESTION\n\n";

    std::cout << "Example: Bank-PIM with 1 GB dataset\n";
    std::cout << "  Local capacity: 512 MB\n";
    std::cout << "  Remote data: 512 MB (must come from 15 other banks via network!)\n";
    std::cout << "  Network BW: ~1.2 GB/s (8-bit DDR4 bank network, shared!)\n";
    std::cout << "  Network time: 512 MB / 1.2 GB/s = 427 ms (!!! KILLS performance)\n\n";

    std::cout << "vs MC-PIM with same 1 GB dataset\n";
    std::cout << "  Local capacity: 16 GB (entire rank)\n";
    std::cout << "  Remote data: 0 (all data local!)\n";
    std::cout << "  Rank BW: 9.6 GB/s (64-bit interface)\n";
    std::cout << "  Data time: 1 GB / 9.6 GB/s = 104 ms\n\n";

    std::cout << "Result: MC-PIM 4.1x FASTER for this global data pattern!\n\n";
}

int main() {
    printHeader("PIM Data Locality and Reach Comprehensive Test");

    // Create DDR4 architecture
    std::shared_ptr<DRAMArchitectureV2> ddr4 = createDDR4_2400_Verified();
    auto bw_tracker = std::make_shared<PIMBandwidthTracker>(ddr4);
    bw_tracker->initialize(1, 1, 4, 16, 16);
    auto network = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);

    std::cout << "DRAM: " << ddr4->name << "\n";
    std::cout << "Configuration: 16 banks, 4 bank groups, 8 chips\n\n";

    // Analyze data reach first
    analyzeDataReach();

    // Compare local vs global workloads
    compareLocalVsGlobalWorkloads(ddr4, bw_tracker, network);

    printHeader("Key Takeaways");
    std::cout << "1. DATA LOCALITY IS CRITICAL for fine-grained PIM\n";
    std::cout << "   - Bank-PIM excels when data fits in local bank (~512 MB)\n";
    std::cout << "   - Performance COLLAPSES when data is spread across banks\n\n";

    std::cout << "2. NETWORK BECOMES BOTTLENECK for non-local access\n";
    std::cout << "   - DDR4 bank network: only 8 bits (1.2 GB/s)\n";
    std::cout << "   - Much slower than rank interface (64 bits, 9.6 GB/s)\n\n";

    std::cout << "3. CHOOSE PIM GRANULARITY based on workload:\n";
    std::cout << "   - MC/Rank-PIM: Best for global data access patterns\n";
    std::cout << "   - Bank/BG-PIM: Only good for highly local workloads\n";
    std::cout << "   - Chip-PIM: Middle ground (1 GB local capacity)\n\n";

    std::cout << "4. DATA PLACEMENT MATTERS:\n";
    std::cout << "   - Co-locate data with PIM units\n";
    std::cout << "   - Partition datasets to match PIM granularity\n";
    std::cout << "   - Avoid random/global access patterns\n\n";

    return 0;
}
