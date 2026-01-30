/**
 * @file pim_bandwidth_tracker.h
 * @brief Bandwidth Tracking for PIM at Different DRAM Hierarchy Levels
 *
 * This file implements bandwidth tracking based on our VERIFIED DRAM architecture
 * specifications. It enforces the CRITICAL internal port bitwidth constraints:
 *
 * DDR4 Internal Ports (from dram_architecture_v2.h):
 * - Subarray GSA:      256 bits (INFERRED from DAS-MICRO15)
 * - Prefetch datapath: 64 bits  (VERIFIED from JEDEC)
 * - Bank serialization: 8 bits  (ESTIMATED - CRITICAL BOTTLENECK!)
 * - Chip I/O:          8 bits  (VERIFIED for x8 device)
 * - Rank interface:    64 bits (VERIFIED - FIRST WIDE interface)
 *
 * HBM2 Internal Ports:
 * - Bank serialization: 64 bits (INFERRED from TSV - 8x wider than DDR4!)
 * - Rank/Channel:      128 bits (VERIFIED from JEDEC)
 *
 * CRITICAL INSIGHT:
 * When multiple PEs share a DRAM hierarchy level, they SHARE the port bandwidth!
 * Example: 4 bank-level PEs in one bank → each gets 1.2 GB/s / 4 = 300 MB/s
 */

#ifndef PIMID_PIM_BANDWIDTH_TRACKER_H
#define PIMID_PIM_BANDWIDTH_TRACKER_H

#include <map>
#include <vector>
#include <memory>
#include <string>
#include "pim_request_payload.h"
#include "memory/dram_architecture_v2.h"

namespace pimid {

/**
 * @brief Bandwidth Usage Entry
 */
struct BandwidthUsage {
    uint64_t bytes_transferred;    // Bytes transferred in current window
    uint64_t cycles_active;        // Cycles port was active
    double utilization;            // Port utilization (0.0 to 1.0)
    int num_concurrent_pes;        // Number of PEs sharing this port
};

/**
 * @brief PIM Bandwidth Tracker
 *
 * Tracks bandwidth usage at each DRAM hierarchy level and enforces
 * port bitwidth constraints from verified DRAM architecture specs.
 */
class PIMBandwidthTracker {
public:
    /**
     * @brief Constructor
     * @param dram_arch Verified DRAM architecture specifications
     */
    explicit PIMBandwidthTracker(
        std::shared_ptr<pimid::memory::DRAMArchitectureV2> dram_arch);

    ~PIMBandwidthTracker() = default;

    /**
     * @brief Initialize tracker for specific DRAM configuration
     */
    void initialize(int num_channels, int num_ranks,
                   int num_bank_groups, int num_banks, int num_subarrays);

    /**
     * @brief Request bandwidth for a PIM operation
     * @param payload PIM request payload with granularity info
     * @param required_bytes Amount of data to transfer
     * @return Latency in cycles (considering port BW limit and contention)
     */
    uint64_t requestBandwidth(const PIMRequestPayload& payload,
                             uint64_t required_bytes);

    /**
     * @brief Check if bandwidth is available for a request
     * @param payload PIM request payload
     * @param required_bytes Amount of data
     * @return true if bandwidth available, false if port saturated
     */
    bool canAcceptRequest(const PIMRequestPayload& payload,
                         uint64_t required_bytes);

    /**
     * @brief Tick the bandwidth tracker (advance by one cycle)
     */
    void tick();

    /**
     * @brief Get bandwidth limit for a specific DRAM level
     * @param granularity PIM granularity level
     * @return Bandwidth in GB/s
     */
    double getBandwidthLimit(PIMGranularity granularity) const;

    /**
     * @brief Get port bitwidth for a specific DRAM level
     * @param granularity PIM granularity level
     * @return Port bitwidth in bits
     */
    int getPortBitwidth(PIMGranularity granularity) const;

    /**
     * @brief Register a PE at a specific DRAM level
     * @param granularity Where the PE is placed
     * @param pe_id PE identifier
     * @param target_bank Which bank this PE operates on
     */
    void registerPE(PIMGranularity granularity, int pe_id, int target_bank);

    /**
     * @brief Get number of PEs sharing a specific port
     * @param granularity DRAM level
     * @param target_id Bank/subarray ID
     * @return Number of concurrent PEs
     */
    int getConcurrentPEs(PIMGranularity granularity, int target_id) const;

    /**
     * @brief Calculate effective bandwidth per PE (considering sharing)
     * @param granularity DRAM level
     * @param target_id Bank/subarray ID
     * @return Effective BW per PE in GB/s
     */
    double getEffectiveBandwidthPerPE(PIMGranularity granularity,
                                     int target_id) const;

    /**
     * @brief Set queue depth limit for a specific granularity level
     * @param granularity PIM granularity level
     * @param limit Maximum queue depth (0 = unlimited)
     */
    void setQueueDepthLimit(PIMGranularity granularity, size_t limit);

    /**
     * @brief Set queue depth limit for all granularity levels
     * @param limit Maximum queue depth (0 = unlimited)
     */
    void setGlobalQueueDepthLimit(size_t limit);

    /**
     * @brief Get queue depth limit for a specific granularity level
     * @param granularity PIM granularity level
     * @return Queue depth limit (0 = unlimited)
     */
    size_t getQueueDepthLimit(PIMGranularity granularity) const;

