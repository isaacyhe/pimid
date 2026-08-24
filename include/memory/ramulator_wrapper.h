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

/* 1.11.61 (rulings R2/R4): THE SIMULATED PRESET'S OWN ORGANISATION.
 *
 * The Ramulator2 org preset the wrapper names is the authority for what part
 * this tree simulates -- it fixes the density, the bank count and the row
 * count the timing model counts cycles against. Until this release PIMID
 * carried three private copies of that organisation (the architecture object's
 * capacity fields, main.cpp's bank_rows table, and the wrapper's own
 * banks_per_rank_), each of which had drifted from it independently: the
 * capacities by 4x-32x and bank_rows by 2x on DDR3, HBM2 and HBM3.
 *
 * Ramulator2's org_presets tables are `inline static const std::map` members
 * of classes defined in .cpp files (external/ramulator/src/dram/impl/*.cpp),
 * with no header exposing them, so they cannot be read at runtime from here.
 * This struct is a TRANSCRIPTION of the exact preset rows PIMID selects, each
 * row carrying the file it was transcribed from, so that there is ONE copy in
 * PIMID instead of three and it names its upstream. `preset_source` is checked
 * for self-consistency at construction (banks x rows x cols x DQ must
 * reproduce the density), which is the guard against a transcription slip.
 *
 * UNITS, and they differ by family exactly as the preset files do:
 *   density_mb        DDR family: the whole DEVICE (GDDR6 included -- its
 *                     density product spans both channels). HBM: ONE CHANNEL
 *                     (HBM2.cpp/HBM3.cpp call it "channel density").
 *   banks_in_density  the banks that density_mb spans, on the same unit.
 *   channels_in_density  the channel levels inside the density product
 *                     (GDDR6 2, everything else 1) -- NOT the run's channel
 *                     count, which is RamulatorWrapper::getNumChannels().
 */
struct PresetOrganization {
    std::string preset_name;        // e.g. "DDR4_8Gb_x8"
    std::string preset_source;      // e.g. "external/ramulator/src/dram/impl/DDR4.cpp"
    uint64_t    density_mb = 0;     // per DEVICE (DDR family) / per CHANNEL (HBM)
    int         dq_bits = 0;
    int         channels_in_density = 1;
    int         banks_in_density = 0;
    uint64_t    rows_per_bank = 0;
    uint64_t    cols_per_row = 0;
    bool        per_channel_density = false;   // true for HBM2/HBM3
    bool        valid = false;

    // One bank of the simulated part, in MB. Same unit for every family.
    uint64_t bankSizeMB() const {
        return (banks_in_density > 0) ? density_mb / static_cast<uint64_t>(banks_in_density) : 0;
    }
    // Bytes in one row of one bank.
    uint64_t rowBytes() const { return cols_per_row * static_cast<uint64_t>(dq_bits) / 8; }
};

/**
 * Wrapper class that adapts Ramulator2 to PIMID's memory model interface
 * This provides a clean separation between PIMID and Ramulator code
 */
class RamulatorWrapper {
public:
    explicit RamulatorWrapper(const std::string& config_path, const std::string& dram_type = "DDR4");
    ~RamulatorWrapper();

    /* 1.11.61 (gate 1171A D4): mark this wrapper as a REFERENCE ANCHOR --
     * constructed only to read one bandwidth number (the B012 REF anchors),
     * never to size anything. Anchor wrappers suppress the capacity and
     * speed-bin provenance WARNINGS, which would otherwise misattribute an
     * HBM3 sizing disagreement into every DDR4/HBM2 run's log. The warnings
     * still fire on every wrapper that is actually used for the run. */
    void setAnchorQuiet(bool q) { anchor_quiet_ = q; }

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
    /* 1.11.59 (audit C018): this used to store the width and stop there, so
     * the JEDEC device width priced the ARRAY ENERGY (4/8/16 devices per
     * access) and changed nothing on the WIDTH side -- chip_io_bits stayed at
     * the architecture object's fixed 8 for every DDR-class part, and the
     * extractor stamped that 8 "Extracted from Ramulator device configuration
     * (x4/x8/x16)". One run then priced a rank as 4 devices for energy and 8
     * for bandwidth. It now applies the width to the architecture object as
     * well; defined out of line for that reason. Order-independent: callers
     * that set the width before initialize() and callers that set it after are
     * both correct. */
    void setDeviceWidth(const std::string& w);  // 1.11.46; 1.11.59 (C018)
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
    /* 1.11.59: the termination band, where Rtt is unsourced. Returns false
     * when the technology's electricals ARE sourced (nothing to band). */
    bool getTerminationEnergyBandNJ(double& lo_nj, double& hi_nj,
                                    std::string& provenance) const;
    double getInterfaceDynamicEnergyNJ() const;
    double getInterfaceAreaMM2() const;
    /* 1.11.60 (audit round 4, C009): true when the last getInterfaceAreaMM2()
     * returned 0.0 because CACTI-IO REFUSED to extrapolate its area
     * polynomial above 3162 MHz -- as distinct from returning 0.0 because the
     * technology has no exact parameter set. GDDR6 is the only live case (its
     * bus runs at 7000 MHz while its electricals ARE sourced), and the caller
     * that consumes the area gates on `> 0.0`, so without this the run said
     * nothing at all about a missing term. The accessor is meaningful only
     * after a getInterfaceAreaMM2() call. */
    bool interfaceAreaWithheld() const { return io_area_withheld_; }
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

