/**
 * @file simple_network_adapter.cpp
 * @brief Lightweight adapter connecting SimpleNetwork to PIMID
 *
 * This is ALL the code needed to integrate an existing model!
 * Only ~50 lines of simple glue code - no changes to the original model.
 */

#include "SimpleNetwork.h"
#include "pimid/external_models/include/external_model_interface.h"
#include <iostream>

// ============================================================================
// Adapter Functions - Simple callbacks that forward to SimpleNetwork methods
// ============================================================================

static int simple_network_init(PimidModelHandle handle, const char* config_file) {
    SimpleNetwork* network = static_cast<SimpleNetwork*>(handle);
    return network->Initialize(config_file) ? 0 : -1;
}

static int simple_network_send_packet(PimidModelHandle handle, const PimidNetworkPacket* packet) {
    SimpleNetwork* network = static_cast<SimpleNetwork*>(handle);

    // Convert PIMID packet to SimpleNetwork format
    SimpleNetwork::Packet pkt;
    pkt.src = packet->src_id;
    pkt.dst = packet->dst_id;
    pkt.size = packet->size;
    pkt.injection_time = packet->timestamp;

    return network->SendPacket(pkt) ? 0 : -1;
}

static void simple_network_tick(PimidModelHandle handle) {
    SimpleNetwork* network = static_cast<SimpleNetwork*>(handle);
    network->AdvanceCycle();
}

static uint64_t simple_network_get_latency(PimidModelHandle handle, uint32_t src, uint32_t dst, uint64_t bytes) {
    SimpleNetwork* network = static_cast<SimpleNetwork*>(handle);
    return network->GetLatency(src, dst);
}

static void simple_network_get_stats(PimidModelHandle handle, PimidNetworkStats* stats) {
    SimpleNetwork* network = static_cast<SimpleNetwork*>(handle);
    SimpleNetwork::Stats network_stats = network->GetStatistics();

    // Convert stats format
    stats->total_packets = network_stats.packets_sent;
    stats->total_bytes = network_stats.bytes_transferred;
    stats->total_cycles = network_stats.total_cycles;
    stats->avg_latency_cycles = network_stats.avg_latency;
}

static void simple_network_destroy(PimidModelHandle handle) {
    SimpleNetwork* network = static_cast<SimpleNetwork*>(handle);
    delete network;
}

// ============================================================================
// Registration - Tell PIMID about this model
// ============================================================================

extern "C" {

// Factory function to create a new instance
PimidModelHandle create_simple_network() {
    return new SimpleNetwork();
}

// Registration function called when library loads
int pimid_register_simple_network() {
    PimidNetworkCallbacks callbacks;
    callbacks.init = simple_network_init;
    callbacks.send_packet = simple_network_send_packet;
    callbacks.tick = simple_network_tick;
    callbacks.get_latency = simple_network_get_latency;
    callbacks.get_stats = simple_network_get_stats;
    callbacks.destroy = simple_network_destroy;

    PimidModelDescriptor descriptor;
    descriptor.model_name = "SimpleNetwork";
    descriptor.model_type = PIMID_MODEL_NETWORK;
    descriptor.version = "1.0.0";
    descriptor.description = "Simple example network model";
    descriptor.create_func = create_simple_network;
    descriptor.network_callbacks = callbacks;
    descriptor.memory_callbacks = {};  // Not a memory model

    std::cout << "[Adapter] Registering SimpleNetwork with PIMID" << std::endl;
    return pimid_register_model(&descriptor);
}

} // extern "C"
