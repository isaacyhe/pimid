/**
 * @file internal_dram_network.cpp
 * @brief Implementation of Internal DRAM Network Model
 */

#include "internal_dram_network.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace pimid {

InternalDRAMNetwork::InternalDRAMNetwork(
    const std::string& dram_type,
    std::shared_ptr<NetworkModel> network_model)
    : dram_type_(dram_type),
      num_subarrays_per_bank_(0),
      num_banks_per_bg_(0),
      num_bg_per_chip_(0),
      num_chips_per_rank_(0),
      external_network_model_(network_model),
      current_cycle_(0),
      total_packets_sent_(0),
      total_packets_completed_(0),
      total_bytes_transferred_(0),
      total_network_latency_(0),
      subarray_network_accesses_(0),
      bank_network_accesses_(0),
      bg_network_accesses_(0),
      chip_network_accesses_(0) {
}

void InternalDRAMNetwork::initialize(int num_subarrays_per_bank,
                                     int num_banks_per_bg,
                                     int num_bg_per_chip,
                                     int num_chips_per_rank) {
    num_subarrays_per_bank_ = num_subarrays_per_bank;
    num_banks_per_bg_ = num_banks_per_bg;
    num_bg_per_chip_ = num_bg_per_chip;
    num_chips_per_rank_ = num_chips_per_rank;

    // Configure network based on DRAM type
    if (dram_type_ == "DDR4") {
        configureDDR4Network();
    } else if (dram_type_ == "DDR5") {
        configureDDR5Network();
    } else if (dram_type_ == "HBM2") {
        configureHBM2Network();
    } else if (dram_type_ == "HBM3") {
        configureHBM3Network();
    } else {
        std::cerr << "WARNING: Unknown DRAM type " << dram_type_
                  << ", using DDR4 defaults\n";
        configureDDR4Network();
    }

    resetStats();

    std::cout << "Internal DRAM Network Initialized (" << dram_type_ << "):\n";
    std::cout << "  Subarray network: " << subarray_network_config_.link_width_bits
              << " bits, " << subarray_network_config_.bandwidth_GBs << " GB/s\n";
    std::cout << "  Bank network:     " << bank_network_config_.link_width_bits
              << " bits, " << bank_network_config_.bandwidth_GBs << " GB/s\n";
    std::cout << "  BankGroup network:" << bg_network_config_.link_width_bits
              << " bits, " << bg_network_config_.bandwidth_GBs << " GB/s\n";
    std::cout << "  Chip network:     " << chip_network_config_.link_width_bits
              << " bits, " << chip_network_config_.bandwidth_GBs << " GB/s\n";
}

void InternalDRAMNetwork::configureDDR4Network() {
    // DDR4 has NARROW internal paths!

    // Subarray network (within bank): Uses GSA (256 bits) but serialized
    subarray_network_config_.link_width_bits = 64;  // Prefetch width
    subarray_network_config_.frequency_GHz = 1.2;    // DDR4-2400
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 5;    // Short distance
    subarray_network_config_.topology = "crossbar"; // Within bank

    // Bank network (within bank group): NARROW 8-16 bit paths
    bank_network_config_.link_width_bits = 8;       // Bank serialization bottleneck
    bank_network_config_.frequency_GHz = 1.2;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 10;       // Medium distance
    bank_network_config_.topology = "bus";          // Shared bus

    // Bank group network (within chip): Slightly wider
    bg_network_config_.link_width_bits = 16;
    bg_network_config_.frequency_GHz = 1.2;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 20;         // Longer distance
    bg_network_config_.topology = "bus";

    // Chip network (within rank): Via external pins (slow!)
    chip_network_config_.link_width_bits = 8;        // x8 device I/O
    chip_network_config_.frequency_GHz = 1.2;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 50;        // Must go off-chip!
    chip_network_config_.topology = "point-to-point";
}

