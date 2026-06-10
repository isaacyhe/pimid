/**
 * @file multi_instance_garnet_network.cpp
 * @brief Implementation of Multi-Instantiated GARNET Network for DRAM Hierarchies
 */

#include "memory/multi_instance_garnet_network.h"
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

std::string MultiInstanceGarnetNetwork::getLevelName(int level_idx) const {
    if (level_idx < 0 || level_idx >= NUM_HIERARCHY_LEVELS) return "Unknown";

    if (isHBM(config_.technology)) {
        switch (level_idx) {
            case 0: return "Subarray";
            case 1: return "Bank";
            case 2: return "BankGroup";
            case 3: return "DieLayer";
            case 4: return "LogicDie";
            case 5: return "Channel";
            case 6: return "System";
            default: return "Unknown";
        }
    } else {
        switch (level_idx) {
            case 0: return "Subarray";
            case 1: return "Bank";
            case 2: return "BankGroup";
            case 3: return "Chip";
            case 4: return "Rank";
            case 5: return "Channel";
            case 6: return "System";
            default: return "Unknown";
        }
    }
}

int MultiInstanceGarnetNetwork::getNodesPerInstance(int level_idx) const {
    switch (level_idx) {
        case 0: return config_.num_subarrays_per_bank;
        case 1: return config_.num_banks_per_bg;
        case 2: return config_.num_bg_per_chip;
        case 3: return config_.num_chips_per_rank;
        case 4: return config_.num_ranks_per_channel;
        case 5: return config_.num_channels;
        case 6: return 1;
        default: return 1;
    }
}

