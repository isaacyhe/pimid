/**
 * @file internal_dram_network.h
 * @brief Internal DRAM Network Model for PIM Data Movement
 *
 * This file models the network INSIDE a DRAM chip that enables data movement
 * between subarrays, banks, and bank groups when PIM units are placed at
 * fine granularity levels.
 *
 * CRITICAL REQUIREMENT:
 * "bank-wide, subarray-wide, bg-wide and chip-wide PE do always, I mean always,
 *  need our network model to enable data communication within."
 *
 * ARCHITECTURE:
 * - Subarray-to-Subarray: Needs network within bank
 * - Bank-to-Bank: Needs network within bank group or chip
 * - BankGroup-to-BankGroup: Needs network within chip
 * - Chip-to-Chip: Needs network within rank (via TSI for HBM, interposer, etc.)
 *
 * NETWORK TOPOLOGY:
 * We use a hierarchical network matching DRAM hierarchy:
 * - Level 1: Subarray network (within bank)
 * - Level 2: Bank network (within bank group)
 * - Level 3: Bank group network (within chip)
 * - Level 4: Chip network (within rank)
 */

#ifndef PIMID_INTERNAL_DRAM_NETWORK_H
#define PIMID_INTERNAL_DRAM_NETWORK_H

#include <vector>
#include <queue>
#include <memory>
#include <string>
#include "pim_request_payload.h"
#include "network_models/include/network_model.h"

namespace pimid {

/**
 * @brief Network Link Configuration inside DRAM
 */
struct InternalNetworkLink {
    int link_width_bits;       // Link width (e.g., 8, 16, 32, 64 bits)
    double frequency_GHz;      // Operating frequency
    double bandwidth_GBs;      // Effective bandwidth
    int latency_cycles;        // Base latency (cycles)

    // Routing information
    int source_id;             // Source unit (bank/subarray ID)
    int dest_id;               // Destination unit
    std::string topology;      // "bus", "crossbar", "mesh", "tree"
};

/**
 * @brief Network Packet for internal DRAM transfers
 */
struct InternalNetworkPacket {
    uint64_t packet_id;
    int source_bank;
    int dest_bank;
    int source_subarray;
    int dest_subarray;
    uint64_t data_bytes;
    uint64_t injection_time;   // When packet entered network
    uint64_t completion_time;  // When packet left network
    bool completed;

    // Callback when packet completes
    std::function<void()> callback;
};

/**
 * @brief Hierarchical Network Level
 */
enum class NetworkLevel {
    SUBARRAY_NETWORK,      // Within-bank network (subarray-to-subarray)
    BANK_NETWORK,          // Within-bankgroup network (bank-to-bank)
    BANK_GROUP_NETWORK,    // Within-chip network (bankgroup-to-bankgroup)
    CHIP_NETWORK           // Within-rank network (chip-to-chip, needs TSI/interposer)
};

/**
 * @brief Internal DRAM Network Model
 *
 * This models the network inside DRAM chips that enables PIM data movement.
 * Each DRAM hierarchy level has its own network with different topology and BW.
 */
class InternalDRAMNetwork {
public:
    /**
     * @brief Constructor
     * @param dram_type DRAM type ("DDR4", "DDR5", "HBM2", "HBM3")
     * @param network_model Optional external network model for detailed simulation
     */
    InternalDRAMNetwork(const std::string& dram_type,
                       std::shared_ptr<NetworkModel> network_model = nullptr);

    ~InternalDRAMNetwork() = default;

    /**
     * @brief Initialize the internal network based on DRAM architecture
     */
    void initialize(int num_subarrays_per_bank,
                   int num_banks_per_bg,
                   int num_bg_per_chip,
                   int num_chips_per_rank);

    /**
     * @brief Send a packet through the internal network
     * @return true if packet accepted, false if network congested
     */
    bool sendPacket(const InternalNetworkPacket& packet);

    /**
     * @brief Tick the network (advance by one cycle)
     */
    void tick();