void InternalDRAMNetwork::configureDDR5Network() {
    // DDR5 has wider prefetch but similar bank bottleneck

    subarray_network_config_.link_width_bits = 128;  // 16n prefetch
    subarray_network_config_.frequency_GHz = 1.6;    // DDR5-3200
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 5;
    subarray_network_config_.topology = "crossbar";

    bank_network_config_.link_width_bits = 16;       // Slightly wider than DDR4
    bank_network_config_.frequency_GHz = 1.6;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 10;
    bank_network_config_.topology = "bus";

    bg_network_config_.link_width_bits = 32;
    bg_network_config_.frequency_GHz = 1.6;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 20;
    bg_network_config_.topology = "bus";

    chip_network_config_.link_width_bits = 8;
    chip_network_config_.frequency_GHz = 1.6;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 50;
    chip_network_config_.topology = "point-to-point";
}

void InternalDRAMNetwork::configureHBM2Network() {
    // HBM2 has WIDE internal paths via TSV!

    subarray_network_config_.link_width_bits = 256;  // Wide column I/O
    subarray_network_config_.frequency_GHz = 1.0;    // HBM2
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 3;     // Short vertical distance
    subarray_network_config_.topology = "crossbar";

    // CRITICAL: HBM2 has WIDE bank paths (64 bits) via TSV!
    bank_network_config_.link_width_bits = 64;       // 8x wider than DDR4!
    bank_network_config_.frequency_GHz = 1.0;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 5;         // TSV is fast
    bank_network_config_.topology = "crossbar";      // 3D crossbar via TSV

    bg_network_config_.link_width_bits = 128;
    bg_network_config_.frequency_GHz = 1.0;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 8;
    bg_network_config_.topology = "crossbar";

    chip_network_config_.link_width_bits = 128;      // Wide channel
    chip_network_config_.frequency_GHz = 1.0;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 10;        // On-interposer
    chip_network_config_.topology = "crossbar";
}

void InternalDRAMNetwork::configureHBM3Network() {
    // HBM3 is even wider and faster

    subarray_network_config_.link_width_bits = 512;
    subarray_network_config_.frequency_GHz = 1.8;
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 3;
    subarray_network_config_.topology = "crossbar";

    bank_network_config_.link_width_bits = 128;      // Even wider
    bank_network_config_.frequency_GHz = 1.8;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 5;
    bank_network_config_.topology = "crossbar";

    bg_network_config_.link_width_bits = 256;
    bg_network_config_.frequency_GHz = 1.8;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 8;
    bg_network_config_.topology = "crossbar";

    chip_network_config_.link_width_bits = 128;
    chip_network_config_.frequency_GHz = 1.8;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 10;
    chip_network_config_.topology = "crossbar";
}

bool InternalDRAMNetwork::sendPacket(const InternalNetworkPacket& packet) {
    // Determine which network level is needed
    NetworkLevel level = determineNetworkLevel(packet.source_bank, packet.dest_bank,
                                              packet.source_subarray, packet.dest_subarray);

    // Get the appropriate network config
    InternalNetworkLink* link_config = nullptr;
    std::queue<InternalNetworkPacket>* queue = nullptr;

    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            link_config = &subarray_network_config_;
            queue = &subarray_network_queue_;
            subarray_network_accesses_++;
            break;
        case NetworkLevel::BANK_NETWORK:
            link_config = &bank_network_config_;
            queue = &bank_network_queue_;
            bank_network_accesses_++;
            break;
        case NetworkLevel::BANK_GROUP_NETWORK:
            link_config = &bg_network_config_;
            queue = &bg_network_queue_;
            bg_network_accesses_++;
            break;
        case NetworkLevel::CHIP_NETWORK:
            link_config = &chip_network_config_;
            queue = &chip_network_queue_;
            chip_network_accesses_++;
            break;
    }

    // Calculate completion time
    uint64_t transfer_time = calculateTransferTime(*link_config, packet.data_bytes);
    uint64_t completion_time = current_cycle_ + link_config->latency_cycles + transfer_time;

    // Create packet copy with timing info
    InternalNetworkPacket timed_packet = packet;
    timed_packet.injection_time = current_cycle_;
    timed_packet.completion_time = completion_time;
    timed_packet.completed = false;

    // Add to in-flight packets
    inflight_packets_.push_back(timed_packet);

    total_packets_sent_++;
    total_bytes_transferred_ += packet.data_bytes;

    return true;
}

