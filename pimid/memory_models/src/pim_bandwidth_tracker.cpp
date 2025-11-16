/**
 * @file pim_bandwidth_tracker.cpp
 * @brief Implementation of PIM Bandwidth Tracker
 */

#include "pim_bandwidth_tracker.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace pimid {

PIMBandwidthTracker::PIMBandwidthTracker(
    std::shared_ptr<pimid::memory::DRAMArchitectureV2> dram_arch)
    : dram_arch_(dram_arch),
      num_channels_(0),
      num_ranks_(0),
      num_bank_groups_(0),
      num_banks_(0),
      num_subarrays_(0),
      current_cycle_(0),
      window_start_cycle_(0),
      total_requests_(0),
      bandwidth_limited_requests_(0),
      total_bytes_transferred_(0) {
}

void PIMBandwidthTracker::initialize(int num_channels, int num_ranks,
                                     int num_bank_groups, int num_banks,
                                     int num_subarrays) {
    num_channels_ = num_channels;
    num_ranks_ = num_ranks;
    num_bank_groups_ = num_bank_groups;
    num_banks_ = num_banks;
    num_subarrays_ = num_subarrays;

    initializeBandwidthLimits();
    resetStats();

    std::cout << "PIM Bandwidth Tracker Initialized:\n";
    std::cout << "  Subarray port: " << subarray_port_bits_ << " bits → "
              << subarray_bw_limit_ << " GB/s\n";
    std::cout << "  Bank port:     " << bank_port_bits_ << " bits → "
              << bank_bw_limit_ << " GB/s (CRITICAL BOTTLENECK!)\n";
    std::cout << "  Rank port:     " << rank_port_bits_ << " bits → "
              << rank_bw_limit_ << " GB/s\n";
}

void PIMBandwidthTracker::initializeBandwidthLimits() {
    if (!dram_arch_) {
        std::cerr << "ERROR: DRAM architecture not set!\n";
        return;
    }

    // Get clock frequency
    clock_freq_GHz_ = dram_arch_->timing.clock_freq_mhz / 1000.0;

    // CRITICAL: Use our verified internal port bitwidths!

    // Subarray: GSA datapath (256 bits for DDR4, 512 for HBM2)
    subarray_port_bits_ = dram_arch_->datapath.gsa_datapath_bits.value_bits;

    // Bank: CRITICAL BOTTLENECK - bank serialization (8 bits DDR4, 64 bits HBM2)
    bank_port_bits_ = dram_arch_->datapath.bank_serialization_bits.value_bits;

    // Rank: Standard wide interface (64 bits DDR4, 128 bits HBM2)
    rank_port_bits_ = dram_arch_->datapath.rank_databus_bits.value_bits;

    // Chip: External I/O (8 bits for x8 DDR4, 1024 bits for HBM2)
    chip_port_bits_ = dram_arch_->datapath.chip_io_bits.value_bits;

    // Bank group: Estimated as 2x bank (16 bits DDR4, 128 bits HBM2)
    bank_group_port_bits_ = bank_port_bits_ * 2;

    // Memory controller: Same as rank typically
    mc_port_bits_ = rank_port_bits_;

    // Calculate bandwidth limits
    subarray_bw_limit_ = calculateBandwidth(subarray_port_bits_, clock_freq_GHz_);
    bank_bw_limit_ = calculateBandwidth(bank_port_bits_, clock_freq_GHz_);
    bank_group_bw_limit_ = calculateBandwidth(bank_group_port_bits_, clock_freq_GHz_);
    chip_bw_limit_ = calculateBandwidth(chip_port_bits_, clock_freq_GHz_);
    rank_bw_limit_ = calculateBandwidth(rank_port_bits_, clock_freq_GHz_);
    mc_bw_limit_ = rank_bw_limit_;

    std::cout << "\nVerified Port Bitwidths from DRAM Architecture:\n";
    std::cout << "  Subarray (GSA):        " << subarray_port_bits_
              << " bits (" << dram_arch_->datapath.gsa_datapath_bits.source << ")\n";
    std::cout << "  Bank (serialization):  " << bank_port_bits_
              << " bits (" << dram_arch_->datapath.bank_serialization_bits.source << ")\n";
    std::cout << "  Rank (interface):      " << rank_port_bits_
              << " bits (" << dram_arch_->datapath.rank_databus_bits.source << ")\n";
}