    /**
     * @brief Get network latency for a transfer
     * @param source_level Source DRAM level
     * @param dest_level Destination DRAM level
     * @param data_bytes Amount of data to transfer
     * @return Latency in cycles
     */
    uint64_t getTransferLatency(NetworkLevel level,
                               int source_id, int dest_id,
                               uint64_t data_bytes);

    /**
     * @brief Check if network can accept more packets
     */
    bool canAcceptPacket(NetworkLevel level);

    /**
     * @brief Get network statistics
     */
    void printStats() const;

    /**
     * @brief Reset network statistics
     */
    void resetStats();

    /**
     * @brief Calculate network requirements for a data access pattern
     * @param pe_bank PE's local bank
     * @param pe_bg PE's local bank group
     * @param pe_chip PE's local chip
     * @param data_distribution Map of (bank_id -> bytes) showing data location
     * @param[out] transfers Vector of required network transfers
     * @return Total network latency in cycles
     */
    uint64_t calculateNetworkRequirements(
        int pe_bank, int pe_bg, int pe_chip,
        const std::map<int, uint64_t>& data_distribution,
        std::vector<InternalDRAMTransfer>& transfers);

    /**
     * @brief Execute a gather operation (collect data from multiple banks)
     * @param pe_bank PE's local bank (destination)
     * @param source_banks Vector of source bank IDs
     * @param bytes_per_bank Bytes to gather from each bank
     * @return Total network latency in cycles
     */
    uint64_t executeGather(
        int pe_bank,
        const std::vector<int>& source_banks,
        uint64_t bytes_per_bank);

    /**
     * @brief Execute a scatter operation (distribute data to multiple banks)
     * @param pe_bank PE's local bank (source)
     * @param dest_banks Vector of destination bank IDs
     * @param bytes_per_bank Bytes to scatter to each bank
     * @return Total network latency in cycles
     */
    uint64_t executeScatter(
        int pe_bank,
        const std::vector<int>& dest_banks,
        uint64_t bytes_per_bank);

    /**
     * @brief Execute a reduction operation (reduce across banks)
     * @param source_banks Vector of source bank IDs
     * @param dest_bank Destination bank for reduced result
     * @param bytes_per_bank Bytes from each bank
     * @return Total network latency in cycles
     */
    uint64_t executeReduce(
        const std::vector<int>& source_banks,
        int dest_bank,
        uint64_t bytes_per_bank);

    /**
     * @brief Execute a broadcast operation (send to all banks)
     * @param source_bank Source bank
     * @param dest_banks Vector of destination banks
     * @param total_bytes Total bytes to broadcast
     * @return Total network latency in cycles
     */
    uint64_t executeBroadcast(
        int source_bank,
        const std::vector<int>& dest_banks,
        uint64_t total_bytes);

    /**
     * @brief Get available network bandwidth at a level
     * @param level Network level
     * @return Available bandwidth in GB/s
     */
    double getAvailableBandwidth(NetworkLevel level) const;

    /**
     * @brief Check if two banks are in same bank group
     */
    bool inSameBankGroup(int bank1, int bank2) const;

    /**
     * @brief Check if two banks are in same chip
     */
    bool inSameChip(int bank1, int bank2) const;

    /**
     * @brief Enable GARNET H-tree simulation for accurate NoC modeling
     *
     * When enabled, GARNET models will be created for each hierarchy level
     * to provide cycle-accurate network simulation with contention, queuing, etc.
     *
     * @param enable True to use GARNET models, false for analytical model
     */
    void enableGarnetSimulation(bool enable = true);

private:
    // DRAM configuration
    std::string dram_type_;
    int num_subarrays_per_bank_;
    int num_banks_per_bg_;
    int num_bg_per_chip_;
    int num_chips_per_rank_;

    // Network configuration for each level
    InternalNetworkLink subarray_network_config_;   // Within bank
    InternalNetworkLink bank_network_config_;       // Within bank group
    InternalNetworkLink bg_network_config_;         // Within chip
    InternalNetworkLink chip_network_config_;       // Within rank