void MultiInstanceGarnetNetwork::initialize() {
    if (initialized_) {
        std::cout << "[MultiInstanceGarnet] Already initialized" << std::endl;
        return;
    }

    std::cout << "\n"
              << "================================================================\n"
              << " Multi-Instance GARNET Network Initialization\n"
              << "================================================================\n"
              << std::endl;

    // Calculate total counts at each level
    int total_ranks = config_.num_channels * config_.num_ranks_per_channel;
    int total_chips = total_ranks * config_.num_chips_per_rank;
    int total_bgs = total_chips * config_.num_bg_per_chip;
    int total_banks = total_bgs * config_.num_banks_per_bg;

    std::cout << "DRAM Hierarchy:" << std::endl;
    std::cout << "  Technology:         " << (isHBM(config_.technology) ? "HBM" : "DDR") << std::endl;
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

    // Number of network instances at each level:
    //   L0 (subarray):   total_banks  (one per bank)
    //   L1 (bank):       total_bgs    (one per bank group)
    //   L2 (bank_group): total_chips  (one per chip)
    //   L3 (chip):       total_ranks  (one per rank)
    //   L4 (rank):       num_channels (one per channel)
    //   L5 (channel):    1            (system-wide)
    //   L6 (system):     1            (root)
    int num_instances[NUM_HIERARCHY_LEVELS] = {
        total_banks,
        total_bgs,
        total_chips,
        total_ranks,
        config_.num_channels,
        1,
        1
    };

    int total_network_instances = 0;

    for (int i = 0; i < NUM_HIERARCHY_LEVELS; ++i) {
        int count = num_instances[i];
        int nodes = getNodesPerInstance(i);
        const auto& lcfg = config_.level_configs[i];
        NetworkLevel level = static_cast<NetworkLevel>(i);

        std::cout << "Creating L" << i << " " << getLevelName(i)
                  << " Networks (" << count << " instances)..." << std::endl;

        networks_[i].resize(count);
        stats_[i].resize(count);

        for (int inst = 0; inst < count; ++inst) {
            networks_[i][inst] = createNetworkInstance(level, nodes, lcfg);
        }

        std::cout << "  Each with " << nodes << " nodes, "
                  << lcfg.link_width_bits << "-bit links, "
                  << lcfg.bandwidth_GBs << " GB/s" << std::endl;

        total_network_instances += count;
    }

    // Summary
    std::cout << "\n================================================================" << std::endl;
    std::cout << "Total GARNET Network Instances: " << total_network_instances << std::endl;
    for (int i = 0; i < NUM_HIERARCHY_LEVELS; ++i) {
        std::cout << "  - L" << i << " " << getLevelName(i) << " networks: "
                  << num_instances[i] << std::endl;
    }
    std::cout << "================================================================\n" << std::endl;

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
    int idx = static_cast<int>(level);

    // Validate instance
    if (idx < 0 || idx >= NUM_HIERARCHY_LEVELS ||
        instance_id < 0 || instance_id >= static_cast<int>(networks_[idx].size())) {
        std::cerr << "[MultiInstanceGarnet] Error: Invalid network instance (level="
                  << idx << ", instance=" << instance_id << ")" << std::endl;
        return false;
    }

    auto& network = networks_[idx][instance_id];
    if (!network) {
        std::cerr << "[MultiInstanceGarnet] Error: Null network instance" << std::endl;
        return false;
    }

    // Compute source and destination node IDs within the network instance
    int src_node = 0, dst_node = 0;
    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            src_node = packet.source_subarray;
            dst_node = packet.dest_subarray;
            break;
        case NetworkLevel::BANK_NETWORK:
            src_node = packet.source_bank % config_.num_banks_per_bg;
            dst_node = packet.dest_bank % config_.num_banks_per_bg;
            break;
        case NetworkLevel::BANK_GROUP_NETWORK:
            src_node = bankToBankGroup(packet.source_bank) % config_.num_bg_per_chip;
            dst_node = bankToBankGroup(packet.dest_bank) % config_.num_bg_per_chip;
            break;
        case NetworkLevel::CHIP_NETWORK:
            src_node = bankToChip(packet.source_bank) % config_.num_chips_per_rank;
            dst_node = bankToChip(packet.dest_bank) % config_.num_chips_per_rank;
            break;
        case NetworkLevel::RANK_NETWORK:
            src_node = bankToRank(packet.source_bank) % config_.num_ranks_per_channel;
            dst_node = bankToRank(packet.dest_bank) % config_.num_ranks_per_channel;
            break;
        case NetworkLevel::CHANNEL_NETWORK:
            src_node = bankToChannel(packet.source_bank) % config_.num_channels;
            dst_node = bankToChannel(packet.dest_bank) % config_.num_channels;
            break;
        case NetworkLevel::SYSTEM_NETWORK:
            src_node = 0;
            dst_node = 0;
            break;
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

    // Tick all network instances across all 7 levels
    for (int i = 0; i < NUM_HIERARCHY_LEVELS; ++i) {
        for (auto& net : networks_[i]) {
            if (net) net->tick();
        }
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
    int idx = static_cast<int>(level);

    uint64_t base_latency = config_.level_configs[idx].link_latency_cycles;
    double bandwidth_GBs = config_.level_configs[idx].bandwidth_GBs;

    // Transfer time based on bandwidth (assuming 1 GHz clock)
    uint64_t transfer_cycles = static_cast<uint64_t>(
        std::ceil(data_bytes / (bandwidth_GBs * 1e9 / 1e9)));  // bytes / (GB/s * ns/cycle)

    return base_latency + transfer_cycles;
}

bool MultiInstanceGarnetNetwork::canAcceptPacket(int src_bank, int dst_bank) {
    NetworkLevel level = determineNetworkLevel(src_bank, dst_bank, 0, 0);
    int instance_id = getInstanceId(level, src_bank);
    int idx = static_cast<int>(level);

    if (idx < 0 || idx >= NUM_HIERARCHY_LEVELS ||
        instance_id < 0 || instance_id >= static_cast<int>(networks_[idx].size())) {
        return false;
    }

    auto& network = networks_[idx][instance_id];
    if (!network) return false;

    int src_node = 0;
    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            src_node = 0;
            break;
        case NetworkLevel::BANK_NETWORK:
            src_node = src_bank % config_.num_banks_per_bg;
            break;
        case NetworkLevel::BANK_GROUP_NETWORK:
            src_node = bankToBankGroup(src_bank) % config_.num_bg_per_chip;
            break;
        case NetworkLevel::CHIP_NETWORK:
            src_node = bankToChip(src_bank) % config_.num_chips_per_rank;
            break;
        case NetworkLevel::RANK_NETWORK:
            src_node = bankToRank(src_bank) % config_.num_ranks_per_channel;
            break;
        case NetworkLevel::CHANNEL_NETWORK:
            src_node = bankToChannel(src_bank) % config_.num_channels;
            break;
        case NetworkLevel::SYSTEM_NETWORK:
            src_node = 0;
            break;
    }

    return network->canInject(src_node);
}

