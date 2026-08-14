#ifndef PIMID_RAMULATOR_WRAPPER_H
#define PIMID_RAMULATOR_WRAPPER_H

#include "common/types.h"
#include <string>
#include <memory>
#include <functional>

// PIMID PIM extensions
#include "memory/pim_request_payload.h"
#include "memory/pim_bandwidth_tracker.h"
#include "memory/pim_controller_plugin.h"
#include "memory/internal_dram_network.h"
#include "memory/dram_architecture_v2.h"

// Forward declarations for Ramulator types
namespace Ramulator {
    class IMemorySystem;
    struct Request;
}

namespace pimid {

/**
 * Wrapper class that adapts Ramulator2 to PIMID's memory model interface
 * This provides a clean separation between PIMID and Ramulator code
 */
class RamulatorWrapper {
public:
    explicit RamulatorWrapper(const std::string& config_path, const std::string& dram_type = "DDR4");
    ~RamulatorWrapper();

    // Initialization
    void initialize();
    void loadConfig(const std::string& config_path);

    // Request handling
    bool send(Address addr, MemoryRequestType type,
              std::function<void(Address)> callback = nullptr);

    // PIM-aware request handling
    bool sendPIM(Address addr, MemoryRequestType type,
                PIMRequestPayload* pim_payload,
                std::function<void(Address)> callback = nullptr);

    bool canAccept() const;
    void tick();

    // Statistics and metrics
    uint64_t getTotalReads() const { return total_reads_; }
    uint64_t getTotalWrites() const { return total_writes_; }
    uint64_t getRowHits() const { return row_hits_; }
    uint64_t getRowMisses() const { return row_misses_; }
    uint64_t getRowConflicts() const { return row_conflicts_; }

    // Energy metrics (from Ramulator statistics)
    double getReadEnergy() const;
    double getWriteEnergy() const;
    double getActivationEnergy() const;
    double getPrechargeEnergy() const;
    double getRefreshEnergy() const;
    double getLeakagePower() const;
    double getTotalEnergy() const;

    // 1.9.10: PER-ACCESS energy accessors + JEDEC-IDD background/refresh.
    // Unlike getReadEnergy()/getWriteEnergy() (which return a cumulative
    // total_reads_ * per-access and were the source of the 0.000 nJ bug when
    // queried on an unfed oracle), these return the *intensive* per-64B-access
    // dynamic energy and per-device standby/refresh power, independent of how
    // many accesses the wrapper has seen. Sourced from the in-tree DRAM-arch
    // bank energy (array) + a per-tech JEDEC IDD/VDD table (interface, background).
    // Override the IDD-derived array energy with a fixed pJ/byte (0 = use IDD default).
    void setBankEnergyOverridePJPerByte(double v) { energy_bank_override_pJ_per_byte_ = v; }
    double getArrayReadEnergyNJ() const;     // array rd (act+col, amortized) per 64B
    double getArrayWriteEnergyNJ() const;    // array wr per 64B
    double getTerminationEnergyNJ() const;   // ODT/termination per 64B (DDR-class; HBM=0)
    // Override termination energy (pJ/bit; <0 = model default, 0 = force no termination).
    void setTerminationOverridePJPerBit(double v) { energy_term_override_pJ_per_bit_ = v; }
    double getBackgroundPowerMW() const;
    double getBackgroundEffectiveMW(double r_idle) const;  // 1.11.8: power-down descent     // per-device active standby + refresh
    double getRefreshPowerMW() const;        // per-device refresh component only

    // Configuration queries
    uint64_t getCapacity() const { return capacity_; }
    uint64_t getBandwidth() const { return bandwidth_; }
    Cycle getAverageLatency() const;

    // Cycle tracking
    Cycle getCurrentCycle() const { return current_cycle_; }

    void printStats() const;
    void resetStats();

    //=========================================================================
    // Subarray-Level Characteristics (for PIM modeling)
    //=========================================================================

    // DRAM timing breakdown (in nanoseconds)
    double getTRCD() const;                   // RAS to CAS delay
    double getTCAS() const;                   // CAS latency
    double getTRP() const;                    // Row precharge time
    double getTRAS() const;                   // Row active time
    double getTRRD() const;                   // Row to row delay
    double getTRC() const;                    // Row cycle time
    double getTBurst() const;                 // Burst transfer time

    // Subarray organization
    uint32_t getSubarraysPerBank() const;
    uint32_t getBanksPerBankGroup() const;
    uint32_t getBankGroupsPerChip() const;
    uint32_t getChipsPerRank() const;
    uint32_t getRanksPerChannel() const;
    uint32_t getNumChannels() const { return channels_; }

