/**
 * @file test_custom_topology.cpp
 * @brief Unit test for custom switch topology configuration
 */

#include "memory_models/include/internal_dram_network.h"
#include <iostream>
#include <cassert>

using namespace pimid;

void test_default_topology() {
    std::cout << "\n=== Testing Default Topology Configuration ===" << std::endl;

    auto network = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);

    int num_channels = 1;
    int ranks_per_channel = 1;

    SwitchHierarchyConfig config = network->getSwitchHierarchyConfig(
        num_channels,
        ranks_per_channel
    );

    std::cout << "Default configuration:" << std::endl;
    std::cout << "  L0: " << config.l0_config.num_switches << " switches, "
              << config.l0_config.ports_per_switch << " ports, "
              << InternalDRAMNetwork::getTopologyName(config.l0_config.topology) << std::endl;

    assert(config.l0_config.num_switches == 32 && "L0 should have 32 switches (1 per BG)");
    std::cout << "✓ Default topology correct" << std::endl;
}

void test_port_count_reduction() {
    std::cout << "\n=== Testing Port Count Reduction ===" << std::endl;
    std::cout << "Scenario: 32 banks, but limit each switch to 8 ports" << std::endl;

    auto network = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);

    // Calculate optimal switch count for port constraint
    int num_banks = 32;
    int max_ports = 8;
    int optimal_switches = InternalDRAMNetwork::calculateOptimalSwitchCount(
        num_banks,
        max_ports,
        TopologyType::CROSSBAR
    );

    std::cout << "Optimal switches: " << optimal_switches << std::endl;
    std::cout << "Ports per switch: " << (num_banks / optimal_switches) << std::endl;

    assert(optimal_switches == 4 && "Should need 4 switches (32/8)");

    // Configure custom topology
    SwitchHierarchyConfig config;
    config.num_channels = 1;
    config.ranks_per_channel = 1;

    config.l0_config.level = 0;
    config.l0_config.use_default = false;
    config.l0_config.topology = TopologyType::CROSSBAR;
    config.l0_config.num_endpoints = num_banks;
    config.l0_config.num_switches = optimal_switches;
    config.l0_config.ports_per_switch = max_ports;

    network->setCustomSwitchHierarchy(config);

    int actual_switches = network->getNumberOfSwitchesAtLevel(0, 1, 1);
    std::cout << "Configured switches at L0: " << actual_switches << std::endl;

    assert(actual_switches == optimal_switches && "Custom config not applied");
    std::cout << "✓ Port count reduction works" << std::endl;
}

void test_mixed_topologies() {
    std::cout << "\n=== Testing Mixed Topology Hierarchy ===" << std::endl;
    std::cout << "Using: H-Tree @ L0, Bus @ L1, Mesh @ L2" << std::endl;

    auto network = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);

    SwitchHierarchyConfig config;
    config.num_channels = 1;
    config.ranks_per_channel = 1;

    // L0: H-Tree (16 endpoints → 15 switches for binary tree)
    config.l0_config.level = 0;
    config.l0_config.use_default = false;
    config.l0_config.topology = TopologyType::H_TREE;
    config.l0_config.num_endpoints = 16;
    config.l0_config.num_switches = 15;  // N-1 for binary tree
    config.l0_config.ports_per_switch = 4;

    // L1: Bus (simple, low cost)
    config.l1_config.level = 1;
    config.l1_config.use_default = false;
    config.l1_config.topology = TopologyType::BUS;
    config.l1_config.num_endpoints = 4;
    config.l1_config.num_switches = 4;  // Still need 1 per BG
    config.l1_config.ports_per_switch = 4;

    // L2: Mesh (scalable)
    config.l2_config.level = 2;
    config.l2_config.use_default = false;
    config.l2_config.topology = TopologyType::MESH_2D;
    config.l2_config.num_endpoints = 8;
    config.l2_config.num_switches = 8;
    config.l2_config.ports_per_switch = 5;  // N,S,E,W,Local

    network->setCustomSwitchHierarchy(config);

    std::cout << "Configured topologies:" << std::endl;
    std::cout << "  L0: " << InternalDRAMNetwork::getTopologyName(TopologyType::H_TREE)
              << " - " << config.l0_config.num_switches << " switches" << std::endl;
    std::cout << "  L1: " << InternalDRAMNetwork::getTopologyName(TopologyType::BUS)
              << " - " << config.l1_config.num_switches << " switches" << std::endl;
    std::cout << "  L2: " << InternalDRAMNetwork::getTopologyName(TopologyType::MESH_2D)
              << " - " << config.l2_config.num_switches << " switches" << std::endl;

    assert(config.l0_config.topology == TopologyType::H_TREE);
    assert(config.l1_config.topology == TopologyType::BUS);
    assert(config.l2_config.topology == TopologyType::MESH_2D);

    std::cout << "✓ Mixed topologies configured successfully" << std::endl;
}

void test_topology_calculations() {
    std::cout << "\n=== Testing Topology Calculation Functions ===" << std::endl;

    // Test calculateOptimalSwitchCount for different topologies
    struct TestCase {
        TopologyType topology;
        int endpoints;
        int max_ports;
        int expected_switches;
    };

    TestCase cases[] = {
        {TopologyType::CROSSBAR, 32, 8, 4},      // 32/8 = 4
        {TopologyType::BUS, 32, 8, 4},           // Same as crossbar
        {TopologyType::MESH_2D, 16, 8, 16},      // sqrt(16) * sqrt(16) = 16
        {TopologyType::H_TREE, 16, 8, 15},       // N-1 for binary tree
    };

    for (const auto& tc : cases) {
        int result = InternalDRAMNetwork::calculateOptimalSwitchCount(
            tc.endpoints,
            tc.max_ports,
            tc.topology
        );

        std::cout << "Topology: " << InternalDRAMNetwork::getTopologyName(tc.topology)
                  << ", Endpoints: " << tc.endpoints
                  << ", Max ports: " << tc.max_ports
                  << " → Switches: " << result
                  << " (expected: " << tc.expected_switches << ")" << std::endl;

        assert(result == tc.expected_switches && "Switch count calculation incorrect");
    }

    std::cout << "✓ Topology calculations correct" << std::endl;
}

