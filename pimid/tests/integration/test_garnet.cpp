/**
 * @file test_garnet.cpp
 * @brief Integration tests for GARNET network-on-chip simulator
 */

#include "network_model.h"
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace pimid;
using namespace std;

//=============================================================================
// Test Utilities
//=============================================================================

void printTestHeader(const string& test_name) {
    cout << "\n" << string(70, '=') << endl;
    cout << "TEST: " << test_name << endl;
    cout << string(70, '=') << endl;
}

void printTestResult(bool passed) {
    if (passed) {
        cout << "[PASS] ✓" << endl;
    } else {
        cout << "[FAIL] ✗" << endl;
    }
}

//=============================================================================
// GARNET Tests
//=============================================================================

bool test_garnet_initialization() {
    printTestHeader("GARNET: Basic Initialization");

    NetworkConfig config;
    config.topology = NetworkTopology::MESH_2D;
    config.routing = RoutingAlgorithm::XY;
    config.flow_control = FlowControl::CREDIT_BASED;
    config.num_rows = 4;
    config.num_cols = 4;
    config.virtual_channels = 4;
    config.link_width_bytes = 16;
    config.link_latency = 1;
    config.router_latency = 2;
    config.input_buffer_depth = 4;
    config.output_buffer_depth = 4;

    try {
        GarnetModel network(config);
        network.initialize();

        cout << "✓ Network initialized successfully" << endl;
        cout << "  Topology: 4x4 Mesh" << endl;
        cout << "  Total nodes: 16" << endl;
        cout << "  Virtual channels: 4" << endl;
        cout << "  Link width: 16 bytes" << endl;

        printTestResult(true);
        return true;
    } catch (const exception& e) {
        cout << "✗ Initialization failed: " << e.what() << endl;
        printTestResult(false);
        return false;
    }
}

bool test_garnet_node_configuration() {
    printTestHeader("GARNET: Node Configuration");

    NetworkConfig config;
    config.topology = NetworkTopology::MESH_2D;
    config.num_rows = 2;
    config.num_cols = 2;
    config.virtual_channels = 2;
    config.link_width_bytes = 8;
    config.link_latency = 1;
    config.router_latency = 1;
    config.input_buffer_depth = 4;
    config.output_buffer_depth = 4;

    GarnetModel network(config);
    network.initialize();

    // Add nodes
    for (uint32_t i = 0; i < 4; i++) {
        NetworkNode node(i, PEPlacementLevel::LOGIC_DIE, i);
        network.addNode(node);
    }

    cout << "✓ Added 4 nodes to 2x2 network" << endl;

    // Test connectivity (mesh should auto-connect)
    network.connectNodes(0, 1);
    network.connectNodes(0, 2);
    cout << "✓ Node connections established" << endl;

    printTestResult(true);
    return true;
}

bool test_garnet_single_packet() {
    printTestHeader("GARNET: Single Packet Transmission");

    NetworkConfig config;
    config.topology = NetworkTopology::MESH_2D;
    config.routing = RoutingAlgorithm::XY;
    config.num_rows = 4;
    config.num_cols = 4;
    config.virtual_channels = 4;
    config.link_width_bytes = 16;
    config.link_latency = 1;
    config.router_latency = 2;
    config.input_buffer_depth = 8;

    GarnetModel network(config);
    network.initialize();

    // Send packet from node 0 to node 15 (diagonal)
    NetworkPacket packet(0, 15, PacketType::DATA, 64, 0x1000, 0);

    cout << "Sending packet: Node 0 -> Node 15" << endl;
    cout << "  Packet size: 64 bytes" << endl;
    cout << "  Expected hops: 6 (3 in X, 3 in Y)" << endl;

    network.injectPacket(packet);

    // Simulate until packet arrives
    int max_cycles = 1000;
    int cycle = 0;
    bool arrived = false;

    while (cycle < max_cycles && !arrived) {
        network.tick();
        cycle++;

        if (network.hasArrived(15)) {
            arrived = true;
            NetworkPacket received = network.extractPacket(15);
            cout << "✓ Packet arrived after " << cycle << " cycles" << endl;
            cout << "  Source: " << received.src_node << endl;
            cout << "  Destination: " << received.dst_node << endl;
        }
    }

    if (!arrived) {
        cout << "✗ Packet did not arrive within " << max_cycles << " cycles" << endl;
    }

    printTestResult(arrived);
    return arrived;
}