// ========================================
// Network Instance Access
// ========================================

std::shared_ptr<NetworkModel> MultiInstanceGarnetNetwork::getNetworkAtLevel(
    NetworkLevel level, int instance_id) {
    int idx = static_cast<int>(level);
    if (idx >= 0 && idx < NUM_HIERARCHY_LEVELS &&
        instance_id >= 0 && instance_id < static_cast<int>(networks_[idx].size())) {
        return networks_[idx][instance_id];
    }
    return nullptr;
}

std::shared_ptr<NetworkModel> MultiInstanceGarnetNetwork::getSubarrayNetwork(int bank_id) {
    return getNetworkAtLevel(NetworkLevel::SUBARRAY_NETWORK, bank_id);
}

std::shared_ptr<NetworkModel> MultiInstanceGarnetNetwork::getBankNetwork(int bg_id) {
    return getNetworkAtLevel(NetworkLevel::BANK_NETWORK, bg_id);
}

std::shared_ptr<NetworkModel> MultiInstanceGarnetNetwork::getBankGroupNetwork(int chip_id) {
    return getNetworkAtLevel(NetworkLevel::BANK_GROUP_NETWORK, chip_id);
}

std::shared_ptr<NetworkModel> MultiInstanceGarnetNetwork::getChipNetwork(int rank_id) {
    return getNetworkAtLevel(NetworkLevel::CHIP_NETWORK, rank_id);
}

std::shared_ptr<NetworkModel> MultiInstanceGarnetNetwork::getRankNetwork(int channel_id) {
    return getNetworkAtLevel(NetworkLevel::RANK_NETWORK, channel_id);
}

std::shared_ptr<NetworkModel> MultiInstanceGarnetNetwork::getChannelNetwork(int instance_id) {
    return getNetworkAtLevel(NetworkLevel::CHANNEL_NETWORK, instance_id);
}

std::shared_ptr<NetworkModel> MultiInstanceGarnetNetwork::getSystemNetwork(int instance_id) {
    return getNetworkAtLevel(NetworkLevel::SYSTEM_NETWORK, instance_id);
}

// ========================================
// Statistics
// ========================================

void MultiInstanceGarnetNetwork::printStats() const {
    std::cout << "\n"
              << "================================================================\n"
              << " Multi-Instance GARNET Network Statistics\n"
              << "================================================================\n"
              << std::endl;

    std::cout << "Current Cycle: " << current_cycle_ << std::endl;
    std::cout << "In-Flight Packets: " << inflight_packets_.size() << std::endl;
    std::cout << std::endl;

    for (int i = 0; i < NUM_HIERARCHY_LEVELS; ++i) {
        printLevelStats(static_cast<NetworkLevel>(i));
    }
}

void MultiInstanceGarnetNetwork::printLevelStats(NetworkLevel level) const {
    int idx = static_cast<int>(level);
    if (idx < 0 || idx >= NUM_HIERARCHY_LEVELS) return;

    NetworkInstanceStats agg = getAggregatedStats(level);
    int num_instances = static_cast<int>(networks_[idx].size());
    std::string level_name = getLevelName(idx) + " Networks";

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

    int idx = static_cast<int>(level);
    if (idx >= 0 && idx < NUM_HIERARCHY_LEVELS &&
        instance_id >= 0 && instance_id < static_cast<int>(stats_[idx].size())) {
        return stats_[idx][instance_id];
    }
    return NetworkInstanceStats();
}