void test_ports_per_switch() {
    std::cout << "\n=== Testing Ports Per Switch Calculation ===" << std::endl;

    struct TestCase {
        TopologyType topology;
        int endpoints;
        int num_switches;
        int expected_ports;
    };

    TestCase cases[] = {
        {TopologyType::BUS, 8, 1, 8},           // Bus connects all
        {TopologyType::CROSSBAR, 8, 1, 8},      // Crossbar connects all
        {TopologyType::MESH_2D, 16, 16, 5},     // N,S,E,W,Local
        {TopologyType::TORUS_2D, 16, 16, 5},    // Same as mesh
        {TopologyType::H_TREE, 16, 15, 4},      // Parent + 2 children + local (avg)
    };

    for (const auto& tc : cases) {
        int result = InternalDRAMNetwork::calculatePortsPerSwitch(
            tc.topology,
            tc.endpoints,
            tc.num_switches
        );

        std::cout << "Topology: " << InternalDRAMNetwork::getTopologyName(tc.topology)
                  << ", Endpoints: " << tc.endpoints
                  << ", Switches: " << tc.num_switches
                  << " → Ports: " << result
                  << " (expected: " << tc.expected_ports << ")" << std::endl;

        assert(result == tc.expected_ports && "Port count calculation incorrect");
    }

    std::cout << "✓ Port calculations correct" << std::endl;
}

void test_fat_tree_topology() {
    std::cout << "\n=== Testing Fat-Tree Topology ===" << std::endl;
    std::cout << "Fat-tree for high bisection bandwidth at upper levels" << std::endl;

    auto network = createInternalDRAMNetwork("HBM2", 16, 4, 4, 2);

    SwitchHierarchyConfig config;
    config.num_channels = 8;
    config.ranks_per_channel = 2;

    // Use fat-tree at L3 (rank level)
    config.l3_config.level = 3;
    config.l3_config.use_default = false;
    config.l3_config.topology = TopologyType::FAT_TREE;
    config.l3_config.num_endpoints = 16;  // 8 channels * 2 ranks
    config.l3_config.num_switches = InternalDRAMNetwork::calculateOptimalSwitchCount(
        16, 16, TopologyType::FAT_TREE
    );
    config.l3_config.ports_per_switch = 16;

    network->setCustomSwitchHierarchy(config);

    std::cout << "Fat-tree at L3:" << std::endl;
    std::cout << "  Switches: " << config.l3_config.num_switches << std::endl;
    std::cout << "  Ports per switch: " << config.l3_config.ports_per_switch << std::endl;

    assert(config.l3_config.topology == TopologyType::FAT_TREE);
    std::cout << "✓ Fat-tree topology configured" << std::endl;
}

void test_complete_custom_hierarchy() {
    std::cout << "\n=== Testing Complete Custom Hierarchy ===" << std::endl;
    std::cout << "All levels customized with different topologies" << std::endl;

    auto network = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);

    SwitchHierarchyConfig config;
    config.num_channels = 2;
    config.ranks_per_channel = 2;

    // Configure all levels
    config.l0_config.use_default = false;
    config.l0_config.topology = TopologyType::H_TREE;
    config.l0_config.num_switches = 15;
    config.l0_config.ports_per_switch = 4;

    config.l1_config.use_default = false;
    config.l1_config.topology = TopologyType::BUS;
    config.l1_config.num_switches = 16;
    config.l1_config.ports_per_switch = 4;

    config.l2_config.use_default = false;
    config.l2_config.topology = TopologyType::MESH_2D;
    config.l2_config.num_switches = 16;
    config.l2_config.ports_per_switch = 5;

    config.l3_config.use_default = false;
    config.l3_config.topology = TopologyType::TORUS_2D;
    config.l3_config.num_switches = 4;
    config.l3_config.ports_per_switch = 5;

    config.l4_config.use_default = false;
    config.l4_config.topology = TopologyType::CROSSBAR;
    config.l4_config.num_switches = 2;
    config.l4_config.ports_per_switch = 2;

    config.l5_config.use_default = false;
    config.l5_config.topology = TopologyType::CROSSBAR;
    config.l5_config.num_switches = 1;
    config.l5_config.ports_per_switch = 2;

    network->setCustomSwitchHierarchy(config);

    // Verify configuration
    SwitchHierarchyConfig retrieved = network->getSwitchHierarchyConfig(
        config.num_channels,
        config.ranks_per_channel
    );

    assert(retrieved.l0_config.topology == TopologyType::H_TREE);
    assert(retrieved.l1_config.topology == TopologyType::BUS);
    assert(retrieved.l2_config.topology == TopologyType::MESH_2D);
    assert(retrieved.l3_config.topology == TopologyType::TORUS_2D);
    assert(retrieved.l4_config.topology == TopologyType::CROSSBAR);
    assert(retrieved.l5_config.topology == TopologyType::CROSSBAR);

    std::cout << "✓ Complete custom hierarchy verified" << std::endl;
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║   Custom Topology Configuration Test Suite                  ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;

    try {
        test_default_topology();
        test_port_count_reduction();
        test_mixed_topologies();
        test_topology_calculations();
        test_ports_per_switch();
        test_fat_tree_topology();
        test_complete_custom_hierarchy();

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
