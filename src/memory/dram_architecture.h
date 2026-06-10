/**
 * @file dram_architecture.h
 * @brief Comprehensive DRAM Architecture Specifications
 *
 * This file contains detailed architectural parameters for various DRAM technologies
 * including DDR4, DDR5, HBM2, HBM3, with focus on INTERNAL port bitwidths which
 * are critical for PIM performance modeling.
 *
 * CRITICAL INSIGHT:
 * Inside DRAM chips, banks and bank groups have NARROW internal ports (8-16 bits)!
 * Only at RANK/CHANNEL level do we get WIDE interfaces (64+ bits).
 * This fundamentally affects optimal PIM granularity.
 *
 * SCALABILITY:
 * All bitwidths can be scaled via multipliers for hypothetical architecture studies.
 */

#ifndef PIMID_DRAM_ARCHITECTURE_H
#define PIMID_DRAM_ARCHITECTURE_H

#include <string>
#include <memory>
#include <vector>
#include <map>

namespace pimid {
namespace memory {

//=============================================================================
// DRAM Hierarchy Port Bitwidths (CRITICAL for PIM!)
//=============================================================================

/**
 * @brief Internal DRAM port bitwidths at each hierarchy level
 *
 * IMPORTANT: These are INTERNAL ports, not external package pins!
 * Banks have very narrow internal ports (8-16 bits typical).
 */
struct DRAMPortBitwidths {
    // Fine-grained internal ports (NARROW!)
    int subarray_port_bits;       // Typically N/A or same as bank (shared)
    int bank_port_bits;           // 8-16 bits for DDR4/5, 32-64 bits for HBM
    int bank_group_port_bits;     // 16-32 bits for DDR4/5, 64-128 bits for HBM

    // Coarse-grained internal ports
    int chip_internal_bits;       // Internal chip routing (16-32 bits DDR, 128-256 HBM)

    // External I/O (WIDE interfaces)
    int chip_io_bits;             // External chip pins (x4/x8/x16 for DDR, 1024+ for HBM)
    int rank_data_bits;           // Rank-level data bus (64-bit typical for DDR)
    int channel_data_bits;        // Memory channel width (64-128 bits)

    // Scalability factor (for hypothetical studies)
    double port_width_scale;      // Multiply all ports by this factor (default: 1.0)

    DRAMPortBitwidths() : port_width_scale(1.0) {}

    // Apply scaling factor to all ports
    void applyScaling() {
        if (port_width_scale != 1.0) {
            subarray_port_bits = static_cast<int>(subarray_port_bits * port_width_scale);
            bank_port_bits = static_cast<int>(bank_port_bits * port_width_scale);
            bank_group_port_bits = static_cast<int>(bank_group_port_bits * port_width_scale);
            chip_internal_bits = static_cast<int>(chip_internal_bits * port_width_scale);
            chip_io_bits = static_cast<int>(chip_io_bits * port_width_scale);
            rank_data_bits = static_cast<int>(rank_data_bits * port_width_scale);
            channel_data_bits = static_cast<int>(channel_data_bits * port_width_scale);
        }
    }
};

//=============================================================================
// DRAM Physical Organization
//=============================================================================

struct DRAMOrganization {
    // Hierarchy depth
    int subarrays_per_bank;
    int banks_per_bank_group;
    int bank_groups_per_chip;
    int chips_per_rank;
    int ranks_per_channel;
    int channels_per_mc;

    // Capacity at each level (in KB)
    size_t subarray_size_kb;
    size_t bank_size_mb;
    size_t chip_size_mb;
    size_t rank_size_gb;
    size_t channel_capacity_gb;
};

//=============================================================================
// DRAM Timing Parameters
//=============================================================================

struct DRAMTiming {
    // Clock frequency
    double clock_freq_mhz;        // Core clock frequency (e.g., 1200 MHz for DDR4-2400)
    double data_rate_mtps;        // Data rate (MT/s) - 2x clock for DDR

    // Basic timing (in nanoseconds)
    double tRCD_ns;               // RAS to CAS delay
    double tCAS_ns;               // CAS latency (CL)
    double tRP_ns;                // Row precharge time
    double tRAS_ns;               // Row active time
    double tBurst_ns;             // Burst transfer time

    // Bank group timing
    double tCCD_L_ns;             // CAS to CAS delay (long, different bank group)
    double tCCD_S_ns;             // CAS to CAS delay (short, same bank group)

