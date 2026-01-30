/**
 * @file test_port_width_scale.cpp
 * @brief Test port width scaling configuration
 *
 * This test demonstrates that port_width_scale from config files
 * correctly scales bus widths and bandwidth in PE placement constraints.
 */

#include "memory/dram_architecture_v2.h"
#include "address_translation/pe_placement.h"
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace pimid;
using namespace pimid::memory;

void printSeparator(const std::string& title) {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::left << std::setw(63) << title << "║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
}

void testLevel(const std::string& level_name,
               PEPlacementLevel level,
               const DRAMArchitectureV2& arch_baseline,
               const DRAMArchitectureV2& arch_2x) {

    // Get constraints for baseline (scale = 1.0)
    auto constraints_baseline = createPEBusConstraintsFromDRAM(arch_baseline, level);

    // Get constraints for 2x (scale = 2.0)
    auto constraints_2x = createPEBusConstraintsFromDRAM(arch_2x, level);

    std::cout << level_name << " Level:\n";
    std::cout << "  Baseline (scale=1.0):\n";
    std::cout << "    Bus width:    " << std::setw(8) << constraints_baseline.data_bus_width_bits << " bits\n";
    std::cout << "    Bandwidth:    " << std::setw(8) << std::fixed << std::setprecision(1)
              << constraints_baseline.max_bandwidth_gbps << " GB/s\n";

    std::cout << "  Scaled (scale=2.0):\n";
    std::cout << "    Bus width:    " << std::setw(8) << constraints_2x.data_bus_width_bits << " bits";
    std::cout << "  (2x = " << constraints_baseline.data_bus_width_bits * 2 << ")\n";
    std::cout << "    Bandwidth:    " << std::setw(8) << std::fixed << std::setprecision(1)
              << constraints_2x.max_bandwidth_gbps << " GB/s";
    std::cout << "  (2x = " << constraints_baseline.max_bandwidth_gbps * 2.0 << ")\n";

    // Verify scaling is correct
    assert(constraints_2x.data_bus_width_bits == constraints_baseline.data_bus_width_bits * 2);
    assert(std::abs(constraints_2x.max_bandwidth_gbps - constraints_baseline.max_bandwidth_gbps * 2.0) < 0.01);

    std::cout << "  ✓ Scaling verified!\n\n";
}

