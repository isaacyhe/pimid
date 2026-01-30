/**
 * @file test_switch_calculation.cpp
 * @brief Unit test for dynamic switch count calculation
 */

#include "memory_models/include/internal_dram_network.h"
#include <iostream>
#include <cassert>

using namespace pimid;

void test_switch_levels() {
    std::cout << "\n=== Testing Switch Level Calculation ===" << std::endl;

    auto network = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);

    int num_levels = network->getNumberOfSwitchLevels();
    std::cout << "Number of switch levels: " << num_levels << std::endl;

    assert(num_levels == 6 && "Should have 6 switch levels (L0-L5)");
    std::cout << "✓ Switch level count correct" << std::endl;
}

void test_ddr4_switches() {
    std::cout << "\n=== Testing DDR4 Switch Counts ===" << std::endl;
    std::cout << "Configuration: 1 channel, 1 rank, 8 chips, 4 BGs/chip, 4 banks/BG" << std::endl;

    auto network = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);

    int num_channels = 1;
    int ranks_per_channel = 1;

    int total_switches = network->getTotalNumberOfSwitches(num_channels, ranks_per_channel);

    // Expected calculation:
    // num_ranks = 1 * 1 = 1
    // num_chips = 1 * 8 = 8
    // num_bgs = 8 * 4 = 32
    // L0: 32 (1 per BG)
    // L1: 32 (1 per BG)
    // L2: 8 (1 per chip)
    // L3: 1 (1 per rank)
    // L4: 1 (1 per channel)
    // L5: 1 (root)
    // Total: 32 + 32 + 8 + 1 + 1 + 1 = 75

    int expected_total = 75;
    std::cout << "\nExpected total: " << expected_total << std::endl;
    std::cout << "Actual total: " << total_switches << std::endl;

    assert(total_switches == expected_total && "DDR4 switch count mismatch");
    std::cout << "✓ DDR4 switch count correct" << std::endl;
}

void test_hbm2_switches() {
    std::cout << "\n=== Testing HBM2 Switch Counts ===" << std::endl;
    std::cout << "Configuration: 8 channels, 2 ranks, 2 chips, 4 BGs/chip, 4 banks/BG" << std::endl;

    auto network = createInternalDRAMNetwork("HBM2", 16, 4, 4, 2);

    int num_channels = 8;
    int ranks_per_channel = 2;

    int total_switches = network->getTotalNumberOfSwitches(num_channels, ranks_per_channel);

    // Expected calculation:
    // num_ranks = 8 * 2 = 16
    // num_chips = 16 * 2 = 32
    // num_bgs = 32 * 4 = 128
    // L0: 128 (1 per BG)
    // L1: 128 (1 per BG)
    // L2: 32 (1 per chip)
    // L3: 16 (1 per rank)
    // L4: 8 (1 per channel)
    // L5: 1 (root)
    // Total: 128 + 128 + 32 + 16 + 8 + 1 = 313

    int expected_total = 313;
    std::cout << "\nExpected total: " << expected_total << std::endl;
    std::cout << "Actual total: " << total_switches << std::endl;

    assert(total_switches == expected_total && "HBM2 switch count mismatch");
    std::cout << "✓ HBM2 switch count correct" << std::endl;
}

void test_per_level_counts() {
    std::cout << "\n=== Testing Per-Level Switch Counts ===" << std::endl;

    auto network = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);

    int num_channels = 1;
    int ranks_per_channel = 1;

    std::cout << "\nSwitch counts by level:" << std::endl;
    for (int level = 0; level <= 5; level++) {
        int count = network->getNumberOfSwitchesAtLevel(level, num_channels, ranks_per_channel);
        std::cout << "  L" << level << ": " << count << " switches" << std::endl;
    }

    // Verify individual level counts
    assert(network->getNumberOfSwitchesAtLevel(0, num_channels, ranks_per_channel) == 32 && "L0 count incorrect");
    assert(network->getNumberOfSwitchesAtLevel(1, num_channels, ranks_per_channel) == 32 && "L1 count incorrect");
    assert(network->getNumberOfSwitchesAtLevel(2, num_channels, ranks_per_channel) == 8 && "L2 count incorrect");
    assert(network->getNumberOfSwitchesAtLevel(3, num_channels, ranks_per_channel) == 1 && "L3 count incorrect");
    assert(network->getNumberOfSwitchesAtLevel(4, num_channels, ranks_per_channel) == 1 && "L4 count incorrect");
    assert(network->getNumberOfSwitchesAtLevel(5, num_channels, ranks_per_channel) == 1 && "L5 count incorrect");

    std::cout << "✓ All per-level counts correct" << std::endl;
}

void test_multi_channel() {
    std::cout << "\n=== Testing Multi-Channel Configuration ===" << std::endl;
    std::cout << "Configuration: 4 channels, 2 ranks, 8 chips, 4 BGs/chip, 4 banks/BG" << std::endl;

    auto network = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);

    int num_channels = 4;
    int ranks_per_channel = 2;

    int total_switches = network->getTotalNumberOfSwitches(num_channels, ranks_per_channel);

    // Expected calculation:
    // num_ranks = 4 * 2 = 8
    // num_chips = 8 * 8 = 64
    // num_bgs = 64 * 4 = 256
    // L0: 256
    // L1: 256
    // L2: 64
    // L3: 8
    // L4: 4
    // L5: 1
    // Total: 256 + 256 + 64 + 8 + 4 + 1 = 589

    int expected_total = 589;
    std::cout << "\nExpected total: " << expected_total << std::endl;
    std::cout << "Actual total: " << total_switches << std::endl;

    assert(total_switches == expected_total && "Multi-channel switch count mismatch");
    std::cout << "✓ Multi-channel switch count correct" << std::endl;
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║   Switch Count Calculation Test Suite                       ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;

    try {
        test_switch_levels();
        test_ddr4_switches();
        test_hbm2_switches();
        test_per_level_counts();
        test_multi_channel();

        std::cout << "\n" << std::string(64, '=') << std::endl;
        std::cout << "All tests passed! ✓" << std::endl;
        std::cout << std::string(64, '=') << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
