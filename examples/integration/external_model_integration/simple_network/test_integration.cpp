/**
 * @file test_integration.cpp
 * @brief Test program demonstrating external model integration
 */

#include "pimid/external_models/include/external_model_interface.h"
#include <iostream>
#include <cassert>

// Forward declare registration function
extern "C" int pimid_register_simple_network();

int main() {
    std::cout << "+==============================================================+" << std::endl;
    std::cout << "|   External Model Integration Test                           |" << std::endl;
    std::cout << "+==============================================================+" << std::endl;

    // Step 1: Register the external model
    std::cout << "\n=== Step 1: Registering External Model ===" << std::endl;
    int result = pimid_register_simple_network();
    assert(result == 0 && "Registration failed");
    std::cout << "[OK] Model registered successfully" << std::endl;

    // Step 2: Create model instance
    std::cout << "\n=== Step 2: Creating Model Instance ===" << std::endl;
    PimidModelHandle handle = pimid_create_model("SimpleNetwork");
    assert(handle != nullptr && "Model creation failed");
    std::cout << "[OK] Model instance created" << std::endl;

    // Step 3: Initialize model
    std::cout << "\n=== Step 3: Initializing Model ===" << std::endl;
    result = pimid_model_init(handle, "SimpleNetwork", "config.txt");
    assert(result == 0 && "Initialization failed");
    std::cout << "[OK] Model initialized" << std::endl;

    // Step 4: Send some packets
    std::cout << "\n=== Step 4: Sending Packets ===" << std::endl;
    for (int i = 0; i < 10; i++) {
        PimidNetworkPacket packet;
        packet.src_id = i % 4;
        packet.dst_id = (i + 1) % 4;
        packet.size = 64;
        packet.timestamp = i * 10;

        result = pimid_network_send_packet(handle, "SimpleNetwork", &packet);
        assert(result == 0 && "Packet send failed");
    }
    std::cout << "[OK] Sent 10 packets" << std::endl;

    // Step 5: Run simulation for some cycles
    std::cout << "\n=== Step 5: Running Simulation ===" << std::endl;
    for (int cycle = 0; cycle < 100; cycle++) {
        pimid_network_tick(handle, "SimpleNetwork");
    }
    std::cout << "[OK] Ran 100 cycles" << std::endl;

    // Step 6: Query latency
    std::cout << "\n=== Step 6: Querying Latency ===" << std::endl;
    uint64_t latency = pimid_network_get_latency(handle, "SimpleNetwork", 0, 3, 64);
    std::cout << "Latency from node 0 to node 3: " << latency << " cycles" << std::endl;
    assert(latency > 0 && "Latency query failed");
    std::cout << "[OK] Latency query successful" << std::endl;

    // Step 7: Get statistics
    std::cout << "\n=== Step 7: Retrieving Statistics ===" << std::endl;
    PimidNetworkStats stats;
    pimid_network_get_stats(handle, "SimpleNetwork", &stats);

    std::cout << "Network Statistics:" << std::endl;
    std::cout << "  Total packets: " << stats.total_packets << std::endl;
    std::cout << "  Total bytes: " << stats.total_bytes << std::endl;
    std::cout << "  Total cycles: " << stats.total_cycles << std::endl;
    std::cout << "  Avg latency: " << stats.avg_latency_cycles << " cycles" << std::endl;

    assert(stats.total_packets == 10 && "Packet count mismatch");
    assert(stats.total_bytes == 640 && "Byte count mismatch");
    std::cout << "[OK] Statistics correct" << std::endl;

    // Step 8: Cleanup
    std::cout << "\n=== Step 8: Cleanup ===" << std::endl;
    pimid_model_destroy(handle, "SimpleNetwork");
    std::cout << "[OK] Model destroyed" << std::endl;

    std::cout << "\n" << std::string(64, '=') << std::endl;
    std::cout << "All integration tests passed! [OK]" << std::endl;
    std::cout << std::string(64, '=') << std::endl;

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "This example demonstrated:" << std::endl;
    std::cout << "  * Registering an external model with ~50 lines of adapter code" << std::endl;
    std::cout << "  * Creating and initializing model instances" << std::endl;
    std::cout << "  * Sending packets through the network" << std::endl;
    std::cout << "  * Running cycle-accurate simulation" << std::endl;
    std::cout << "  * Querying latency and statistics" << std::endl;
    std::cout << "  * Proper cleanup" << std::endl;
    std::cout << "\nKey insight: The original SimpleNetwork class was NOT modified!" << std::endl;
    std::cout << "Only a thin adapter layer was needed for integration." << std::endl;

    return 0;
}