    // Refresh
    double tREFI_ns;              // Refresh interval
    double tRFC_ns;               // Refresh cycle time

    // Hierarchical access latencies (total latency from compute to data)
    double subarray_access_ns;    // Access within subarray (tRCD + tCAS)
    double bank_access_ns;        // Cross-subarray within bank (tRP + tRCD + tCAS)
    double bank_group_access_ns;  // Cross-bank within bank group
    double chip_access_ns;        // Cross-bank-group within chip
    double rank_access_ns;        // Rank switching overhead
    double channel_access_ns;     // Channel/MC access latency
};

//=============================================================================
// DRAM Energy Parameters (per byte transferred)
//=============================================================================

struct DRAMEnergy {
    // Hierarchical data movement energy (pJ per byte)
    double subarray_energy_pJ;    // Sense amplifier energy
    double bank_energy_pJ;        // + Bank switching
    double bank_group_energy_pJ;  // + Bank group multiplexing
    double chip_energy_pJ;        // + On-chip routing
    double rank_energy_pJ;        // + Rank selection + I/O drivers
    double channel_energy_pJ;     // + Memory controller

    // Background power (mW)
    double active_standby_power_mw;
    double precharge_standby_power_mw;

    // Scalability factor
    double energy_scale;          // Scale all energy values (default: 1.0)

    DRAMEnergy() : energy_scale(1.0) {}

    void applyScaling() {
        if (energy_scale != 1.0) {
            subarray_energy_pJ *= energy_scale;
            bank_energy_pJ *= energy_scale;
            bank_group_energy_pJ *= energy_scale;
            chip_energy_pJ *= energy_scale;
            rank_energy_pJ *= energy_scale;
            channel_energy_pJ *= energy_scale;
        }
    }
};

//=============================================================================
// Complete DRAM Architecture Specification
//=============================================================================

class DRAMArchitecture {
public:
    std::string name;
    std::string technology;       // "DDR4", "DDR5", "HBM2", "HBM3", "LPDDR5", etc.

    DRAMPortBitwidths ports;
    DRAMOrganization organization;
    DRAMTiming timing;
    DRAMEnergy energy;

    // Constructor
    DRAMArchitecture(const std::string& name, const std::string& tech)
        : name(name), technology(tech) {}

    // Calculate bandwidth at each level (GB/s)
    double getSubarrayBandwidth() const {
        return (ports.subarray_port_bits / 8.0) * (timing.clock_freq_mhz / 1000.0) * ports.port_width_scale;
    }

    double getBankBandwidth() const {
        return (ports.bank_port_bits / 8.0) * (timing.clock_freq_mhz / 1000.0) * ports.port_width_scale;
    }

    double getBankGroupBandwidth() const {
        return (ports.bank_group_port_bits / 8.0) * (timing.clock_freq_mhz / 1000.0) * ports.port_width_scale;
    }

    double getChipIOBandwidth() const {
        return (ports.chip_io_bits / 8.0) * (timing.data_rate_mtps / 1000.0) * ports.port_width_scale;
    }

    double getRankBandwidth() const {
        return (ports.rank_data_bits / 8.0) * (timing.data_rate_mtps / 1000.0) * ports.port_width_scale;
    }

    double getChannelBandwidth() const {
        return (ports.channel_data_bits / 8.0) * (timing.data_rate_mtps / 1000.0) * ports.port_width_scale;
    }

    // Apply all scaling factors
    void applyScaling() {
        ports.applyScaling();
        energy.applyScaling();
    }