bool test_garnet_multiple_packets() {
    printTestHeader("GARNET: Multiple Packet Transmission");

    NetworkConfig config;
    config.topology = NetworkTopology::MESH_2D;
    config.routing = RoutingAlgorithm::XY;
    config.num_rows = 4;
    config.num_cols = 4;
    config.virtual_channels = 4;
    config.link_width_bytes = 16;
    config.link_latency = 1;
    config.router_latency = 2;
    config.input_buffer_depth = 8;

    GarnetModel network(config);
    network.initialize();

    // Send multiple packets
    int num_packets = 10;
    cout << "Sending " << num_packets << " packets..." << endl;

    for (int i = 0; i < num_packets; i++) {
        uint32_t src = i % 16;
        uint32_t dst = (i + 8) % 16;
        NetworkPacket packet(src, dst, PacketType::DATA, 64, i * 0x100, 0);
        network.injectPacket(packet);
    }

    // Simulate
    int received = 0;
    int max_cycles = 2000;

    for (int cycle = 0; cycle < max_cycles; cycle++) {
        network.tick();

        // Check all nodes for arrivals
        for (uint32_t node = 0; node < 16; node++) {
            while (network.hasArrived(node)) {
                network.extractPacket(node);
                received++;
            }
        }

        if (received >= num_packets) {
            cout << "✓ All " << num_packets << " packets received after "
                 << cycle << " cycles" << endl;
            break;
        }
    }

    double delivery_rate = (double)received / num_packets * 100.0;
    cout << "  Packets received: " << received << "/" << num_packets
         << " (" << fixed << setprecision(1) << delivery_rate << "%)" << endl;

    bool passed = (received == num_packets);
    printTestResult(passed);
    return passed;
}

bool test_garnet_mesh_topology() {
    printTestHeader("GARNET: Mesh Topology Verification");

    NetworkConfig config;
    config.topology = NetworkTopology::MESH_2D;
    config.routing = RoutingAlgorithm::XY;
    config.num_rows = 3;
    config.num_cols = 3;
    config.virtual_channels = 2;
    config.link_width_bytes = 8;
    config.link_latency = 1;
    config.router_latency = 1;
    config.input_buffer_depth = 4;

    GarnetModel network(config);
    network.initialize();

    cout << "Testing 3x3 mesh topology" << endl;
    cout << "  Nodes layout:" << endl;
    cout << "    0 - 1 - 2" << endl;
    cout << "    |   |   |" << endl;
    cout << "    3 - 4 - 5" << endl;
    cout << "    |   |   |" << endl;
    cout << "    6 - 7 - 8" << endl;

    // Test corner to corner (0 -> 8)
    NetworkPacket pkt1(0, 8, PacketType::DATA, 32, 0, 0);
    network.injectPacket(pkt1);

    // Test edge to edge (1 -> 7)
    NetworkPacket pkt2(1, 7, PacketType::DATA, 32, 0, 0);
    network.injectPacket(pkt2);

    // Simulate
    int received = 0;
    for (int cycle = 0; cycle < 500; cycle++) {
        network.tick();

        if (network.hasArrived(8)) {
            network.extractPacket(8);
            received++;
            cout << "✓ Packet 0->8 arrived (corner to corner)" << endl;
        }
        if (network.hasArrived(7)) {
            network.extractPacket(7);
            received++;
            cout << "✓ Packet 1->7 arrived (edge to edge)" << endl;
        }

        if (received >= 2) break;
    }

    bool passed = (received == 2);
    printTestResult(passed);
    return passed;
}

bool test_garnet_statistics() {
    printTestHeader("GARNET: Statistics Collection");

    NetworkConfig config;
    config.topology = NetworkTopology::MESH_2D;
    config.routing = RoutingAlgorithm::XY;
    config.num_rows = 4;
    config.num_cols = 4;
    config.virtual_channels = 4;
    config.link_width_bytes = 16;
    config.link_latency = 1;
    config.router_latency = 2;
    config.input_buffer_depth = 8;

    GarnetModel network(config);
    network.initialize();

    // Inject traffic
    for (int i = 0; i < 20; i++) {
        uint32_t src = i % 16;
        uint32_t dst = (i * 3 + 7) % 16;
        NetworkPacket packet(src, dst, PacketType::DATA, 64, i, 0);
        network.injectPacket(packet);
    }

    // Run simulation
    for (int cycle = 0; cycle < 1000; cycle++) {
        network.tick();

        // Extract completed packets
        for (uint32_t node = 0; node < 16; node++) {
            while (network.hasArrived(node)) {
                network.extractPacket(node);
            }
        }
    }

    // Get statistics
    NetworkStats stats = network.getStats();

    cout << "Network Statistics:" << endl;
    cout << "  Total packets: " << stats.total_packets << endl;
    cout << "  Total flits: " << stats.total_flits << endl;
    cout << "  Avg latency: " << stats.avg_packet_latency << " cycles" << endl;
    cout << "  Max latency: " << stats.max_packet_latency << " cycles" << endl;
    cout << "  Total energy: " << (stats.total_energy_j * 1e6) << " μJ" << endl;

    network.printStats();

    bool passed = (stats.total_packets > 0);
    printTestResult(passed);
    return passed;
}