void InternalDRAMNetwork::tick() {
    current_cycle_++;
    processInflightPackets();
}

uint64_t InternalDRAMNetwork::getTransferLatency(NetworkLevel level,
                                                 int source_id, int dest_id,
                                                 uint64_t data_bytes) {
    InternalNetworkLink* link_config = nullptr;

    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            link_config = &subarray_network_config_;
            break;
        case NetworkLevel::BANK_NETWORK:
            link_config = &bank_network_config_;
            break;
        case NetworkLevel::BANK_GROUP_NETWORK:
            link_config = &bg_network_config_;
            break;
        case NetworkLevel::CHIP_NETWORK:
            link_config = &chip_network_config_;
            break;
    }

    uint64_t transfer_time = calculateTransferTime(*link_config, data_bytes);
    return link_config->latency_cycles + transfer_time;
}

bool InternalDRAMNetwork::canAcceptPacket(NetworkLevel level) {
    // For now, always accept (no queue depth limit)
    // TODO: Add configurable queue limits
    return true;
}

NetworkLevel InternalDRAMNetwork::determineNetworkLevel(int source_bank, int dest_bank,
                                                       int source_subarray, int dest_subarray) {
    // If within same bank, use subarray network
    if (source_bank == dest_bank) {
        return NetworkLevel::SUBARRAY_NETWORK;
    }

    // If within same bank group (banks 0-3 vs 4-7 in typical config)
    int source_bg = source_bank / num_banks_per_bg_;
    int dest_bg = dest_bank / num_banks_per_bg_;
    if (source_bg == dest_bg) {
        return NetworkLevel::BANK_NETWORK;
    }

    // If within same chip
    int source_chip = source_bank / (num_banks_per_bg_ * num_bg_per_chip_);
    int dest_chip = dest_bank / (num_banks_per_bg_ * num_bg_per_chip_);
    if (source_chip == dest_chip) {
        return NetworkLevel::BANK_GROUP_NETWORK;
    }

    // Different chips - need chip network
    return NetworkLevel::CHIP_NETWORK;
}

uint64_t InternalDRAMNetwork::calculateTransferTime(const InternalNetworkLink& link,
                                                    uint64_t data_bytes) {
    // transfer_time = data_bytes / bandwidth_GBs / (1 / freq_GHz)
    //               = data_bytes / bandwidth_GBs * freq_GHz

    // Safety check: prevent division by zero
    if (link.bandwidth_GBs <= 0.0 || link.frequency_GHz <= 0.0) {
        std::cerr << "ERROR: Invalid link parameters - bandwidth: "
                  << link.bandwidth_GBs << " GB/s, frequency: "
                  << link.frequency_GHz << " GHz" << std::endl;
        // Return conservative estimate: 1 cycle per byte
        return std::max(data_bytes, (uint64_t)1);
    }

    double transfer_time_ns = data_bytes / link.bandwidth_GBs;
    uint64_t transfer_cycles = std::ceil(transfer_time_ns * link.frequency_GHz);
    return std::max(transfer_cycles, (uint64_t)1);
}

void InternalDRAMNetwork::processInflightPackets() {
    auto it = inflight_packets_.begin();
    while (it != inflight_packets_.end()) {
        if (current_cycle_ >= it->completion_time && !it->completed) {
            // Packet completed
            it->completed = true;
            total_packets_completed_++;
            total_network_latency_ += (it->completion_time - it->injection_time);

            // Call callback if exists
            if (it->callback) {
                it->callback();
            }

            // Remove from inflight
            it = inflight_packets_.erase(it);
        } else {
            ++it;
        }
    }
}

