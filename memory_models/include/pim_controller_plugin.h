/**
 * @file pim_controller_plugin.h
 * @brief Ramulator Controller Plugin for PIM Support
 */

#ifndef PIMID_PIM_CONTROLLER_PLUGIN_H
#define PIMID_PIM_CONTROLLER_PLUGIN_H

#include <memory>
#include <string>
#include "pim_request_payload.h"
#include "pim_bandwidth_tracker.h"
#include "internal_dram_network.h"
#include "memory/dram_architecture_v2.h"

// Ramulator includes
#include "base/request.h"
#include "dram_controller/plugin.h"

namespace pimid {

/**
 * @brief PIM Controller Plugin for Ramulator
 *
 * This plugin adds PIM support to Ramulator's memory controller.
 * It tracks bandwidth usage, models internal networks, and enforces
 * port bitwidth constraints for PIM operations.
 */
class PIMControllerPlugin : public Ramulator::IControllerPlugin {
public:
    /**
     * @brief Constructor
     * @param dram_arch Verified DRAM architecture specifications
     * @param dram_type DRAM type string ("DDR4", "DDR5", "HBM2", "HBM3")
     */
    PIMControllerPlugin(
        std::shared_ptr<pimid::memory::DRAMArchitectureV2> dram_arch,
        const std::string& dram_type);

    ~PIMControllerPlugin() override;

    /**
     * @brief Initialize the plugin with DRAM configuration
     */
    void initialize(int num_channels, int num_ranks,
                   int num_bank_groups, int num_banks, int num_subarrays);

    /**
     * @brief Update function called by Ramulator controller
     * @param request_found Whether a request was found
     * @param req_it Iterator to the request
     */
    void update(bool request_found, Ramulator::ReqBuffer::iterator& req_it) override;

    /**
     * @brief Tick the plugin (advance by one cycle)
     */
    void tick();

    /**
     * @brief Register a PE at a specific DRAM hierarchy level
     * @param granularity Where the PE is placed
     * @param pe_id PE identifier
     * @param target_bank Which bank this PE operates on
     */
    void registerPE(PIMGranularity granularity, int pe_id, int target_bank);

    /**
     * @brief Get bandwidth limit for a specific granularity level
     * @param granularity PIM granularity
     * @return Bandwidth in GB/s
     */
    double getBandwidthLimit(PIMGranularity granularity) const;

    /**
     * @brief Get port bitwidth for a specific granularity level
     * @param granularity PIM granularity
     * @return Port bitwidth in bits
     */
    int getPortBitwidth(PIMGranularity granularity) const;

    /**
     * @brief Get effective bandwidth per PE (considering contention)
     * @param granularity PIM granularity
     * @param target_id Bank/subarray ID
     * @return Effective bandwidth in GB/s
     */
    double getEffectiveBandwidthPerPE(PIMGranularity granularity,
                                     int target_id) const;

    /**
     * @brief Print statistics
     */
    void printStats() const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

    /**
     * @brief Get bandwidth tracker
     */
    std::shared_ptr<PIMBandwidthTracker> getBandwidthTracker() const {
        return bandwidth_tracker_;
    }

    /**
     * @brief Get internal network
     */
    std::shared_ptr<InternalDRAMNetwork> getInternalNetwork() const {
        return internal_network_;
    }

private:
    // DRAM architecture
    std::shared_ptr<pimid::memory::DRAMArchitectureV2> dram_arch_;
    std::string dram_type_;

    // PIM components
    std::shared_ptr<PIMBandwidthTracker> bandwidth_tracker_;
    std::shared_ptr<InternalDRAMNetwork> internal_network_;

    // Current cycle
    uint64_t current_cycle_;

    // Statistics
    uint64_t total_pim_requests_;
    uint64_t total_normal_requests_;

    /**
     * @brief Handle internal DRAM transfer for PIM operation
     * @param transfer Transfer specification
     * @param payload PIM request payload to update
     */
    void handleInternalTransfer(const InternalDRAMTransfer& transfer,
                               PIMRequestPayload* payload);
};

} // namespace pimid

#endif // PIMID_PIM_CONTROLLER_PLUGIN_H
