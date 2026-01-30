/**
 * @file multi_instance_garnet_network.cpp
 * @brief Implementation of Multi-Instantiated GARNET Network for DRAM Hierarchies
 */

#include "memory_models/include/multi_instance_garnet_network.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace pimid {

// ============================================================================
// MultiInstanceGarnetNetwork Implementation
// ============================================================================

MultiInstanceGarnetNetwork::MultiInstanceGarnetNetwork(const MultiInstanceNetworkConfig& config)
    : config_(config),
      current_cycle_(0),
      initialized_(false) {
}

void MultiInstanceGarnetNetwork::initialize() {
    if (initialized_) {
        std::cout << "[MultiInstanceGarnet] Already initialized" << std::endl;
        return;
    }

    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║ Multi-Instance GARNET Network Initialization                      ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n" << std::endl;

    // Calculate total counts at each level
    int total_ranks = config_.num_channels * config_.num_ranks_per_channel;
    int total_chips = total_ranks * config_.num_chips_per_rank;
    int total_bgs = total_chips * config_.num_bg_per_chip;
    int total_banks = total_bgs * config_.num_banks_per_bg;

    std::cout << "DRAM Hierarchy:" << std::endl;
    std::cout << "  Channels:           " << config_.num_channels << std::endl;
    std::cout << "  Ranks per channel:  " << config_.num_ranks_per_channel << std::endl;
    std::cout << "  Chips per rank:     " << config_.num_chips_per_rank << std::endl;
    std::cout << "  Bank groups per chip: " << config_.num_bg_per_chip << std::endl;
    std::cout << "  Banks per BG:       " << config_.num_banks_per_bg << std::endl;
    std::cout << "  Subarrays per bank: " << config_.num_subarrays_per_bank << std::endl;
    std::cout << std::endl;
    std::cout << "  Total ranks:        " << total_ranks << std::endl;
    std::cout << "  Total chips:        " << total_chips << std::endl;
    std::cout << "  Total bank groups:  " << total_bgs << std::endl;
    std::cout << "  Total banks:        " << total_banks << std::endl;
    std::cout << std::endl;

    // ========================================
    // Create Subarray Networks (one per bank)
    // ========================================
    std::cout << "Creating Subarray Networks (one per bank)..." << std::endl;
    subarray_networks_.resize(total_banks);
    subarray_stats_.resize(total_banks);

    for (int bank = 0; bank < total_banks; bank++) {
        subarray_networks_[bank] = createNetworkInstance(
            NetworkLevel::SUBARRAY_NETWORK,
            config_.num_subarrays_per_bank,
            config_.subarray_config);
    }
    std::cout << "  Created " << total_banks << " subarray network instances" << std::endl;
    std::cout << "  Each with " << config_.num_subarrays_per_bank << " nodes, "
              << config_.subarray_config.link_width_bits << "-bit links, "
              << config_.subarray_config.bandwidth_GBs << " GB/s" << std::endl;

    // ========================================
    // Create Bank Networks (one per bank group)
    // ========================================
    std::cout << "\nCreating Bank Networks (one per bank group)..." << std::endl;
    bank_networks_.resize(total_bgs);
    bank_stats_.resize(total_bgs);

    for (int bg = 0; bg < total_bgs; bg++) {
        bank_networks_[bg] = createNetworkInstance(
            NetworkLevel::BANK_NETWORK,
            config_.num_banks_per_bg,
            config_.bank_config);
    }
    std::cout << "  Created " << total_bgs << " bank network instances" << std::endl;
    std::cout << "  Each with " << config_.num_banks_per_bg << " nodes, "
              << config_.bank_config.link_width_bits << "-bit links, "
              << config_.bank_config.bandwidth_GBs << " GB/s" << std::endl;

    // ========================================
    // Create Bank Group Networks (one per chip)
    // ========================================
    std::cout << "\nCreating Bank Group Networks (one per chip)..." << std::endl;
    bg_networks_.resize(total_chips);
    bg_stats_.resize(total_chips);

    for (int chip = 0; chip < total_chips; chip++) {
        bg_networks_[chip] = createNetworkInstance(
            NetworkLevel::BANK_GROUP_NETWORK,
            config_.num_bg_per_chip,
            config_.bg_config);
    }
    std::cout << "  Created " << total_chips << " bank group network instances" << std::endl;
    std::cout << "  Each with " << config_.num_bg_per_chip << " nodes, "
              << config_.bg_config.link_width_bits << "-bit links, "
              << config_.bg_config.bandwidth_GBs << " GB/s" << std::endl;

    // ========================================
    // Create Chip Networks (one per rank)
    // ========================================
    std::cout << "\nCreating Chip Networks (one per rank)..." << std::endl;
    chip_networks_.resize(total_ranks);
    chip_stats_.resize(total_ranks);

    for (int rank = 0; rank < total_ranks; rank++) {
        chip_networks_[rank] = createNetworkInstance(
            NetworkLevel::CHIP_NETWORK,
            config_.num_chips_per_rank,
            config_.chip_config);
    }
    std::cout << "  Created " << total_ranks << " chip network instances" << std::endl;
    std::cout << "  Each with " << config_.num_chips_per_rank << " nodes, "
              << config_.chip_config.link_width_bits << "-bit links, "
              << config_.chip_config.bandwidth_GBs << " GB/s" << std::endl;

    // ========================================
    // Create Rank Networks (one per channel)
    // ========================================
    if (config_.num_ranks_per_channel > 1) {
        std::cout << "\nCreating Rank Networks (one per channel)..." << std::endl;
        rank_networks_.resize(config_.num_channels);
        rank_stats_.resize(config_.num_channels);

        for (int ch = 0; ch < config_.num_channels; ch++) {
            rank_networks_[ch] = createNetworkInstance(
                NetworkLevel::CHIP_NETWORK,  // Reuse chip network level for ranks
                config_.num_ranks_per_channel,
                config_.rank_config);
        }
        std::cout << "  Created " << config_.num_channels << " rank network instances" << std::endl;
        std::cout << "  Each with " << config_.num_ranks_per_channel << " nodes, "
                  << config_.rank_config.link_width_bits << "-bit links, "
                  << config_.rank_config.bandwidth_GBs << " GB/s" << std::endl;
    }

    // Summary
    int total_instances = total_banks + total_bgs + total_chips + total_ranks;
    if (config_.num_ranks_per_channel > 1) {
        total_instances += config_.num_channels;
    }

    std::cout << "\n════════════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "Total GARNET Network Instances: " << total_instances << std::endl;
    std::cout << "  - Subarray networks: " << total_banks << " (one per bank)" << std::endl;
    std::cout << "  - Bank networks:     " << total_bgs << " (one per bank group)" << std::endl;
    std::cout << "  - BG networks:       " << total_chips << " (one per chip)" << std::endl;
    std::cout << "  - Chip networks:     " << total_ranks << " (one per rank)" << std::endl;
    if (config_.num_ranks_per_channel > 1) {
        std::cout << "  - Rank networks:     " << config_.num_channels << " (one per channel)" << std::endl;
    }
    std::cout << "════════════════════════════════════════════════════════════════════\n" << std::endl;

    initialized_ = true;
}