    // Packet queues for each network level
    std::queue<InternalNetworkPacket> subarray_network_queue_;
    std::queue<InternalNetworkPacket> bank_network_queue_;
    std::queue<InternalNetworkPacket> bg_network_queue_;
    std::queue<InternalNetworkPacket> chip_network_queue_;

    // In-flight packets
    std::vector<InternalNetworkPacket> inflight_packets_;

    // External network model (optional, for detailed simulation)
    std::shared_ptr<NetworkModel> external_network_model_;

    // GARNET H-tree models for each hierarchy level (optional)
    // When enabled, these provide cycle-accurate NoC simulation
    std::shared_ptr<NetworkModel> garnet_subarray_network_;   // H-tree within bank
    std::shared_ptr<NetworkModel> garnet_bank_network_;       // Bus/tree within BG
    std::shared_ptr<NetworkModel> garnet_bg_network_;         // Bus/tree within chip
    std::shared_ptr<NetworkModel> garnet_chip_network_;       // Bus/tree within rank
    bool use_garnet_models_;  // Flag to enable GARNET-based simulation

    // Current cycle
    uint64_t current_cycle_;

    // Statistics
    uint64_t total_packets_sent_;
    uint64_t total_packets_completed_;
    uint64_t total_bytes_transferred_;
    uint64_t total_network_latency_;
    uint64_t subarray_network_accesses_;
    uint64_t bank_network_accesses_;
    uint64_t bg_network_accesses_;
    uint64_t chip_network_accesses_;

    /**
     * @brief Configure network based on memory type (DRAM and NVM)
     */
    // SRAM configurations
    void configureSRAMNetwork();

    // NVM configurations
    void configureSTTMRAMNetwork();
    void configurePCMNetwork();
    void configureReRAMNetwork();

    // DRAM configurations
    void configureDDR3Network();
    void configureDDR4Network();
    void configureDDR5Network();
    void configureLPDDR5Network();
    void configureGDDR6Network();
    void configureHBMNetwork();
    void configureHBM2Network();
    void configureHBM3Network();

    /**
     * @brief Determine which network level is needed for a transfer
     */
    NetworkLevel determineNetworkLevel(int source_bank, int dest_bank,
                                      int source_subarray, int dest_subarray);

    /**
     * @brief Calculate transfer time based on bandwidth
     */
    uint64_t calculateTransferTime(const InternalNetworkLink& link,
                                   uint64_t data_bytes);

    /**
     * @brief Process packets in flight
     */
    void processInflightPackets();
};

/**
 * @brief Helper function to create internal network configuration
 *
 * This reads from our verified DRAM architecture specifications
 * to configure realistic network parameters.
 */
std::shared_ptr<InternalDRAMNetwork> createInternalDRAMNetwork(
    const std::string& dram_type,
    int num_subarrays_per_bank = 16,
    int num_banks_per_bg = 4,
    int num_bg_per_chip = 4,
    int num_chips_per_rank = 8);

/**
 * @brief Create GARNET H-tree network for DRAM internal interconnect
 *
 * This creates a GARNET network configured as an H-tree to mimic
 * the actual DRAM internal interconnects (e.g., Global Sense Amplifier
 * H-tree for subarray-to-subarray communication).
 *
 * @param level Network level (SUBARRAY, BANK, BANK_GROUP, CHIP)
 * @param num_nodes Number of leaf nodes (e.g., 16 subarrays per bank)
 * @param link_width_bits Link width in bits (from DRAM specs)
 * @param link_latency_cycles Base link latency in cycles
 * @param bandwidth_GBs Available bandwidth in GB/s
 * @return Configured GARNET network model
 */
std::shared_ptr<NetworkModel> createGarnetHTreeForDRAM(
    NetworkLevel level,
    int num_nodes,
    int link_width_bits,
    int link_latency_cycles,
    double bandwidth_GBs);

} // namespace pimid

#endif // PIMID_INTERNAL_DRAM_NETWORK_H
