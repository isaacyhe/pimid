/**
 * @file garnet_network.h
 * @brief Garnet-based network model for ZSim
 *
 * Replaces ZSim's simple fixed-delay network with topology-aware
 * Garnet NoC simulation from PIMID.
 */

#ifndef GARNET_NETWORK_H_
#define GARNET_NETWORK_H_

#include <string>
#include <unordered_map>
#include <memory>
#include "log.h"
#include "network.h"

// Forward declaration - actual implementation in PIMID's network_models
namespace pimid {
    class NetworkModel;
    struct NetworkConfig;
}

/**
 * NoC activity statistics for McPAT power modeling
 * Matches the NoCActivityStats structure in PIMID's mcpat_wrapper.h
 */
struct GarnetStats {
    // Traffic statistics
    uint64_t total_packets = 0;
    uint64_t total_flits = 0;
    uint64_t total_hops = 0;

    // Router activity
    uint64_t buffer_reads = 0;
    uint64_t buffer_writes = 0;
    uint64_t crossbar_traversals = 0;
    uint64_t arbiter_events = 0;

    // Link activity
    uint64_t link_traversals = 0;

    // Timing
    uint64_t total_cycles = 0;
    uint64_t total_latency = 0;  // Sum of all packet latencies

    // Configuration (for power model)
    uint32_t num_routers = 0;
    uint32_t num_rows = 0;
    uint32_t num_cols = 0;
    uint32_t flit_size_bits = 128;
    uint32_t vcs_per_vnet = 4;
    double clock_mhz = 1000.0;
};

/**
 * Garnet-based network for ZSim (inherits from Network)
 *
 * Provides topology-aware NoC latency using mesh-based routing.
 * Can operate in two modes:
 * 1. Analytical: Pre-computed latencies based on topology (fast)
 * 2. Cycle-accurate: Actual Garnet simulation per access (slow but accurate)
 */
class GarnetNetwork : public Network {
private:
    // Network topology
    uint32_t numRows_;
    uint32_t numCols_;
    uint32_t numNodes_;

    // Latency parameters
    uint32_t routerLatency_;      // Cycles per router hop
    uint32_t linkLatency_;        // Cycles per link
    uint32_t injectionLatency_;   // Network interface latency
    uint32_t flitSizeBits_;       // Flit size in bits
    double clockMhz_;             // Network clock frequency

    // Node name to ID mapping
    std::unordered_map<std::string, uint32_t> nodeMap_;

    // Pre-computed latencies for analytical mode
    std::unordered_map<std::string, uint32_t> latencyCache_;

    // Mode selection
    bool cycleAccurate_;

    // Comprehensive statistics for McPAT integration
    GarnetStats stats_;

public:
    /**
     * Create Garnet network from PIMID config
     * @param rows Number of rows in mesh
     * @param cols Number of columns in mesh
     * @param routerLat Router pipeline latency
     * @param linkLat Link traversal latency
     * @param cycleAccurate Use cycle-accurate simulation (vs analytical)
     * @param clockMhz Network clock frequency in MHz
     * @param flitSizeBits Flit size in bits
     */
    GarnetNetwork(uint32_t rows, uint32_t cols,
                  uint32_t routerLat = 1, uint32_t linkLat = 1,
                  bool cycleAccurate = false,
                  double clockMhz = 1000.0, uint32_t flitSizeBits = 128)
        : numRows_(rows), numCols_(cols),
          numNodes_(rows * cols),
          routerLatency_(routerLat), linkLatency_(linkLat),
          injectionLatency_(2),
          flitSizeBits_(flitSizeBits), clockMhz_(clockMhz),
          cycleAccurate_(cycleAccurate) {

        // Initialize stats with configuration
        stats_.num_routers = numNodes_;
        stats_.num_rows = rows;
        stats_.num_cols = cols;
        stats_.flit_size_bits = flitSizeBits;
        stats_.clock_mhz = clockMhz;

        info("[GarnetNetwork] Initializing %dx%d mesh NoC", rows, cols);
        info("  Router latency: %d cycles", routerLat);
        info("  Link latency: %d cycles", linkLat);
        info("  Clock: %.1f MHz, Flit size: %d bits", clockMhz, flitSizeBits);
        info("  Mode: %s", cycleAccurate ? "cycle-accurate" : "analytical");
    }

