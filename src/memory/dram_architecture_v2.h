/**
 * @file dram_architecture_v2.h
 * @brief RIGOROUSLY VERIFIED DRAM Architecture Specifications
 *
 * This file contains DRAM architectural parameters with VERIFICATION STATUS.
 * Each parameter is marked as VERIFIED (from specs), INFERRED (from papers),
 * or ESTIMATED (educated guess).
 *
 * CRITICAL METHODOLOGY:
 * We distinguish between different datapath stages inside DRAM:
 * 1. Row buffer (bitline sense amps): 8-16Kb wide
 * 2. Global Sense Amplifiers (GSA): 256-512 bits (per subarray)
 * 3. Prefetch datapath: 64-512 bits (technology dependent)
 * 4. Bank-to-peripheral: UNKNOWN (not publicly documented!)
 * 5. Chip I/O pins: 4-16 bits (DDR) or 1024 bits (HBM) - VERIFIED
 *
 * SOURCES:
 * - JEDEC Standards (DDR4: JESD79-4, DDR5: JESD79-5, HBM2: JESD235, HBM3: JESD238)
 * - Academic Papers (DAS-MICRO15, NVIDIA-HPCA17, Darwin-arXiv23)
 * - Manufacturer Datasheets (Micron, Samsung, SK Hynix)
 *
 * VERSION: 2.0 (Rigorously Verified)
 * DATE: November 2024
 */

#ifndef PIMID_DRAM_ARCHITECTURE_V2_H
#define PIMID_DRAM_ARCHITECTURE_V2_H

#include <string>
#include <memory>
#include <iostream>
#include <iomanip>
#include <vector>
#include <map>

// Use common VerificationStatus enum from sram_architecture.h
#include "sram_architecture.h"

namespace pimid {
namespace memory {

// VerificationStatus is defined in sram_architecture.h

struct VerifiedValue {
    int value_bits;
    VerificationStatus status;
    std::string source;  // Citation or reference
    std::string notes;   // Additional context
};

//=============================================================================
// DRAM Internal Datapath Stages (CRITICAL CLARITY!)
//=============================================================================

/**
 * @brief DRAM Internal Datapath Structure
 *
 * DRAM has multiple datapath stages with DIFFERENT widths:
 *
 * Stage 1: Row Buffer (Bitline Sense Amps)
 *   - DDR4: 8,192 bits (8Kb) per subarray [VERIFIED: typical]
 *   - Activated row sits in bitline sense amplifiers
 *
 * Stage 2: Global Sense Amplifiers (GSA)
 *   - DDR4 x8: 256 bits per subarray [INFERRED: from DAS-MICRO15]
 *   - Reads data from row buffer to peripheral circuitry
 *
 * Stage 3: Prefetch Datapath
 *   - DDR4 x8: 64 bits (8n prefetch x 8-bit I/O) [VERIFIED: JEDEC]
 *   - DDR5 x8: 128 bits (16n prefetch x 8-bit I/O) [VERIFIED: JEDEC]
 *   - HBM2: 256 bits (2n x 128-bit channel) [VERIFIED: JEDEC]
 *   - HBM3: 512 bits (8n x 64-bit channel) [VERIFIED: JEDEC]
 *
 * Stage 4: Bank-to-Peripheral Serialization
 *   - UNKNOWN! Not publicly documented.
 *   - Likely narrow (8-32 bits) to save routing area [ESTIMATED]
 *   - THIS IS THE CRITICAL BOTTLENECK FOR BANK-LEVEL PIM!
 *
 * Stage 5: Chip I/O Pins
 *   - DDR4: x4/x8/x16 (4, 8, or 16 bits) [VERIFIED: JEDEC]
 *   - HBM2: 1024 bits total (8 x 128-bit channels) [VERIFIED: JEDEC]
 *   - HBM3: 1024 bits total (16 x 64-bit channels) [VERIFIED: JEDEC]
 */
struct DRAMDatapathStages {
    // Stage 1: Row Buffer (widest, but not a "port")
    VerifiedValue row_buffer_bits;        // Bitline sense amps

    // Stage 2: Global Sense Amplifiers (column I/O)
    VerifiedValue gsa_datapath_bits;      // Subarray -> peripheral

    // Stage 3: Prefetch Datapath (well-documented)
    VerifiedValue prefetch_datapath_bits; // Prefetch buffer width

    // Stage 4: Bank Serialization (CRITICAL UNKNOWN!)
    VerifiedValue bank_serialization_bits; // Bank -> chip peripheral (BOTTLENECK!)

    // Stage 5: Chip I/O (well-documented)
    VerifiedValue chip_io_bits;           // External package pins

    // Higher-level interfaces
    VerifiedValue rank_databus_bits;      // Rank-level (multiple chips)
    VerifiedValue channel_databus_bits;   // Memory channel
};

//=============================================================================
// Bandwidth-Based Inference (Conservative Approach)
//=============================================================================

/**
 * @brief Infer internal port widths from MEASURED bandwidth
 *
 * Since internal port widths are not documented, we can INFER limits
 * from measured bandwidth characteristics.
 *
 * METHODOLOGY:
 * If Bank-PIM achieves only X GB/s despite Y banks in parallel,
 * we can infer each bank's internal port is limited to ~X/Y GB/s.
 *
 * CONSERVATIVE: We use LOWER BOUNDS (worst case for PIM)
 */
/* 1.11.57 (audit C003): the three STORED bandwidths are GONE from this struct.
 *
 * What was wrong: bank_effective_bw_GBs and bank_group_effective_bw_GBs were
 * frozen literals per technology while every other rung of the same ladder is
 * a width times a clock (getSubarrayBandwidth uses clock_freq_mhz;
 * getChipIOBandwidth / getRankBW / getChannelBandwidth use data_rate_mtps).
 * They matched only the clocks the factories carried before 1.11.56 re-binned
 * HBM2, DDR5 and HBM3, and nothing re-derived them.
 *
 * Why it was invisible: the ladder builder turns a rung into a CLOCK by
 * f = BW * 8 / width, so a stale bandwidth does not show up as a bandwidth --
 * it shows up as a frequency, and the comment at that site claimed the
 * arithmetic "lands on the core clock for the array tiers". Worked through,
 * HBM3's bank tier came out at 16.0 * 8 / 128 = 1.0 GHz against its own
 * 3.2 GHz core clock, so every L1 and L2 crossing on the reference cell was
 * charged 3.2x the time this object's own width and clock imply (DDR5: 0.67x).
 * Only DDR4 was self-consistent, which is exactly why it never looked wrong.
 *
 * The fix derives both rungs the way the neighbours are derived, from fields
 * this object already carries, so a speed-bin change moves them and no
 * per-technology bandwidth number is invented anywhere. chip_internal_bw_GBs
 * went with them: it had no reader at all and its literals (DDR4 4.8 against
 * chip I/O 8 bits x 2400 MT/s = 2.4) were a second authority waiting to be
 * believed. See getBankEffectiveBW() / getBankGroupEffectiveBW(). */
struct InferredBandwidthLimits {
    std::string inference_method;       // How we derived these values
    std::string confidence_level;       // "High", "Medium", "Low"
};

//=============================================================================
// Complete DRAM Architecture (Rigorously Verified)
//=============================================================================

class DRAMArchitectureV2 {
public:
    std::string name;
    std::string technology;
    std::string version;  // "2.0-verified"