    // Print architecture summary
    void printSummary() const;
};

//=============================================================================
// Predefined DRAM Architecture Configurations
//=============================================================================

/**
 * @brief DDR4-2400 (JEDEC standard)
 *
 * CRITICAL: Banks have only 8-bit internal ports!
 * Wide 64-bit interface only at rank level.
 */
inline std::unique_ptr<DRAMArchitecture> createDDR4_2400() {
    auto arch = std::make_unique<DRAMArchitecture>("DDR4-2400", "DDR4");

    // Port bitwidths (THE CRITICAL PART!)
    arch->ports.subarray_port_bits = 8;       // Shared with bank port
    arch->ports.bank_port_bits = 8;           // 8-bit internal bank port (NARROW!)
    arch->ports.bank_group_port_bits = 16;    // 16-bit bank group (still narrow)
    arch->ports.chip_internal_bits = 16;      // Internal chip routing
    arch->ports.chip_io_bits = 8;             // x8 device (1 byte per cycle)
    arch->ports.rank_data_bits = 64;          // 8 chips × 8 bits = 64-bit WIDE interface
    arch->ports.channel_data_bits = 64;       // Single channel 64-bit

    // Organization
    arch->organization.subarrays_per_bank = 4;
    arch->organization.banks_per_bank_group = 4;
    arch->organization.bank_groups_per_chip = 4;
    arch->organization.chips_per_rank = 8;    // x8 organization
    arch->organization.ranks_per_channel = 2;
    arch->organization.channels_per_mc = 1;
    arch->organization.subarray_size_kb = 512;
    arch->organization.bank_size_mb = 2;      // 4 subarrays × 512 KB
    arch->organization.chip_size_mb = 128;    // 16 banks × 2 MB
    arch->organization.rank_size_gb = 1;      // 8 chips × 128 MB
    arch->organization.channel_capacity_gb = 2; // 2 ranks × 1 GB

    // Timing (DDR4-2400, CL17)
    arch->timing.clock_freq_mhz = 1200;
    arch->timing.data_rate_mtps = 2400;
    arch->timing.tRCD_ns = 13.32;
    arch->timing.tCAS_ns = 13.32;             // CL17
    arch->timing.tRP_ns = 13.32;
    arch->timing.tRAS_ns = 32.0;
    arch->timing.tBurst_ns = 3.33;            // 8-beat burst
    arch->timing.tCCD_L_ns = 5.0;
    arch->timing.tCCD_S_ns = 2.5;
    arch->timing.tREFI_ns = 7800;
    arch->timing.tRFC_ns = 350;

    // Hierarchical latencies
    arch->timing.subarray_access_ns = 26.64;  // tRCD + tCAS
    arch->timing.bank_access_ns = 39.96;      // tRP + tRCD + tCAS
    arch->timing.bank_group_access_ns = 50.0;
    arch->timing.chip_access_ns = 60.0;
    arch->timing.rank_access_ns = 80.0;
    arch->timing.channel_access_ns = 100.0;

    // Energy (based on Micron DDR4 datasheet)
    arch->energy.subarray_energy_pJ = 1.0;
    arch->energy.bank_energy_pJ = 2.0;
    arch->energy.bank_group_energy_pJ = 3.0;
    arch->energy.chip_energy_pJ = 5.0;
    arch->energy.rank_energy_pJ = 10.0;
    arch->energy.channel_energy_pJ = 15.0;
    arch->energy.active_standby_power_mw = 50.0;
    arch->energy.precharge_standby_power_mw = 30.0;

    return arch;
}

/**
 * @brief DDR5-4800 (JEDEC standard)
 *
 * DDR5 has independent sub-channels, but banks still have narrow internal ports!
 */
inline std::unique_ptr<DRAMArchitecture> createDDR5_4800() {
    auto arch = std::make_unique<DRAMArchitecture>("DDR5-4800", "DDR5");

    // Port bitwidths
    arch->ports.subarray_port_bits = 16;      // Slightly wider than DDR4
    arch->ports.bank_port_bits = 16;          // 16-bit internal bank port (still narrow!)
    arch->ports.bank_group_port_bits = 32;    // 32-bit bank group
    arch->ports.chip_internal_bits = 32;      // Wider internal routing
    arch->ports.chip_io_bits = 8;             // x8 device (but 2 sub-channels)
    arch->ports.rank_data_bits = 64;          // 2 sub-channels × 32 bits
    arch->ports.channel_data_bits = 64;       // 64-bit channel (2 × 32-bit sub-channels)

    // Organization (DDR5 has 2 independent sub-channels per chip)
    arch->organization.subarrays_per_bank = 4;
    arch->organization.banks_per_bank_group = 4;
    arch->organization.bank_groups_per_chip = 8; // More bank groups
    arch->organization.chips_per_rank = 8;
    arch->organization.ranks_per_channel = 2;
    arch->organization.channels_per_mc = 1;
    arch->organization.subarray_size_kb = 512;
    arch->organization.bank_size_mb = 2;
    arch->organization.chip_size_mb = 256;    // 32 banks × 2 MB
    arch->organization.rank_size_gb = 2;
    arch->organization.channel_capacity_gb = 4;

    // Timing (DDR5-4800)
    arch->timing.clock_freq_mhz = 2400;
    arch->timing.data_rate_mtps = 4800;
    arch->timing.tRCD_ns = 13.75;
    arch->timing.tCAS_ns = 13.75;             // CL40
    arch->timing.tRP_ns = 13.75;
    arch->timing.tRAS_ns = 32.0;
    arch->timing.tBurst_ns = 1.67;            // 16-beat burst
    arch->timing.tCCD_L_ns = 4.0;
    arch->timing.tCCD_S_ns = 2.0;
    arch->timing.tREFI_ns = 3900;             // 2x refresh rate
    arch->timing.tRFC_ns = 295;

    // Hierarchical latencies (similar to DDR4)
    arch->timing.subarray_access_ns = 27.5;
    arch->timing.bank_access_ns = 41.25;
    arch->timing.bank_group_access_ns = 50.0;
    arch->timing.chip_access_ns = 60.0;
    arch->timing.rank_access_ns = 75.0;
    arch->timing.channel_access_ns = 90.0;

    // Energy (DDR5 is more energy efficient per bit)
    arch->energy.subarray_energy_pJ = 0.8;
    arch->energy.bank_energy_pJ = 1.6;
    arch->energy.bank_group_energy_pJ = 2.5;
    arch->energy.chip_energy_pJ = 4.0;
    arch->energy.rank_energy_pJ = 8.0;
    arch->energy.channel_energy_pJ = 12.0;
    arch->energy.active_standby_power_mw = 45.0;
    arch->energy.precharge_standby_power_mw = 25.0;

    return arch;
}

/**
 * @brief HBM2 (High Bandwidth Memory)
 *
 * CRITICAL DIFFERENCE: HBM has MUCH WIDER internal ports!
 * Through-silicon vias (TSVs) provide wide paths between layers.
 * This is why HBM-PIM can work at finer granularities than DDR-PIM!
 */
inline std::unique_ptr<DRAMArchitecture> createHBM2() {
    auto arch = std::make_unique<DRAMArchitecture>("HBM2", "HBM2");

    // Port bitwidths (MUCH WIDER than DDR!)
    arch->ports.subarray_port_bits = 32;      // Wider due to TSV
    arch->ports.bank_port_bits = 64;          // 64-bit bank port (8x wider than DDR4!)
    arch->ports.bank_group_port_bits = 128;   // 128-bit bank group (8x wider!)
    arch->ports.chip_internal_bits = 256;     // Very wide internal paths via TSV
    arch->ports.chip_io_bits = 1024;          // 1024-bit I/O (128 bytes per cycle!)
    arch->ports.rank_data_bits = 1024;        // Single HBM stack = 1024-bit
    arch->ports.channel_data_bits = 1024;     // 1024-bit channel

    // Organization (HBM is organized as stacks with TSVs)
    arch->organization.subarrays_per_bank = 4;
    arch->organization.banks_per_bank_group = 4;
    arch->organization.bank_groups_per_chip = 4; // Per channel
    arch->organization.chips_per_rank = 1;    // Stacked dies, not chips
    arch->organization.ranks_per_channel = 8; // 8 channels per stack
    arch->organization.channels_per_mc = 8;   // 8 pseudo-channels
    arch->organization.subarray_size_kb = 512;
    arch->organization.bank_size_mb = 2;
    arch->organization.chip_size_mb = 1024;   // 1 GB per stack
    arch->organization.rank_size_gb = 1;
    arch->organization.channel_capacity_gb = 8;

    // Timing (HBM2, lower latency than DDR)
    arch->timing.clock_freq_mhz = 1000;
    arch->timing.data_rate_mtps = 2000;
    arch->timing.tRCD_ns = 12.5;
    arch->timing.tCAS_ns = 12.5;
    arch->timing.tRP_ns = 12.5;
    arch->timing.tRAS_ns = 28.0;
    arch->timing.tBurst_ns = 2.0;
    arch->timing.tCCD_L_ns = 3.0;
    arch->timing.tCCD_S_ns = 2.0;
    arch->timing.tREFI_ns = 3900;
    arch->timing.tRFC_ns = 260;

    // Hierarchical latencies (MUCH LOWER due to TSV!)
    arch->timing.subarray_access_ns = 25.0;
    arch->timing.bank_access_ns = 37.5;
    arch->timing.bank_group_access_ns = 40.0; // Very low overhead
    arch->timing.chip_access_ns = 45.0;       // TSV is fast!
    arch->timing.rank_access_ns = 50.0;
    arch->timing.channel_access_ns = 60.0;

    // Energy (HBM is more efficient due to TSV, shorter distances)
    arch->energy.subarray_energy_pJ = 0.5;
    arch->energy.bank_energy_pJ = 1.0;
    arch->energy.bank_group_energy_pJ = 1.5;
    arch->energy.chip_energy_pJ = 2.0;        // TSV is very efficient
    arch->energy.rank_energy_pJ = 3.0;
    arch->energy.channel_energy_pJ = 5.0;
    arch->energy.active_standby_power_mw = 100.0; // Higher due to many banks active
    arch->energy.precharge_standby_power_mw = 50.0;

    return arch;
}

/**
 * @brief HBM3 (Latest generation)
 *
 * Even wider and faster than HBM2!
 */
inline std::unique_ptr<DRAMArchitecture> createHBM3() {
    auto arch = std::make_unique<DRAMArchitecture>("HBM3", "HBM3");

    // Port bitwidths (Even wider than HBM2!)
    arch->ports.subarray_port_bits = 64;      // 2x HBM2
    arch->ports.bank_port_bits = 128;         // 128-bit bank port
    arch->ports.bank_group_port_bits = 256;   // 256-bit bank group
    arch->ports.chip_internal_bits = 512;     // Very wide TSV paths
    arch->ports.chip_io_bits = 1024;          // 1024-bit I/O
    arch->ports.rank_data_bits = 1024;        // 1024-bit per stack
    arch->ports.channel_data_bits = 1024;     // 1024-bit channel

    // Organization (similar to HBM2 but more capacity)
    arch->organization.subarrays_per_bank = 4;
    arch->organization.banks_per_bank_group = 4;
    arch->organization.bank_groups_per_chip = 4;
    arch->organization.chips_per_rank = 1;
    arch->organization.ranks_per_channel = 16; // 16 channels per stack
    arch->organization.channels_per_mc = 16;
    arch->organization.subarray_size_kb = 1024; // Larger subarrays
    arch->organization.bank_size_mb = 4;
    arch->organization.chip_size_mb = 2048;   // 2 GB per stack
    arch->organization.rank_size_gb = 2;
    arch->organization.channel_capacity_gb = 32;

    // Timing (HBM3, faster than HBM2)
    arch->timing.clock_freq_mhz = 1875;       // Higher frequency
    arch->timing.data_rate_mtps = 6400;       // 6.4 GT/s
    arch->timing.tRCD_ns = 10.0;              // Lower latency
    arch->timing.tCAS_ns = 10.0;
    arch->timing.tRP_ns = 10.0;
    arch->timing.tRAS_ns = 24.0;
    arch->timing.tBurst_ns = 1.25;
    arch->timing.tCCD_L_ns = 2.5;
    arch->timing.tCCD_S_ns = 1.5;
    arch->timing.tREFI_ns = 3900;
    arch->timing.tRFC_ns = 240;

    // Hierarchical latencies
    arch->timing.subarray_access_ns = 20.0;
    arch->timing.bank_access_ns = 30.0;
    arch->timing.bank_group_access_ns = 35.0;
    arch->timing.chip_access_ns = 40.0;
    arch->timing.rank_access_ns = 45.0;
    arch->timing.channel_access_ns = 55.0;

    // Energy (improved efficiency)
    arch->energy.subarray_energy_pJ = 0.4;
    arch->energy.bank_energy_pJ = 0.8;
    arch->energy.bank_group_energy_pJ = 1.2;
    arch->energy.chip_energy_pJ = 1.8;
    arch->energy.rank_energy_pJ = 2.5;
    arch->energy.channel_energy_pJ = 4.0;
    arch->energy.active_standby_power_mw = 120.0;
    arch->energy.precharge_standby_power_mw = 60.0;

    return arch;
}

//=============================================================================
// Factory for creating DRAM architectures
//=============================================================================

class DRAMArchitectureFactory {
public:
    static std::unique_ptr<DRAMArchitecture> create(const std::string& type) {
        if (type == "DDR4-2400" || type == "DDR4") {
            return createDDR4_2400();
        } else if (type == "DDR5-4800" || type == "DDR5") {
            return createDDR5_4800();
        } else if (type == "HBM2") {
            return createHBM2();
        } else if (type == "HBM3") {
            return createHBM3();
        } else {
            // Default to DDR4
            return createDDR4_2400();
        }
    }

    static std::vector<std::string> availableTypes() {
        return {"DDR4-2400", "DDR5-4800", "HBM2", "HBM3"};
    }
};

} // namespace memory
} // namespace pimid

#endif // PIMID_DRAM_ARCHITECTURE_H
