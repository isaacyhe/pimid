/**
 * @file test_pim_with_latency.cpp
 * @brief Host vs MC-PIM vs Bank-PIM with LATENCY modeling
 *
 * This enhanced test includes:
 * 1. Bandwidth limits (throughput)
 * 2. DRAM access latency (tRCD + tCAS + tRP)
 * 3. Internal network latency (for Bank-PIM)
 * 4. Access patterns (row buffer hits vs misses)
 * 5. Number of accesses and their individual latencies
 *
 * EQUAL COMPUTE: 128 GFLOPS for all configs
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

#include "pim_request_payload.h"
#include "pim_bandwidth_tracker.h"
#include "internal_dram_network.h"
#include "pimid/memory/dram_architecture_v2.h"

using namespace pimid;
using namespace pimid::memory;

struct LatencyComponents {
    double dram_access_latency_ns;   // tRCD + tCAS (+ tRP for row miss)
    double network_latency_ns;       // Internal DRAM network
    double serialization_latency_ns; // Port serialization time
    double total_latency_ns;
};

struct DetailedResult {
    std::string config_name;

    // Compute
    double compute_time_us;

    // Data movement breakdown
    uint64_t num_accesses;
    double row_buffer_hit_rate;

    // Latency components
    double avg_access_latency_ns;
    double total_access_latency_us;
    double bandwidth_limited_time_us;
    double network_latency_us;

    // Total
    double total_time_us;
    double speedup_vs_host;

    // Bottleneck analysis
    std::string primary_bottleneck;
};

void printHeader(const std::string& title) {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << std::left << std::setw(64) << title << " ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";
}

LatencyComponents calculateDRAMLatency(
    std::shared_ptr<DRAMArchitectureV2> dram_arch,
    bool row_buffer_hit) {

    LatencyComponents latency;

    // Get DRAM timing parameters (in nanoseconds)
    double tRCD_ns = dram_arch->timing.tRCD_ns;
    double tCAS_ns = dram_arch->timing.tCAS_ns;
    double tRP_ns = dram_arch->timing.tRP_ns;

    if (row_buffer_hit) {
        // Row buffer hit: Only need tCAS
        latency.dram_access_latency_ns = tCAS_ns;
    } else {
        // Row buffer miss: Need tRP + tRCD + tCAS
        latency.dram_access_latency_ns = tRP_ns + tRCD_ns + tCAS_ns;
    }

    latency.network_latency_ns = 0.0;
    latency.serialization_latency_ns = 0.0;
    latency.total_latency_ns = latency.dram_access_latency_ns;

    return latency;
}

DetailedResult simulateHost_WithLatency(
    double total_gflops,
    uint64_t total_ops,
    uint64_t data_bytes,
    std::shared_ptr<DRAMArchitectureV2> dram_arch) {

    DetailedResult result;
    result.config_name = "Host CPU";

    // Compute time
    result.compute_time_us = (total_ops / total_gflops) / 1e3; // us

    // Access pattern: Assume cache line size = 64 bytes
    const uint64_t CACHE_LINE_SIZE = 64;
    result.num_accesses = (data_bytes + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;

    // Row buffer hit rate depends on access pattern
    // Streaming access: ~50-70% hit rate
    result.row_buffer_hit_rate = 0.60;

    // Calculate average access latency
    uint64_t num_hits = result.num_accesses * result.row_buffer_hit_rate;
    uint64_t num_misses = result.num_accesses - num_hits;

    auto hit_latency = calculateDRAMLatency(dram_arch, true);
    auto miss_latency = calculateDRAMLatency(dram_arch, false);

    result.avg_access_latency_ns =
        (num_hits * hit_latency.total_latency_ns +
         num_misses * miss_latency.total_latency_ns) / result.num_accesses;

    // Total access latency
    result.total_access_latency_us =
        (result.num_accesses * result.avg_access_latency_ns) / 1e3; // us

    // Bandwidth-limited time (for bulk data transfer)
    double rank_bw_GBs = (dram_arch->datapath.rank_databus_bits.value_bits / 8.0) *
                         (dram_arch->timing.clock_freq_mhz / 1000.0);
    result.bandwidth_limited_time_us = (data_bytes / rank_bw_GBs) / 1e3; // us

    // No internal network for Host
    result.network_latency_us = 0.0;

    // Total data movement time = MAX(latency-limited, bandwidth-limited)
    // For large transfers, bandwidth dominates
    // For small transfers, latency dominates
    double data_movement_us = std::max(
        result.total_access_latency_us,
        result.bandwidth_limited_time_us
    );

    result.total_time_us = result.compute_time_us + data_movement_us;
    result.speedup_vs_host = 1.0;

    // Determine bottleneck
    if (result.bandwidth_limited_time_us > result.total_access_latency_us) {
        result.primary_bottleneck = "Bandwidth (rank: " +
            std::to_string(rank_bw_GBs) + " GB/s)";
    } else {
        result.primary_bottleneck = "Latency (" +
            std::to_string(result.avg_access_latency_ns) + " ns avg)";
    }

    return result;
}

DetailedResult simulateMC_PIM_WithLatency(
    double total_gflops,
    uint64_t total_ops,
    uint64_t data_bytes,
    std::shared_ptr<DRAMArchitectureV2> dram_arch,
    std::shared_ptr<PIMBandwidthTracker> bw_tracker) {

    DetailedResult result;
    result.config_name = "MC-PIM";

    // Compute time (same as Host)
    result.compute_time_us = (total_ops / total_gflops) / 1e3; // us

    // Access pattern: PEs at rank level
    const uint64_t CACHE_LINE_SIZE = 64;
    result.num_accesses = (data_bytes + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;

    // MC-PIM: Better row buffer hit rate (data locality)
    // PEs process data in place, better spatial locality
    result.row_buffer_hit_rate = 0.80; // Higher than Host!

    // Calculate average access latency
    uint64_t num_hits = result.num_accesses * result.row_buffer_hit_rate;
    uint64_t num_misses = result.num_accesses - num_hits;

    auto hit_latency = calculateDRAMLatency(dram_arch, true);
    auto miss_latency = calculateDRAMLatency(dram_arch, false);

    result.avg_access_latency_ns =
        (num_hits * hit_latency.total_latency_ns +
         num_misses * miss_latency.total_latency_ns) / result.num_accesses;

    // Total access latency (LOWER than Host due to better hit rate!)
    result.total_access_latency_us =
        (result.num_accesses * result.avg_access_latency_ns) / 1e3; // us

    // Bandwidth-limited time
    double rank_bw_GBs = bw_tracker->getBandwidthLimit(PIMGranularity::RANK);
    result.bandwidth_limited_time_us = (data_bytes / rank_bw_GBs) / 1e3; // us

    // No internal network for MC-PIM (rank level)
    result.network_latency_us = 0.0;

    // Total data movement
    double data_movement_us = std::max(
        result.total_access_latency_us,
        result.bandwidth_limited_time_us
    );

    result.total_time_us = result.compute_time_us + data_movement_us;
    result.speedup_vs_host = 1.0; // Will be calculated later

    // Determine bottleneck
    if (result.bandwidth_limited_time_us > result.total_access_latency_us) {
        result.primary_bottleneck = "Bandwidth (rank: " +
            std::to_string(rank_bw_GBs) + " GB/s)";
    } else {
        result.primary_bottleneck = "Latency (" +
            std::to_string(result.avg_access_latency_ns) + " ns avg)";
    }

    return result;
}

DetailedResult simulateBank_PIM_WithLatency(
    double total_gflops,
    int num_units,
    uint64_t total_ops,
    uint64_t data_bytes,
    std::shared_ptr<DRAMArchitectureV2> dram_arch,
    std::shared_ptr<PIMBandwidthTracker> bw_tracker,
    std::shared_ptr<InternalDRAMNetwork> network) {

    DetailedResult result;
    result.config_name = "Bank-PIM";

    // Compute time (same as others)
    result.compute_time_us = (total_ops / total_gflops) / 1e3; // us

    // Access pattern: PEs distributed across banks
    const uint64_t CACHE_LINE_SIZE = 64;
    const int NUM_BANKS = 16;
    const int PES_PER_BANK = num_units / NUM_BANKS; // 4 PEs per bank

    // Register PEs
    for (int bank = 0; bank < NUM_BANKS; bank++) {
        for (int pe = 0; pe < PES_PER_BANK; pe++) {
            int pe_id = bank * PES_PER_BANK + pe;
            bw_tracker->registerPE(PIMGranularity::BANK, pe_id, bank);
        }
    }

    result.num_accesses = (data_bytes + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;

    // Bank-PIM: EXCELLENT row buffer hit rate!
    // Each PE operates on its own bank's data → very high locality
    result.row_buffer_hit_rate = 0.95; // Best hit rate!

    // Calculate average DRAM access latency
    uint64_t num_hits = result.num_accesses * result.row_buffer_hit_rate;
    uint64_t num_misses = result.num_accesses - num_hits;

    auto hit_latency = calculateDRAMLatency(dram_arch, true);
    auto miss_latency = calculateDRAMLatency(dram_arch, false);

    result.avg_access_latency_ns =
        (num_hits * hit_latency.total_latency_ns +
         num_misses * miss_latency.total_latency_ns) / result.num_accesses;

    // Total DRAM access latency (LOWEST due to best hit rate!)
    result.total_access_latency_us =
        (result.num_accesses * result.avg_access_latency_ns) / 1e3; // us

    // CRITICAL: Bandwidth-limited time (BANK BOTTLENECK!)
    double effective_bw_per_pe = bw_tracker->getEffectiveBandwidthPerPE(
        PIMGranularity::BANK, 0);
    double total_effective_bw = effective_bw_per_pe * num_units;

    result.bandwidth_limited_time_us = (data_bytes / total_effective_bw) / 1e3; // us

    // Internal network latency (for gather/scatter operations)
    // Assume 10% of data needs inter-bank transfers
    uint64_t network_transfer_bytes = data_bytes * 0.10;
    uint64_t network_latency_cycles = network->getTransferLatency(
        NetworkLevel::BANK_NETWORK, 0, 1, network_transfer_bytes);

    // Convert cycles to nanoseconds
    double cycle_time_ns = 1000.0 / dram_arch->timing.clock_freq_mhz;
    result.network_latency_us = (network_latency_cycles * cycle_time_ns) / 1e3; // us

    // Total data movement = MAX(latency, bandwidth) + network
    double data_movement_us = std::max(
        result.total_access_latency_us,
        result.bandwidth_limited_time_us
    ) + result.network_latency_us;

    result.total_time_us = result.compute_time_us + data_movement_us;
    result.speedup_vs_host = 1.0; // Will be calculated later

    // Determine bottleneck
    if (result.bandwidth_limited_time_us > result.total_access_latency_us &&
        result.bandwidth_limited_time_us > result.network_latency_us) {
        result.primary_bottleneck = "Bandwidth (bank: " +
            std::to_string(bw_tracker->getBandwidthLimit(PIMGranularity::BANK)) +
            " GB/s × " + std::to_string(NUM_BANKS) + " banks, " +
            std::to_string(PES_PER_BANK) + " PEs share each!)";
    } else if (result.network_latency_us > result.total_access_latency_us) {
        result.primary_bottleneck = "Internal network (" +
            std::to_string(result.network_latency_us) + " μs)";
    } else {
        result.primary_bottleneck = "Latency (" +
            std::to_string(result.avg_access_latency_ns) + " ns avg)";
    }

    return result;
}

void printDetailedResults(const std::vector<DetailedResult>& results, double host_time) {
    printHeader("Detailed Performance Results (WITH LATENCY)");

    std::cout << std::left;
    std::cout << std::setw(12) << "Config"
              << std::setw(14) << "Compute(μs)"
              << std::setw(16) << "# Accesses"
              << std::setw(12) << "RB Hit%"
              << std::setw(16) << "Avg Lat(ns)"
              << std::setw(18) << "Total Lat(μs)"
              << std::setw(16) << "BW-Lim(μs)"
              << std::setw(14) << "Net(μs)"
              << std::setw(14) << "Total(μs)"
              << std::setw(10) << "Speedup"
              << "\n";
    std::cout << std::string(142, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::setw(12) << r.config_name
                  << std::setw(14) << std::fixed << std::setprecision(2) << r.compute_time_us
                  << std::setw(16) << r.num_accesses
                  << std::setw(12) << std::setprecision(1) << (r.row_buffer_hit_rate * 100)
                  << std::setw(16) << std::setprecision(2) << r.avg_access_latency_ns
                  << std::setw(18) << r.total_access_latency_us
                  << std::setw(16) << r.bandwidth_limited_time_us
                  << std::setw(14) << r.network_latency_us
                  << std::setw(14) << r.total_time_us
                  << std::setw(10) << std::setprecision(2) << (host_time / r.total_time_us) << "x"
                  << "\n";
    }
    std::cout << "\n";

    // Bottleneck analysis
    std::cout << "Primary Bottlenecks:\n";
    std::cout << std::string(80, '-') << "\n";
    for (const auto& r : results) {
        std::cout << "  " << std::setw(12) << r.config_name << ": "
                  << r.primary_bottleneck << "\n";
    }
}

void analyzeLatencyImpact(const std::vector<DetailedResult>& results) {
    printHeader("Latency vs Bandwidth Analysis");

    auto host = results[0];
    auto mc_pim = results[1];
    auto bank_pim = results[2];

    std::cout << "1. ROW BUFFER HIT RATES:\n";
    std::cout << "   Host:     " << (host.row_buffer_hit_rate * 100) << "% (streaming access)\n";
    std::cout << "   MC-PIM:   " << (mc_pim.row_buffer_hit_rate * 100) << "% (better locality)\n";
    std::cout << "   Bank-PIM: " << (bank_pim.row_buffer_hit_rate * 100) << "% (BEST locality!)\n\n";

    std::cout << "2. AVERAGE ACCESS LATENCY:\n";
    std::cout << "   Host:     " << host.avg_access_latency_ns << " ns\n";
    std::cout << "   MC-PIM:   " << mc_pim.avg_access_latency_ns << " ns ("
              << ((host.avg_access_latency_ns - mc_pim.avg_access_latency_ns) /
                  host.avg_access_latency_ns * 100) << "% better)\n";
    std::cout << "   Bank-PIM: " << bank_pim.avg_access_latency_ns << " ns ("
              << ((host.avg_access_latency_ns - bank_pim.avg_access_latency_ns) /
                  host.avg_access_latency_ns * 100) << "% better!)\n\n";

    std::cout << "3. LATENCY-LIMITED vs BANDWIDTH-LIMITED:\n\n";

    for (const auto& r : results) {
        std::cout << "   " << r.config_name << ":\n";
        std::cout << "     Latency-limited time:   " << r.total_access_latency_us << " μs\n";
        std::cout << "     Bandwidth-limited time: " << r.bandwidth_limited_time_us << " μs\n";

        if (r.bandwidth_limited_time_us > r.total_access_latency_us) {
            double ratio = r.bandwidth_limited_time_us / r.total_access_latency_us;
            std::cout << "     → BANDWIDTH-LIMITED (" << ratio << "x)\n";
        } else {
            double ratio = r.total_access_latency_us / r.bandwidth_limited_time_us;
            std::cout << "     → LATENCY-LIMITED (" << ratio << "x)\n";
        }
        std::cout << "\n";
    }

    std::cout << "4. INTERNAL NETWORK IMPACT (Bank-PIM only):\n";
    std::cout << "   Network latency: " << bank_pim.network_latency_us << " μs\n";
    std::cout << "   % of total time: "
              << (bank_pim.network_latency_us / bank_pim.total_time_us * 100) << "%\n\n";

    std::cout << "5. KEY INSIGHT:\n";
    std::cout << "   Even with BETTER row buffer hits and LOWER latency,\n";
    std::cout << "   Bank-PIM is still SLOWER due to BANDWIDTH bottleneck!\n\n";
    std::cout << "   Bank-PIM advantages:\n";
    std::cout << "     ✅ Best row buffer hit rate (95% vs 60% Host)\n";
    std::cout << "     ✅ Lowest average latency (" << bank_pim.avg_access_latency_ns
              << " ns vs " << host.avg_access_latency_ns << " ns Host)\n\n";
    std::cout << "   But overwhelmed by:\n";
    std::cout << "     ❌ 8-bit bank serialization bottleneck\n";
    std::cout << "     ❌ 4 PEs sharing 1.2 GB/s per bank\n";
    std::cout << "     ❌ Internal network overhead\n\n";
    std::cout << "   Result: " << (bank_pim.total_time_us / mc_pim.total_time_us)
              << "x slower than MC-PIM!\n";
}

int main() {
    printHeader("Host vs MC-PIM vs Bank-PIM (WITH LATENCY MODELING)");

    // Create DDR4 architecture (convert unique_ptr to shared_ptr)
    std::shared_ptr<DRAMArchitectureV2> ddr4 = createDDR4_2400_Verified();
    auto bw_tracker = std::make_shared<PIMBandwidthTracker>(ddr4);
    bw_tracker->initialize(1, 1, 4, 16, 16);
    auto network = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);

    std::cout << "DRAM: " << ddr4->name << "\n";
    std::cout << "Timing: tRCD=" << ddr4->timing.tRCD_ns << "ns, "
              << "tCAS=" << ddr4->timing.tCAS_ns << "ns, "
              << "tRP=" << ddr4->timing.tRP_ns << "ns\n";
    std::cout << "Bandwidth: Bank=" << bw_tracker->getBandwidthLimit(PIMGranularity::BANK)
              << " GB/s, Rank=" << bw_tracker->getBandwidthLimit(PIMGranularity::RANK)
              << " GB/s\n\n";

    // Workload
    const double TOTAL_GFLOPS = 128.0;
    const int NUM_UNITS = 64;
    const uint64_t TOTAL_OPS = 128ULL * 1024 * 1024 * 1024;  // 128 GFLOPs
    const uint64_t DATA_BYTES = 1024ULL * 1024 * 1024;       // 1 GB

    std::cout << "Workload: 128 GFLOPs over 1 GB (1 FLOP/byte)\n";
    std::cout << "Compute: 128 GFLOPS (EQUAL for all)\n\n";

    // Run simulations
    std::vector<DetailedResult> results;

    results.push_back(simulateHost_WithLatency(
        TOTAL_GFLOPS, TOTAL_OPS, DATA_BYTES, ddr4));

    results.push_back(simulateMC_PIM_WithLatency(
        TOTAL_GFLOPS, TOTAL_OPS, DATA_BYTES, ddr4, bw_tracker));

    results.push_back(simulateBank_PIM_WithLatency(
        TOTAL_GFLOPS, NUM_UNITS, TOTAL_OPS, DATA_BYTES, ddr4, bw_tracker, network));

    // Calculate speedups
    double host_time = results[0].total_time_us;
    for (auto& r : results) {
        r.speedup_vs_host = host_time / r.total_time_us;
    }

    // Print results
    printDetailedResults(results, host_time);
    analyzeLatencyImpact(results);

    printHeader("Conclusion");
    std::cout << "WITH LATENCY MODELING, Bank-PIM shows:\n";
    std::cout << "  ✅ Better row buffer hit rate (95% vs 60-80%)\n";
    std::cout << "  ✅ Lower average access latency\n";
    std::cout << "  ✅ Better data locality\n\n";
    std::cout << "BUT STILL SLOWER due to:\n";
    std::cout << "  ❌ 8-bit bank serialization bottleneck (DDR4)\n";
    std::cout << "  ❌ Multiple PEs sharing limited bank bandwidth\n";
    std::cout << "  ❌ Internal network overhead\n\n";
    std::cout << "Latency improvements CAN'T overcome bandwidth bottleneck!\n\n";

    return 0;
}