uint64_t PIMBandwidthTracker::requestBandwidth(const PIMRequestPayload& payload,
                                              uint64_t required_bytes) {
    total_requests_++;
    requests_per_level_[payload.granularity]++;
    bytes_per_level_[payload.granularity] += required_bytes;
    total_bytes_transferred_ += required_bytes;

    // Calculate transfer latency based on granularity and contention
    uint64_t latency = calculateTransferLatency(payload.granularity,
                                                payload.target_bank,
                                                required_bytes);

    // Update usage statistics
    updateUsageStats(payload.granularity, payload.target_bank,
                    required_bytes, latency);

    return latency;
}

bool PIMBandwidthTracker::canAcceptRequest(const PIMRequestPayload& payload,
                                           uint64_t required_bytes) {
    // For now, always accept (backpressure handled by latency)
    // TODO: Add queue depth limits if needed
    return true;
}

void PIMBandwidthTracker::tick() {
    current_cycle_++;
    checkStatsWindow();
}

double PIMBandwidthTracker::getBandwidthLimit(PIMGranularity granularity) const {
    switch (granularity) {
        case PIMGranularity::SUBARRAY:
            return subarray_bw_limit_;
        case PIMGranularity::BANK:
            return bank_bw_limit_;
        case PIMGranularity::BANK_GROUP:
            return bank_group_bw_limit_;
        case PIMGranularity::CHIP:
            return chip_bw_limit_;
        case PIMGranularity::RANK:
        case PIMGranularity::MEMORY_CONTROLLER:
            return rank_bw_limit_;
        case PIMGranularity::CPU:
        default:
            return mc_bw_limit_;
    }
}

int PIMBandwidthTracker::getPortBitwidth(PIMGranularity granularity) const {
    switch (granularity) {
        case PIMGranularity::SUBARRAY:
            return subarray_port_bits_;
        case PIMGranularity::BANK:
            return bank_port_bits_;
        case PIMGranularity::BANK_GROUP:
            return bank_group_port_bits_;
        case PIMGranularity::CHIP:
            return chip_port_bits_;
        case PIMGranularity::RANK:
        case PIMGranularity::MEMORY_CONTROLLER:
            return rank_port_bits_;
        case PIMGranularity::CPU:
        default:
            return mc_port_bits_;
    }
}

void PIMBandwidthTracker::registerPE(PIMGranularity granularity,
                                    int pe_id, int target_bank) {
    auto key = std::make_pair(granularity, target_bank);
    pe_registry_[key].push_back(pe_id);

    std::cout << "Registered PE " << pe_id << " at "
              << (int)granularity << " level, target bank " << target_bank
              << " (now " << pe_registry_[key].size() << " PEs sharing)\n";
}

int PIMBandwidthTracker::getConcurrentPEs(PIMGranularity granularity,
                                         int target_id) const {
    auto key = std::make_pair(granularity, target_id);
    auto it = pe_registry_.find(key);
    if (it != pe_registry_.end()) {
        return it->second.size();
    }
    return 1;  // Default: 1 PE
}

double PIMBandwidthTracker::getEffectiveBandwidthPerPE(PIMGranularity granularity,
                                                      int target_id) const {
    double total_bw = getBandwidthLimit(granularity);
    int num_pes = getConcurrentPEs(granularity, target_id);

    if (num_pes > 1) {
        return total_bw / num_pes;
    }
    return total_bw;
}

uint64_t PIMBandwidthTracker::calculateTransferLatency(PIMGranularity granularity,
                                                       int target_id,
                                                       uint64_t bytes) {
    // Get effective bandwidth per PE (considering sharing)
    double effective_bw_GBs = getEffectiveBandwidthPerPE(granularity, target_id);

    // Calculate cycles needed
    // latency = (bytes / BW_GB/s) / (cycle_time_ns)
    // cycle_time_ns = 1 / (freq_GHz)
    // So: latency_cycles = (bytes / BW_GB/s) * freq_GHz * 1e9 / 1e9
    //                    = (bytes / BW_GB/s) * freq_GHz

    double latency_ns = (bytes / effective_bw_GBs);  // nanoseconds
    uint64_t latency_cycles = std::ceil(latency_ns * clock_freq_GHz_);

    // Check if bandwidth limited
    int num_pes = getConcurrentPEs(granularity, target_id);
    if (num_pes > 1) {
        bandwidth_limited_requests_++;
    }

    return std::max(latency_cycles, (uint64_t)1);
}