void InternalDRAMNetwork::printStats() const {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Internal DRAM Network Statistics (" << dram_type_ << ")                    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "Total Packets:             " << total_packets_sent_ << "\n";
    std::cout << "Completed Packets:         " << total_packets_completed_ << "\n";
    std::cout << "In-Flight Packets:         " << inflight_packets_.size() << "\n";
    std::cout << "Total Bytes Transferred:   " << total_bytes_transferred_ << " B ("
              << total_bytes_transferred_ / (1024.0 * 1024) << " MB)\n";

    if (total_packets_completed_ > 0) {
        std::cout << "Average Network Latency:   "
                  << static_cast<double>(total_network_latency_) / total_packets_completed_
                  << " cycles\n";
    }

    std::cout << "\nNetwork Level Usage:\n";
    std::cout << "────────────────────\n";
    std::cout << "  Subarray network:  " << subarray_network_accesses_ << " transfers\n";
    std::cout << "  Bank network:      " << bank_network_accesses_ << " transfers\n";
    std::cout << "  BankGroup network: " << bg_network_accesses_ << " transfers\n";
    std::cout << "  Chip network:      " << chip_network_accesses_ << " transfers\n";
    std::cout << "\n";
}

void InternalDRAMNetwork::resetStats() {
    total_packets_sent_ = 0;
    total_packets_completed_ = 0;
    total_bytes_transferred_ = 0;
    total_network_latency_ = 0;
    subarray_network_accesses_ = 0;
    bank_network_accesses_ = 0;
    bg_network_accesses_ = 0;
    chip_network_accesses_ = 0;
    inflight_packets_.clear();
}

uint64_t InternalDRAMNetwork::calculateNetworkRequirements(
    int pe_bank, int pe_bg, int pe_chip,
    const std::map<int, uint64_t>& data_distribution,
    std::vector<InternalDRAMTransfer>& transfers) {

    uint64_t total_latency = 0;
    transfers.clear();

    for (const auto& [bank_id, bytes] : data_distribution) {
        if (bank_id == pe_bank) {
            // Local access - no network needed
            continue;
        }

        // Remote access - need network transfer
        InternalDRAMTransfer transfer;
        transfer.source_bank = bank_id;
        transfer.dest_bank = pe_bank;
        transfer.source_subarray = -1; // Not specified
        transfer.dest_subarray = -1;
        transfer.source_bank_group = bank_id / num_banks_per_bg_;
        transfer.dest_bank_group = pe_bg;
        transfer.source_chip = bank_id / (num_banks_per_bg_ * num_bg_per_chip_);
        transfer.dest_chip = pe_chip;
        transfer.transfer_bytes = bytes;
        transfer.requires_network = true;

        // Determine network level and calculate latency
        NetworkLevel level;
        if (transfer.source_bank_group == transfer.dest_bank_group) {
            level = NetworkLevel::BANK_NETWORK;
        } else if (transfer.source_chip == transfer.dest_chip) {
            level = NetworkLevel::BANK_GROUP_NETWORK;
        } else {
            level = NetworkLevel::CHIP_NETWORK;
        }

        uint64_t latency = getTransferLatency(level, bank_id, pe_bank, bytes);
        transfer.network_latency = latency;
        total_latency += latency;

        // Determine locality
        if (transfer.source_bank_group == transfer.dest_bank_group) {
            transfer.locality = DataLocality::REMOTE_SAME_BG;
        } else if (transfer.source_chip == transfer.dest_chip) {
            transfer.locality = DataLocality::REMOTE_SAME_CHIP;
        } else {
            transfer.locality = DataLocality::REMOTE_SAME_RANK;
        }

        transfers.push_back(transfer);
    }

    return total_latency;
}

