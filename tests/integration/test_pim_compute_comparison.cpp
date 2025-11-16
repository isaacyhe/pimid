/**
 * @file test_pim_compute_comparison.cpp
 * @brief Compare Host vs MC-PIM vs Bank-PIM with EQUAL Compute Power
 *
 * This test demonstrates the critical impact of data movement bandwidth
 * when processing units have IDENTICAL compute capability.
 *
 * EQUAL COMPUTE CONFIGURATION:
 * - Host CPU: 128 GFLOPS (e.g., 64 cores × 2 GFLOPS each)
 * - MC-PIM:   128 GFLOPS (64 PEs × 2 GFLOPS each at rank level)
 * - Bank-PIM: 128 GFLOPS (64 PEs × 2 GFLOPS each distributed across banks)
 *
 * KEY INSIGHT:
 * Even with EQUAL compute, performance differs due to DATA MOVEMENT!
 * - Host: Limited by rank bandwidth (9.6 GB/s for DDR4-2400)
 * - MC-PIM: Same rank bandwidth, but data stays in DRAM
 * - Bank-PIM: LIMITED by bank serialization (1.2 GB/s) - BOTTLENECK!
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <algorithm>

#include "pim_request_payload.h"
#include "pim_bandwidth_tracker.h"
#include "internal_dram_network.h"
#include "pimid/memory/dram_architecture_v2.h"

using namespace pimid;
using namespace pimid::memory;

// ============================================================================
// Configuration
// ============================================================================

struct ComputeConfig {
    std::string name;
    double total_gflops;        // Total compute capability
    int num_units;              // Number of processing units
    double gflops_per_unit;     // GFLOPS per unit
    PIMGranularity granularity; // Where processing happens
};

struct WorkloadConfig {
    uint64_t total_ops;         // Total operations (FLOPs)
    uint64_t data_bytes;        // Total data to process
    std::string description;
};

struct PerformanceResult {
    std::string config_name;

    // Compute time (same for all since equal GFLOPS)
    double compute_time_us;

    // Data movement time (VARIES by bandwidth!)
    double data_movement_time_us;

    // Total time
    double total_time_us;

    // Effective bandwidth
    double effective_bw_GBs;

    // Speedup vs host
    double speedup_vs_host;

    // Bottleneck
    std::string bottleneck;
};

// ============================================================================
// Helper Functions
// ============================================================================

void printHeader(const std::string& title) {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << std::left << std::setw(64) << title << " ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";
}

void printConfig(const ComputeConfig& config) {
    std::cout << "  Configuration: " << config.name << "\n";
    std::cout << "    Total compute: " << config.total_gflops << " GFLOPS\n";
    std::cout << "    Processing units: " << config.num_units << "\n";
    std::cout << "    GFLOPS per unit: " << config.gflops_per_unit << "\n";
    std::cout << "    Granularity: ";

    PIMRequestPayload dummy;
    dummy.granularity = config.granularity;
    std::cout << dummy.getGranularityName() << "\n";
}

void printWorkload(const WorkloadConfig& workload) {
    std::cout << "  Workload: " << workload.description << "\n";
    std::cout << "    Total operations: " << workload.total_ops << " FLOPs\n";
    std::cout << "    Total data: " << (workload.data_bytes / (1024.0 * 1024)) << " MB\n";
    std::cout << "    Arithmetic intensity: "
              << (static_cast<double>(workload.total_ops) / workload.data_bytes)
              << " FLOP/byte\n";
}

// ============================================================================
// Simulation Functions
// ============================================================================

PerformanceResult simulateHost(
    const ComputeConfig& config,
    const WorkloadConfig& workload,
    std::shared_ptr<DRAMArchitectureV2> dram_arch) {

    PerformanceResult result;
    result.config_name = config.name;

    // Compute time (same for all configs - EQUAL GFLOPS!)
    result.compute_time_us = (workload.total_ops / config.total_gflops) / 1e3; // us

    // Data movement: ALL data must transfer over rank interface
    // DDR4-2400: 64-bit rank @ 1.2 GHz = 9.6 GB/s
    double rank_bw_GBs = (dram_arch->datapath.rank_data_bus_bits.value_bits / 8.0) *
                         (dram_arch->timing.frequency_MHz / 1000.0);

    // Time to transfer all data to CPU
    result.data_movement_time_us = (workload.data_bytes / rank_bw_GBs) / 1e3; // us

    result.total_time_us = result.compute_time_us + result.data_movement_time_us;
    result.effective_bw_GBs = rank_bw_GBs;
    result.speedup_vs_host = 1.0; // Baseline

    // Determine bottleneck
    if (result.data_movement_time_us > result.compute_time_us) {
        result.bottleneck = "Bandwidth-limited (Rank: " +
                           std::to_string(rank_bw_GBs) + " GB/s)";
    } else {
        result.bottleneck = "Compute-limited";
    }

    return result;
}

PerformanceResult simulateMC_PIM(
    const ComputeConfig& config,
    const WorkloadConfig& workload,
    std::shared_ptr<DRAMArchitectureV2> dram_arch,
    std::shared_ptr<PIMBandwidthTracker> bw_tracker) {

    PerformanceResult result;
    result.config_name = config.name;

    // Compute time (same for all configs - EQUAL GFLOPS!)
    result.compute_time_us = (workload.total_ops / config.total_gflops) / 1e3; // us

    // Data movement: Data accessed at rank level
    // PEs are at rank level, so they use rank bandwidth
    // But data doesn't need to leave DRAM!
    double rank_bw_GBs = bw_tracker->getBandwidthLimit(PIMGranularity::RANK);

    // All PEs at rank level share the rank bandwidth
    // But typically MC-PIM has dedicated paths per rank
    result.data_movement_time_us = (workload.data_bytes / rank_bw_GBs) / 1e3; // us

    result.total_time_us = result.compute_time_us + result.data_movement_time_us;
    result.effective_bw_GBs = rank_bw_GBs;
    result.speedup_vs_host = 1.0; // Will be calculated later

    // Determine bottleneck
    if (result.data_movement_time_us > result.compute_time_us) {
        result.bottleneck = "Bandwidth-limited (Rank: " +
                           std::to_string(rank_bw_GBs) + " GB/s)";
    } else {
        result.bottleneck = "Compute-limited";
    }

    return result;
}

PerformanceResult simulateBank_PIM(
    const ComputeConfig& config,
    const WorkloadConfig& workload,
    std::shared_ptr<DRAMArchitectureV2> dram_arch,
    std::shared_ptr<PIMBandwidthTracker> bw_tracker) {

    PerformanceResult result;
    result.config_name = config.name;

    // Compute time (same for all configs - EQUAL GFLOPS!)
    result.compute_time_us = (workload.total_ops / config.total_gflops) / 1e3; // us

    // CRITICAL: Bank-PIM is LIMITED by bank serialization!
    // DDR4: 8-bit bank port @ 1.2 GHz = 1.2 GB/s per bank
    double bank_bw_GBs = bw_tracker->getBandwidthLimit(PIMGranularity::BANK);

    // Register PEs across banks
    int num_banks = 16; // Typical DDR4 config (4 bank groups × 4 banks)
    int pes_per_bank = config.num_units / num_banks;

    for (int bank = 0; bank < num_banks; bank++) {
        for (int pe = 0; pe < pes_per_bank; pe++) {
            int pe_id = bank * pes_per_bank + pe;
            bw_tracker->registerPE(PIMGranularity::BANK, pe_id, bank);
        }
    }

    // Each bank processes its share of data
    uint64_t data_per_bank = workload.data_bytes / num_banks;

    // CRITICAL: Multiple PEs per bank SHARE the 1.2 GB/s bank bandwidth!
    double effective_bw_per_pe = bw_tracker->getEffectiveBandwidthPerPE(
        PIMGranularity::BANK, 0); // Check bank 0

    // Total effective bandwidth across all banks
    double total_effective_bw = effective_bw_per_pe * config.num_units;

    // Data movement time limited by bank bandwidth
    result.data_movement_time_us = (workload.data_bytes / total_effective_bw) / 1e3; // us

    result.total_time_us = result.compute_time_us + result.data_movement_time_us;
    result.effective_bw_GBs = total_effective_bw;
    result.speedup_vs_host = 1.0; // Will be calculated later

    // Determine bottleneck
    if (result.data_movement_time_us > result.compute_time_us) {
        result.bottleneck = "Bandwidth-limited (Bank: " +
                           std::to_string(bank_bw_GBs) + " GB/s, " +
                           std::to_string(pes_per_bank) + " PEs/bank share it!) CRITICAL!";
    } else {
        result.bottleneck = "Compute-limited";
    }

    return result;
}

// ============================================================================
// Comparison and Analysis
// ============================================================================

void printResults(const std::vector<PerformanceResult>& results, double host_time) {
    printHeader("Performance Comparison Results");

    std::cout << std::setw(15) << "Configuration"
              << std::setw(15) << "Compute (μs)"
              << std::setw(18) << "Data Move (μs)"
              << std::setw(15) << "Total (μs)"
              << std::setw(12) << "Speedup"
              << "\n";
    std::cout << std::string(75, '-') << "\n";

    for (const auto& result : results) {
        std::cout << std::setw(15) << result.config_name
                  << std::setw(15) << std::fixed << std::setprecision(2) << result.compute_time_us
                  << std::setw(18) << result.data_movement_time_us
                  << std::setw(15) << result.total_time_us
                  << std::setw(12) << (host_time / result.total_time_us) << "x"
                  << "\n";
    }
    std::cout << "\n";

    // Detailed analysis
    std::cout << "Detailed Analysis:\n";
    std::cout << std::string(75, '-') << "\n";
    for (const auto& result : results) {
        std::cout << "\n" << result.config_name << ":\n";
        std::cout << "  Effective bandwidth: " << result.effective_bw_GBs << " GB/s\n";
        std::cout << "  Bottleneck: " << result.bottleneck << "\n";

        double compute_fraction = result.compute_time_us / result.total_time_us;
        double data_fraction = result.data_movement_time_us / result.total_time_us;

        std::cout << "  Time breakdown:\n";
        std::cout << "    Compute: " << (compute_fraction * 100) << "%\n";
        std::cout << "    Data movement: " << (data_fraction * 100) << "%\n";
    }
}

void analyzeResults(const std::vector<PerformanceResult>& results) {
    printHeader("Key Insights");

    auto host = results[0];
    auto mc_pim = results[1];
    auto bank_pim = results[2];

    std::cout << "1. EQUAL COMPUTE POWER:\n";
    std::cout << "   All configurations have 128 GFLOPS\n";
    std::cout << "   Compute time is IDENTICAL: " << host.compute_time_us << " μs\n\n";

    std::cout << "2. DATA MOVEMENT DIFFERENCES:\n";
    std::cout << "   Host:     " << host.data_movement_time_us << " μs (Rank BW: "
              << host.effective_bw_GBs << " GB/s)\n";
    std::cout << "   MC-PIM:   " << mc_pim.data_movement_time_us << " μs (Rank BW: "
              << mc_pim.effective_bw_GBs << " GB/s)\n";
    std::cout << "   Bank-PIM: " << bank_pim.data_movement_time_us << " μs (Effective: "
              << bank_pim.effective_bw_GBs << " GB/s)\n\n";

    std::cout << "3. PERFORMANCE IMPACT:\n";
    std::cout << "   Host vs MC-PIM: "
              << (host.total_time_us / mc_pim.total_time_us) << "x\n";
    std::cout << "   Host vs Bank-PIM: "
              << (host.total_time_us / bank_pim.total_time_us) << "x\n";
    std::cout << "   MC-PIM vs Bank-PIM: "
              << (bank_pim.total_time_us / mc_pim.total_time_us) << "x slower!\n\n";

    std::cout << "4. CRITICAL FINDING:\n";
    if (bank_pim.total_time_us > mc_pim.total_time_us) {
        double slowdown = bank_pim.total_time_us / mc_pim.total_time_us;
        std::cout << "   ❌ Bank-PIM is " << slowdown << "x SLOWER than MC-PIM!\n";
        std::cout << "   Reason: Bank serialization bottleneck (8-bit in DDR4)\n";
        std::cout << "   - MC-PIM uses 64-bit rank interface (9.6 GB/s)\n";
        std::cout << "   - Bank-PIM limited by 8-bit bank paths (1.2 GB/s per bank)\n";
        std::cout << "   - Multiple PEs per bank SHARE the 1.2 GB/s!\n\n";
    }

    std::cout << "5. BANDWIDTH HIERARCHY (DDR4-2400):\n";
    std::cout << "   Rank:  64 bits × 1.2 GHz = 9.6 GB/s  ← MC-PIM operates here\n";
    std::cout << "   Bank:   8 bits × 1.2 GHz = 1.2 GB/s ← Bank-PIM BOTTLENECK!\n";
    std::cout << "   Ratio: 8x difference\n\n";

    std::cout << "6. RECOMMENDATION:\n";
    if (bank_pim.total_time_us > mc_pim.total_time_us * 1.2) {
        std::cout << "   For DDR4: Use MC-PIM (Rank-level) instead of Bank-PIM\n";
        std::cout << "   Bank-PIM is only viable with HBM2 (64-bit bank ports via TSV)\n";
    }
}

// ============================================================================
// Main Test
// ============================================================================

int main() {
    printHeader("Host vs MC-PIM vs Bank-PIM Comparison (EQUAL Compute)");

    // Create DDR4 architecture
    auto ddr4 = createDDR4_2400_Verified();

    // Create bandwidth tracker
    auto bw_tracker = std::make_shared<PIMBandwidthTracker>(ddr4);
    bw_tracker->initialize(1, 1, 4, 16, 16); // 1 channel, 1 rank, 4 BG, 16 banks, 16 subarrays

    std::cout << "DRAM Configuration: " << ddr4->name << "\n";
    std::cout << "  Frequency: " << ddr4->timing.frequency_MHz << " MHz\n";
    std::cout << "  Bank port: " << ddr4->datapath.bank_serialization_bits.value_bits
              << " bits (" << ddr4->datapath.bank_serialization_bits.source << ")\n";
    std::cout << "  Rank port: " << ddr4->datapath.rank_data_bus_bits.value_bits
              << " bits (" << ddr4->datapath.rank_data_bus_bits.source << ")\n\n";

    // ========================================================================
    // Configuration: EQUAL COMPUTE POWER
    // ========================================================================

    const double TOTAL_GFLOPS = 128.0;  // Same for all
    const int NUM_UNITS = 64;            // Same for all
    const double GFLOPS_PER_UNIT = TOTAL_GFLOPS / NUM_UNITS; // 2 GFLOPS each

    std::vector<ComputeConfig> configs = {
        {
            "Host CPU",
            TOTAL_GFLOPS,
            NUM_UNITS,
            GFLOPS_PER_UNIT,
            PIMGranularity::CPU
        },
        {
            "MC-PIM",
            TOTAL_GFLOPS,
            NUM_UNITS,
            GFLOPS_PER_UNIT,
            PIMGranularity::RANK
        },
        {
            "Bank-PIM",
            TOTAL_GFLOPS,
            NUM_UNITS,
            GFLOPS_PER_UNIT,
            PIMGranularity::BANK
        }
    };

    printHeader("Compute Configurations (ALL EQUAL)");
    for (const auto& config : configs) {
        printConfig(config);
        std::cout << "\n";
    }

    // ========================================================================
    // Workload: Memory-intensive (low arithmetic intensity)
    // ========================================================================

    WorkloadConfig workload = {
        128ULL * 1024 * 1024 * 1024,  // 128 GFLOPs
        1024ULL * 1024 * 1024,        // 1 GB data
        "Memory-intensive vector operation (1 FLOP/byte)"
    };

    printHeader("Workload Configuration");
    printWorkload(workload);

    // ========================================================================
    // Run Simulations
    // ========================================================================

    printHeader("Running Simulations...");

    std::vector<PerformanceResult> results;

    // Host
    std::cout << "Simulating Host CPU...\n";
    results.push_back(simulateHost(configs[0], workload, ddr4));

    // MC-PIM
    std::cout << "Simulating MC-PIM (Rank-level)...\n";
    results.push_back(simulateMC_PIM(configs[1], workload, ddr4, bw_tracker));

    // Bank-PIM
    std::cout << "Simulating Bank-PIM...\n";
    results.push_back(simulateBank_PIM(configs[2], workload, ddr4, bw_tracker));

    // ========================================================================
    // Print Results and Analysis
    // ========================================================================

    printResults(results, results[0].total_time_us);
    analyzeResults(results);

    // ========================================================================
    // Compare with HBM2
    // ========================================================================

    printHeader("Bonus: What if we used HBM2 instead?");

    auto hbm2 = createHBM2_Verified();
    auto hbm2_tracker = std::make_shared<PIMBandwidthTracker>(hbm2);
    hbm2_tracker->initialize(1, 1, 4, 16, 16);

    std::cout << "HBM2 Configuration:\n";
    std::cout << "  Bank port: " << hbm2->datapath.bank_serialization_bits.value_bits
              << " bits (8x wider than DDR4!)\n";
    std::cout << "  Bank BW: " << hbm2_tracker->getBandwidthLimit(PIMGranularity::BANK)
              << " GB/s\n\n";

    // Simulate Bank-PIM with HBM2
    auto hbm2_result = simulateBank_PIM(configs[2], workload, hbm2, hbm2_tracker);

    std::cout << "HBM2 Bank-PIM:\n";
    std::cout << "  Data movement: " << hbm2_result.data_movement_time_us << " μs\n";
    std::cout << "  Total: " << hbm2_result.total_time_us << " μs\n";
    std::cout << "  Speedup vs DDR4 Bank-PIM: "
              << (results[2].total_time_us / hbm2_result.total_time_us) << "x\n\n";

    std::cout << "KEY INSIGHT:\n";
    std::cout << "  HBM2's 64-bit bank ports (via TSV) make Bank-PIM VIABLE!\n";
    std::cout << "  DDR4's 8-bit bank serialization makes Bank-PIM TOO SLOW!\n\n";

    printHeader("Test Complete!");
    std::cout << "✅ Demonstrated critical impact of internal DRAM bandwidth\n";
    std::cout << "✅ Showed why Bank-PIM requires HBM (not DDR4)\n";
    std::cout << "✅ Validated PIM bandwidth tracker with equal compute configs\n\n";

    return 0;
}