int main() {
    printSeparator("Port Width Scale Configuration Test");

    std::cout << "This test demonstrates that port_width_scale configuration\n";
    std::cout << "correctly scales bus widths and bandwidth across all PE\n";
    std::cout << "placement levels.\n";

    // Test 1: DDR4 baseline vs 2x scaled
    printSeparator("Test 1: DDR4-2400 Baseline vs 2x Scaled");

    auto ddr4_baseline = createDDR4_2400_Verified();
    auto ddr4_2x = createDDR4_2400_Verified(2.0);

    std::cout << "Configuration:\n";
    std::cout << "  DRAM Type:        DDR4-2400\n";
    std::cout << "  Baseline scale:   " << ddr4_baseline->port_width_scale << "\n";
    std::cout << "  Test scale:       " << ddr4_2x->port_width_scale << "\n";
    std::cout << "\n";

    testLevel("SUBARRAY", PEPlacementLevel::SUBARRAY, *ddr4_baseline, *ddr4_2x);
    testLevel("BANK", PEPlacementLevel::BANK, *ddr4_baseline, *ddr4_2x);
    testLevel("CHIP", PEPlacementLevel::CHIP, *ddr4_baseline, *ddr4_2x);
    testLevel("RANK", PEPlacementLevel::RANK, *ddr4_baseline, *ddr4_2x);

    // Test 2: HBM2 baseline vs 2x scaled
    printSeparator("Test 2: HBM2 Baseline vs 2x Scaled");

    auto hbm2_baseline = createHBM2_Verified();
    auto hbm2_2x = createHBM2_Verified(2.0);

    std::cout << "Configuration:\n";
    std::cout << "  DRAM Type:        HBM2\n";
    std::cout << "  Baseline scale:   " << hbm2_baseline->port_width_scale << "\n";
    std::cout << "  Test scale:       " << hbm2_2x->port_width_scale << "\n";
    std::cout << "\n";

    testLevel("BANK", PEPlacementLevel::BANK, *hbm2_baseline, *hbm2_2x);
    testLevel("LOGIC_DIE", PEPlacementLevel::LOGIC_DIE, *hbm2_baseline, *hbm2_2x);

    // Test 3: Different scaling factors
    printSeparator("Test 3: Multiple Scaling Factors (DDR4)");

    std::cout << "Testing different scaling values for BANK level:\n\n";

    double scales[] = {0.5, 1.0, 2.0, 4.0};
    auto ddr4_ref = createDDR4_2400_Verified();
    auto ref_constraints = createPEBusConstraintsFromDRAM(*ddr4_ref, PEPlacementLevel::BANK);

    std::cout << "Scale | Bus Width (bits) | Bandwidth (GB/s)\n";
    std::cout << "------|------------------|------------------\n";

    for (double scale : scales) {
        auto arch = createDDR4_2400_Verified(scale);
        auto constraints = createPEBusConstraintsFromDRAM(*arch, PEPlacementLevel::BANK);

        std::cout << std::setw(5) << std::fixed << std::setprecision(1) << scale << " | ";
        std::cout << std::setw(16) << constraints.data_bus_width_bits << " | ";
        std::cout << std::setw(16) << std::fixed << std::setprecision(1)
                  << constraints.max_bandwidth_gbps << "\n";

        // Verify scaling
        uint64_t expected_width = static_cast<uint64_t>(ref_constraints.data_bus_width_bits * scale);
        double expected_bw = ref_constraints.max_bandwidth_gbps * scale;
        assert(constraints.data_bus_width_bits == expected_width);
        assert(std::abs(constraints.max_bandwidth_gbps - expected_bw) < 0.01);
    }

    std::cout << "\n✓ All scaling factors verified!\n";

    // Test 4: Config file simulation
    printSeparator("Test 4: Simulating Config File Loading");

    std::cout << "Simulating loading from config/memory_config.yaml:\n";
    std::cout << "  port_width_scale: 1.0  (default/production)\n\n";

    double config_scale = 1.0;  // Would be loaded from YAML
    auto ddr4_from_config = createDDR4_2400_Verified(config_scale);
    auto config_constraints = createPEBusConstraintsFromDRAM(
        *ddr4_from_config, PEPlacementLevel::BANK);

    std::cout << "Resulting PE Bus Constraints (BANK level):\n";
    std::cout << "  Bus width:      " << config_constraints.data_bus_width_bits << " bits\n";
    std::cout << "  Bandwidth:      " << config_constraints.max_bandwidth_gbps << " GB/s\n";
    std::cout << "  Shared PEs:     " << config_constraints.shared_bus_pes << "\n";
    std::cout << "  Dedicated bus:  " << (config_constraints.has_dedicated_bus ? "Yes" : "No") << "\n";

    std::cout << "\nTo test 2x wider buses, set in config file:\n";
    std::cout << "  port_width_scale: 2.0\n";

    // Summary
    printSeparator("Summary");

    std::cout << "✓ Port width scaling works correctly!\n";
    std::cout << "✓ All bus widths scale proportionally\n";
    std::cout << "✓ All bandwidths scale proportionally\n";
    std::cout << "✓ Both DDR4 and HBM2 architectures supported\n";
    std::cout << "✓ Multiple scaling factors tested (0.5x to 4x)\n";
    std::cout << "\n";

    std::cout << "Configuration Usage:\n";
    std::cout << "  1. Edit config/memory_config.yaml\n";
    std::cout << "  2. Set port_width_scale to desired value\n";
    std::cout << "  3. Load config and create DRAM architecture:\n";
    std::cout << "       auto arch = createDDR4_2400_Verified(config_scale);\n";
    std::cout << "  4. Use with PE placement:\n";
    std::cout << "       auto constraints = createPEBusConstraintsFromDRAM(*arch, level);\n";
    std::cout << "\n";

    std::cout << "All tests passed! ✓\n\n";

    return 0;
}