    /**
     * Register a named node (cache/memory) at a mesh position
     */
    void registerNode(const char* name, uint32_t nodeId) {
        if (nodeId >= numNodes_) {
            warn("[GarnetNetwork] Node ID %d out of range (max %d), wrapping",
                 nodeId, numNodes_ - 1);
            nodeId = nodeId % numNodes_;
        }
        nodeMap_[name] = nodeId;
    }

    /**
     * Auto-register node based on name pattern
     * Extracts ID from names like "l2-0", "mem-3", etc.
     */
    void autoRegisterNode(const char* name) {
        std::string s(name);
        size_t pos = s.find_last_of("-");
        if (pos != std::string::npos) {
            uint32_t id = std::stoul(s.substr(pos + 1));
            registerNode(name, id);
        } else {
            // No ID in name, use hash
            uint32_t id = std::hash<std::string>{}(s) % numNodes_;
            registerNode(name, id);
        }
    }

    /**
     * Get round-trip time between two nodes
     * This is the main interface used by ZSim's cache hierarchy
     * Overrides Network::getRTT() to provide topology-aware latencies
     */
    uint32_t getRTT(const char* src, const char* dst) override {
        // Ensure nodes are registered
        if (nodeMap_.find(src) == nodeMap_.end()) {
            autoRegisterNode(src);
        }
        if (nodeMap_.find(dst) == nodeMap_.end()) {
            autoRegisterNode(dst);
        }

        // Check cache first
        std::string key = std::string(src) + " " + dst;
        auto it = latencyCache_.find(key);
        if (it != latencyCache_.end()) {
            return it->second;
        }

        // Calculate latency
        uint32_t srcId = nodeMap_[src];
        uint32_t dstId = nodeMap_[dst];

        uint32_t latency;
        if (cycleAccurate_) {
            latency = getCycleAccurateLatency(srcId, dstId);
        } else {
            latency = getAnalyticalLatency(srcId, dstId);
        }

        // Cache and return (RTT = 2x one-way)
        uint32_t rtt = 2 * latency;
        latencyCache_[key] = rtt;

        stats_.total_packets++;

        return rtt;
    }

    /**
     * Get basic network statistics (legacy interface)
     */
    void getStats(uint64_t& packets, uint64_t& hops, uint64_t& avgLat) const {
        packets = stats_.total_packets;
        hops = stats_.total_hops;
        avgLat = (stats_.total_packets > 0) ? stats_.total_latency / stats_.total_packets : 0;
    }

    /**
     * Get comprehensive stats for McPAT power modeling
     */
    const GarnetStats& getGarnetStats() const {
        return stats_;
    }

    /**
     * Set total simulation cycles (called at end of simulation)
     */
    void setTotalCycles(uint64_t cycles) {
        stats_.total_cycles = cycles;
    }

    void printStats() const {
        info("[GarnetNetwork] Statistics:");
        info("  Total packets: %lu", stats_.total_packets);
        info("  Total flits: %lu", stats_.total_flits);
        info("  Total hops: %lu", stats_.total_hops);
        info("  Link traversals: %lu", stats_.link_traversals);
        info("  Buffer reads: %lu", stats_.buffer_reads);
        info("  Buffer writes: %lu", stats_.buffer_writes);
        info("  Crossbar traversals: %lu", stats_.crossbar_traversals);
        if (stats_.total_packets > 0) {
            info("  Avg latency: %lu cycles", stats_.total_latency / stats_.total_packets);
        }
    }

