/**
 * @file pim_controller_plugin.cpp
 * @brief Ramulator Controller Plugin for PIM Support
 *
 * This implements a Ramulator IControllerPlugin that adds PIM support
 * to Ramulator2's memory controller. It integrates:
 * - Bandwidth tracking at each DRAM level
 * - Internal DRAM network modeling
 * - Port bitwidth constraint enforcement
 * - PE placement and contention tracking
 *
 * IMPORTANT: This plugin does NOT modify DRAM correctness!
 * - DRAM timing constraints are still enforced by Ramulator
 * - Commands (ACT, PRE, RD, WR) work identically
 * - We only ADD latency for data movement based on port BW
 */

#include "pim_controller_plugin.h"
#include <iostream>
#include <iomanip>

namespace pimid {

PIMControllerPlugin::PIMControllerPlugin(
    std::shared_ptr<pimid::memory::DRAMArchitectureV2> dram_arch,
    const std::string& dram_type)
    : dram_arch_(dram_arch),
      dram_type_(dram_type),
      current_cycle_(0),
      total_pim_requests_(0),
      total_normal_requests_(0) {

    // Create bandwidth tracker
    bandwidth_tracker_ = std::make_shared<PIMBandwidthTracker>(dram_arch_);

    // Internal network will be created after we know DRAM organization
    internal_network_ = nullptr;
}

PIMControllerPlugin::~PIMControllerPlugin() {
    // Print final statistics
    printStats();
}

void PIMControllerPlugin::initialize(int num_channels, int num_ranks,
                                     int num_bank_groups, int num_banks,
                                     int num_subarrays) {
    // Initialize bandwidth tracker
    bandwidth_tracker_->initialize(num_channels, num_ranks,
                                   num_bank_groups, num_banks, num_subarrays);

    // Create internal DRAM network
    internal_network_ = createInternalDRAMNetwork(
        dram_type_,
        num_subarrays,      // subarrays per bank
        num_banks / num_bank_groups,  // banks per bank group
        num_bank_groups,    // bank groups per chip
        8                   // chips per rank (typical)
    );

    std::cout << "PIM Controller Plugin initialized for " << dram_type_ << "\n";
    std::cout << "  Channels: " << num_channels << ", Ranks: " << num_ranks << "\n";
    std::cout << "  Bank Groups: " << num_bank_groups << ", Banks: " << num_banks << "\n";
    std::cout << "  Subarrays per bank: " << num_subarrays << "\n";
}

void PIMControllerPlugin::update(bool request_found, Ramulator::ReqBuffer::iterator& req_it) {
    // This is called by Ramulator controller after scheduling
    // We use it to:
    // 1. Track PIM-specific bandwidth usage
    // 2. Model internal network transfers
    // 3. Add data movement latency

    if (!request_found) {
        return;  // No request to process
    }

    // Check if this request has PIM payload
    if (req_it->m_payload == nullptr) {
        // Normal DRAM request
        total_normal_requests_++;
        return;
    }

    // Extract PIM payload
    auto* pim_payload = static_cast<PIMRequestPayload*>(req_it->m_payload);

    // Handle based on operation type
    if (pim_payload->operation == PIMOperationType::NORMAL_READ ||
        pim_payload->operation == PIMOperationType::NORMAL_WRITE) {
        // Normal operation, no special handling
        total_normal_requests_++;
        return;
    }

    // PIM operation detected!
    total_pim_requests_++;

    // Calculate data movement latency based on port bandwidth
    // Safety check: ensure bandwidth_tracker_ is initialized
    uint64_t data_movement_latency = 0;
    if (bandwidth_tracker_) {
        data_movement_latency = bandwidth_tracker_->requestBandwidth(
            *pim_payload, pim_payload->data_bytes);
    } else {
        // Fallback: estimate latency based on typical bandwidth
        // Assume 10 GB/s for bank-level PIM
        const double FALLBACK_BANDWIDTH_GBps = 10.0;
        data_movement_latency = static_cast<uint64_t>(
            (pim_payload->data_bytes / FALLBACK_BANDWIDTH_GBps) * 1e9  // Convert to ns
        );
    }

    pim_payload->data_movement_cycles = data_movement_latency;

    // If operation requires internal network, model it
    if (pim_payload->requiresInternalNetwork()) {
        for (const auto& transfer : pim_payload->internal_transfers) {
            if (transfer.requires_network) {
                handleInternalTransfer(transfer, pim_payload);
            }
        }
    }

    // Add total latency to request
    // Note: We don't directly modify Ramulator's timing!
    // Instead, we track it in the payload for PIMID's use
    pim_payload->data_movement_cycles += pim_payload->network_cycles;
}

void PIMControllerPlugin::tick() {
    current_cycle_++;

    // Tick bandwidth tracker
    if (bandwidth_tracker_) {
        bandwidth_tracker_->tick();
    }

    // Tick internal network
    if (internal_network_) {
        internal_network_->tick();
    }
}

void PIMControllerPlugin::registerPE(PIMGranularity granularity,
                                    int pe_id, int target_bank) {
    if (bandwidth_tracker_) {
        bandwidth_tracker_->registerPE(granularity, pe_id, target_bank);
    }

    std::cout << "Registered PIM PE " << pe_id << " at "
              << PIMRequestPayload().getGranularityName()
              << " level, bank " << target_bank << "\n";
}

void PIMControllerPlugin::handleInternalTransfer(
    const InternalDRAMTransfer& transfer,
    PIMRequestPayload* payload) {

    if (!internal_network_) {
        std::cerr << "WARNING: Internal network not initialized!\n";
        return;
    }

    // Create network packet
    InternalNetworkPacket packet;
    packet.packet_id = total_pim_requests_;
    packet.source_bank = transfer.source_bank;
    packet.dest_bank = transfer.dest_bank;
    packet.source_subarray = transfer.source_subarray;
    packet.dest_subarray = transfer.dest_subarray;
    packet.data_bytes = transfer.transfer_bytes;
    packet.completed = false;

    // Set callback to update payload when transfer completes
    packet.callback = [payload]() {
        if (payload->pim_completion_callback) {
            payload->pim_completion_callback();
        }
    };

    // Send packet through internal network
    if (internal_network_->sendPacket(packet)) {
        // Estimate network latency (detailed calculation in network model)
        NetworkLevel level = NetworkLevel::BANK_NETWORK;
        if (transfer.source_bank == transfer.dest_bank) {
            level = NetworkLevel::SUBARRAY_NETWORK;
        }

        uint64_t network_latency = internal_network_->getTransferLatency(
            level, transfer.source_bank, transfer.dest_bank,
            transfer.transfer_bytes);

        payload->network_cycles += network_latency;
    } else {
        std::cerr << "WARNING: Internal network congested!\n";
    }
}

double PIMControllerPlugin::getBandwidthLimit(PIMGranularity granularity) const {
    if (bandwidth_tracker_) {
        return bandwidth_tracker_->getBandwidthLimit(granularity);
    }
    return 0.0;
}

int PIMControllerPlugin::getPortBitwidth(PIMGranularity granularity) const {
    if (bandwidth_tracker_) {
        return bandwidth_tracker_->getPortBitwidth(granularity);
    }
    return 0;
}

double PIMControllerPlugin::getEffectiveBandwidthPerPE(PIMGranularity granularity,
                                                       int target_id) const {
    if (bandwidth_tracker_) {
        return bandwidth_tracker_->getEffectiveBandwidthPerPE(granularity, target_id);
    }
    return 0.0;
}

void PIMControllerPlugin::printStats() const {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ PIM Controller Plugin Statistics                                 ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "Total Cycles:          " << current_cycle_ << "\n";
    std::cout << "Total PIM Requests:    " << total_pim_requests_ << "\n";
    std::cout << "Total Normal Requests: " << total_normal_requests_ << "\n";

    if (total_pim_requests_ + total_normal_requests_ > 0) {
        double pim_ratio = 100.0 * total_pim_requests_ /
                          (total_pim_requests_ + total_normal_requests_);
        std::cout << "PIM Request Ratio:     " << std::fixed << std::setprecision(2)
                  << pim_ratio << "%\n";
    }

    // Print bandwidth tracker stats
    if (bandwidth_tracker_) {
        bandwidth_tracker_->printStats();
    }

    // Print internal network stats
    if (internal_network_) {
        internal_network_->printStats();
    }
}

void PIMControllerPlugin::resetStats() {
    current_cycle_ = 0;
    total_pim_requests_ = 0;
    total_normal_requests_ = 0;

    if (bandwidth_tracker_) {
        bandwidth_tracker_->resetStats();
    }

    if (internal_network_) {
        internal_network_->resetStats();
    }
}

} // namespace pimid