std::shared_ptr<NetworkModel> MultiInstanceGarnetNetwork::createNetworkInstance(
    NetworkLevel level,
    int num_nodes,
    const MultiInstanceNetworkConfig::LevelConfig& level_config) {

    // Create network configuration
    NetworkConfig config;

    // Set topology based on level config
    switch (level_config.topology) {
        case TopologyType::H_TREE:
            config.topology = NetworkTopology::H_TREE;
            config.routing = RoutingAlgorithm::TREE_BASED;
            break;
        case TopologyType::BUS:
        case TopologyType::CROSSBAR:
            config.topology = NetworkTopology::CROSSBAR;
            config.routing = RoutingAlgorithm::XY;
            break;
        case TopologyType::MESH_2D:
            config.topology = NetworkTopology::MESH_2D;
            config.routing = RoutingAlgorithm::XY;
            break;
        default:
            config.topology = NetworkTopology::H_TREE;
            config.routing = RoutingAlgorithm::TREE_BASED;
    }

    config.flow_control = FlowControl::CREDIT_BASED;
    config.num_rows = num_nodes;
    config.num_cols = 1;
    config.num_layers = 1;

    config.virtual_networks = 2;  // Read and Write
    config.virtual_channels_per_vn = level_config.virtual_channels / 2;
    config.virtual_channels = level_config.virtual_channels;

    config.link_width_bytes = level_config.link_width_bits / 8;
    config.link_latency = level_config.link_latency_cycles;

    config.router_pipeline = RouterPipelineComplexity::MINIMAL;
    config.router_latency = 1;
    config.enable_router_bypass = true;

    config.input_buffer_depth = level_config.buffer_depth;
    config.output_buffer_depth = level_config.buffer_depth;

    // Create and initialize GARNET model
    auto network = std::make_shared<GarnetModel>(config);
    network->initialize();

    // Add nodes
    for (int i = 0; i < num_nodes; i++) {
        NetworkNode node(i, PEPlacementLevel::SUBARRAY, i);
        network->addNode(node);
    }

    return network;
}