    /* 1.11.61 (rulings R2/R4): the organisation of the Ramulator org preset
     * this wrapper names -- the part whose cycles the timing model counts.
     * Resolved from dram_type_ and the configured device width; always valid
     * for the seven DRAM technologies PIMID supports (an unrecognised type
     * falls back to DDR4_8Gb_x8, the same substitution the architecture object
     * makes and announces). */
    const PresetOrganization& getPresetOrganization() const { return preset_org_; }
    // Rows per bank of the simulated preset. 0 only if the preset is unknown.
    uint64_t getPresetRowsPerBank() const { return preset_org_.rows_per_bank; }
    /* The preset's DEVICE capacity in MB: the device itself for the DDR
     * family, the whole STACK (per-channel density x channels) for HBM. */
    uint64_t getPresetDeviceCapacityMB() const;
    /* HBM only: the CORE DIES the preset stacks, at two channels per die --
     * the same 2-channels-per-die relation memorySystemDieCount() uses in
     * main.cpp. 0 for the DDR family, which has no die stacking. */
    int getPresetDiesPerStack() const;

    // Internal port bitwidths (critical for PIM bandwidth!)
    // 1.11.60 (one fabric): the rung's upstream reference, verbatim from the
    // architecture object's VerifiedValue -- status + citation.
    std::string getLadderRungProvenance(int rung) const;
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
    /* 1.11.60 (audit round 4, C009): the withheld-area record and its
     * say-it-once latch. See getInterfaceAreaMM2(). */
    mutable bool io_area_withheld_ = false;
    mutable bool warned_io_area_withheld_ = false;
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
    bool anchor_quiet_ = false;   // 1.11.61 (D4): reference anchor, no provenance warnings
    double row_miss_frac_ = -1.0;   // 1.11.52 (D003): measured; <0 = unmeasured
    /* 1.11.46 (L181): device width for the whole-rank array-energy basis.
     * Empty = x8 default (the same convention backgroundUnits uses). */
    std::string device_width_;
    /* 1.11.59 (audit C018): which device width is currently STAMPED on
     * dram_arch_, in bits. 0 = the factory object's own organization. It is
     * reset to 0 wherever a fresh architecture object is built, so applying a
     * width twice is a no-op and applying it to a rebuilt object is not. */
    int arch_device_width_bits_ = 0;
    /* 1.11.61 (rulings R1/R2/R4): the simulated org preset's organisation.
     * Resolved in initialize() (and by parseConfiguration on the default
     * path) before anything reads a density or a row count off it. */
    PresetOrganization preset_org_;
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
    // 1.11.59 (audit C018): stamp the configured JEDEC device width onto the
    // architecture object (chip DQ pins, devices per rank, and the JEDEC
    // organization coupled to them). No-op when no width is configured.
    void applyDeviceWidthToArchitecture();
    /* 1.11.61 (rulings R1/R2/R4): resolve preset_org_ from dram_type_ and the
     * configured device width. Idempotent; safe to call again after the width
     * changes. */
    void resolvePresetOrganization();
    /* 1.11.61 (ruling R1): stamp the SIMULATED PRESET's density onto the
     * architecture object -- chip_size_mb and bank_size_mb. DDR family only;
     * HBM keeps the core-die semantics ruling R2 fixed. */
    void applyPresetDensityToArchitecture();
};

} // namespace pimid

#endif // PIMID_RAMULATOR_WRAPPER_H