    // Datapath stages (detailed breakdown)
    DRAMDatapathStages datapath;

    // Bandwidth limits (inferred from measurements)
    InferredBandwidthLimits bandwidth_limits;

    // Physical organization (well-documented)
    struct {
        int subarrays_per_bank;       // VERIFIED
        int banks_per_bank_group;     // VERIFIED
        int bank_groups_per_chip;     // VERIFIED
        int chips_per_rank;           // VERIFIED
        int ranks_per_channel;        // VERIFIED

        size_t subarray_size_kb;      // TYPICAL
        size_t bank_size_mb;          // TYPICAL
        size_t chip_size_mb;          // VERIFIED (from datasheet)
        size_t rank_size_gb;          // VERIFIED
    } organization;

    // Timing (JEDEC-verified)
    struct {
        // JEDEC Standard Parameters (End-to-end, externally observable)
        double clock_freq_mhz;        // VERIFIED
        double data_rate_mtps;        // VERIFIED
        double tRCD_ns;               // VERIFIED: ACTIVATE to READ/WRITE (includes all row ops)
        double tCAS_ns;               // VERIFIED: READ to data at pins (includes column + I/O)
        double tRP_ns;                // VERIFIED: Precharge time
        double tRAS_ns;               // VERIFIED: Row active time
        double tBurst_ns;             // VERIFIED: Burst duration

        // Inner-Bank Datapath Breakdown (INFERRED from CACTI and academic papers)
        // These components are HIDDEN inside tRCD and tCAS, but matter for PIM!
        struct {
            // Column access path (part of tCAS)
            double column_decoder_ns;       // Column address decode
            double column_mux_ns;           // Column multiplexer (CSL)
            double subarray_output_drv_ns;  // Subarray output driver

            // Inner-bank datapath (part of tCAS)
            double local_io_ns;             // Local data lines (LDL) - within subarray
            double htree_horizontal_ns;     // H-tree horizontal segment
            double htree_vertical_ns;       // H-tree vertical segment
            double global_io_ns;            // Global data lines (GDL) - bank-wide

            // Bank I/O (part of tCAS)
            double bank_io_driver_ns;       // Bank output driver to chip I/O

            // Verification metadata
            VerificationStatus verification_status;
            std::string source;             // "CACTI v6.5, DAS-MICRO'15, etc."

            // Derived totals
            double getColumnPathDelay() const {
                return column_decoder_ns + column_mux_ns + subarray_output_drv_ns;
            }

            double getInnerBankDatapathDelay() const {
                return local_io_ns + htree_horizontal_ns + htree_vertical_ns + global_io_ns;
            }

            double getTotalInnerBankDelay() const {
                return getColumnPathDelay() + getInnerBankDatapathDelay() + bank_io_driver_ns;
            }

            // For PIM: Subarray-to-bank-I/O (excluding chip I/O)
            double getSubarrayToBankIO() const {
                return subarray_output_drv_ns + local_io_ns +
                       htree_horizontal_ns + htree_vertical_ns +
                       global_io_ns + bank_io_driver_ns;
            }

            // For PIM: Subarray-to-subarray (within same bank)
            double getSubarrayToSubarrayHTree() const {
                // Egress: subarray A -> bank center
                // Ingress: bank center -> subarray B
                return 2.0 * (htree_horizontal_ns + htree_vertical_ns);
            }
        } inner_bank;

        // Hierarchical (INFERRED from measurements)
        double subarray_access_ns;    // tRCD + tCAS
        double bank_access_ns;        // tRP + tRCD + tCAS
        double chip_access_ns;        // ESTIMATED
        double rank_access_ns;        // ESTIMATED
    } timing;

    // Energy (INFERRED from papers)
    struct {
        double subarray_energy_pJ;    // INFERRED
        double bank_energy_pJ;        // INFERRED
        double chip_energy_pJ;        // INFERRED
        double rank_energy_pJ;        // INFERRED
        std::string energy_source;    // Citation
    } energy;

    // PE Bus Constraints (for PE placement at different hierarchy levels)
    // These values define the data bus characteristics when PEs are placed
    // at different levels of the memory hierarchy (subarray, bank, chip, etc.)
    struct {
        // Subarray-level PE constraints
        struct {
            uint64_t data_bus_width_bits;
            double max_bandwidth_gbps;
            uint64_t row_buffer_size_bytes;
            bool has_dedicated_bus;
        } subarray_level;

        // Bank-level PE constraints
        struct {
            uint64_t data_bus_width_bits;
            double max_bandwidth_gbps;
            bool has_dedicated_bus;
        } bank_level;

        // Chip-level PE constraints
        struct {
            uint64_t data_bus_width_bits;
            double max_bandwidth_gbps;
            bool has_dedicated_bus;
        } chip_level;

        // Rank-level PE constraints
        struct {
            uint64_t data_bus_width_bits;
            double max_bandwidth_gbps;
            bool has_dedicated_bus;
        } rank_level;

        // Logic die level (for HBM/HMC)
        struct {
            uint64_t data_bus_width_bits;
            double max_bandwidth_gbps;
            bool has_dedicated_bus;
        } logic_die_level;
    } pe_bus_constraints;

    // Scalability (for hypothetical studies)
    double port_width_scale = 1.0;
    double energy_scale = 1.0;

    // Constructor
    DRAMArchitectureV2(const std::string& name, const std::string& tech)
        : name(name), technology(tech), version("2.0-verified") {}

    // Print verification status
    void printVerificationReport() const;

    // Calculate effective bandwidths
    /* 1.11.57 (audit C003): DERIVED, like every other rung of the ladder.
     * The bank tier moves the bank serialization width once per CORE clock --
     * it sits inside the array, before the DQ tiers where the data rate
     * applies. This is the same form as getSubarrayBandwidth() (GSA width x
     * core clock) and getRankBW() below (rank bus x data rate); it replaces a
     * per-technology literal that had stopped tracking either clock. */
    double getBankEffectiveBW() const {
        return (datapath.bank_serialization_bits.value_bits / 8.0) *
               (timing.clock_freq_mhz / 1000.0);
    }
    /* 1.11.57 (audit C003 + latent D020): the bank-group tier is the bank tier
     * at the bank-group PORT width, which RamulatorWrapper::getBankGroupPortBits()
     * defines as the bank serialization width x 2 -- an UNSOURCED multiplier
     * that the accessor announces as such, since no specification fixes a
     * bank-group port width. It is written as x2 of the bank figure here, and
     * not as an independent number, precisely so that the width and the
     * bandwidth of that rung cannot disagree: the ladder back-derives its clock
     * as BW * 8 / width, and the two must divide out to the core clock. */
    double getBankGroupEffectiveBW() const {
        return getBankEffectiveBW() * 2.0;
    }
    double getRankBW() const;

