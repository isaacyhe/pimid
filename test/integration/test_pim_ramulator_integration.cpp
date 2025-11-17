/**
 * @file test_pim_ramulator_integration.cpp
 * @brief Comprehensive Verification Tests for PIM-Ramulator Integration
 *
 * This test validates:
 * 1. Bandwidth limits match verified DRAM specifications
 * 2. Port bitwidths are correct for each DRAM type
 * 3. Bandwidth contention works correctly (multiple PEs)
 * 4. Internal network is invoked for fine-grained PIM
 * 5. DRAM correctness is preserved (timing model intact)
 * 6. Different configurations (DDR4, HBM2, various granularities)
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cassert>
#include <cmath>

#include "ramulator_wrapper.h"
#include "pim_request_payload.h"
#include "pim_bandwidth_tracker.h"
#include "internal_dram_network.h"
#include "pimid/memory/dram_architecture_v2.h"

using namespace pimid;

// Test result tracking
struct TestResult {
    std::string test_name;
    bool passed;
    std::string message;
};

std::vector<TestResult> test_results;

void reportTest(const std::string& name, bool passed, const std::string& message = "") {
    test_results.push_back({name, passed, message});
    std::cout << (passed ? "✅ PASS" : "❌ FAIL") << ": " << name;
    if (!message.empty()) {
        std::cout << " - " << message;
    }
    std::cout << "\n";
}

// ============================================================================
// Test 1: Verify DDR4 Bandwidth Limits Match Specifications
// ============================================================================

void test_DDR4_BandwidthLimits() {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Test 1: DDR4 Bandwidth Limits (Verified Specifications)         ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    RamulatorWrapper ram("");
    ram.initialize();
    ram.enablePIMSupport("DDR4");

    // DDR4-2400 @ 1.2 GHz
    const double CLOCK_GHz = 1.2;

    // Expected values from dram_architecture_v2.h
    const int EXPECTED_BANK_PORT_BITS = 8;          // Bank serialization (ESTIMATED)
    const int EXPECTED_RANK_PORT_BITS = 64;         // Rank interface (VERIFIED)
    const double EXPECTED_BANK_BW = (8.0 / 8.0) * CLOCK_GHz;   // 1.2 GB/s
    const double EXPECTED_RANK_BW = (64.0 / 8.0) * CLOCK_GHz;  // 9.6 GB/s

    // Test bank port bitwidth
    int actual_bank_port = ram.getPortBitwidth(PIMGranularity::BANK);
    bool bank_port_correct = (actual_bank_port == EXPECTED_BANK_PORT_BITS);
    reportTest("DDR4 Bank Port Bitwidth",
               bank_port_correct,
               "Expected: " + std::to_string(EXPECTED_BANK_PORT_BITS) + " bits, " +
               "Actual: " + std::to_string(actual_bank_port) + " bits");

    // Test rank port bitwidth
    int actual_rank_port = ram.getPortBitwidth(PIMGranularity::RANK);
    bool rank_port_correct = (actual_rank_port == EXPECTED_RANK_PORT_BITS);
    reportTest("DDR4 Rank Port Bitwidth",
               rank_port_correct,
               "Expected: " + std::to_string(EXPECTED_RANK_PORT_BITS) + " bits, " +
               "Actual: " + std::to_string(actual_rank_port) + " bits");

    // Test bank bandwidth limit
    double actual_bank_bw = ram.getBandwidthLimit(PIMGranularity::BANK);
    bool bank_bw_correct = std::abs(actual_bank_bw - EXPECTED_BANK_BW) < 0.01;
    reportTest("DDR4 Bank Bandwidth Limit",
               bank_bw_correct,
               "Expected: " + std::to_string(EXPECTED_BANK_BW) + " GB/s, " +
               "Actual: " + std::to_string(actual_bank_bw) + " GB/s");

    // Test rank bandwidth limit
    double actual_rank_bw = ram.getBandwidthLimit(PIMGranularity::RANK);
    bool rank_bw_correct = std::abs(actual_rank_bw - EXPECTED_RANK_BW) < 0.01;
    reportTest("DDR4 Rank Bandwidth Limit",
               rank_bw_correct,
               "Expected: " + std::to_string(EXPECTED_RANK_BW) + " GB/s, " +
               "Actual: " + std::to_string(actual_rank_bw) + " GB/s");

    // Verify critical bottleneck: Bank BW << Rank BW
    double ratio = actual_rank_bw / actual_bank_bw;
    bool bottleneck_correct = (ratio >= 7.5 && ratio <= 8.5);  // Should be ~8x
    reportTest("DDR4 Bank is Bottleneck (Rank/Bank BW ratio)",
               bottleneck_correct,
               "Ratio: " + std::to_string(ratio) + "x (expected ~8x)");
}

// ============================================================================
// Test 2: Verify HBM2 Bandwidth Limits (TSV Advantage)
// ============================================================================

void test_HBM2_BandwidthLimits() {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Test 2: HBM2 Bandwidth Limits (TSV Advantage)                   ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    RamulatorWrapper ram("");
    ram.initialize();
    ram.enablePIMSupport("HBM2");

    // HBM2 @ 1.0 GHz
    const double CLOCK_GHz = 1.0;

    // Expected values from dram_architecture_v2.h
    const int EXPECTED_BANK_PORT_BITS = 64;         // Bank via TSV (INFERRED)
    const int EXPECTED_CHANNEL_PORT_BITS = 128;     // Channel (VERIFIED)
    const double EXPECTED_BANK_BW = (64.0 / 8.0) * CLOCK_GHz;   // 8 GB/s
    const double EXPECTED_CHANNEL_BW = (128.0 / 8.0) * CLOCK_GHz; // 16 GB/s

    // Test bank port bitwidth
    int actual_bank_port = ram.getPortBitwidth(PIMGranularity::BANK);
    bool bank_port_correct = (actual_bank_port == EXPECTED_BANK_PORT_BITS);
    reportTest("HBM2 Bank Port Bitwidth (TSV)",
               bank_port_correct,
               "Expected: " + std::to_string(EXPECTED_BANK_PORT_BITS) + " bits, " +
               "Actual: " + std::to_string(actual_bank_port) + " bits");

    // Test channel/rank port bitwidth
    int actual_channel_port = ram.getPortBitwidth(PIMGranularity::RANK);
    bool channel_port_correct = (actual_channel_port == EXPECTED_CHANNEL_PORT_BITS);
    reportTest("HBM2 Channel Port Bitwidth",
               channel_port_correct,
               "Expected: " + std::to_string(EXPECTED_CHANNEL_PORT_BITS) + " bits, " +
               "Actual: " + std::to_string(actual_channel_port) + " bits");

    // Test bank bandwidth limit
    double actual_bank_bw = ram.getBandwidthLimit(PIMGranularity::BANK);
    bool bank_bw_correct = std::abs(actual_bank_bw - EXPECTED_BANK_BW) < 0.1;
    reportTest("HBM2 Bank Bandwidth Limit",
               bank_bw_correct,
               "Expected: " + std::to_string(EXPECTED_BANK_BW) + " GB/s, " +
               "Actual: " + std::to_string(actual_bank_bw) + " GB/s");

    // Compare DDR4 vs HBM2 bank bandwidth
    RamulatorWrapper ddr4("");
    ddr4.initialize();
    ddr4.enablePIMSupport("DDR4");
    double ddr4_bank_bw = ddr4.getBandwidthLimit(PIMGranularity::BANK);

    double advantage = actual_bank_bw / ddr4_bank_bw;
    bool tsv_advantage = (advantage >= 6.0 && advantage <= 7.5);
    reportTest("HBM2 TSV Advantage (vs DDR4 bank BW)",
               tsv_advantage,
               "HBM2/DDR4 ratio: " + std::to_string(advantage) + "x (expected ~6.7x)");
}

// ============================================================================
// Test 3: Verify Bandwidth Contention (Multiple PEs)
// ============================================================================

void test_BandwidthContention() {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Test 3: Bandwidth Contention (Multiple PEs Sharing)             ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    RamulatorWrapper ram("");
    ram.initialize();
    ram.enablePIMSupport("DDR4");

    const int BANK_ID = 0;
    const double TOTAL_BANK_BW = 1.2;  // GB/s for DDR4 bank

    // Test 1: Single PE gets full bandwidth
    ram.registerPE(PIMGranularity::BANK, 0, BANK_ID);
    double bw_1pe = ram.getEffectiveBandwidthPerPE(PIMGranularity::BANK, BANK_ID);
    bool single_pe_correct = std::abs(bw_1pe - TOTAL_BANK_BW) < 0.01;
    reportTest("Single PE gets full bandwidth",
               single_pe_correct,
               "Expected: " + std::to_string(TOTAL_BANK_BW) + " GB/s, " +
               "Actual: " + std::to_string(bw_1pe) + " GB/s");

    // Test 2: Two PEs share bandwidth (50% each)
    ram.registerPE(PIMGranularity::BANK, 1, BANK_ID);
    double bw_2pe = ram.getEffectiveBandwidthPerPE(PIMGranularity::BANK, BANK_ID);
    double expected_2pe = TOTAL_BANK_BW / 2.0;
    bool two_pe_correct = std::abs(bw_2pe - expected_2pe) < 0.01;
    reportTest("Two PEs share bandwidth equally",
               two_pe_correct,
               "Expected: " + std::to_string(expected_2pe) + " GB/s, " +
               "Actual: " + std::to_string(bw_2pe) + " GB/s");

    // Test 3: Four PEs share bandwidth (25% each)
    ram.registerPE(PIMGranularity::BANK, 2, BANK_ID);
    ram.registerPE(PIMGranularity::BANK, 3, BANK_ID);
    double bw_4pe = ram.getEffectiveBandwidthPerPE(PIMGranularity::BANK, BANK_ID);
    double expected_4pe = TOTAL_BANK_BW / 4.0;
    bool four_pe_correct = std::abs(bw_4pe - expected_4pe) < 0.01;
    reportTest("Four PEs share bandwidth equally",
               four_pe_correct,
               "Expected: " + std::to_string(expected_4pe) + " GB/s (300 MB/s), " +
               "Actual: " + std::to_string(bw_4pe) + " GB/s");

    // Test 4: Different banks don't interfere
    RamulatorWrapper ram2("");
    ram2.initialize();
    ram2.enablePIMSupport("DDR4");
    ram2.registerPE(PIMGranularity::BANK, 0, 0);  // PE 0 on bank 0
    ram2.registerPE(PIMGranularity::BANK, 1, 1);  // PE 1 on bank 1
    double bw_bank0 = ram2.getEffectiveBandwidthPerPE(PIMGranularity::BANK, 0);
    double bw_bank1 = ram2.getEffectiveBandwidthPerPE(PIMGranularity::BANK, 1);
    bool no_interference = (std::abs(bw_bank0 - TOTAL_BANK_BW) < 0.01) &&
                          (std::abs(bw_bank1 - TOTAL_BANK_BW) < 0.01);
    reportTest("Different banks don't interfere",
               no_interference,
               "Bank 0: " + std::to_string(bw_bank0) + " GB/s, " +
               "Bank 1: " + std::to_string(bw_bank1) + " GB/s");
}

// ============================================================================
// Test 4: Verify Internal Network Configuration
// ============================================================================

void test_InternalNetwork() {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Test 4: Internal DRAM Network Configuration                     ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    // Test DDR4 internal network
    auto ddr4_network = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);
    bool ddr4_created = (ddr4_network != nullptr);
    reportTest("DDR4 internal network created", ddr4_created);

    // Test HBM2 internal network
    auto hbm2_network = createInternalDRAMNetwork("HBM2", 16, 4, 4, 8);
    bool hbm2_created = (hbm2_network != nullptr);
    reportTest("HBM2 internal network created", hbm2_created);

    // Test network latency difference (HBM2 should be faster due to TSV)
    if (ddr4_created && hbm2_created) {
        uint64_t ddr4_latency = ddr4_network->getTransferLatency(
            NetworkLevel::BANK_NETWORK, 0, 1, 4096);
        uint64_t hbm2_latency = hbm2_network->getTransferLatency(
            NetworkLevel::BANK_NETWORK, 0, 1, 4096);

        bool hbm2_faster = (hbm2_latency < ddr4_latency);
        reportTest("HBM2 network faster than DDR4 (TSV advantage)",
                   hbm2_faster,
                   "DDR4: " + std::to_string(ddr4_latency) + " cycles, " +
                   "HBM2: " + std::to_string(hbm2_latency) + " cycles");

        double speedup = static_cast<double>(ddr4_latency) / hbm2_latency;
        bool realistic_speedup = (speedup >= 3.0 && speedup <= 10.0);
        reportTest("HBM2 network speedup is realistic",
                   realistic_speedup,
                   "Speedup: " + std::to_string(speedup) + "x");
    }
}

// ============================================================================
// Test 5: Verify PIM Request Processing
// ============================================================================

void test_PIMRequestProcessing() {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Test 5: PIM Request Processing                                  ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    RamulatorWrapper ram("");
    ram.initialize();
    ram.enablePIMSupport("DDR4");
    ram.registerPE(PIMGranularity::BANK, 0, 0);

    // Test normal read/write (no PIM payload)
    bool normal_send_ok = ram.send(0x1000, MemoryRequestType::READ);
    reportTest("Normal read request accepted", normal_send_ok);

    // Test PIM request (with payload)
    PIMRequestPayload pim_payload;
    pim_payload.granularity = PIMGranularity::BANK;
    pim_payload.operation = PIMOperationType::PIM_COMPUTE;
    pim_payload.pe_id = 0;
    pim_payload.target_bank = 0;
    pim_payload.data_bytes = 4096;

    bool pim_request_accepted = false;
    bool callback_invoked = false;

    bool pim_send_ok = ram.sendPIM(0x2000, MemoryRequestType::READ, &pim_payload,
        [&callback_invoked](Address addr) {
            callback_invoked = true;
        });
    pim_request_accepted = pim_send_ok;

    reportTest("PIM request accepted", pim_request_accepted);

    // Simulate some cycles
    for (int i = 0; i < 100; i++) {
        ram.tick();
    }

    // Check if latency was calculated
    bool latency_calculated = (pim_payload.data_movement_cycles > 0);
    reportTest("Data movement latency calculated",
               latency_calculated,
               "Latency: " + std::to_string(pim_payload.data_movement_cycles) + " cycles");
}

// ============================================================================
// Test 6: Verify Different PIM Granularities
// ============================================================================

void test_PIMGranularities() {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Test 6: Different PIM Granularities                             ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    RamulatorWrapper ram("");
    ram.initialize();
    ram.enablePIMSupport("DDR4");

    // Test bandwidth hierarchy: Subarray > Bank > Rank
    double subarray_bw = ram.getBandwidthLimit(PIMGranularity::SUBARRAY);
    double bank_bw = ram.getBandwidthLimit(PIMGranularity::BANK);
    double rank_bw = ram.getBandwidthLimit(PIMGranularity::RANK);

    // Subarray should have higher BW than bank (GSA is wide)
    bool subarray_wider = (subarray_bw > bank_bw);
    reportTest("Subarray BW > Bank BW (GSA is wider)",
               subarray_wider,
               "Subarray: " + std::to_string(subarray_bw) + " GB/s, " +
               "Bank: " + std::to_string(bank_bw) + " GB/s");

    // Rank should have higher BW than bank (64-bit vs 8-bit)
    bool rank_wider = (rank_bw > bank_bw);
    reportTest("Rank BW > Bank BW (first wide interface)",
               rank_wider,
               "Rank: " + std::to_string(rank_bw) + " GB/s, " +
               "Bank: " + std::to_string(bank_bw) + " GB/s");

    // Bank is the bottleneck in DDR4!
    bool bank_is_bottleneck = (bank_bw < subarray_bw) && (bank_bw < rank_bw);
    reportTest("Bank is the bottleneck in DDR4",
               bank_is_bottleneck,
               "Bank: " + std::to_string(bank_bw) + " GB/s " +
               "(vs Subarray: " + std::to_string(subarray_bw) + " GB/s, " +
               "Rank: " + std::to_string(rank_bw) + " GB/s)");
}

// ============================================================================
// Test 7: Verify DRAM Correctness (Non-Intrusive Integration)
// ============================================================================

void test_DRAMCorrectness() {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Test 7: DRAM Correctness (Non-Intrusive Integration)            ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    // Test 1: Normal requests work without PIM enabled
    RamulatorWrapper ram_nopim("");
    ram_nopim.initialize();
    bool normal_works = ram_nopim.send(0x1000, MemoryRequestType::READ);
    reportTest("Normal requests work without PIM enabled", normal_works);

    // Test 2: Normal requests still work with PIM enabled
    RamulatorWrapper ram_withpim("");
    ram_withpim.initialize();
    ram_withpim.enablePIMSupport("DDR4");
    bool normal_with_pim = ram_withpim.send(0x1000, MemoryRequestType::READ);
    reportTest("Normal requests work with PIM enabled", normal_with_pim);

    // Test 3: Can tick both versions
    bool can_tick_nopim = true;
    bool can_tick_withpim = true;
    try {
        for (int i = 0; i < 10; i++) {
            ram_nopim.tick();
        }
    } catch (...) {
        can_tick_nopim = false;
    }

    try {
        for (int i = 0; i < 10; i++) {
            ram_withpim.tick();
        }
    } catch (...) {
        can_tick_withpim = false;
    }

    reportTest("Can tick without PIM", can_tick_nopim);
    reportTest("Can tick with PIM enabled", can_tick_withpim);

    // Test 4: Statistics work
    bool stats_work = true;
    try {
        ram_withpim.printStats();
    } catch (...) {
        stats_work = false;
    }
    reportTest("Statistics work with PIM enabled", stats_work);
}

// ============================================================================
// Test 8: Comprehensive Configuration Matrix
// ============================================================================

void test_ConfigurationMatrix() {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Test 8: Configuration Matrix (Various Combinations)             ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    struct Config {
        std::string dram_type;
        PIMGranularity granularity;
        int num_pes;
        std::string description;
    };

    std::vector<Config> configs = {
        {"DDR4", PIMGranularity::BANK, 1, "DDR4 Single Bank-PE"},
        {"DDR4", PIMGranularity::BANK, 4, "DDR4 Four Bank-PEs (contention)"},
        {"DDR4", PIMGranularity::RANK, 1, "DDR4 Rank-PE"},
        {"HBM2", PIMGranularity::BANK, 1, "HBM2 Single Bank-PE (TSV)"},
        {"HBM2", PIMGranularity::BANK, 4, "HBM2 Four Bank-PEs"},
        {"HBM2", PIMGranularity::RANK, 1, "HBM2 Channel-PE"},
    };

    int passed = 0;
    for (const auto& config : configs) {
        RamulatorWrapper ram("");
        ram.initialize();

        bool success = true;
        try {
            ram.enablePIMSupport(config.dram_type);

            for (int i = 0; i < config.num_pes; i++) {
                ram.registerPE(config.granularity, i, 0);
            }

            double bw = ram.getEffectiveBandwidthPerPE(config.granularity, 0);
            int port_bits = ram.getPortBitwidth(config.granularity);

            // Verify bandwidth is positive
            success = (bw > 0.0) && (port_bits > 0);

        } catch (...) {
            success = false;
        }

        if (success) passed++;
        reportTest(config.description, success);
    }

    bool all_configs_work = (passed == configs.size());
    reportTest("All configurations work",
               all_configs_work,
               std::to_string(passed) + "/" + std::to_string(configs.size()) + " passed");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                                    ║\n";
    std::cout << "║    PIM-Ramulator Integration Verification Tests                   ║\n";
    std::cout << "║    Detailed Bandwidth Modeling with Internal Networks             ║\n";
    std::cout << "║                                                                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════════╝\n";

    // Run all tests
    test_DDR4_BandwidthLimits();
    test_HBM2_BandwidthLimits();
    test_BandwidthContention();
    test_InternalNetwork();
    test_PIMRequestProcessing();
    test_PIMGranularities();
    test_DRAMCorrectness();
    test_ConfigurationMatrix();

    // Print summary
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ TEST SUMMARY                                                       ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════════╝\n\n";

    int total = test_results.size();
    int passed = 0;
    int failed = 0;

    for (const auto& result : test_results) {
        if (result.passed) {
            passed++;
        } else {
            failed++;
            std::cout << "❌ FAILED: " << result.test_name << "\n";
            if (!result.message.empty()) {
                std::cout << "   " << result.message << "\n";
            }
        }
    }

    std::cout << "\nTotal Tests:  " << total << "\n";
    std::cout << "Passed:       " << passed << " (" << (100.0 * passed / total) << "%)\n";
    std::cout << "Failed:       " << failed << "\n";

    if (failed == 0) {
        std::cout << "\n✅ ALL TESTS PASSED! PIM-Ramulator integration verified.\n\n";
        return 0;
    } else {
        std::cout << "\n❌ SOME TESTS FAILED. Please review failures above.\n\n";
        return 1;
    }
}
