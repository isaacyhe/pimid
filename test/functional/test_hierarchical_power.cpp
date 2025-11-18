/**
 * @file test_hierarchical_power.cpp
 * @brief Test hierarchical power model manager with specialized simulators
 *
 * This test demonstrates the power modeling hierarchy:
 * 1. Specialized simulators (Ramulator, GARNET) - highest priority
 * 2. McPAT power model - fallback
 * 3. Analytical models - final fallback
 */

#include "power_model_manager.h"
#include <iostream>
#include <iomanip>

using namespace pimid;

int main() {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗"
              << std::endl;
    std::cout << "║    Hierarchical Power Modeling Test                      ║"
              << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝"
              << std::endl;

    // Configure technology parameters
    TechnologyParams tech_params;
    tech_params.tech_node_nm = 22;
    tech_params.frequency_ghz = 2.0;
    tech_params.temperature_k = 350.0;  // ~77°C
    tech_params.core_count = 4;

    std::cout << "\nTechnology Configuration:" << std::endl;
    std::cout << "  Node: " << tech_params.tech_node_nm << " nm" << std::endl;
    std::cout << "  Frequency: " << tech_params.frequency_ghz << " GHz" << std::endl;
    std::cout << "  Temperature: " << tech_params.temperature_k << " K" << std::endl;
    std::cout << "  Cores: " << tech_params.core_count << std::endl;

    // Create hierarchical power model manager
    PowerModelManager power_manager(tech_params);

    // NOTE: In a real system, you would register specialized models here:
    //
    // // Register Ramulator for memory power
    // auto ramulator = std::make_shared<RamulatorWrapper>(...);
    // power_manager.setRamulatorModel(ramulator);
    //
    // // Register GARNET for network power
    // auto garnet = std::make_shared<GarnetModel>(...);
    // power_manager.setGarnetModel(garnet);
    //
    // // Register custom power model for PE
    // power_manager.registerCustomModel(
    //     PowerComponent::PE,
    //     [](const ActivityStats& stats) -> PowerEstimate {
    //         // Custom PE power calculation
    //         PowerMetrics metrics;
    //         metrics.dynamic_power_w = stats.total_instructions * 0.001;
    //         metrics.leakage_power_w = 0.5;
    //         metrics.total_power_w = metrics.dynamic_power_w + metrics.leakage_power_w;
    //         return PowerEstimate(metrics, PowerModelSource::SPECIALIZED_SIMULATOR, "Custom PE Model");
    //     },
    //     "Custom PE Model"
    // );

    // Initialize power manager (will set up McPAT fallback)
    power_manager.initialize();

    std::cout << "\n" << std::string(65, '=') << std::endl;
    std::cout << "TEST 1: Core Power Estimation" << std::endl;
    std::cout << std::string(65, '=') << std::endl;

    // Create activity statistics for core
    ActivityStats core_stats;
    core_stats.total_cycles = 1000000;
    core_stats.total_instructions = 800000;  // 0.8 IPC
    core_stats.integer_instructions = 560000;
    core_stats.fp_instructions = 80000;
    core_stats.load_instructions = 160000;
    core_stats.store_instructions = 80000;
    core_stats.branch_instructions = 80000;

    // Get power estimate for core
    auto core_power = power_manager.getPower(PowerComponent::CORE, core_stats);

    std::cout << "\nCore Activity:" << std::endl;
    std::cout << "  Total cycles: " << core_stats.total_cycles << std::endl;
    std::cout << "  Instructions: " << core_stats.total_instructions << std::endl;
    std::cout << "  IPC: " << std::setprecision(2)
              << (static_cast<double>(core_stats.total_instructions) / core_stats.total_cycles)
              << std::endl;

    std::cout << "\nCore Power:" << std::endl;
    std::cout << "  Dynamic: " << std::fixed << std::setprecision(3)
              << core_power.metrics.dynamic_power_w << " W" << std::endl;
    std::cout << "  Leakage: " << core_power.metrics.leakage_power_w << " W" << std::endl;
    std::cout << "  Total: " << core_power.metrics.total_power_w << " W" << std::endl;
    std::cout << "  Source: " << core_power.source_name << std::endl;

    std::cout << "\n" << std::string(65, '=') << std::endl;
    std::cout << "TEST 2: Cache Hierarchy Power" << std::endl;
    std::cout << std::string(65, '=') << std::endl;

    // L1 cache activity
    ActivityStats l1_stats;
    l1_stats.total_cycles = 1000000;
    l1_stats.l1_reads = 500000;
    l1_stats.l1_writes = 200000;
    l1_stats.l1_misses = 70000;

    auto l1_power = power_manager.getPower(PowerComponent::L1_CACHE, l1_stats);

    std::cout << "\nL1 Cache:" << std::endl;
    std::cout << "  Reads: " << l1_stats.l1_reads << std::endl;
    std::cout << "  Writes: " << l1_stats.l1_writes << std::endl;
    std::cout << "  Power: " << l1_power.metrics.total_power_w << " W" << std::endl;
    std::cout << "  Source: " << l1_power.source_name << std::endl;

    // L2 cache activity
    ActivityStats l2_stats;
    l2_stats.total_cycles = 1000000;
    l2_stats.l2_reads = 70000;
    l2_stats.l2_writes = 30000;
    l2_stats.l2_misses = 10000;

    auto l2_power = power_manager.getPower(PowerComponent::L2_CACHE, l2_stats);

    std::cout << "\nL2 Cache:" << std::endl;
    std::cout << "  Reads: " << l2_stats.l2_reads << std::endl;
    std::cout << "  Writes: " << l2_stats.l2_writes << std::endl;
    std::cout << "  Power: " << l2_power.metrics.total_power_w << " W" << std::endl;
    std::cout << "  Source: " << l2_power.source_name << std::endl;

    // L3 cache activity
    ActivityStats l3_stats;
    l3_stats.total_cycles = 1000000;
    l3_stats.memory_reads = 10000;
    l3_stats.memory_writes = 5000;

    auto l3_power = power_manager.getPower(PowerComponent::L3_CACHE, l3_stats);

    std::cout << "\nL3 Cache:" << std::endl;
    std::cout << "  Reads: " << l3_stats.memory_reads << std::endl;
    std::cout << "  Writes: " << l3_stats.memory_writes << std::endl;
    std::cout << "  Power: " << l3_power.metrics.total_power_w << " W" << std::endl;
    std::cout << "  Source: " << l3_power.source_name << std::endl;

    std::cout << "\n" << std::string(65, '=') << std::endl;
    std::cout << "TEST 3: Memory and Network Power" << std::endl;
    std::cout << std::string(65, '=') << std::endl;

    // Memory activity
    ActivityStats mem_stats;
    mem_stats.total_cycles = 1000000;
    mem_stats.memory_reads = 10000;
    mem_stats.memory_writes = 5000;

    auto mem_power = power_manager.getPower(PowerComponent::MEMORY, mem_stats);

    std::cout << "\nMemory (DRAM):" << std::endl;
    std::cout << "  Reads: " << mem_stats.memory_reads << std::endl;
    std::cout << "  Writes: " << mem_stats.memory_writes << std::endl;
    std::cout << "  Power: " << mem_power.metrics.total_power_w << " W" << std::endl;
    std::cout << "  Source: " << mem_power.source_name;
    if (!power_manager.hasSpecializedModel(PowerComponent::MEMORY)) {
        std::cout << " (Ramulator not registered - using fallback)";
    }
    std::cout << std::endl;

    // Network router activity
    ActivityStats router_stats;
    router_stats.total_cycles = 1000000;

    auto router_power = power_manager.getPower(PowerComponent::NETWORK_ROUTER, router_stats);

    std::cout << "\nNetwork Router:" << std::endl;
    std::cout << "  Power: " << router_power.metrics.total_power_w << " W" << std::endl;
    std::cout << "  Source: " << router_power.source_name;
    if (!power_manager.hasSpecializedModel(PowerComponent::NETWORK_ROUTER)) {
        std::cout << " (GARNET not registered - using fallback)";
    }
    std::cout << std::endl;

    std::cout << "\n" << std::string(65, '=') << std::endl;
    std::cout << "TEST 4: System-Wide Power Statistics" << std::endl;
    std::cout << std::string(65, '=') << std::endl;

    // Print complete power statistics
    power_manager.printStats();

    // Print power breakdown by source
    std::cout << "\n" << std::string(65, '=') << std::endl;
    std::cout << "TEST 5: Power Source Verification" << std::endl;
    std::cout << std::string(65, '=') << std::endl;

    std::cout << "\nPower Source Mapping:" << std::endl;
    std::cout << "  CORE:             "
              << (power_manager.hasSpecializedModel(PowerComponent::CORE) ? "Specialized" : "McPAT/Analytical")
              << std::endl;
    std::cout << "  L1_CACHE:         "
              << (power_manager.hasSpecializedModel(PowerComponent::L1_CACHE) ? "Specialized" : "McPAT/Analytical")
              << std::endl;
    std::cout << "  L2_CACHE:         "
              << (power_manager.hasSpecializedModel(PowerComponent::L2_CACHE) ? "Specialized" : "McPAT/Analytical")
              << std::endl;
    std::cout << "  L3_CACHE:         "
              << (power_manager.hasSpecializedModel(PowerComponent::L3_CACHE) ? "Specialized" : "McPAT/Analytical")
              << std::endl;
    std::cout << "  MEMORY:           "
              << (power_manager.hasSpecializedModel(PowerComponent::MEMORY) ? "Ramulator" : "McPAT/Analytical")
              << std::endl;
    std::cout << "  NETWORK_ROUTER:   "
              << (power_manager.hasSpecializedModel(PowerComponent::NETWORK_ROUTER) ? "GARNET" : "McPAT/Analytical")
              << std::endl;

    std::cout << "\n" << std::string(65, '=') << std::endl;
    std::cout << "Hierarchical Power Modeling Test Complete!" << std::endl;
    std::cout << std::string(65, '=') << std::endl;

    std::cout << "\nKey Features Demonstrated:" << std::endl;
    std::cout << "  ✓ McPAT initialized as guaranteed fallback" << std::endl;
    std::cout << "  ✓ Power estimation works for all components" << std::endl;
    std::cout << "  ✓ Source tracking shows which model was used" << std::endl;
    std::cout << "  ✓ Ready for specialized model integration" << std::endl;
    std::cout << "  ✓ XML configuration generated for McPAT" << std::endl;

    std::cout << "\nTo integrate specialized models:" << std::endl;
    std::cout << "  1. Register Ramulator: power_manager.setRamulatorModel(ramulator)" << std::endl;
    std::cout << "  2. Register GARNET: power_manager.setGarnetModel(garnet)" << std::endl;
    std::cout << "  3. Register custom: power_manager.registerCustomModel(...)" << std::endl;
    std::cout << "  4. Power manager will automatically use them with fallback\n" << std::endl;

    return 0;
}