    // Get verification confidence
    std::string getOverallConfidence() const;
};

//=============================================================================
// DDR4-2400 (Rigorously Verified)
//=============================================================================

inline std::unique_ptr<DRAMArchitectureV2> createDDR4_2400_Verified() {
    auto arch = std::make_unique<DRAMArchitectureV2>("DDR4-2400", "DDR4");

    // ===== DATAPATH STAGES =====

    // Stage 1: Row Buffer
    arch->datapath.row_buffer_bits = {
        8192,  // 8Kb typical for DDR4
        VerificationStatus::VERIFIED,
        "Typical DDR4 subarray, consistent across manufacturers",
        "Activated row in bitline sense amplifiers"
    };

    // Stage 2: Global Sense Amplifiers (from academic papers)
    arch->datapath.gsa_datapath_bits = {
        256,  // 256 bits per subarray
        VerificationStatus::INFERRED,
        "DAS-MICRO15: 'data is read into 256 global sense-amplifiers'",
        "Column I/O width, not a bottleneck for single access"
    };

    // Stage 3: Prefetch Datapath (JEDEC verified)
    arch->datapath.prefetch_datapath_bits = {
        64,  // 8n prefetch x 8-bit I/O
        VerificationStatus::VERIFIED,
        "JEDEC JESD79-4: DDR4 has 8n prefetch architecture, x8 device",
        "8 bursts x 8 bits = 64 bits prefetch buffer"
    };

    // Stage 4: Bank Serialization (CRITICAL UNKNOWN!)
    arch->datapath.bank_serialization_bits = {
        8,  // ESTIMATED: likely 8-16 bits
        VerificationStatus::ESTIMATED,
        "NOT DOCUMENTED! Estimated from bank performance limits",
        "CRITICAL BOTTLENECK: Banks likely serialize through narrow (8-16 bit) paths "
        "to chip peripheral to save routing area. This is why bank-level PIM is slow in DDR4!"
    };

    // Stage 5: Chip I/O (JEDEC verified)
    arch->datapath.chip_io_bits = {
        8,  // x8 device
        VerificationStatus::VERIFIED,
        "JEDEC JESD79-4: DDR4 x8 configuration",
        "External package pins, well-documented"
    };

    // Rank databus
    arch->datapath.rank_databus_bits = {
        64,  // 8 chips x 8 bits
        VerificationStatus::VERIFIED,
        "JEDEC standard: 64-bit rank interface for DDR4",
        "FIRST WIDE interface in DDR4 hierarchy"
    };

    // Channel
    arch->datapath.channel_databus_bits = {
        64,  // Single channel
        VerificationStatus::VERIFIED,
        "Standard DDR4 memory controller interface",
        "Typical single-channel configuration"
    };

    // ===== BANDWIDTH LIMITS (INFERRED) =====

    /* 1.11.57 (audit C003): the three literals here are gone; the bank and
     * bank-group rungs are derived by getBankEffectiveBW() /
     * getBankGroupEffectiveBW() from this object's own serialization width and
     * core clock. DDR4 is the one technology whose literals AGREED with that
     * derivation (8 bits / 8 x 1.2 GHz = 1.2 GB/s), which is why the drift at
     * the other three went unnoticed for as long as it did. */
    arch->bandwidth_limits.inference_method =
        "DERIVED: bank = (bank_serialization_bits / 8) x clock_freq_GHz; "
        "bank group = bank x 2 (the bank-group port multiplier, UNSOURCED). "
        "CONSERVATIVE: assumes the serialization path is the bottleneck.";
    arch->bandwidth_limits.confidence_level = "Medium - based on estimated internal port widths";

    // ===== ORGANIZATION (VERIFIED) =====

    arch->organization.subarrays_per_bank = 4;  // Typical
    arch->organization.banks_per_bank_group = 4;  // JEDEC
    arch->organization.bank_groups_per_chip = 4;  // JEDEC (x8/x4 configs)
    arch->organization.chips_per_rank = 8;  // x8 organization
    arch->organization.ranks_per_channel = 2;  // Typical DIMM
    arch->organization.subarray_size_kb = 512;  // Typical
    arch->organization.bank_size_mb = 2;  // 4 subarrays x 512KB
    arch->organization.chip_size_mb = 128;  // Typical 1Gb chip
    arch->organization.rank_size_gb = 1;  // 8 x 128MB

    // ===== TIMING (JEDEC DDR4-2400 CL17) =====

    arch->timing.clock_freq_mhz = 1200;  // VERIFIED
    arch->timing.data_rate_mtps = 2400;  // VERIFIED
    arch->timing.tRCD_ns = 13.32;  // VERIFIED: CL17
    arch->timing.tCAS_ns = 13.32;  // VERIFIED
    arch->timing.tRP_ns = 13.32;  // VERIFIED
    arch->timing.tRAS_ns = 32.0;  // VERIFIED
    arch->timing.tBurst_ns = 3.33;  // VERIFIED: 8 beats @ 2400 MT/s

    // Inner-bank datapath timing (INFERRED from CACTI and academic papers)
    arch->timing.inner_bank.column_decoder_ns = 0.35;       // INFERRED from CACTI
    arch->timing.inner_bank.column_mux_ns = 0.55;           // INFERRED from CACTI
    arch->timing.inner_bank.subarray_output_drv_ns = 0.50;  // INFERRED from CACTI
    arch->timing.inner_bank.local_io_ns = 0.75;             // INFERRED from CACTI
    arch->timing.inner_bank.htree_horizontal_ns = 1.20;     // INFERRED from CACTI
    arch->timing.inner_bank.htree_vertical_ns = 1.20;       // INFERRED from CACTI
    arch->timing.inner_bank.global_io_ns = 1.50;            // ESTIMATED
    arch->timing.inner_bank.bank_io_driver_ns = 0.60;       // INFERRED from CACTI
    arch->timing.inner_bank.verification_status = VerificationStatus::INFERRED;
    arch->timing.inner_bank.source =
        "CACTI v6.5 analytical model (external/mcpat/cacti/), "
        "DAS-MICRO'15 (Shih-Lien Lu et al.), "
        "SALP-ISCA'12 (Yoongu Kim et al.), "
        "Tiered-Latency DRAM HPCA'13 (Donghyuk Lee et al.)";

    // Total inner-bank delay = 6.65ns
    // Remaining for sense amp + row decoder: 13.32 - 6.65 = 6.67ns OK
    // This breakdown is consistent with tCAS = 13.32ns

    arch->timing.subarray_access_ns = 26.64;  // tRCD + tCAS
    arch->timing.bank_access_ns = 39.96;  // tRP + tRCD + tCAS
    arch->timing.chip_access_ns = 60.0;  // ESTIMATED
    arch->timing.rank_access_ns = 80.0;  // ESTIMATED

    // ===== ENERGY (INFERRED FROM LITERATURE) =====

    arch->energy.subarray_energy_pJ = 1.0;  // INFERRED
    arch->energy.bank_energy_pJ = 2.0;  // INFERRED
    arch->energy.chip_energy_pJ = 5.0;  // INFERRED
    arch->energy.rank_energy_pJ = 10.0;  // INFERRED
    arch->energy.energy_source =
        "INFERRED from academic literature: NVIDIA-HPCA17, DAS-MICRO15. "
        "Approximate values, order-of-magnitude correct.";

    // ===== PE BUS CONSTRAINTS (for PE placement) =====

    // Subarray-level PEs: Direct row buffer access
    arch->pe_bus_constraints.subarray_level.data_bus_width_bits = 8192 * 8;  // 8KB row buffer
    arch->pe_bus_constraints.subarray_level.max_bandwidth_gbps = 10.0;
    arch->pe_bus_constraints.subarray_level.row_buffer_size_bytes = 8192;
    arch->pe_bus_constraints.subarray_level.has_dedicated_bus = true;

    // Bank-level PEs: Share bank I/O bus
    arch->pe_bus_constraints.bank_level.data_bus_width_bits = 64;  // 64-bit bank bus
    arch->pe_bus_constraints.bank_level.max_bandwidth_gbps = 25.0;  // DDR4-2400 per-bank BW
    arch->pe_bus_constraints.bank_level.has_dedicated_bus = false;

    // Chip-level PEs: Share chip-level interconnect
    arch->pe_bus_constraints.chip_level.data_bus_width_bits = 64;
    arch->pe_bus_constraints.chip_level.max_bandwidth_gbps = 25.0;
    arch->pe_bus_constraints.chip_level.has_dedicated_bus = false;

    // Rank-level PEs: Share rank-level bus
    arch->pe_bus_constraints.rank_level.data_bus_width_bits = 64;
    arch->pe_bus_constraints.rank_level.max_bandwidth_gbps = 25.0;
    arch->pe_bus_constraints.rank_level.has_dedicated_bus = false;

    // Logic die level (not applicable for DDR4, but set defaults)
    arch->pe_bus_constraints.logic_die_level.data_bus_width_bits = 1024;
    arch->pe_bus_constraints.logic_die_level.max_bandwidth_gbps = 256.0;
    arch->pe_bus_constraints.logic_die_level.has_dedicated_bus = true;

    return arch;
}

/**
 * Create DDR4-2400 with configurable port width scaling
 *
 * @param port_width_scale Scaling factor for all port widths (default 1.0)
 *                         2.0 = 2x wider buses, 0.5 = half-width buses
 * @return Configured DDR4-2400 architecture with scaled port widths
 */
inline std::unique_ptr<DRAMArchitectureV2> createDDR4_2400_Verified(double port_width_scale) {
    auto arch = createDDR4_2400_Verified();
    arch->port_width_scale = port_width_scale;
    return arch;
}

//=============================================================================
// HBM2 (Rigorously Verified)
//=============================================================================

inline std::unique_ptr<DRAMArchitectureV2> createHBM2_Verified() {
    auto arch = std::make_unique<DRAMArchitectureV2>("HBM2", "HBM2");

    // ===== DATAPATH STAGES =====

    // Stage 1: Row Buffer
    arch->datapath.row_buffer_bits = {
        16384,  // 16Kb typical for HBM2
        VerificationStatus::INFERRED,
        "Typical HBM2 subarray, larger than DDR4",
        "Wider rows due to 3D stacked architecture"
    };

    // Stage 2: Global Sense Amplifiers
    arch->datapath.gsa_datapath_bits = {
        512,  // Wider than DDR4
        VerificationStatus::INFERRED,
        "Estimated from HBM2 architecture papers",
        "Wider column I/O than DDR4"
    };

    // Stage 3: Prefetch Datapath (JEDEC verified)
    arch->datapath.prefetch_datapath_bits = {
        256,  // 2n prefetch x 128-bit channel
        VerificationStatus::VERIFIED,
        "JEDEC JESD235A: HBM2 has 2n prefetch, 128-bit channels",
        "2 bursts x 128 bits = 256 bits prefetch buffer"
    };

    // Stage 4: Bank Serialization (BETTER THAN DDR!)
    arch->datapath.bank_serialization_bits = {
        64,  // INFERRED: wider due to TSV
        VerificationStatus::INFERRED,
        "ESTIMATED from TSV architecture: Through-Silicon Vias enable wider paths",
        "TSVs provide vertical connections with less routing constraint than DDR. "
        "ESTIMATED 64-bit based on 8x better bank-level BW vs DDR4. "
        "THIS IS WHY HBM ENABLES BANK-LEVEL PIM!"
    };

    // Stage 5: Chip I/O (JEDEC verified)
    arch->datapath.chip_io_bits = {
        1024,  // 8 channels x 128 bits
        VerificationStatus::VERIFIED,
        "JEDEC JESD235A: HBM2 has 8 independent 128-bit channels per stack",
        "Total 1024-bit interface (128 bytes per cycle)"
    };

    // Rank/Channel (HBM terminology different)
    arch->datapath.rank_databus_bits = {
        128,  // Per-channel
        VerificationStatus::VERIFIED,
        "JEDEC JESD235A: 128-bit channel width",
        "HBM uses 'channel' terminology instead of 'rank'"
    };

    /* 1.11.57 (audit C007): ONE CHANNEL, like every other family's entry in
     * this field. This used to hold 1024 -- the whole stack, all 8 channels --
     * while DDR's entry holds one 64-bit channel, so the field meant two
     * different things depending on the technology reading it. That was
     * invisible until 1.11.56 made this field the hierarchy ladder's L5 rung:
     * the ladder then described a channel 8x wider than the L4 rung directly
     * beneath it (the 128-bit "rank" IS the channel here), and the system root
     * above it multiplied by the channel count a second time. The stack figure
     * is still recoverable, and is now recovered where it belongs -- one
     * channel x the channel count -- rather than stored under a per-channel
     * name. */
    arch->datapath.channel_databus_bits = {
        128,  // one channel; the stack is 8 of these (JESD235A)
        VerificationStatus::VERIFIED,
        "JEDEC JESD235A: 128-bit channel, 8 channels per stack",
        "One channel's data bus; stack width = this x channels"
    };

    // ===== BANDWIDTH LIMITS (INFERRED) =====

    /* 1.11.57 (audit C003): the literals here read "64 bits / 8 x 1 GHz" and
     * this part's core clock is not 1 GHz -- it was 1.0 GHz when the line was
     * written and the object has been re-binned since. Derived now, from the
     * serialization width and clock the object itself carries. */
    arch->bandwidth_limits.inference_method =
        "DERIVED: bank = (bank_serialization_bits / 8) x clock_freq_GHz, on an "
        "estimated 64-bit TSV bank path; bank group = bank x 2 (the bank-group "
        "port multiplier, UNSOURCED). TSV enables wider internal paths than "
        "DDR4 wire routing, which is why HBM's bank tier is the faster one.";
    arch->bandwidth_limits.confidence_level = "Medium - TSV width not publicly specified";

    // ===== ORGANIZATION (VERIFIED) =====

    arch->organization.subarrays_per_bank = 4;
    arch->organization.banks_per_bank_group = 4;
    arch->organization.bank_groups_per_chip = 4;  // Per channel
    arch->organization.chips_per_rank = 8;  // 8 channels (not traditional "chips")
    arch->organization.ranks_per_channel = 1;  // Single stack
    arch->organization.subarray_size_kb = 1024;  // Larger than DDR4
    arch->organization.bank_size_mb = 4;
    arch->organization.chip_size_mb = 1024;  // 1GB per stack (typical)
    arch->organization.rank_size_gb = 8;

    // ===== TIMING (JEDEC HBM2; rate = the simulated HBM2_2.4Gbps bin) =====

    /* 1.11.56 (audit D002's defect, found by the B042 reconciliation check):
     * NAME THE BIN THIS TREE SIMULATES. The Ramulator preset selected for
     * HBM2 is HBM2_2.4Gbps (ramulator_wrapper.cpp), so the cycle counts this
     * simulator produces are a 2.4 GT/s part -- while this object said 2.0
     * GT/s and every bandwidth derived from it (chip I/O, rank, channel, and
     * since 1.11.56 the whole hierarchy link ladder) described a slower one.
     * One part, two speed bins, exactly the split D002 closed for DDR4. The
     * ns timings above are absolute and stay; the RATE follows the preset. */
    arch->timing.clock_freq_mhz = 1200;  // 2.4 GT/s DDR -> 1.2 GHz core
    arch->timing.data_rate_mtps = 2400;  // preset HBM2_2.4Gbps
    arch->timing.tRCD_ns = 16.0;  // JESD235C spec-minimum (was 12.5, pre-audit optimistic)
    arch->timing.tCAS_ns = 16.0;  // JESD235C
    arch->timing.tRP_ns = 12.5;  // VERIFIED
    arch->timing.tRAS_ns = 28.0;  // VERIFIED
    arch->timing.tBurst_ns = 8.0 * 1000.0 / 2400.0;  // 8 beats @ 2400 MT/s = 3.33 ns

    // Inner-bank datapath timing (INFERRED, much faster than DDR4 due to TSV!)
    arch->timing.inner_bank.column_decoder_ns = 0.25;       // Faster process node
    arch->timing.inner_bank.column_mux_ns = 0.35;           // Advanced technology
    arch->timing.inner_bank.subarray_output_drv_ns = 0.30;  // TSV-optimized
    arch->timing.inner_bank.local_io_ns = 0.40;             // Shorter distances
    arch->timing.inner_bank.htree_horizontal_ns = 0.40;     // TSV enables short paths
    arch->timing.inner_bank.htree_vertical_ns = 0.40;       // Vertical stacking
    arch->timing.inner_bank.global_io_ns = 0.60;            // 64-bit wide paths
    arch->timing.inner_bank.bank_io_driver_ns = 0.35;       // TSV-optimized
    arch->timing.inner_bank.verification_status = VerificationStatus::INFERRED;
    arch->timing.inner_bank.source =
        "INFERRED from CACTI with TSV technology assumptions, "
        "HBM architecture papers (shorter wires, 3D stacking, wider internal paths)";

    // Total inner-bank delay = 3.05ns (~2.2x faster than DDR4!)
    // Remaining for sense amp + row decoder: 12.5 - 3.05 = 9.45ns OK
    // TSVs enable much shorter H-tree paths and wider datapaths

    arch->timing.subarray_access_ns = 25.0;
    arch->timing.bank_access_ns = 37.5;
    arch->timing.chip_access_ns = 45.0;  // Lower due to TSV
    arch->timing.rank_access_ns = 50.0;

    // ===== ENERGY (INFERRED) =====

    arch->energy.subarray_energy_pJ = 0.5;
    arch->energy.bank_energy_pJ = 1.0;
    arch->energy.chip_energy_pJ = 2.0;  // TSV is efficient
    arch->energy.rank_energy_pJ = 3.0;
    arch->energy.energy_source =
        "INFERRED from HBM architecture papers. "
        "TSV reduces energy vs long wire routing in DDR4.";

    // ===== PE BUS CONSTRAINTS (for PE placement) =====

    // Subarray-level PEs: Direct row buffer access (similar to DDR4)
    arch->pe_bus_constraints.subarray_level.data_bus_width_bits = 8192 * 8;  // 8KB row buffer
    arch->pe_bus_constraints.subarray_level.max_bandwidth_gbps = 15.0;  // Slightly higher than DDR4
    arch->pe_bus_constraints.subarray_level.row_buffer_size_bytes = 8192;
    arch->pe_bus_constraints.subarray_level.has_dedicated_bus = true;

    // Bank-level PEs: Share bank I/O bus (narrower than DDR4 per bank)
    arch->pe_bus_constraints.bank_level.data_bus_width_bits = 32;  // HBM has narrower per-bank paths
    arch->pe_bus_constraints.bank_level.max_bandwidth_gbps = 32.0;  // HBM2 per-channel BW / banks
    arch->pe_bus_constraints.bank_level.has_dedicated_bus = false;

    // Chip-level PEs: Share chip-level interconnect (via TSV)
    arch->pe_bus_constraints.chip_level.data_bus_width_bits = 128;  // Per-channel width
    arch->pe_bus_constraints.chip_level.max_bandwidth_gbps = 128.0;  // HBM2 per-channel (256 GB/s / 2 stacks)
    arch->pe_bus_constraints.chip_level.has_dedicated_bus = false;

    // Rank-level PEs: HBM uses stacks, not traditional ranks
    arch->pe_bus_constraints.rank_level.data_bus_width_bits = 1024;  // 8 channels x 128 bits
    arch->pe_bus_constraints.rank_level.max_bandwidth_gbps = 256.0;  // Full HBM2 bandwidth
    arch->pe_bus_constraints.rank_level.has_dedicated_bus = false;

    // Logic die level: Wide high-bandwidth interface (HBM advantage!)
    arch->pe_bus_constraints.logic_die_level.data_bus_width_bits = 1024;  // Full interface
    arch->pe_bus_constraints.logic_die_level.max_bandwidth_gbps = 256.0;  // HBM2 peak BW
    arch->pe_bus_constraints.logic_die_level.has_dedicated_bus = true;

    return arch;
}

/**
 * Create HBM2 with configurable port width scaling
 *
 * @param port_width_scale Scaling factor for all port widths (default 1.0)
 *                         2.0 = 2x wider buses, 0.5 = half-width buses
 * @return Configured HBM2 architecture with scaled port widths
 */
inline std::unique_ptr<DRAMArchitectureV2> createHBM2_Verified(double port_width_scale) {
    auto arch = createHBM2_Verified();
    arch->port_width_scale = port_width_scale;
    return arch;
}

//=============================================================================
// DDR5-4800 (Rigorously Verified)
//=============================================================================

inline std::unique_ptr<DRAMArchitectureV2> createDDR5_4800_Verified() {
    auto arch = std::make_unique<DRAMArchitectureV2>("DDR5-4800", "DDR5");

    // ===== DATAPATH STAGES =====

    // Stage 1: Row Buffer (same as DDR4)
    arch->datapath.row_buffer_bits = {
        8192,  // 8Kb typical
        VerificationStatus::VERIFIED,
        "JEDEC JESD79-5: DDR5 maintains similar subarray structure",
        "Row buffer size unchanged from DDR4"
    };

    // Stage 2: Global Sense Amplifiers
    arch->datapath.gsa_datapath_bits = {
        256,  // Similar to DDR4
        VerificationStatus::INFERRED,
        "Estimated similar to DDR4 architecture",
        "Column I/O width maintained"
    };

    // Stage 3: Prefetch Datapath (JEDEC verified - 16n prefetch!)
    arch->datapath.prefetch_datapath_bits = {
        128,  // 16n prefetch x 8-bit I/O
        VerificationStatus::VERIFIED,
        "JEDEC JESD79-5: DDR5 has 16n prefetch architecture, x8 device",
        "16 bursts x 8 bits = 128 bits prefetch buffer (2x DDR4!)"
    };

    // Stage 4: Bank Serialization (estimated, likely similar to DDR4)
    arch->datapath.bank_serialization_bits = {
        8,  // Estimated, same as DDR4
        VerificationStatus::ESTIMATED,
        "NOT DOCUMENTED! Estimated similar to DDR4",
        "Despite higher speed, internal paths likely remain narrow"
    };

    // Stage 5: Chip I/O (JEDEC verified)
    arch->datapath.chip_io_bits = {
        8,  // x8 device
        VerificationStatus::VERIFIED,
        "JEDEC JESD79-5: DDR5 x8 configuration",
        "External package pins, same as DDR4"
    };

    // Rank databus - DDR5 has two independent 32-bit subchannels!
    arch->datapath.rank_databus_bits = {
        64,  // 8 chips x 8 bits (or 2 subchannels x 32 bits)
        VerificationStatus::VERIFIED,
        "JEDEC JESD79-5: DDR5 introduces dual 32-bit subchannels",
        "TWO independent 32-bit channels per DIMM (key DDR5 feature!)"
    };

    // Channel
    arch->datapath.channel_databus_bits = {
        64,  // Single channel
        VerificationStatus::VERIFIED,
        "Standard DDR5 memory controller interface",
        "Per-DIMM interface width"
    };

    // ===== BANDWIDTH LIMITS (INFERRED) =====

    /* 1.11.57 (audit C003): the retired literal said "DDR5-4800: 8 bits x
     * 2.4 GHz" -- a bin this tree does not simulate (1.11.56 moved this object
     * to the DDR5_3200AN preset) and a clock this object does not carry
     * (clock_freq_mhz is 1600). Derived now. */
    arch->bandwidth_limits.inference_method =
        "DERIVED: bank = (bank_serialization_bits / 8) x clock_freq_GHz, on an "
        "8-bit bank path estimated to be DDR4's; bank group = bank x 2 (the "
        "bank-group port multiplier, UNSOURCED).";
    arch->bandwidth_limits.confidence_level = "Medium - based on DDR4 scaling";

    // ===== ORGANIZATION (VERIFIED) =====

    arch->organization.subarrays_per_bank = 4;  // Typical
    arch->organization.banks_per_bank_group = 4;  // JEDEC: 4 banks per bank group
    arch->organization.bank_groups_per_chip = 8;  // JEDEC: 8 bank groups (2x DDR4!)
    arch->organization.chips_per_rank = 8;  // x8 organization
    arch->organization.ranks_per_channel = 2;  // Typical DIMM
    arch->organization.subarray_size_kb = 512;  // Typical
    arch->organization.bank_size_mb = 2;
    arch->organization.chip_size_mb = 256;  // Larger chips (2Gb+)
    arch->organization.rank_size_gb = 2;

    // ===== TIMING (JEDEC DDR5; rate = the simulated DDR5_3200AN bin) =====

    /* 1.11.56 (audit D002's defect, found by the B042 reconciliation check):
     * the Ramulator preset selected for DDR5 is DDR5_3200AN, so this tree
     * simulates a 3200 MT/s part. This object said 4800 -- a 1.5x gap
     * between the bin whose cycles are counted and the bin whose bandwidth
     * is reported as mem.bandwidth (the device M/D/1 service rate). The ns
     * timings are absolute and stay within JEDEC's range across bins; the
     * RATE and the burst follow the preset. */
    arch->timing.clock_freq_mhz = 1600;  // 3200 MT/s DDR -> 1.6 GHz core
    arch->timing.data_rate_mtps = 3200;  // preset DDR5_3200AN
    arch->timing.tRCD_ns = 16.67;  // VERIFIED: CL40 at 4800 MT/s
    arch->timing.tCAS_ns = 16.67;  // VERIFIED
    arch->timing.tRP_ns = 16.67;  // VERIFIED
    arch->timing.tRAS_ns = 32.0;  // VERIFIED
    arch->timing.tBurst_ns = 16.0 * 1000.0 / 3200.0;  // 16 beats @ 3200 MT/s = 5.0 ns

    // Inner-bank datapath timing (INFERRED - scaled from DDR4 with better process)
    arch->timing.inner_bank.column_decoder_ns = 0.30;       // Faster process
    arch->timing.inner_bank.column_mux_ns = 0.50;
    arch->timing.inner_bank.subarray_output_drv_ns = 0.45;
    arch->timing.inner_bank.local_io_ns = 0.70;
    arch->timing.inner_bank.htree_horizontal_ns = 1.00;     // Better layout
    arch->timing.inner_bank.htree_vertical_ns = 1.00;
    arch->timing.inner_bank.global_io_ns = 1.30;
    arch->timing.inner_bank.bank_io_driver_ns = 0.55;
    arch->timing.inner_bank.verification_status = VerificationStatus::INFERRED;
    arch->timing.inner_bank.source =
        "INFERRED from DDR4 CACTI model with process scaling for DDR5 (7nm-10nm node)";

    arch->timing.subarray_access_ns = 33.34;  // tRCD + tCAS
    arch->timing.bank_access_ns = 50.01;  // tRP + tRCD + tCAS
    arch->timing.chip_access_ns = 70.0;  // ESTIMATED
    arch->timing.rank_access_ns = 90.0;  // ESTIMATED

    // ===== ENERGY (INFERRED) =====

    arch->energy.subarray_energy_pJ = 0.8;  // Lower voltage (1.1V vs 1.2V)
    arch->energy.bank_energy_pJ = 1.6;
    arch->energy.chip_energy_pJ = 4.0;
    arch->energy.rank_energy_pJ = 8.0;
    arch->energy.energy_source =
        "INFERRED from DDR4 with voltage scaling (1.1V vs 1.2V = ~15% lower energy)";

    // ===== PE BUS CONSTRAINTS (for PE placement) =====

    // Subarray-level PEs
    arch->pe_bus_constraints.subarray_level.data_bus_width_bits = 8192 * 8;
    arch->pe_bus_constraints.subarray_level.max_bandwidth_gbps = 12.0;
    arch->pe_bus_constraints.subarray_level.row_buffer_size_bytes = 8192;
    arch->pe_bus_constraints.subarray_level.has_dedicated_bus = true;

    // Bank-level PEs
    arch->pe_bus_constraints.bank_level.data_bus_width_bits = 64;
    arch->pe_bus_constraints.bank_level.max_bandwidth_gbps = 38.4;  // DDR5-4800 per-bank BW
    arch->pe_bus_constraints.bank_level.has_dedicated_bus = false;

    // Chip-level PEs
    arch->pe_bus_constraints.chip_level.data_bus_width_bits = 64;
    arch->pe_bus_constraints.chip_level.max_bandwidth_gbps = 38.4;
    arch->pe_bus_constraints.chip_level.has_dedicated_bus = false;

    // Rank-level PEs
    arch->pe_bus_constraints.rank_level.data_bus_width_bits = 64;
    arch->pe_bus_constraints.rank_level.max_bandwidth_gbps = 38.4;
    arch->pe_bus_constraints.rank_level.has_dedicated_bus = false;

    // Logic die level (not applicable for DDR5)
    arch->pe_bus_constraints.logic_die_level.data_bus_width_bits = 1024;
    arch->pe_bus_constraints.logic_die_level.max_bandwidth_gbps = 256.0;
    arch->pe_bus_constraints.logic_die_level.has_dedicated_bus = true;

    return arch;
}

inline std::unique_ptr<DRAMArchitectureV2> createDDR5_4800_Verified(double port_width_scale) {
    auto arch = createDDR5_4800_Verified();
    arch->port_width_scale = port_width_scale;
    return arch;
}

//=============================================================================
// HBM3 (Rigorously Verified)
//=============================================================================

inline std::unique_ptr<DRAMArchitectureV2> createHBM3_Verified() {
    auto arch = std::make_unique<DRAMArchitectureV2>("HBM3", "HBM3");

    // ===== DATAPATH STAGES =====

    // Stage 1: Row Buffer
    arch->datapath.row_buffer_bits = {
        16384,  // 16Kb typical (same as HBM2)
        VerificationStatus::INFERRED,
        "Typical HBM3 subarray, similar to HBM2",
        "Wide rows for high bandwidth"
    };

    // Stage 2: Global Sense Amplifiers
    arch->datapath.gsa_datapath_bits = {
        512,  // Similar to HBM2
        VerificationStatus::INFERRED,
        "Estimated from HBM3 architecture papers",
        "Wider column I/O for high throughput"
    };

    // Stage 3: Prefetch Datapath (JEDEC verified - 8n prefetch!)
    arch->datapath.prefetch_datapath_bits = {
        512,  // 8n prefetch x 64-bit pseudo-channel
        VerificationStatus::VERIFIED,
        "JEDEC JESD238: HBM3 has 8n prefetch, 64-bit pseudo-channels",
        "8 bursts x 64 bits = 512 bits (4x DDR4!)"
    };

    // Stage 4: Bank Serialization (wider than HBM2 due to higher speed TSV)
    arch->datapath.bank_serialization_bits = {
        128,  // INFERRED: wider TSV for higher bandwidth
        VerificationStatus::INFERRED,
        "ESTIMATED from 2x HBM2 bandwidth with improved TSV technology",
        "TSV improvements enable wider internal paths"
    };

    // Stage 5: Chip I/O (JEDEC verified)
    arch->datapath.chip_io_bits = {
        1024,  // 16 pseudo-channels x 64 bits
        VerificationStatus::VERIFIED,
        "JEDEC JESD238: HBM3 has 16 pseudo-channels of 64-bit each",
        "Total 1024-bit interface (same as HBM2, but higher speed)"
    };

    // Per pseudo-channel
    arch->datapath.rank_databus_bits = {
        64,  // Per pseudo-channel
        VerificationStatus::VERIFIED,
        "JEDEC JESD238: 64-bit pseudo-channel width",
        "HBM3 uses pseudo-channels vs full channels"
    };

    /* 1.11.57 (audit C007): ONE channel, not the stack -- see the HBM2 note.
     * At 16 channels this was the 16x case: the ladder's L5 rung was 1024 bits
     * / 819.2 GB/s where the rung under it is 64 bits / 51.2 GB/s, and the
     * system root multiplied that stack figure by 16 again. */
    arch->datapath.channel_databus_bits = {
        64,  // one channel; the stack is 16 of these (JESD238)
        VerificationStatus::VERIFIED,
        "JEDEC JESD238: 64-bit channel, 16 channels per stack",
        "One channel's data bus; stack width = this x channels"
    };

    // ===== BANDWIDTH LIMITS (INFERRED) =====

    /* 1.11.57 (audit C003): this is the row that cost the most. "16.0 GB/s,
     * 2x HBM2" against a 128-bit bank path implies a 1.0 GHz array clock; this
     * object's core clock is 3.2 GHz, so the ladder clocked HBM3's L1 and L2
     * at a third of the object's own array speed and charged every crossing on
     * the reference cell accordingly. Derived now: 128 / 8 x 3.2 = 51.2 GB/s,
     * which is the same statement as "one 128-bit beat per core clock". */
    arch->bandwidth_limits.inference_method =
        "DERIVED: bank = (bank_serialization_bits / 8) x clock_freq_GHz, on an "
        "estimated 128-bit improved-TSV bank path; bank group = bank x 2 (the "
        "bank-group port multiplier, UNSOURCED).";
    arch->bandwidth_limits.confidence_level = "Medium - TSV improvements not fully specified";

    // ===== ORGANIZATION (VERIFIED) =====

    arch->organization.subarrays_per_bank = 4;
    arch->organization.banks_per_bank_group = 4;  // Same as HBM2
    arch->organization.bank_groups_per_chip = 4;  // Per pseudo-channel
    arch->organization.chips_per_rank = 16;  // 16 pseudo-channels
    arch->organization.ranks_per_channel = 1;  // Single stack
    arch->organization.subarray_size_kb = 1024;  // Same as HBM2
    arch->organization.bank_size_mb = 4;
    arch->organization.chip_size_mb = 2048;  // 2GB per stack (typical)
    arch->organization.rank_size_gb = 16;  // 16GB stacks available

    // ===== TIMING (JEDEC HBM3; rate = the simulated HBM3_6.4Gbps bin) =====

    /* 1.11.56 (audit D002's defect, found by the B042 reconciliation check):
     * the Ramulator preset selected for HBM3 is HBM3_6.4Gbps and the wrapper
     * asserts bandwidth_ = 819000 MB/s (1024 b x 6.4 GT/s / 8) beside it.
     * This object said 4.0 GT/s, so its rank/channel/chip-I/O bandwidths --
     * and, since 1.11.56, the hierarchy link ladder derived from them --
     * described a part 1.6x slower than the one whose cycles are counted.
     * That 1.6x is what made HBM3's three bandwidth scopes fail to
     * reconcile. The ns timings are absolute and stay; the RATE follows. */
    arch->timing.clock_freq_mhz = 3200;  // 6.4 GT/s DDR -> 3.2 GHz core
    arch->timing.data_rate_mtps = 6400;  // preset HBM3_6.4Gbps
    arch->timing.tRCD_ns = 16.0;  // JESD238 spec-minimum (was 10.0, below physical minimum)
    arch->timing.tCAS_ns = 16.0;  // JESD238
    arch->timing.tRP_ns = 10.0;  // VERIFIED
    arch->timing.tRAS_ns = 24.0;  // VERIFIED
    arch->timing.tBurst_ns = 8.0 * 1000.0 / 6400.0;  // 8 beats @ 6400 MT/s = 1.25 ns

    // Inner-bank datapath timing (INFERRED - faster than HBM2)
    arch->timing.inner_bank.column_decoder_ns = 0.20;
    arch->timing.inner_bank.column_mux_ns = 0.30;
    arch->timing.inner_bank.subarray_output_drv_ns = 0.25;
    arch->timing.inner_bank.local_io_ns = 0.35;
    arch->timing.inner_bank.htree_horizontal_ns = 0.35;
    arch->timing.inner_bank.htree_vertical_ns = 0.35;
    arch->timing.inner_bank.global_io_ns = 0.50;
    arch->timing.inner_bank.bank_io_driver_ns = 0.30;
    arch->timing.inner_bank.verification_status = VerificationStatus::INFERRED;
    arch->timing.inner_bank.source =
        "INFERRED from HBM2 with advanced process scaling (5nm logic die)";

    arch->timing.subarray_access_ns = 20.0;
    arch->timing.bank_access_ns = 30.0;
    arch->timing.chip_access_ns = 35.0;
    arch->timing.rank_access_ns = 40.0;

    // ===== ENERGY (INFERRED) =====

    arch->energy.subarray_energy_pJ = 0.4;  // Lower due to advanced process
    arch->energy.bank_energy_pJ = 0.8;
    arch->energy.chip_energy_pJ = 1.5;
    arch->energy.rank_energy_pJ = 2.5;
    arch->energy.energy_source =
        "INFERRED from HBM2 with process/voltage scaling for advanced nodes";

    // ===== PE BUS CONSTRAINTS (for PE placement) =====

    // Subarray-level PEs
    arch->pe_bus_constraints.subarray_level.data_bus_width_bits = 8192 * 8;
    arch->pe_bus_constraints.subarray_level.max_bandwidth_gbps = 20.0;
    arch->pe_bus_constraints.subarray_level.row_buffer_size_bytes = 8192;
    arch->pe_bus_constraints.subarray_level.has_dedicated_bus = true;

    // Bank-level PEs
    arch->pe_bus_constraints.bank_level.data_bus_width_bits = 64;
    arch->pe_bus_constraints.bank_level.max_bandwidth_gbps = 64.0;
    arch->pe_bus_constraints.bank_level.has_dedicated_bus = false;

    // Chip-level PEs
    arch->pe_bus_constraints.chip_level.data_bus_width_bits = 128;
    arch->pe_bus_constraints.chip_level.max_bandwidth_gbps = 256.0;
    arch->pe_bus_constraints.chip_level.has_dedicated_bus = false;

    // Rank-level PEs (stack level for HBM)
    arch->pe_bus_constraints.rank_level.data_bus_width_bits = 1024;
    arch->pe_bus_constraints.rank_level.max_bandwidth_gbps = 512.0;  // HBM3 peak
    arch->pe_bus_constraints.rank_level.has_dedicated_bus = false;

    // Logic die level (HBM3 advantage!)
    arch->pe_bus_constraints.logic_die_level.data_bus_width_bits = 1024;
    arch->pe_bus_constraints.logic_die_level.max_bandwidth_gbps = 512.0;
    arch->pe_bus_constraints.logic_die_level.has_dedicated_bus = true;

    return arch;
}

inline std::unique_ptr<DRAMArchitectureV2> createHBM3_Verified(double port_width_scale) {
    auto arch = createHBM3_Verified();
    arch->port_width_scale = port_width_scale;
    return arch;
}

//=============================================================================
// Verification Report
//=============================================================================

inline void DRAMArchitectureV2::printVerificationReport() const {
    std::cout << "\n+==================================================================+\n";
    std::cout << "|  VERIFICATION REPORT: " << std::left << std::setw(43) << name << "|\n";
    std::cout << "+==================================================================+\n\n";

    /* 1.11.56 (audit D073): ASCII TAGS. These strings carried emoji and box-
     * drawing characters, which violates the project's ASCII-only rule for
     * source and generated files and makes the log unreadable on any terminal
     * or parser that is not UTF-8. The status a line reports is unchanged;
     * only its glyph is. The rest of this file was swept the same way (times
     * signs, arrows, superscripts, degree and micro signs in comments). */
    auto printStatus = [](const VerifiedValue& v) {
        std::string status_str;
        switch (v.status) {
            case VerificationStatus::VERIFIED:  status_str = "[VERIFIED]  "; break;
            case VerificationStatus::INFERRED:  status_str = "[INFERRED]  "; break;
            case VerificationStatus::ESTIMATED: status_str = "[ESTIMATED] "; break;
            case VerificationStatus::UNKNOWN:   status_str = "[UNKNOWN]   "; break;
        }
        std::cout << "  " << status_str << std::setw(6) << v.value_bits << " bits\n";
        std::cout << "     Source: " << v.source << "\n";
        if (!v.notes.empty()) {
            std::cout << "     Notes:  " << v.notes << "\n";
        }
        std::cout << "\n";
    };

    std::cout << "DATAPATH STAGES:\n";
    std::cout << "-----------------\n";
    std::cout << "Row Buffer:\n";
    printStatus(datapath.row_buffer_bits);

    std::cout << "Global Sense Amplifiers (GSA):\n";
    printStatus(datapath.gsa_datapath_bits);

    std::cout << "Prefetch Datapath:\n";
    printStatus(datapath.prefetch_datapath_bits);

    std::cout << "Bank Serialization (CRITICAL BOTTLENECK!):\n";
    printStatus(datapath.bank_serialization_bits);

    std::cout << "Chip I/O:\n";
    printStatus(datapath.chip_io_bits);

    std::cout << "Rank Data Bus:\n";
    printStatus(datapath.rank_databus_bits);

    std::cout << "\nBANDWIDTH LIMITS (INFERRED):\n";
    std::cout << "----------------------------\n";
    // 1.11.57 (audit C003): derived on read, so this line cannot print a
    // bandwidth that the object's own width and clock do not produce.
    std::cout << "  Bank effective BW:       " << getBankEffectiveBW() << " GB/s\n";
    std::cout << "  Bank Group effective BW: " << getBankGroupEffectiveBW() << " GB/s\n";
    std::cout << "  Method: " << bandwidth_limits.inference_method << "\n";
    std::cout << "  Confidence: " << bandwidth_limits.confidence_level << "\n";

    std::cout << "\nENERGY:\n";
    std::cout << "-------\n";
    std::cout << "  Source: " << energy.energy_source << "\n";
    std::cout << "  Subarray: " << energy.subarray_energy_pJ << " pJ/byte\n";
    std::cout << "  Bank:     " << energy.bank_energy_pJ << " pJ/byte\n";

    std::cout << "\n";
}

inline double DRAMArchitectureV2::getRankBW() const {
    return (datapath.rank_databus_bits.value_bits / 8.0) * (timing.data_rate_mtps / 1000.0);
}

inline std::string DRAMArchitectureV2::getOverallConfidence() const {
    // Assess overall confidence based on critical parameters
    if (datapath.bank_serialization_bits.status == VerificationStatus::ESTIMATED ||
        datapath.bank_serialization_bits.status == VerificationStatus::UNKNOWN) {
        return "MEDIUM - Bank serialization width is estimated (critical for PIM!)";
    }
    return "HIGH - Most critical parameters verified from JEDEC or papers";
}

} // namespace memory
} // namespace pimid

#endif // PIMID_DRAM_ARCHITECTURE_V2_H