bool MultiInstanceGarnetNetwork::sendPacket(const InternalNetworkPacket& packet) {
    if (!initialized_) {
        std::cerr << "[MultiInstanceGarnet] Error: Network not initialized" << std::endl;
        return false;
    }

    // Determine which network level and instance to use
    NetworkLevel level = determineNetworkLevel(
        packet.source_bank, packet.dest_bank,
        packet.source_subarray, packet.dest_subarray);

    int instance_id = getInstanceId(level, packet.source_bank);

    // Get the appropriate network instance
    std::shared_ptr<NetworkModel> network = nullptr;
    int src_node = 0, dst_node = 0;

    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            if (instance_id < static_cast<int>(subarray_networks_.size())) {
                network = subarray_networks_[instance_id];
                src_node = packet.source_subarray;
                dst_node = packet.dest_subarray;
            }
            break;

        case NetworkLevel::BANK_NETWORK:
            if (instance_id < static_cast<int>(bank_networks_.size())) {
                network = bank_networks_[instance_id];
                src_node = packet.source_bank % config_.num_banks_per_bg;
                dst_node = packet.dest_bank % config_.num_banks_per_bg;
            }
            break;

        case NetworkLevel::BANK_GROUP_NETWORK:
            if (instance_id < static_cast<int>(bg_networks_.size())) {
                network = bg_networks_[instance_id];
                src_node = bankToBankGroup(packet.source_bank) % config_.num_bg_per_chip;
                dst_node = bankToBankGroup(packet.dest_bank) % config_.num_bg_per_chip;
            }
            break;

        case NetworkLevel::CHIP_NETWORK:
            if (instance_id < static_cast<int>(chip_networks_.size())) {
                network = chip_networks_[instance_id];
                src_node = bankToChip(packet.source_bank) % config_.num_chips_per_rank;
                dst_node = bankToChip(packet.dest_bank) % config_.num_chips_per_rank;
            }
            break;
    }

    if (!network) {
        std::cerr << "[MultiInstanceGarnet] Error: Invalid network instance" << std::endl;
        return false;
    }

    // Check if network can accept packet
    if (!network->canInject(src_node)) {
        return false;  // Network congested
    }

    // Create and inject packet
    NetworkPacket net_packet(
        src_node,
        dst_node,
        PacketType::DATA,
        packet.data_bytes,
        packet.packet_id,
        current_cycle_);

    network->injectPacket(net_packet);

    // Calculate expected latency
    uint64_t latency = getTransferLatency(
        packet.source_bank, packet.dest_bank,
        packet.source_subarray, packet.dest_subarray,
        packet.data_bytes);

    // Track in-flight packet
    InflightPacket inflight;
    inflight.packet = packet;
    inflight.level = level;
    inflight.instance_id = instance_id;
    inflight.expected_completion = current_cycle_ + latency;
    inflight_packets_.push_back(inflight);

    // Update statistics
    updateStats(level, instance_id, packet.data_bytes, latency);

    return true;
}

void MultiInstanceGarnetNetwork::tick() {
    current_cycle_++;

    // Tick all network instances
    for (auto& net : subarray_networks_) {
        if (net) net->tick();
    }
    for (auto& net : bank_networks_) {
        if (net) net->tick();
    }
    for (auto& net : bg_networks_) {
        if (net) net->tick();
    }
    for (auto& net : chip_networks_) {
        if (net) net->tick();
    }
    for (auto& net : rank_networks_) {
        if (net) net->tick();
    }

    // Process completed packets
    auto it = inflight_packets_.begin();
    while (it != inflight_packets_.end()) {
        if (current_cycle_ >= it->expected_completion) {
            // Packet completed - invoke callback if set
            if (it->packet.callback) {
                it->packet.callback();
            }
            it = inflight_packets_.erase(it);
        } else {
            ++it;
        }
    }
}

