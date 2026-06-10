/**
 * @file multi_instance_garnet_network.h
 * @brief Multi-Instantiated GARNET Network for DRAM Hierarchies
 *
 * This extension creates separate GARNET network instances for each unit
 * in the DRAM hierarchy (7 levels, L0-L6), enabling accurate modeling of:
 * - Per-bank subarray networks (L0)
 * - Per-bank-group bank networks (L1)
 * - Per-chip bank group networks (L2)
 * - Per-rank chip/die-layer networks (L3)
 * - Per-channel rank/logic-die networks (L4)
 * - System-wide channel network (L5)
 * - System root network (L6)
 *
 * This is critical for accurate PIM modeling where data movement occurs
 * at multiple hierarchy levels simultaneously.
 */

#ifndef PIMID_MULTI_INSTANCE_GARNET_NETWORK_H
#define PIMID_MULTI_INSTANCE_GARNET_NETWORK_H

#include <array>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <functional>
#include "network/network_model.h"
#include "memory/internal_dram_network.h"

namespace pimid {

/**
 * @brief Statistics for a single network instance
 */
struct NetworkInstanceStats {
    uint64_t packets_sent;
    uint64_t packets_completed;
    uint64_t bytes_transferred;
    uint64_t total_latency_cycles;
    double avg_latency;
    double utilization;

    NetworkInstanceStats()
        : packets_sent(0), packets_completed(0), bytes_transferred(0),
          total_latency_cycles(0), avg_latency(0.0), utilization(0.0) {}
};

/**
 * @brief Configuration for multi-instance network
 */
struct MultiInstanceNetworkConfig {
    // DRAM hierarchy configuration
    int num_channels;
    int num_ranks_per_channel;
    int num_chips_per_rank;
    int num_bg_per_chip;
    int num_banks_per_bg;
    int num_subarrays_per_bank;

    // DRAM technology
    MemoryTechnology technology;

    // Network parameters per level
    struct LevelConfig {
        int link_width_bits;
        int link_latency_cycles;
        double bandwidth_GBs;
        TopologyType topology;
        int virtual_channels;
        int buffer_depth;
    };

    // Array-based level configs indexed by NetworkLevel (0-6)
    std::array<LevelConfig, NUM_HIERARCHY_LEVELS> level_configs;

    // Constructor with DDR4-like defaults
    MultiInstanceNetworkConfig()
        : num_channels(1), num_ranks_per_channel(1),
          num_chips_per_rank(1), num_bg_per_chip(4),
          num_banks_per_bg(4), num_subarrays_per_bank(4),
          technology(MemoryTechnology::DDR4) {

        // L0: Subarray network (wide, fast - within bank)
        level_configs[0] = {64, 3, 9.6, TopologyType::H_TREE, 2, 4};
        // L1: Bank network (within bank group)
        level_configs[1] = {8, 6, 1.2, TopologyType::H_TREE, 2, 4};
        // L2: Bank group network (within chip)
        level_configs[2] = {16, 10, 2.4, TopologyType::H_TREE, 2, 4};
        // L3: Chip network (within rank)
        level_configs[3] = {8, 12, 1.2, TopologyType::H_TREE, 2, 4};
        // L4: Rank network (within channel, TSI/interposer)
        level_configs[4] = {128, 20, 25.6, TopologyType::BUS, 4, 8};
        // L5: Channel network (to memory controller)
        level_configs[5] = {64, 25, 19.2, TopologyType::BUS, 4, 8};
        // L6: System network (root)
        level_configs[6] = {64, 30, 19.2, TopologyType::BUS, 4, 8};
    }
};

/**
 * @brief Multi-Instantiated GARNET Network for DRAM Hierarchies
 *
 * Creates and manages multiple GARNET network instances, one for each
 * unit at each hierarchy level. This accurately models the fact that
 * different banks/bank groups/chips have independent networks.
 */
class MultiInstanceGarnetNetwork {
public:
    /**
     * @brief Constructor
     * @param config Multi-instance network configuration
     */
    explicit MultiInstanceGarnetNetwork(const MultiInstanceNetworkConfig& config);

    ~MultiInstanceGarnetNetwork() = default;

    /**
     * @brief Initialize all network instances
     */
    void initialize();

    /**
     * @brief Send a packet through the appropriate network instance
     * @param packet Packet to send
     * @return true if accepted, false if congested
     */
    bool sendPacket(const InternalNetworkPacket& packet);

    /**
     * @brief Tick all network instances
     */
    void tick();

    /**
     * @brief Get current cycle
     */
    uint64_t getCurrentCycle() const { return current_cycle_; }

    /**
     * @brief Get transfer latency for a specific route
     */
    uint64_t getTransferLatency(int src_bank, int dst_bank,
                                int src_subarray, int dst_subarray,
                                uint64_t data_bytes);

    /**
     * @brief Check if network can accept packet
     */
    bool canAcceptPacket(int src_bank, int dst_bank);

    // ========================================
    // Network Instance Access
    // ========================================

    /**
     * @brief Get network instance at a given level and instance ID
     * @param level Network hierarchy level
     * @param instance_id Instance index within the level
     * @return Network instance, or nullptr if out of range
     */
    std::shared_ptr<NetworkModel> getNetworkAtLevel(NetworkLevel level, int instance_id);