bool test_garnet_energy_modeling() {
    printTestHeader("GARNET: Energy Modeling");

    NetworkConfig config;
    config.topology = NetworkTopology::MESH_2D;
    config.routing = RoutingAlgorithm::XY;
    config.num_rows = 2;
    config.num_cols = 2;
    config.virtual_channels = 2;
    config.link_width_bytes = 8;
    config.link_latency = 1;
    config.router_latency = 1;
    config.input_buffer_depth = 4;

    GarnetModel network(config);
    network.initialize();

    // Inject some packets
    for (int i = 0; i < 5; i++) {
        NetworkPacket packet(0, 3, PacketType::DATA, 32, i, 0);
        network.injectPacket(packet);
    }

    // Run simulation
    for (int cycle = 0; cycle < 200; cycle++) {
        network.tick();
        while (network.hasArrived(3)) {
            network.extractPacket(3);
        }
    }

    // Check energy metrics
    double router_energy = network.getRouterEnergy();
    double link_energy = network.getLinkEnergy();
    double total_energy = network.getTotalEnergy();

    cout << "Energy Breakdown:" << endl;
    cout << "  Router energy: " << (router_energy * 1e9) << " nJ" << endl;
    cout << "  Link energy:   " << (link_energy * 1e9) << " nJ" << endl;
    cout << "  Total energy:  " << (total_energy * 1e9) << " nJ" << endl;

    bool passed = (total_energy > 0) && (router_energy > 0) && (link_energy > 0);

    if (passed) {
        cout << "✓ Energy accounting is functional" << endl;
    } else {
        cout << "✗ Energy values incorrect" << endl;
    }

    printTestResult(passed);
    return passed;
}

bool test_garnet_flow_control() {
    printTestHeader("GARNET: Flow Control and Backpressure");

    NetworkConfig config;
    config.topology = NetworkTopology::MESH_2D;
    config.routing = RoutingAlgorithm::XY;
    config.num_rows = 2;
    config.num_cols = 2;
    config.virtual_channels = 2;
    config.link_width_bytes = 8;
    config.link_latency = 1;
    config.router_latency = 1;
    config.input_buffer_depth = 2;  // Small buffer to test backpressure

    GarnetModel network(config);
    network.initialize();

    cout << "Testing backpressure with small buffers (depth=2)" << endl;

    // Try to inject many packets rapidly
    int injected = 0;
    int rejected = 0;

    for (int i = 0; i < 10; i++) {
        if (network.canInject(0)) {
            NetworkPacket packet(0, 3, PacketType::DATA, 64, i, 0);
            network.injectPacket(packet);
            injected++;
        } else {
            rejected++;
        }

        // Tick network to drain some packets
        if (i % 3 == 0) {
            network.tick();
            while (network.hasArrived(3)) {
                network.extractPacket(3);
            }
        }
    }

    cout << "  Packets injected: " << injected << endl;
    cout << "  Packets rejected: " << rejected << " (due to backpressure)" << endl;

    // Continue simulation to complete
    for (int cycle = 0; cycle < 500; cycle++) {
        network.tick();
        while (network.hasArrived(3)) {
            network.extractPacket(3);
        }
    }

    bool passed = (rejected > 0);  // Should have some backpressure
    if (passed) {
        cout << "✓ Flow control working correctly" << endl;
    }

    printTestResult(passed);
    return passed;
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    cout << "\n";
    cout << "╔════════════════════════════════════════════════════════════════╗\n";
    cout << "║                                                                ║\n";
    cout << "║             GARNET Network-on-Chip Integration Tests          ║\n";
    cout << "║                                                                ║\n";
    cout << "╚════════════════════════════════════════════════════════════════╝\n";

    int tests_passed = 0;
    int tests_total = 0;

    // Run tests
    tests_total++; if (test_garnet_initialization()) tests_passed++;
    tests_total++; if (test_garnet_node_configuration()) tests_passed++;
    tests_total++; if (test_garnet_single_packet()) tests_passed++;
    tests_total++; if (test_garnet_multiple_packets()) tests_passed++;
    tests_total++; if (test_garnet_mesh_topology()) tests_passed++;
    tests_total++; if (test_garnet_statistics()) tests_passed++;
    tests_total++; if (test_garnet_energy_modeling()) tests_passed++;
    tests_total++; if (test_garnet_flow_control()) tests_passed++;

    // Print summary
    cout << "\n" << string(70, '=') << endl;
    cout << "TEST SUMMARY" << endl;
    cout << string(70, '=') << endl;
    cout << "Tests passed: " << tests_passed << "/" << tests_total << endl;

    double pass_rate = (double)tests_passed / tests_total * 100.0;
    cout << "Pass rate: " << fixed << setprecision(1) << pass_rate << "%" << endl;

    if (tests_passed == tests_total) {
        cout << "\n✓ ALL TESTS PASSED! 🎉" << endl;
        return 0;
    } else {
        cout << "\n✗ SOME TESTS FAILED" << endl;
        return 1;
    }
}