uint64_t InternalDRAMNetwork::executeGather(
    int pe_bank,
    const std::vector<int>& source_banks,
    uint64_t bytes_per_bank) {

    uint64_t total_latency = 0;

    // Create map of data distribution
    std::map<int, uint64_t> data_dist;
    for (int bank : source_banks) {
        data_dist[bank] = bytes_per_bank;
    }

    // Calculate network requirements
    std::vector<InternalDRAMTransfer> transfers;
    int pe_bg = pe_bank / num_banks_per_bg_;
    int pe_chip = pe_bank / (num_banks_per_bg_ * num_bg_per_chip_);

    total_latency = calculateNetworkRequirements(
        pe_bank, pe_bg, pe_chip, data_dist, transfers);

    return total_latency;
}

uint64_t InternalDRAMNetwork::executeScatter(
    int pe_bank,
    const std::vector<int>& dest_banks,
    uint64_t bytes_per_bank) {

    uint64_t total_latency = 0;

    for (int dest_bank : dest_banks) {
        if (dest_bank == pe_bank) continue; // No transfer needed

        NetworkLevel level;
        int pe_bg = pe_bank / num_banks_per_bg_;
        int dest_bg = dest_bank / num_banks_per_bg_;

        if (pe_bg == dest_bg) {
            level = NetworkLevel::BANK_NETWORK;
        } else {
            level = NetworkLevel::BANK_GROUP_NETWORK;
        }

        uint64_t latency = getTransferLatency(level, pe_bank, dest_bank, bytes_per_bank);
        total_latency += latency;
    }

    return total_latency;
}

uint64_t InternalDRAMNetwork::executeReduce(
    const std::vector<int>& source_banks,
    int dest_bank,
    uint64_t bytes_per_bank) {

    // Similar to gather, but with reduction operation overhead
    uint64_t gather_latency = executeGather(dest_bank, source_banks, bytes_per_bank);

    // Add reduction computation overhead (simplified model)
    uint64_t reduction_overhead = source_banks.size() * 10; // 10 cycles per source

    return gather_latency + reduction_overhead;
}

uint64_t InternalDRAMNetwork::executeBroadcast(
    int source_bank,
    const std::vector<int>& dest_banks,
    uint64_t total_bytes) {

    // Broadcast can potentially use multicast if network supports it
    // For now, model as series of point-to-point transfers
    uint64_t bytes_per_dest = total_bytes; // Full copy to each destination

    return executeScatter(source_bank, dest_banks, bytes_per_dest);
}

double InternalDRAMNetwork::getAvailableBandwidth(NetworkLevel level) const {
    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            return subarray_network_config_.bandwidth_GBs;
        case NetworkLevel::BANK_NETWORK:
            return bank_network_config_.bandwidth_GBs;
        case NetworkLevel::BANK_GROUP_NETWORK:
            return bg_network_config_.bandwidth_GBs;
        case NetworkLevel::CHIP_NETWORK:
            return chip_network_config_.bandwidth_GBs;
        default:
            return 0.0;
    }
}

bool InternalDRAMNetwork::inSameBankGroup(int bank1, int bank2) const {
    int bg1 = bank1 / num_banks_per_bg_;
    int bg2 = bank2 / num_banks_per_bg_;
    return bg1 == bg2;
}

bool InternalDRAMNetwork::inSameChip(int bank1, int bank2) const {
    int chip1 = bank1 / (num_banks_per_bg_ * num_bg_per_chip_);
    int chip2 = bank2 / (num_banks_per_bg_ * num_bg_per_chip_);
    return chip1 == chip2;
}

std::shared_ptr<InternalDRAMNetwork> createInternalDRAMNetwork(
    const std::string& dram_type,
    int num_subarrays_per_bank,
    int num_banks_per_bg,
    int num_bg_per_chip,
    int num_chips_per_rank) {

    auto network = std::make_shared<InternalDRAMNetwork>(dram_type);
    network->initialize(num_subarrays_per_bank, num_banks_per_bg,
                       num_bg_per_chip, num_chips_per_rank);
    return network;
}

} // namespace pimid