void PIMBandwidthTracker::updateUsageStats(PIMGranularity granularity,
                                          int target_id,
                                          uint64_t bytes,
                                          uint64_t cycles) {
    auto key = std::make_pair(granularity, target_id);

    bandwidth_usage_[key].bytes_transferred += bytes;
    bandwidth_usage_[key].cycles_active += cycles;
    bandwidth_usage_[key].num_concurrent_pes = getConcurrentPEs(granularity, target_id);

    // Calculate utilization
    if (current_cycle_ > window_start_cycle_) {
        uint64_t window_cycles = current_cycle_ - window_start_cycle_;
        bandwidth_usage_[key].utilization =
            static_cast<double>(bandwidth_usage_[key].cycles_active) / window_cycles;
    }
}

void PIMBandwidthTracker::checkStatsWindow() {
    if (current_cycle_ - window_start_cycle_ >= STATS_WINDOW_CYCLES) {
        // Reset window
        window_start_cycle_ = current_cycle_;

        // Clear usage stats for new window
        for (auto& [key, usage] : bandwidth_usage_) {
            usage.bytes_transferred = 0;
            usage.cycles_active = 0;
            usage.utilization = 0.0;
        }
    }
}

void PIMBandwidthTracker::printStats() const {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ PIM Bandwidth Tracker Statistics                                 ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "Total Requests:            " << total_requests_ << "\n";
    std::cout << "Bandwidth-Limited:         " << bandwidth_limited_requests_
              << " (" << (total_requests_ > 0 ?
                        100.0 * bandwidth_limited_requests_ / total_requests_ : 0.0)
              << "%)\n";
    std::cout << "Total Bytes Transferred:   " << total_bytes_transferred_ << " B ("
              << total_bytes_transferred_ / (1024.0 * 1024) << " MB)\n\n";

    std::cout << "Requests per Granularity Level:\n";
    std::cout << "────────────────────────────────\n";
    for (const auto& [granularity, count] : requests_per_level_) {
        PIMRequestPayload dummy;
        dummy.granularity = granularity;
        std::cout << "  " << std::setw(20) << std::left << dummy.getGranularityName()
                  << ": " << count << " requests, "
                  << bytes_per_level_.at(granularity) / (1024.0 * 1024) << " MB\n";
    }

    std::cout << "\nPort Utilization:\n";
    std::cout << "─────────────────\n";
    for (const auto& [key, usage] : bandwidth_usage_) {
        PIMRequestPayload dummy;
        dummy.granularity = key.first;
        std::cout << "  " << std::setw(20) << std::left << dummy.getGranularityName()
                  << " [ID " << key.second << "]: "
                  << std::fixed << std::setprecision(2)
                  << usage.utilization * 100.0 << "% ("
                  << usage.num_concurrent_pes << " PEs)\n";
    }

    std::cout << "\nBandwidth Limits:\n";
    std::cout << "─────────────────\n";
    std::cout << "  Subarray:      " << subarray_bw_limit_ << " GB/s\n";
    std::cout << "  Bank:          " << bank_bw_limit_ << " GB/s  ← BOTTLENECK in DDR4!\n";
    std::cout << "  Bank Group:    " << bank_group_bw_limit_ << " GB/s\n";
    std::cout << "  Chip:          " << chip_bw_limit_ << " GB/s\n";
    std::cout << "  Rank:          " << rank_bw_limit_ << " GB/s\n";
    std::cout << "\n";
}

void PIMBandwidthTracker::resetStats() {
    total_requests_ = 0;
    bandwidth_limited_requests_ = 0;
    total_bytes_transferred_ = 0;
    requests_per_level_.clear();
    bytes_per_level_.clear();
    bandwidth_usage_.clear();
    active_transfers_.clear();
    window_start_cycle_ = current_cycle_;
}

} // namespace pimid