uint64_t MultiInstanceGarnetNetwork::getTransferLatency(
    int src_bank, int dst_bank,
    int src_subarray, int dst_subarray,
    uint64_t data_bytes) {

    NetworkLevel level = determineNetworkLevel(src_bank, dst_bank, src_subarray, dst_subarray);

    // Base latency based on level
    uint64_t base_latency = 0;
    double bandwidth_GBs = 0;

    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            base_latency = config_.subarray_config.link_latency_cycles;
            bandwidth_GBs = config_.subarray_config.bandwidth_GBs;
            break;
        case NetworkLevel::BANK_NETWORK:
            base_latency = config_.bank_config.link_latency_cycles;
            bandwidth_GBs = config_.bank_config.bandwidth_GBs;
            break;
        case NetworkLevel::BANK_GROUP_NETWORK:
            base_latency = config_.bg_config.link_latency_cycles;
            bandwidth_GBs = config_.bg_config.bandwidth_GBs;
            break;
        case NetworkLevel::CHIP_NETWORK:
            base_latency = config_.chip_config.link_latency_cycles;
            bandwidth_GBs = config_.chip_config.bandwidth_GBs;
            break;
    }

    // Transfer time based on bandwidth (assuming 1 GHz clock)
    uint64_t transfer_cycles = static_cast<uint64_t>(
        std::ceil(data_bytes / (bandwidth_GBs * 1e9 / 1e9)));  // bytes / (GB/s * ns/cycle)

    return base_latency + transfer_cycles;
}

bool MultiInstanceGarnetNetwork::canAcceptPacket(int src_bank, int dst_bank) {
    NetworkLevel level = determineNetworkLevel(src_bank, dst_bank, 0, 0);
    int instance_id = getInstanceId(level, src_bank);

    std::shared_ptr<NetworkModel> network = nullptr;
    int src_node = 0;

    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            if (instance_id < static_cast<int>(subarray_networks_.size())) {
                network = subarray_networks_[instance_id];
                src_node = 0;
            }
            break;
        case NetworkLevel::BANK_NETWORK:
            if (instance_id < static_cast<int>(bank_networks_.size())) {
                network = bank_networks_[instance_id];
                src_node = src_bank % config_.num_banks_per_bg;
            }
            break;
        case NetworkLevel::BANK_GROUP_NETWORK:
            if (instance_id < static_cast<int>(bg_networks_.size())) {
                network = bg_networks_[instance_id];
                src_node = bankToBankGroup(src_bank) % config_.num_bg_per_chip;
            }
            break;
        case NetworkLevel::CHIP_NETWORK:
            if (instance_id < static_cast<int>(chip_networks_.size())) {
                network = chip_networks_[instance_id];
                src_node = bankToChip(src_bank) % config_.num_chips_per_rank;
            }
            break;
    }

    return network ? network->canInject(src_node) : false;
}

// ========================================
// Network Instance Access
// ========================================

std::shared_ptr<NetworkModel> MultiInstanceGarnetNetwork::getSubarrayNetwork(int bank_id) {
    if (bank_id < static_cast<int>(subarray_networks_.size())) {
        return subarray_networks_[bank_id];
    }
    return nullptr;
}

std::shared_ptr<NetworkModel> MultiInstanceGarnetNetwork::getBankNetwork(int bg_id) {
    if (bg_id < static_cast<int>(bank_networks_.size())) {
        return bank_networks_[bg_id];
    }
    return nullptr;
}

std::shared_ptr<NetworkModel> MultiInstanceGarnetNetwork::getBankGroupNetwork(int chip_id) {
    if (chip_id < static_cast<int>(bg_networks_.size())) {
        return bg_networks_[chip_id];
    }
    return nullptr;
}

std::shared_ptr<NetworkModel> MultiInstanceGarnetNetwork::getChipNetwork(int rank_id) {
    if (rank_id < static_cast<int>(chip_networks_.size())) {
        return chip_networks_[rank_id];
    }
    return nullptr;
}

std::shared_ptr<NetworkModel> MultiInstanceGarnetNetwork::getRankNetwork(int channel_id) {
    if (channel_id < static_cast<int>(rank_networks_.size())) {
        return rank_networks_[channel_id];
    }
    return nullptr;
}

// ========================================
// Statistics
// ========================================

