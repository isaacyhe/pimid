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
    /* 1.11.57 (latent D009): getRowHits/getRowMisses/getRowConflicts and the
     * three counters behind them are DELETED. They were never incremented --
     * handleRequestCompletion() carried a "placeholder for now" comment where
     * the update belonged, and the only assignments were the constructor's
     * zeroes and resetStats()' zeroes -- so every consumer read a live-looking
     * statistic that was structurally 0. They could not be repaired either: no
     * Ramulator2 instance is ever created in this tree (all construction sites
     * pass an empty config path), so there are no internal row statistics to
     * copy. The measured row behaviour PIMID does have arrives by a different
     * road, setRowMissFraction(), from the PE memory interface's own
     * rowHits/rowMisses; the consumers that used to divide by zeroed counters
     * now use that instead. Deleted rather than left at zero so that nobody
     * treats a permanently-empty counter as a measurement. */

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
    void setDeviceWidth(const std::string& w) { device_width_ = w; }  // 1.11.46
    /* 1.11.52 (audit D003): the MEASURED row-buffer miss fraction from the
     * run (PE-MI rowHits/rowMisses). <0 = not measured, and the array energy
     * then uses the stated 0.5 fallback while the caller says so. */
    void setRowMissFraction(double f) { row_miss_frac_ = f; }
    double getRowMissFraction() const { return row_miss_frac_; }

    double getArrayReadEnergyNJ() const;     // array rd (act+col, amortized) per 64B
    double getArrayWriteEnergyNJ() const;    // array wr per 64B
    double getTerminationEnergyNJ() const;   // ODT/termination per 64B, from CACTI-IO
    /* 1.11.40 (N8): interface terms PIMID never modelled -- driver switching
     * and PHY per 64 B, and the IO area. Previously the DQ interface was
     * termination-only, which understated LPDDR5 by ~71x (the 142x quoted here
     * before rested on a superseded LPDDR5 termination row; see the D010 note
     * in getTerminationEnergyNJ) because LVSTL exists to make termination
     * negligible.
     * 1.11.57 (latent D011): UNUSED AND UNVALIDATED. Neither accessor has a
     * caller anywhere in src/ or include/; every reported DQ-interface number
     * still comes from getTerminationEnergyNJ() alone, so the interface is
     * termination-only in results even though this correction exists in code.
     * Do not cite the 1.11.40 fix as landed until a caller consumes these. */
    double getInterfaceDynamicEnergyNJ() const;
    double getInterfaceAreaMM2() const;
    // Override termination energy (pJ/bit; <0 = model default, 0 = force no termination).
    void setTerminationOverridePJPerBit(double v) { energy_term_override_pJ_per_bit_ = v; }
    double getBackgroundPowerMW() const;     // per-unit active standby + refresh
    double getRefreshPowerMW() const;        // per-unit refresh component only
    /* 1.11.20 (D13+D15): the memory system's background. Population-scaled
     * (one DDR chip / one HBM channel is the IDD unit) and state-aware
     * (IDD3N busy, IDD2N idle, IDD2P idle-with-pg). device_width is the
     * JEDEC x4/x8/x16 string; "" means the x8 default. */
    /* 1.11.52 (audit A015): population = devices/rank x ranks x channels, the
     * same basis memorySystemDieCount() uses for AREA, so the Power and Area
     * lines of one report describe the same memory system. */
    int    getBackgroundUnits(const std::string& device_width = "",
                              int ranks_per_channel = 1, int channels = 1) const;
    double getBackgroundSystemMW(double r_idle, bool pg_enabled,
                                 const std::string& device_width = "",
                                 int ranks_per_channel = 1,
                                 int channels = 1) const;   // 1.11.52 (A015)

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
    /* 1.11.57 (latent D020): getTRRD() removed -- it returned tRAS/4, an
     * invented relation between two independently specified JEDEC timings,
     * and had no callers. */
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
    int getChannelDataBits() const;   // ONE channel, every family (1.11.57 C007)

    /* 1.11.57 (audit C004): the technology the architecture object actually
     * describes, which is NOT always the technology this wrapper was
     * constructed for -- DDR3, LPDDR5 and GDDR6 have no object and read
     * DDR4-2400's. Empty when there is no object at all. */
    std::string getArchitectureTechnology() const;

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
    double getChipEnergyPerByte() const;
    double getRankEnergyPerByte() const;
    /* 1.11.57 (latent D020): getBankGroupEnergyPerByte() (bank x 1.5) and
     * getChannelEnergyPerByte() (rank x 1.5) removed -- unsourced tier
     * surcharges with no callers. */

    // Hierarchical latency (ns)
    double getSubarrayAccessLatency() const;
    double getBankAccessLatency() const;
    double getBankGroupAccessLatency() const;
    double getChipAccessLatency() const;
    double getRankAccessLatency() const;
    /* 1.11.57 (latent D020): getChannelAccessLatency() removed -- rank
     * latency x 1.2 as an unsourced "MC overhead", with no callers. */

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
    // 1.11.57 (latent D009): row_hits_/row_misses_/row_conflicts_ removed --
    // never incremented, and unincrementable without a Ramulator2 instance.

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
    double row_miss_frac_ = -1.0;   // 1.11.52 (D003): measured; <0 = unmeasured
    /* 1.11.46 (L181): device width for the whole-rank array-energy basis.
     * Empty = x8 default (the same convention backgroundUnits uses). */
    std::string device_width_;
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