    /**
     * @brief Get current queue depth for a specific port
     * @param granularity PIM granularity level
     * @param target_id Target bank/subarray ID
     * @return Current queue depth
     */
    size_t getCurrentQueueDepth(PIMGranularity granularity, int target_id) const;

    /**
     * @brief Complete a request (decrement queue depth)
     * @param payload Request payload
     */
    void completeRequest(const PIMRequestPayload& payload);

    /**
     * @brief Print bandwidth statistics
     */
    void printStats() const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

    /**
     * @brief Calculate local capacity for a PE at given granularity
     * @param granularity PIM granularity level
     * @param pe_id PE identifier
     * @return Local capacity in bytes
     */
    uint64_t calculateLocalCapacity(PIMGranularity granularity, int pe_id) const;

    /**
     * @brief Determine if data access is local or requires network
     * @param payload PIM request with PE location
     * @param data_bank Bank where data resides
     * @param data_bg Bank group where data resides
     * @param data_chip Chip where data resides
     * @return Data locality classification
     */
    DataLocality determineDataLocality(
        const PIMRequestPayload& payload,
        int data_bank, int data_bg, int data_chip) const;

    /**
     * @brief Calculate data reach breakdown (local vs remote)
     * @param payload PIM request
     * @param total_data_bytes Total data needed
     * @param data_distribution Map of (bank_id -> bytes) showing where data lives
     * @param[out] local_bytes Output: bytes that are local
     * @param[out] remote_bytes Output: bytes requiring network
     */
    void calculateDataReach(
        const PIMRequestPayload& payload,
        uint64_t total_data_bytes,
        const std::map<int, uint64_t>& data_distribution,
        uint64_t& local_bytes,
        uint64_t& remote_bytes) const;

    /**
     * @brief Get the bank ID where a PE's local data resides
     * @param granularity PIM granularity
     * @param pe_id PE identifier
     * @return Bank ID for PE's local data
     */
    int getPELocalBank(PIMGranularity granularity, int pe_id) const;

    /**
     * @brief Get the bank group ID for a PE
     * @param pe_id PE identifier
     * @return Bank group ID
     */
    int getPEBankGroup(int pe_id) const;

    /**
     * @brief Get the chip ID for a PE
     * @param pe_id PE identifier
     * @return Chip ID
     */
    int getPEChip(int pe_id) const;

private:
    // DRAM architecture specifications
    std::shared_ptr<pimid::memory::DRAMArchitectureV2> dram_arch_;

    // DRAM configuration
    int num_channels_;
    int num_ranks_;
    int num_bank_groups_;
    int num_banks_;
    int num_subarrays_;

    // Bandwidth limits (GB/s) for each level
    double subarray_bw_limit_;
    double bank_bw_limit_;
    double bank_group_bw_limit_;
    double chip_bw_limit_;
    double rank_bw_limit_;
    double mc_bw_limit_;

    // Port bitwidths (bits) for each level
    int subarray_port_bits_;    // GSA + prefetch
    int bank_port_bits_;        // Bank serialization (CRITICAL!)
    int bank_group_port_bits_;  // BG aggregation
    int chip_port_bits_;        // Chip I/O
    int rank_port_bits_;        // Rank interface
    int mc_port_bits_;          // Memory controller

    // Clock frequency
    double clock_freq_GHz_;

    // PE registration: maps (granularity, target_id) -> list of PE IDs
    std::map<std::pair<PIMGranularity, int>, std::vector<int>> pe_registry_;

    // Bandwidth usage tracking: maps (granularity, target_id) -> usage
    std::map<std::pair<PIMGranularity, int>, BandwidthUsage> bandwidth_usage_;

    // Active transfers: maps (granularity, target_id) -> bytes in flight
    std::map<std::pair<PIMGranularity, int>, uint64_t> active_transfers_;

    // Current cycle
    uint64_t current_cycle_;

    // Statistics window
    static constexpr uint64_t STATS_WINDOW_CYCLES = 10000;
    uint64_t window_start_cycle_;

    // Queue depth limits per granularity (0 = unlimited)
    static constexpr size_t DEFAULT_QUEUE_DEPTH_LIMIT = 32;
    std::map<PIMGranularity, size_t> queue_depth_limits_;
    std::map<std::pair<PIMGranularity, int>, size_t> active_queue_depths_;

    // Statistics
    uint64_t total_requests_;
    uint64_t bandwidth_limited_requests_;
    uint64_t total_bytes_transferred_;
    std::map<PIMGranularity, uint64_t> requests_per_level_;
    std::map<PIMGranularity, uint64_t> bytes_per_level_;

    /**
     * @brief Initialize bandwidth limits from DRAM architecture
     */
    void initializeBandwidthLimits();

    /**
     * @brief Calculate transfer latency based on port BW and contention
     */
    uint64_t calculateTransferLatency(PIMGranularity granularity,
                                     int target_id,
                                     uint64_t bytes);

    /**
     * @brief Update bandwidth usage statistics
     */
    void updateUsageStats(PIMGranularity granularity,
                         int target_id,
                         uint64_t bytes,
                         uint64_t cycles);

    /**
     * @brief Check if stats window should reset
     */
    void checkStatsWindow();

    /**
     * @brief Get bandwidth limit based on port bitwidth and clock
     */
    double calculateBandwidth(int port_bits, double clock_GHz) const {
        return (port_bits / 8.0) * clock_GHz;  // GB/s
    }
};

} // namespace pimid

#endif // PIMID_PIM_BANDWIDTH_TRACKER_H