void MultiInstanceGarnetNetwork::printStats() const {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║ Multi-Instance GARNET Network Statistics                          ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n" << std::endl;

    std::cout << "Current Cycle: " << current_cycle_ << std::endl;
    std::cout << "In-Flight Packets: " << inflight_packets_.size() << std::endl;
    std::cout << std::endl;

    // Print per-level aggregated stats
    printLevelStats(NetworkLevel::SUBARRAY_NETWORK);
    printLevelStats(NetworkLevel::BANK_NETWORK);
    printLevelStats(NetworkLevel::BANK_GROUP_NETWORK);
    printLevelStats(NetworkLevel::CHIP_NETWORK);
}

void MultiInstanceGarnetNetwork::printLevelStats(NetworkLevel level) const {
    NetworkInstanceStats agg = getAggregatedStats(level);

    std::string level_name;
    int num_instances = 0;

    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            level_name = "Subarray Networks";
            num_instances = subarray_networks_.size();
            break;
        case NetworkLevel::BANK_NETWORK:
            level_name = "Bank Networks";
            num_instances = bank_networks_.size();
            break;
        case NetworkLevel::BANK_GROUP_NETWORK:
            level_name = "Bank Group Networks";
            num_instances = bg_networks_.size();
            break;
        case NetworkLevel::CHIP_NETWORK:
            level_name = "Chip Networks";
            num_instances = chip_networks_.size();
            break;
    }

    std::cout << level_name << " (" << num_instances << " instances):" << std::endl;
    std::cout << "  Packets sent:     " << agg.packets_sent << std::endl;
    std::cout << "  Packets completed:" << agg.packets_completed << std::endl;
    std::cout << "  Bytes transferred:" << agg.bytes_transferred << " B";
    if (agg.bytes_transferred > 1024*1024) {
        std::cout << " (" << std::fixed << std::setprecision(2)
                  << (agg.bytes_transferred / (1024.0*1024.0)) << " MB)";
    }
    std::cout << std::endl;
    std::cout << "  Avg latency:      " << std::fixed << std::setprecision(2)
              << agg.avg_latency << " cycles" << std::endl;
    std::cout << std::endl;
}

NetworkInstanceStats MultiInstanceGarnetNetwork::getInstanceStats(
    NetworkLevel level, int instance_id) const {

    const std::vector<NetworkInstanceStats>* stats = nullptr;

    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            stats = &subarray_stats_;
            break;
        case NetworkLevel::BANK_NETWORK:
            stats = &bank_stats_;
            break;
        case NetworkLevel::BANK_GROUP_NETWORK:
            stats = &bg_stats_;
            break;
        case NetworkLevel::CHIP_NETWORK:
            stats = &chip_stats_;
            break;
    }

    if (stats && instance_id < static_cast<int>(stats->size())) {
        return (*stats)[instance_id];
    }

    return NetworkInstanceStats();
}

NetworkInstanceStats MultiInstanceGarnetNetwork::getAggregatedStats(NetworkLevel level) const {
    NetworkInstanceStats agg;

    const std::vector<NetworkInstanceStats>* stats = nullptr;

    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            stats = &subarray_stats_;
            break;
        case NetworkLevel::BANK_NETWORK:
            stats = &bank_stats_;
            break;
        case NetworkLevel::BANK_GROUP_NETWORK:
            stats = &bg_stats_;
            break;
        case NetworkLevel::CHIP_NETWORK:
            stats = &chip_stats_;
            break;
    }

    if (!stats) return agg;

    for (const auto& s : *stats) {
        agg.packets_sent += s.packets_sent;
        agg.packets_completed += s.packets_completed;
        agg.bytes_transferred += s.bytes_transferred;
        agg.total_latency_cycles += s.total_latency_cycles;
    }

    if (agg.packets_completed > 0) {
        agg.avg_latency = static_cast<double>(agg.total_latency_cycles) / agg.packets_completed;
    }

    return agg;
}

void MultiInstanceGarnetNetwork::resetStats() {
    for (auto& s : subarray_stats_) s = NetworkInstanceStats();
    for (auto& s : bank_stats_) s = NetworkInstanceStats();
    for (auto& s : bg_stats_) s = NetworkInstanceStats();
    for (auto& s : chip_stats_) s = NetworkInstanceStats();
    for (auto& s : rank_stats_) s = NetworkInstanceStats();
}

// ========================================
// Configuration Queries
// ========================================

int MultiInstanceGarnetNetwork::getTotalNetworkInstances() const {
    return subarray_networks_.size() + bank_networks_.size() +
           bg_networks_.size() + chip_networks_.size() + rank_networks_.size();
}