    /**
     * Write stats to file for PIMID McPAT integration
     */
    void writeStatsFile(const char* filename) const {
        FILE* f = fopen(filename, "w");
        if (!f) {
            warn("[GarnetNetwork] Failed to write stats to %s", filename);
            return;
        }
        fprintf(f, "# GarnetNetwork Statistics for McPAT\n");
        fprintf(f, "garnet.total_packets = %lu\n", stats_.total_packets);
        fprintf(f, "garnet.total_flits = %lu\n", stats_.total_flits);
        fprintf(f, "garnet.total_hops = %lu\n", stats_.total_hops);
        fprintf(f, "garnet.buffer_reads = %lu\n", stats_.buffer_reads);
        fprintf(f, "garnet.buffer_writes = %lu\n", stats_.buffer_writes);
        fprintf(f, "garnet.crossbar_traversals = %lu\n", stats_.crossbar_traversals);
        fprintf(f, "garnet.arbiter_events = %lu\n", stats_.arbiter_events);
        fprintf(f, "garnet.link_traversals = %lu\n", stats_.link_traversals);
        fprintf(f, "garnet.total_cycles = %lu\n", stats_.total_cycles);
        fprintf(f, "garnet.total_latency = %lu\n", stats_.total_latency);
        fprintf(f, "garnet.num_routers = %u\n", stats_.num_routers);
        fprintf(f, "garnet.num_rows = %u\n", stats_.num_rows);
        fprintf(f, "garnet.num_cols = %u\n", stats_.num_cols);
        fprintf(f, "garnet.flit_size_bits = %u\n", stats_.flit_size_bits);
        fprintf(f, "garnet.clock_mhz = %.1f\n", stats_.clock_mhz);
        fclose(f);
        info("[GarnetNetwork] Stats written to %s", filename);
    }

private:
    /**
     * Calculate Manhattan distance in mesh
     */
    uint32_t getManhattanDistance(uint32_t src, uint32_t dst) const {
        uint32_t srcRow = src / numCols_;
        uint32_t srcCol = src % numCols_;
        uint32_t dstRow = dst / numCols_;
        uint32_t dstCol = dst % numCols_;

        uint32_t rowDist = (srcRow > dstRow) ? (srcRow - dstRow) : (dstRow - srcRow);
        uint32_t colDist = (srcCol > dstCol) ? (srcCol - dstCol) : (dstCol - srcCol);

        return rowDist + colDist;
    }

    /**
     * Analytical latency calculation based on XY routing
     * Also tracks detailed router/link activity for McPAT
     */
    uint32_t getAnalyticalLatency(uint32_t src, uint32_t dst) {
        if (src == dst) {
            return 0;  // Same node, no network traversal
        }

        uint32_t hops = getManhattanDistance(src, dst);

        // Latency model:
        // - Injection at source NI
        // - For each hop: router pipeline + link traversal
        // - Ejection at destination NI
        uint32_t latency = injectionLatency_ +
                          hops * (routerLatency_ + linkLatency_) +
                          injectionLatency_;

        // Update comprehensive stats for McPAT
        stats_.total_hops += hops;
        stats_.total_latency += latency;

        // Estimate flit count (assume 1 flit per packet for cache line)
        uint32_t flits_per_packet = 1;  // Simplified: cache line fits in one flit
        stats_.total_flits += flits_per_packet;

        // Router activity per hop:
        // - 2 buffer accesses (read input, write output) per router
        // - 1 crossbar traversal per router
        // - 1 arbiter event per router
        stats_.buffer_reads += hops;
        stats_.buffer_writes += hops;
        stats_.crossbar_traversals += hops;
        stats_.arbiter_events += hops;

        // Link activity: one traversal per hop
        stats_.link_traversals += hops;

        return latency;
    }

    /**
     * Cycle-accurate latency using Garnet simulation
     * TODO: Integrate with PIMID's Garnet wrapper for true cycle-accuracy
     */
    uint32_t getCycleAccurateLatency(uint32_t src, uint32_t dst) {
        // For now, fall back to analytical with contention estimate
        uint32_t baseLat = getAnalyticalLatency(src, dst);

        // Add estimated contention (simple model: 10% overhead)
        uint32_t contentionLat = baseLat / 10;

        return baseLat + contentionLat;
    }
};

#endif  // GARNET_NETWORK_H_
