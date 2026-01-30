/**
 * @file multi_instance_garnet_network.h
 * @brief Multi-Instantiated GARNET Network for DRAM Hierarchies
 *
 * This extension creates separate GARNET network instances for each unit
 * in the DRAM hierarchy, enabling accurate modeling of:
 * - Per-bank subarray networks
 * - Per-bank-group bank networks
 * - Per-chip bank group networks
 * - Per-rank chip networks
 *
 * This is critical for accurate PIM modeling where data movement occurs
 * at multiple hierarchy levels simultaneously.
 */

#ifndef PIMID_MULTI_INSTANCE_GARNET_NETWORK_H
#define PIMID_MULTI_INSTANCE_GARNET_NETWORK_H

#include <vector>
#include <map>
#include <memory>
#include <string>
#include <functional>
#include "network_models/include/network_model.h"
#include "internal_dram_network.h"

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

    // Network parameters per level
    struct LevelConfig {
        int link_width_bits;
        int link_latency_cycles;
        double bandwidth_GBs;
        TopologyType topology;
        int virtual_channels;
        int buffer_depth;
    };

    LevelConfig subarray_config;   // Within bank
    LevelConfig bank_config;       // Within bank group
    LevelConfig bg_config;         // Within chip
    LevelConfig chip_config;       // Within rank
    LevelConfig rank_config;       // Within channel

    // Constructor with DDR4-like defaults
    MultiInstanceNetworkConfig()
        : num_channels(1), num_ranks_per_channel(1),
          num_chips_per_rank(1), num_bg_per_chip(4),
          num_banks_per_bg(4), num_subarrays_per_bank(4) {

        // Subarray network (wide, fast)
        subarray_config = {64, 3, 9.6, TopologyType::H_TREE, 2, 4};
        // Bank network
        bank_config = {8, 6, 1.2, TopologyType::H_TREE, 2, 4};
        // Bank group network
        bg_config = {16, 10, 2.4, TopologyType::H_TREE, 2, 4};
        // Chip network
        chip_config = {8, 12, 1.2, TopologyType::H_TREE, 2, 4};
        // Rank network (TSI/interposer)
        rank_config = {128, 20, 25.6, TopologyType::BUS, 4, 8};
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
     * @brief Get the subarray network for a specific bank
     * @param bank_id Bank ID
     * @return Network instance for that bank's subarray network
     */
    std::shared_ptr<NetworkModel> getSubarrayNetwork(int bank_id);

    /**
     * @brief Get the bank network for a specific bank group
     * @param bg_id Bank group ID
     * @return Network instance for that bank group's bank network
     */
    std::shared_ptr<NetworkModel> getBankNetwork(int bg_id);

    /**
     * @brief Get the bank group network for a specific chip
     * @param chip_id Chip ID
     * @return Network instance for that chip's bank group network
     */
    std::shared_ptr<NetworkModel> getBankGroupNetwork(int chip_id);

    /**
     * @brief Get the chip network for a specific rank
     * @param rank_id Rank ID
     * @return Network instance for that rank's chip network
     */
    std::shared_ptr<NetworkModel> getChipNetwork(int rank_id);

    /**
     * @brief Get the rank network for a specific channel
     * @param channel_id Channel ID
     * @return Network instance for that channel's rank network
     */
    std::shared_ptr<NetworkModel> getRankNetwork(int channel_id);

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

    /**
     * @brief Convert bank ID to bank group ID
     */
    int bankToBankGroup(int bank_id) const;

    /**
     * @brief Convert bank ID to chip ID
     */
    int bankToChip(int bank_id) const;

    /**
     * @brief Convert bank ID to rank ID
     */
    int bankToRank(int bank_id) const;

    /**
     * @brief Convert bank ID to channel ID
     */
    int bankToChannel(int bank_id) const;

    /**
     * @brief Check if two banks are in the same bank group
     */
    bool inSameBankGroup(int bank1, int bank2) const;

    /**
     * @brief Check if two banks are in the same chip
     */
    bool inSameChip(int bank1, int bank2) const;

    /**
     * @brief Check if two banks are in the same rank
     */
    bool inSameRank(int bank1, int bank2) const;

    /**
     * @brief Check if two banks are in the same channel
     */
    bool inSameChannel(int bank1, int bank2) const;

private:
    MultiInstanceNetworkConfig config_;

    // Multi-instance network storage
    // Key: instance ID (bank_id, bg_id, chip_id, rank_id, channel_id)
    std::vector<std::shared_ptr<NetworkModel>> subarray_networks_;  // One per bank
    std::vector<std::shared_ptr<NetworkModel>> bank_networks_;       // One per bank group
    std::vector<std::shared_ptr<NetworkModel>> bg_networks_;         // One per chip
    std::vector<std::shared_ptr<NetworkModel>> chip_networks_;       // One per rank
    std::vector<std::shared_ptr<NetworkModel>> rank_networks_;       // One per channel

    // Per-instance statistics
    std::vector<NetworkInstanceStats> subarray_stats_;
    std::vector<NetworkInstanceStats> bank_stats_;
    std::vector<NetworkInstanceStats> bg_stats_;
    std::vector<NetworkInstanceStats> chip_stats_;
    std::vector<NetworkInstanceStats> rank_stats_;

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