int MultiInstanceGarnetNetwork::getNumInstancesAtLevel(NetworkLevel level) const {
    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            return subarray_networks_.size();
        case NetworkLevel::BANK_NETWORK:
            return bank_networks_.size();
        case NetworkLevel::BANK_GROUP_NETWORK:
            return bg_networks_.size();
        case NetworkLevel::CHIP_NETWORK:
            return chip_networks_.size();
    }
    return 0;
}

// ========================================
// Address Mapping Helpers
// ========================================

int MultiInstanceGarnetNetwork::bankToBankGroup(int bank_id) const {
    return bank_id / config_.num_banks_per_bg;
}

int MultiInstanceGarnetNetwork::bankToChip(int bank_id) const {
    int bg = bankToBankGroup(bank_id);
    return bg / config_.num_bg_per_chip;
}

int MultiInstanceGarnetNetwork::bankToRank(int bank_id) const {
    int chip = bankToChip(bank_id);
    return chip / config_.num_chips_per_rank;
}

int MultiInstanceGarnetNetwork::bankToChannel(int bank_id) const {
    int rank = bankToRank(bank_id);
    return rank / config_.num_ranks_per_channel;
}

bool MultiInstanceGarnetNetwork::inSameBankGroup(int bank1, int bank2) const {
    return bankToBankGroup(bank1) == bankToBankGroup(bank2);
}

bool MultiInstanceGarnetNetwork::inSameChip(int bank1, int bank2) const {
    return bankToChip(bank1) == bankToChip(bank2);
}

bool MultiInstanceGarnetNetwork::inSameRank(int bank1, int bank2) const {
    return bankToRank(bank1) == bankToRank(bank2);
}

bool MultiInstanceGarnetNetwork::inSameChannel(int bank1, int bank2) const {
    return bankToChannel(bank1) == bankToChannel(bank2);
}

// ========================================
// Private Helper Functions
// ========================================

NetworkLevel MultiInstanceGarnetNetwork::determineNetworkLevel(
    int src_bank, int dst_bank,
    int src_subarray, int dst_subarray) {

    // Same bank - use subarray network
    if (src_bank == dst_bank) {
        return NetworkLevel::SUBARRAY_NETWORK;
    }

    // Same bank group - use bank network
    if (inSameBankGroup(src_bank, dst_bank)) {
        return NetworkLevel::BANK_NETWORK;
    }

    // Same chip - use bank group network
    if (inSameChip(src_bank, dst_bank)) {
        return NetworkLevel::BANK_GROUP_NETWORK;
    }

    // Different chips - use chip network
    return NetworkLevel::CHIP_NETWORK;
}

int MultiInstanceGarnetNetwork::getInstanceId(NetworkLevel level, int bank_id) {
    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            return bank_id;  // One network per bank
        case NetworkLevel::BANK_NETWORK:
            return bankToBankGroup(bank_id);  // One network per bank group
        case NetworkLevel::BANK_GROUP_NETWORK:
            return bankToChip(bank_id);  // One network per chip
        case NetworkLevel::CHIP_NETWORK:
            return bankToRank(bank_id);  // One network per rank
    }
    return 0;
}

void MultiInstanceGarnetNetwork::updateStats(
    NetworkLevel level, int instance_id,
    uint64_t bytes, uint64_t latency) {

    std::vector<NetworkInstanceStats>* stats = nullptr;

    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            stats = &subarray_stats_;
            break;
        case NetworkLevel::BANK_NETWORK:
            stats = &bank_stats_;
            break;
        case NetworkLevel::BANK_GROUP_NETWORK:
            stats = &bg_stats_;
            break;
        case NetworkLevel::CHIP_NETWORK:
            stats = &chip_stats_;
            break;
    }

    if (stats && instance_id < static_cast<int>(stats->size())) {
        (*stats)[instance_id].packets_sent++;
        (*stats)[instance_id].packets_completed++;
        (*stats)[instance_id].bytes_transferred += bytes;
        (*stats)[instance_id].total_latency_cycles += latency;

        if ((*stats)[instance_id].packets_completed > 0) {
            (*stats)[instance_id].avg_latency =
                static_cast<double>((*stats)[instance_id].total_latency_cycles) /
                (*stats)[instance_id].packets_completed;
        }
    }
}

// ============================================================================
// Factory Functions
// ============================================================================