NetworkInstanceStats MultiInstanceGarnetNetwork::getAggregatedStats(NetworkLevel level) const {
    NetworkInstanceStats agg;
    int idx = static_cast<int>(level);
    if (idx < 0 || idx >= NUM_HIERARCHY_LEVELS) return agg;

    for (const auto& s : stats_[idx]) {
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
    for (int i = 0; i < NUM_HIERARCHY_LEVELS; ++i) {
        for (auto& s : stats_[i]) {
            s = NetworkInstanceStats();
        }
    }
}

// ========================================
// Configuration Queries
// ========================================

int MultiInstanceGarnetNetwork::getTotalNetworkInstances() const {
    int total = 0;
    for (int i = 0; i < NUM_HIERARCHY_LEVELS; ++i) {
        total += static_cast<int>(networks_[i].size());
    }
    return total;
}

int MultiInstanceGarnetNetwork::getNumInstancesAtLevel(NetworkLevel level) const {
    int idx = static_cast<int>(level);
    if (idx >= 0 && idx < NUM_HIERARCHY_LEVELS) {
        return static_cast<int>(networks_[idx].size());
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

    // Same rank - use chip network
    if (inSameRank(src_bank, dst_bank)) {
        return NetworkLevel::CHIP_NETWORK;
    }

    // Same channel - use rank network
    if (inSameChannel(src_bank, dst_bank)) {
        return NetworkLevel::RANK_NETWORK;
    }

    // Different channels - use channel network
    return NetworkLevel::CHANNEL_NETWORK;
}

int MultiInstanceGarnetNetwork::getInstanceId(NetworkLevel level, int bank_id) {
    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            return bank_id;
        case NetworkLevel::BANK_NETWORK:
            return bankToBankGroup(bank_id);
        case NetworkLevel::BANK_GROUP_NETWORK:
            return bankToChip(bank_id);
        case NetworkLevel::CHIP_NETWORK:
            return bankToRank(bank_id);
        case NetworkLevel::RANK_NETWORK:
            return bankToChannel(bank_id);
        case NetworkLevel::CHANNEL_NETWORK:
            return 0;
        case NetworkLevel::SYSTEM_NETWORK:
            return 0;
    }
    return 0;
}

void MultiInstanceGarnetNetwork::updateStats(
    NetworkLevel level, int instance_id,
    uint64_t bytes, uint64_t latency) {

    int idx = static_cast<int>(level);
    if (idx < 0 || idx >= NUM_HIERARCHY_LEVELS) return;
    if (instance_id < 0 || instance_id >= static_cast<int>(stats_[idx].size())) return;

    auto& s = stats_[idx][instance_id];
    s.packets_sent++;
    s.packets_completed++;
    s.bytes_transferred += bytes;
    s.total_latency_cycles += latency;

    if (s.packets_completed > 0) {
        s.avg_latency = static_cast<double>(s.total_latency_cycles) / s.packets_completed;
    }
}

// ============================================================================
// Factory Functions
// ============================================================================

std::shared_ptr<MultiInstanceGarnetNetwork> createDDR4MultiInstanceNetwork(
    int num_channels, int ranks_per_channel) {

    MultiInstanceNetworkConfig config;
    config.technology = MemoryTechnology::DDR4;
    config.num_channels = num_channels;
    config.num_ranks_per_channel = ranks_per_channel;
    config.num_chips_per_rank = 8;
    config.num_bg_per_chip = 4;
    config.num_banks_per_bg = 4;
    config.num_subarrays_per_bank = 64;

    // DDR4 network parameters per level
    config.level_configs[0] = {256, 3, 19.2, TopologyType::H_TREE, 2, 4};   // L0: Subarray
    config.level_configs[1] = {8, 6, 1.2, TopologyType::H_TREE, 2, 4};      // L1: Bank
    config.level_configs[2] = {16, 10, 2.4, TopologyType::H_TREE, 2, 4};    // L2: BankGroup
    config.level_configs[3] = {8, 15, 1.2, TopologyType::BUS, 2, 4};        // L3: Chip
    config.level_configs[4] = {64, 20, 19.2, TopologyType::BUS, 4, 8};      // L4: Rank
    config.level_configs[5] = {64, 25, 19.2, TopologyType::BUS, 4, 8};      // L5: Channel
    config.level_configs[6] = {64, 30, 19.2, TopologyType::BUS, 4, 8};      // L6: System

    auto network = std::make_shared<MultiInstanceGarnetNetwork>(config);
    network->initialize();
    return network;
}

std::shared_ptr<MultiInstanceGarnetNetwork> createDDR5MultiInstanceNetwork(
    int num_channels, int ranks_per_channel) {

    MultiInstanceNetworkConfig config;
    config.technology = MemoryTechnology::DDR5;
    config.num_channels = num_channels;
    config.num_ranks_per_channel = ranks_per_channel;
    config.num_chips_per_rank = 8;
    config.num_bg_per_chip = 8;
    config.num_banks_per_bg = 4;
    config.num_subarrays_per_bank = 64;

    // DDR5 network parameters (2x bandwidth of DDR4)
    config.level_configs[0] = {512, 2, 38.4, TopologyType::H_TREE, 4, 8};   // L0: Subarray
    config.level_configs[1] = {16, 4, 4.8, TopologyType::H_TREE, 4, 8};     // L1: Bank
    config.level_configs[2] = {32, 8, 9.6, TopologyType::H_TREE, 4, 8};     // L2: BankGroup
    config.level_configs[3] = {16, 12, 4.8, TopologyType::BUS, 4, 8};       // L3: Chip
    config.level_configs[4] = {64, 15, 38.4, TopologyType::BUS, 4, 8};      // L4: Rank
    config.level_configs[5] = {64, 20, 38.4, TopologyType::BUS, 4, 8};      // L5: Channel
    config.level_configs[6] = {64, 25, 38.4, TopologyType::BUS, 4, 8};      // L6: System

    auto network = std::make_shared<MultiInstanceGarnetNetwork>(config);
    network->initialize();
    return network;
}

std::shared_ptr<MultiInstanceGarnetNetwork> createHBM2MultiInstanceNetwork(int num_channels) {
    MultiInstanceNetworkConfig config;
    config.technology = MemoryTechnology::HBM2;
    config.num_channels = num_channels;
    config.num_ranks_per_channel = 1;
    config.num_chips_per_rank = 1;
    config.num_bg_per_chip = 4;
    config.num_banks_per_bg = 4;
    config.num_subarrays_per_bank = 128;

    // HBM2 network parameters (high bandwidth, low latency via TSVs)
    config.level_configs[0] = {512, 2, 32.0, TopologyType::H_TREE, 4, 8};       // L0: Subarray
    config.level_configs[1] = {128, 3, 32.0, TopologyType::H_TREE, 4, 8};       // L1: Bank
    config.level_configs[2] = {256, 5, 64.0, TopologyType::H_TREE, 4, 8};       // L2: BankGroup
    config.level_configs[3] = {128, 8, 32.0, TopologyType::CROSSBAR, 4, 8};     // L3: DieLayer (TSVs)
    config.level_configs[4] = {1024, 10, 256.0, TopologyType::CROSSBAR, 8, 16}; // L4: LogicDie
    config.level_configs[5] = {1024, 12, 256.0, TopologyType::CROSSBAR, 8, 16}; // L5: Channel
    config.level_configs[6] = {1024, 15, 256.0, TopologyType::CROSSBAR, 8, 16}; // L6: System

    auto network = std::make_shared<MultiInstanceGarnetNetwork>(config);
    network->initialize();
    return network;
}

std::shared_ptr<MultiInstanceGarnetNetwork> createHBM3MultiInstanceNetwork(int num_channels) {
    MultiInstanceNetworkConfig config;
    config.technology = MemoryTechnology::HBM3;
    config.num_channels = num_channels;
    config.num_ranks_per_channel = 1;
    config.num_chips_per_rank = 1;
    config.num_bg_per_chip = 4;
    config.num_banks_per_bg = 8;
    config.num_subarrays_per_bank = 128;

    // HBM3 network parameters (2x bandwidth of HBM2)
    config.level_configs[0] = {1024, 2, 64.0, TopologyType::H_TREE, 8, 16};      // L0: Subarray
    config.level_configs[1] = {256, 2, 64.0, TopologyType::H_TREE, 8, 16};       // L1: Bank
    config.level_configs[2] = {512, 4, 128.0, TopologyType::H_TREE, 8, 16};      // L2: BankGroup
    config.level_configs[3] = {256, 6, 64.0, TopologyType::CROSSBAR, 8, 16};     // L3: DieLayer (TSVs)
    config.level_configs[4] = {2048, 8, 512.0, TopologyType::CROSSBAR, 16, 32};  // L4: LogicDie
    config.level_configs[5] = {2048, 10, 512.0, TopologyType::CROSSBAR, 16, 32}; // L5: Channel
    config.level_configs[6] = {2048, 12, 512.0, TopologyType::CROSSBAR, 16, 32}; // L6: System

    auto network = std::make_shared<MultiInstanceGarnetNetwork>(config);
    network->initialize();
    return network;
}

} // namespace pimid