    // Subarray capacity
    uint64_t getSubarraySizeKB() const;
    uint64_t getBankSizeMB() const;
    uint64_t getChipSizeMB() const;

    // Internal port bitwidths (critical for PIM bandwidth!)
    int getSubarrayPortBits() const;
    int getBankPortBits() const;
    int getBankGroupPortBits() const;
    int getChipIOBits() const;
    int getRankDataBits() const;
    int getChannelDataBits() const;

    // Hierarchical bandwidth (GB/s)
    double getSubarrayBandwidth() const;
    double getBankBandwidth() const;
    double getBankGroupBandwidth() const;
    double getChipIOBandwidth() const;
    double getRankBandwidth() const;
    double getChannelBandwidth() const;

    // Hierarchical energy (pJ per byte)
    double getSubarrayEnergyPerByte() const;
    double getBankEnergyPerByte() const;
    double getBankGroupEnergyPerByte() const;
    double getChipEnergyPerByte() const;
    double getRankEnergyPerByte() const;
    double getChannelEnergyPerByte() const;

    // Hierarchical latency (ns)
    double getSubarrayAccessLatency() const;
    double getBankAccessLatency() const;
    double getBankGroupAccessLatency() const;
    double getChipAccessLatency() const;
    double getRankAccessLatency() const;
    double getChannelAccessLatency() const;

    // Get DRAM architecture specification
    const pimid::memory::DRAMArchitectureV2* getDRAMArchitecture() const;

    //=========================================================================

    // PIM-specific configuration and queries
    void enablePIMSupport(const std::string& dram_type = "DDR4");
    void registerPE(PIMGranularity granularity, int pe_id, int target_bank);
    double getBandwidthLimit(PIMGranularity granularity) const;
    int getPortBitwidth(PIMGranularity granularity) const;
    double getEffectiveBandwidthPerPE(PIMGranularity granularity, int target_id) const;

    // Get PIM components
    std::shared_ptr<PIMBandwidthTracker> getBandwidthTracker() const {
        return bandwidth_tracker_;
    }
    std::shared_ptr<InternalDRAMNetwork> getInternalNetwork() const {
        return internal_network_;
    }
    std::shared_ptr<PIMControllerPlugin> getPIMPlugin() const {
        return pim_plugin_;
    }

private:
    // Ramulator memory system instance
    std::shared_ptr<Ramulator::IMemorySystem> ramulator_memory_system_;

    // Configuration
    std::string config_path_;
    std::string config_yaml_;  // YAML configuration content

    // Memory system parameters
    uint64_t capacity_;
    uint64_t bandwidth_;
    uint32_t channels_;
    uint32_t ranks_per_channel_;
    uint32_t banks_per_rank_;

    // Statistics tracking
    uint64_t total_reads_;
    uint64_t total_writes_;
    uint64_t row_hits_;
    uint64_t row_misses_;
    uint64_t row_conflicts_;

    // Energy tracking
    mutable double cached_read_energy_;
    mutable double cached_write_energy_;
    mutable double cached_leakage_power_;
    mutable Cycle last_energy_update_;
    double energy_bank_override_pJ_per_byte_ = 0.0;  // 0 = IDD default; >0 = user override
    double energy_term_override_pJ_per_bit_ = -1.0;  // <0 = model default; >=0 = user override

    // Cycle counter
    Cycle current_cycle_;

    // Pending request tracking
    struct PendingRequest {
        Address addr;
        MemoryRequestType type;
        Cycle issue_cycle;
        std::function<void(Address)> callback;
    };
    std::vector<PendingRequest> pending_requests_;

    // PIM Support Components
    bool pim_enabled_;
    std::string dram_type_;
    std::shared_ptr<pimid::memory::DRAMArchitectureV2> dram_arch_;
    std::shared_ptr<PIMBandwidthTracker> bandwidth_tracker_;
    std::shared_ptr<InternalDRAMNetwork> internal_network_;
    std::shared_ptr<PIMControllerPlugin> pim_plugin_;

    // Helper functions
    void parseConfiguration();
    void createRamulatorInstance();
    Ramulator::Request createRamulatorRequest(Address addr, MemoryRequestType type);
    Ramulator::Request createPIMRequest(Address addr, MemoryRequestType type,
                                       PIMRequestPayload* pim_payload);
    void handleRequestCompletion(Ramulator::Request& req);
    void updateEnergyMetrics() const;
    void initializePIMComponents();
};

} // namespace pimid

#endif // PIMID_RAMULATOR_WRAPPER_H