std::shared_ptr<MultiInstanceGarnetNetwork> createDDR4MultiInstanceNetwork(
    int num_channels, int ranks_per_channel) {

    MultiInstanceNetworkConfig config;
    config.num_channels = num_channels;
    config.num_ranks_per_channel = ranks_per_channel;
    config.num_chips_per_rank = 8;
    config.num_bg_per_chip = 4;
    config.num_banks_per_bg = 4;
    config.num_subarrays_per_bank = 64;

    // DDR4 network parameters
    config.subarray_config = {256, 3, 19.2, TopologyType::H_TREE, 2, 4};
    config.bank_config = {8, 6, 1.2, TopologyType::H_TREE, 2, 4};
    config.bg_config = {16, 10, 2.4, TopologyType::H_TREE, 2, 4};
    config.chip_config = {8, 15, 1.2, TopologyType::BUS, 2, 4};
    config.rank_config = {64, 20, 19.2, TopologyType::BUS, 4, 8};

    auto network = std::make_shared<MultiInstanceGarnetNetwork>(config);
    network->initialize();
    return network;
}

std::shared_ptr<MultiInstanceGarnetNetwork> createDDR5MultiInstanceNetwork(
    int num_channels, int ranks_per_channel) {

    MultiInstanceNetworkConfig config;
    config.num_channels = num_channels;
    config.num_ranks_per_channel = ranks_per_channel;
    config.num_chips_per_rank = 8;
    config.num_bg_per_chip = 8;
    config.num_banks_per_bg = 4;
    config.num_subarrays_per_bank = 64;

    // DDR5 network parameters (2x bandwidth of DDR4)
    config.subarray_config = {512, 2, 38.4, TopologyType::H_TREE, 4, 8};
    config.bank_config = {16, 4, 4.8, TopologyType::H_TREE, 4, 8};
    config.bg_config = {32, 8, 9.6, TopologyType::H_TREE, 4, 8};
    config.chip_config = {16, 12, 4.8, TopologyType::BUS, 4, 8};
    config.rank_config = {64, 15, 38.4, TopologyType::BUS, 4, 8};

    auto network = std::make_shared<MultiInstanceGarnetNetwork>(config);
    network->initialize();
    return network;
}

std::shared_ptr<MultiInstanceGarnetNetwork> createHBM2MultiInstanceNetwork(int num_channels) {
    MultiInstanceNetworkConfig config;
    config.num_channels = num_channels;
    config.num_ranks_per_channel = 1;
    config.num_chips_per_rank = 1;
    config.num_bg_per_chip = 4;
    config.num_banks_per_bg = 4;
    config.num_subarrays_per_bank = 128;

    // HBM2 network parameters (high bandwidth, low latency)
    config.subarray_config = {512, 2, 32.0, TopologyType::H_TREE, 4, 8};
    config.bank_config = {128, 3, 32.0, TopologyType::H_TREE, 4, 8};
    config.bg_config = {256, 5, 64.0, TopologyType::H_TREE, 4, 8};
    config.chip_config = {128, 8, 32.0, TopologyType::CROSSBAR, 4, 8};
    config.rank_config = {1024, 10, 256.0, TopologyType::CROSSBAR, 8, 16};

    auto network = std::make_shared<MultiInstanceGarnetNetwork>(config);
    network->initialize();
    return network;
}

std::shared_ptr<MultiInstanceGarnetNetwork> createHBM3MultiInstanceNetwork(int num_channels) {
    MultiInstanceNetworkConfig config;
    config.num_channels = num_channels;
    config.num_ranks_per_channel = 1;
    config.num_chips_per_rank = 1;
    config.num_bg_per_chip = 4;
    config.num_banks_per_bg = 8;
    config.num_subarrays_per_bank = 128;

    // HBM3 network parameters (2x bandwidth of HBM2)
    config.subarray_config = {1024, 2, 64.0, TopologyType::H_TREE, 8, 16};
    config.bank_config = {256, 2, 64.0, TopologyType::H_TREE, 8, 16};
    config.bg_config = {512, 4, 128.0, TopologyType::H_TREE, 8, 16};
    config.chip_config = {256, 6, 64.0, TopologyType::CROSSBAR, 8, 16};
    config.rank_config = {2048, 8, 512.0, TopologyType::CROSSBAR, 16, 32};

    auto network = std::make_shared<MultiInstanceGarnetNetwork>(config);
    network->initialize();
    return network;
}

} // namespace pimid
