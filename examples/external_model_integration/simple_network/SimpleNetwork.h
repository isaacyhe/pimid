/**
 * @file SimpleNetwork.h
 * @brief Example of an EXISTING network model (unchanged)
 *
 * This represents a user's existing model that they don't want to modify.
 */

#ifndef SIMPLE_NETWORK_H
#define SIMPLE_NETWORK_H

#include <cstdint>
#include <vector>

/**
 * @brief Simple network simulator (user's existing code)
 *
 * This class remains UNCHANGED - no inheritance, no modifications needed
 */
class SimpleNetwork {
public:
    struct Packet {
        uint32_t src;
        uint32_t dst;
        uint64_t size;
        uint64_t injection_time;
    };

    struct Stats {
        uint64_t packets_sent;
        uint64_t bytes_transferred;
        uint64_t total_cycles;
        double avg_latency;
    };

    SimpleNetwork();
    ~SimpleNetwork();

    // Existing API (don't modify these)
    bool Initialize(const char* config_file);
    bool SendPacket(const Packet& pkt);
    void AdvanceCycle();
    uint64_t GetLatency(uint32_t src, uint32_t dst) const;
    Stats GetStatistics() const;
    void Reset();

private:
    uint32_t num_nodes_;
    uint64_t current_cycle_;
    std::vector<Packet> in_flight_;
    Stats stats_;
};

#endif // SIMPLE_NETWORK_H
