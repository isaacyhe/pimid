/**
 * @file SimpleNetwork.cpp
 * @brief Implementation of the simple network model
 *
 * This is the user's EXISTING implementation - completely unchanged
 */

#include "SimpleNetwork.h"
#include <iostream>
#include <cstring>

SimpleNetwork::SimpleNetwork()
    : num_nodes_(0), current_cycle_(0) {
    stats_.packets_sent = 0;
    stats_.bytes_transferred = 0;
    stats_.total_cycles = 0;
    stats_.avg_latency = 0.0;
}

SimpleNetwork::~SimpleNetwork() {
    // Cleanup
}

bool SimpleNetwork::Initialize(const char* config_file) {
    // Simple initialization - just set number of nodes
    // In a real network model, this would parse config
    std::cout << "[SimpleNetwork] Initializing with config: " << config_file << std::endl;

    num_nodes_ = 16;  // Default to 16 nodes
    current_cycle_ = 0;

    return true;
}

bool SimpleNetwork::SendPacket(const Packet& pkt) {
    // Add packet to in-flight queue
    in_flight_.push_back(pkt);

    stats_.packets_sent++;
    stats_.bytes_transferred += pkt.size;

    return true;
}

void SimpleNetwork::AdvanceCycle() {
    current_cycle_++;
    stats_.total_cycles = current_cycle_;

    // Simple model: packets arrive after fixed latency
    std::vector<Packet> remaining;
    for (const auto& pkt : in_flight_) {
        uint64_t time_in_flight = current_cycle_ - pkt.injection_time;
        uint64_t expected_latency = GetLatency(pkt.src, pkt.dst);

        if (time_in_flight < expected_latency) {
            remaining.push_back(pkt);
        }
    }

    in_flight_ = remaining;

    // Update average latency
    if (stats_.packets_sent > 0) {
        stats_.avg_latency = static_cast<double>(stats_.total_cycles) / stats_.packets_sent;
    }
}

uint64_t SimpleNetwork::GetLatency(uint32_t src, uint32_t dst) const {
    // Simple model: latency based on distance
    int distance = std::abs(static_cast<int>(dst) - static_cast<int>(src));
    return 10 + (distance * 2);  // Base latency + hop latency
}

SimpleNetwork::Stats SimpleNetwork::GetStatistics() const {
    return stats_;
}

void SimpleNetwork::Reset() {
    current_cycle_ = 0;
    in_flight_.clear();

    stats_.packets_sent = 0;
    stats_.bytes_transferred = 0;
    stats_.total_cycles = 0;
    stats_.avg_latency = 0.0;
}