    /**
     * @brief Get the subarray network for a specific bank
     */
    std::shared_ptr<NetworkModel> getSubarrayNetwork(int bank_id);

    /**
     * @brief Get the bank network for a specific bank group
     */
    std::shared_ptr<NetworkModel> getBankNetwork(int bg_id);

    /**
     * @brief Get the bank group network for a specific chip
     */
    std::shared_ptr<NetworkModel> getBankGroupNetwork(int chip_id);

    /**
     * @brief Get the chip network for a specific rank
     */
    std::shared_ptr<NetworkModel> getChipNetwork(int rank_id);

    /**
     * @brief Get the rank network for a specific channel
     */
    std::shared_ptr<NetworkModel> getRankNetwork(int channel_id);

    /**
     * @brief Get the channel network (system-wide, single instance)
     */
    std::shared_ptr<NetworkModel> getChannelNetwork(int instance_id = 0);

    /**
     * @brief Get the system network (root, single instance)
     */
    std::shared_ptr<NetworkModel> getSystemNetwork(int instance_id = 0);

    // ========================================
    // Statistics
    // ========================================

    /**
     * @brief Print statistics for all network instances
     */
    void printStats() const;

    /**
     * @brief Print statistics for a specific hierarchy level
     */
    void printLevelStats(NetworkLevel level) const;

    /**
     * @brief Get statistics for a specific network instance
     */
    NetworkInstanceStats getInstanceStats(NetworkLevel level, int instance_id) const;

    /**
     * @brief Get aggregated statistics for a hierarchy level
     */
    NetworkInstanceStats getAggregatedStats(NetworkLevel level) const;

    /**
     * @brief Reset all statistics
     */
    void resetStats();

    // ========================================
    // Configuration Queries
    // ========================================

    /**
     * @brief Get total number of network instances
     */
    int getTotalNetworkInstances() const;

    /**
     * @brief Get number of instances at a specific level
     */
    int getNumInstancesAtLevel(NetworkLevel level) const;

    /**
     * @brief Get the configuration
     */
    const MultiInstanceNetworkConfig& getConfig() const { return config_; }

    // ========================================
    // Address Mapping Helpers
    // ========================================

    int bankToBankGroup(int bank_id) const;
    int bankToChip(int bank_id) const;
    int bankToRank(int bank_id) const;
    int bankToChannel(int bank_id) const;

    bool inSameBankGroup(int bank1, int bank2) const;
    bool inSameChip(int bank1, int bank2) const;
    bool inSameRank(int bank1, int bank2) const;
    bool inSameChannel(int bank1, int bank2) const;

private:
    MultiInstanceNetworkConfig config_;

    // Array-based network storage indexed by level (0-6)
    std::array<std::vector<std::shared_ptr<NetworkModel>>, NUM_HIERARCHY_LEVELS> networks_;

    // Array-based per-instance statistics indexed by level (0-6)
    std::array<std::vector<NetworkInstanceStats>, NUM_HIERARCHY_LEVELS> stats_;

    // Global state
    uint64_t current_cycle_;
    bool initialized_;

    // In-flight packets with routing info
    struct InflightPacket {
        InternalNetworkPacket packet;
        NetworkLevel level;
        int instance_id;
        uint64_t expected_completion;
    };
    std::vector<InflightPacket> inflight_packets_;

    // Helper functions
    std::shared_ptr<NetworkModel> createNetworkInstance(
        NetworkLevel level,
        int num_nodes,
        const MultiInstanceNetworkConfig::LevelConfig& level_config);

    NetworkLevel determineNetworkLevel(int src_bank, int dst_bank,
                                       int src_subarray, int dst_subarray);

    int getInstanceId(NetworkLevel level, int bank_id);

    void updateStats(NetworkLevel level, int instance_id,
                    uint64_t bytes, uint64_t latency);

    /**
     * @brief Get human-readable name for a network level
     */
    std::string getLevelName(int level_idx) const;

    /**
     * @brief Get the number of nodes (endpoints) for a given level
     */
    int getNodesPerInstance(int level_idx) const;
};

// ============================================================================
// Factory Functions for Common Configurations
// ============================================================================

/**
 * @brief Create multi-instance network for DDR4 configuration
 */
std::shared_ptr<MultiInstanceGarnetNetwork> createDDR4MultiInstanceNetwork(
    int num_channels = 1,
    int ranks_per_channel = 2);

/**
 * @brief Create multi-instance network for DDR5 configuration
 */
std::shared_ptr<MultiInstanceGarnetNetwork> createDDR5MultiInstanceNetwork(
    int num_channels = 2,
    int ranks_per_channel = 2);

/**
 * @brief Create multi-instance network for HBM2 configuration
 */
std::shared_ptr<MultiInstanceGarnetNetwork> createHBM2MultiInstanceNetwork(
    int num_channels = 8);

/**
 * @brief Create multi-instance network for HBM3 configuration
 */
std::shared_ptr<MultiInstanceGarnetNetwork> createHBM3MultiInstanceNetwork(
    int num_channels = 16);

} // namespace pimid

#endif // PIMID_MULTI_INSTANCE_GARNET_NETWORK_H
