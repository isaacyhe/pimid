/**
 * @file sram_architecture.h
 * @brief SRAM Architecture Specifications for PIM
 *
 * This file contains SRAM architectural parameters based on CACTI models.
 * SRAM is typically used for on-chip caches (L1/L2/L3/LLC).
 *
 * HIERARCHY:
 * Chip → Bank → Mat → Subarray → 6T Cell Array
 *
 * KEY DIFFERENCES from DRAM:
 * - No memory controller / rank levels (on-chip)
 * - No bank groups (independent banks)
 * - No refresh required (static cells)
 * - Much faster access (2-4ns vs 26ns DRAM)
 * - Symmetric read/write
 *
 * SOURCES:
 * - CACTI 6.5 (external/mcpat/cacti/)
 * - Cache architecture papers
 * - INNER_BANK_TIMING_ALL_MEMORY_TECH.md
 *
 * VERSION: 1.0
 * DATE: November 2024
 */

#ifndef PIMID_SRAM_ARCHITECTURE_H
#define PIMID_SRAM_ARCHITECTURE_H

#include <string>
#include <memory>
#include <iostream>

namespace pimid {
namespace memory {

//=============================================================================
// Verification Status (same as DRAM)
//=============================================================================

enum class VerificationStatus {
    VERIFIED,    // From CACTI models or definitive sources
    INFERRED,    // Derived from architectural analysis
    ESTIMATED,   // Educated guess based on typical values
    UNKNOWN      // Not documented
};

//=============================================================================
// SRAM Hierarchy (Different from DRAM!)
//=============================================================================

/**
 * @brief SRAM Hierarchy
 *
 * SRAM (on-chip caches) has a simpler hierarchy than DRAM:
 *
 * Level 1: Chip (Last-Level Cache)
 *   - Multiple banks (typically 4-16)
 *   - Shared by CPU cores
 *   - No memory controller (direct CPU access)
 *
 * Level 2: Bank
 *   - Multiple mats (e.g., 2x2, 4x4 grid)
 *   - H-tree network for routing
 *   - Independent access (no bank groups!)
 *
 * Level 3: Mat
 *   - 4 subarrays (standard CACTI configuration)
 *   - Shared predecode logic
 *   - Subarray I/O
 *
 * Level 4: Subarray
 *   - 6T SRAM cell array
 *   - Wordline decoder
 *   - Bitline sense amplifiers
 *   - Column multiplexer
 */
struct SRAMOrganization {
    // Chip level (on-chip cache)
    int banks_per_chip;           // Typically 4-16 for L3/LLC
    size_t chip_size_mb;          // Total cache size

    // Bank level
    int mats_per_bank_rows;       // Mat grid (e.g., 4x4)
    int mats_per_bank_cols;
    size_t bank_size_kb;          // Per bank capacity

    // Mat level
    int subarrays_per_mat;        // Typically 4 (CACTI standard)
    size_t mat_size_kb;           // Per mat capacity

    // Subarray level
    int rows_per_subarray;        // Wordlines
    int cols_per_subarray;        // Bitlines
    size_t subarray_size_kb;      // Per subarray capacity

    int getMatsPerBank() const {
        return mats_per_bank_rows * mats_per_bank_cols;
    }
};

//=============================================================================
// SRAM Inner-Bank Timing
//=============================================================================

/**
 * @brief SRAM Inner-Bank Datapath Timing
 *
 * Based on CACTI 6.5 analytical models.
 * Much faster than DRAM due to:
 * - 6T cells (no capacitor charging)
 * - On-chip (shorter wires)
 * - No precharge needed
 * - No refresh
 */
struct SRAMInnerBankTiming {
    // Row path
    double row_decoder_ns;        // Row address decode
    double wordline_ns;           // Wordline propagation

    // Column path
    double bitline_ns;            // Bitline development (6T cell)
    double sense_amp_ns;          // Differential sensing
    double column_mux_ns;         // Column multiplexer
    double subarray_output_drv_ns; // Subarray output driver

    // Inner-bank datapath
    double local_io_ns;           // Local I/O (within mat)
    double htree_horizontal_ns;   // H-tree horizontal segment
    double htree_vertical_ns;     // H-tree vertical segment
    double global_io_ns;          // Global I/O (bank-level)
    double bank_output_drv_ns;    // Bank output driver

    // Verification
    VerificationStatus verification_status;
    std::string source;

    // Derived totals
    double getRowPath() const {
        return row_decoder_ns + wordline_ns;
    }

    double getColumnPath() const {
        return bitline_ns + sense_amp_ns + column_mux_ns + subarray_output_drv_ns;
    }

    double getInnerBankDatapath() const {
        return local_io_ns + htree_horizontal_ns + htree_vertical_ns +
               global_io_ns + bank_output_drv_ns;
    }

    double getTotalInnerBank() const {
        return getRowPath() + getColumnPath() + getInnerBankDatapath();
    }

    // For PIM: Subarray-to-subarray (within same bank)
    double getSubarrayToSubarrayHTree() const {
        // Egress + Ingress
        return 2.0 * (htree_horizontal_ns + htree_vertical_ns);
    }
};

//=============================================================================
// SRAM Access Timing
//=============================================================================

struct SRAMTiming {
    double clock_freq_ghz;        // SRAM clock frequency

    // Access latencies (total)
    double subarray_access_ns;    // Access within subarray
    double mat_access_ns;         // Cross-subarray access
    double bank_access_ns;        // Access within bank
    double chip_access_ns;        // Cross-bank access

    // Inner-bank breakdown
    SRAMInnerBankTiming inner_bank;
};

//=============================================================================
// SRAM Energy
//=============================================================================

struct SRAMEnergy {
    // Energy per access (pJ)
    double subarray_energy_pJ;
    double mat_energy_pJ;
    double bank_energy_pJ;
    double chip_energy_pJ;

    // Energy per byte (pJ/byte)
    double subarray_energy_per_byte;
    double bank_energy_per_byte;
    double chip_energy_per_byte;

    // Leakage power (mW)
    double subarray_leakage_mw;
    double bank_leakage_mw;
    double chip_leakage_mw;

    std::string energy_source;
};

//=============================================================================
// SRAM Datapath
//=============================================================================

struct SRAMDatapath {
    // Bitwidths at each level
    int subarray_local_io_bits;   // Within subarray
    int mat_io_bits;              // Mat-level I/O
    int bank_io_bits;             // Bank-level I/O
    int chip_io_bits;             // Chip-level (to CPU)

    VerificationStatus verification_status;
    std::string source;
};

//=============================================================================
// Complete SRAM Architecture
//=============================================================================

class SRAMArchitecture {
public:
    std::string name;
    std::string technology;       // e.g., "SRAM", "L3 Cache"
    std::string process_node;     // e.g., "22nm", "14nm"

    SRAMOrganization organization;
    SRAMTiming timing;
    SRAMEnergy energy;
    SRAMDatapath datapath;

    // Constructor
    SRAMArchitecture(const std::string& name, const std::string& tech)
        : name(name), technology(tech) {}

    // Print summary
    void printSummary() const;

    // PIM-specific calculations
    double getSubarrayBandwidth() const {
        return (datapath.subarray_local_io_bits / 8.0) *
               (timing.clock_freq_ghz * 1000.0);  // MB/s
    }

    double getBankBandwidth() const {
        return (datapath.bank_io_bits / 8.0) *
               (timing.clock_freq_ghz * 1000.0);  // MB/s
    }
};

//=============================================================================
// Example: 8MB L3 Cache (22nm)
//=============================================================================

inline std::unique_ptr<SRAMArchitecture> createSRAM_L3_8MB_22nm() {
    auto arch = std::make_unique<SRAMArchitecture>("SRAM-L3-8MB", "SRAM");
    arch->process_node = "22nm";

    // ===== ORGANIZATION =====

    arch->organization.banks_per_chip = 8;
    arch->organization.chip_size_mb = 8;
    arch->organization.bank_size_kb = 1024;  // 1MB per bank

    arch->organization.mats_per_bank_rows = 4;
    arch->organization.mats_per_bank_cols = 4;  // 4x4 = 16 mats
    arch->organization.mat_size_kb = 64;  // 1024 / 16

    arch->organization.subarrays_per_mat = 4;  // CACTI standard
    arch->organization.subarray_size_kb = 16;  // 64 / 4

    arch->organization.rows_per_subarray = 512;
    arch->organization.cols_per_subarray = 256;

    // ===== TIMING (CACTI-derived, 22nm) =====

    arch->timing.clock_freq_ghz = 3.0;  // Typical CPU frequency

    // Total access latencies
    arch->timing.subarray_access_ns = 2.5;   // Fast!
    arch->timing.mat_access_ns = 3.0;
    arch->timing.bank_access_ns = 3.5;
    arch->timing.chip_access_ns = 4.0;

    // Inner-bank breakdown (CACTI 6.5)
    arch->timing.inner_bank.row_decoder_ns = 0.20;
    arch->timing.inner_bank.wordline_ns = 0.30;
    arch->timing.inner_bank.bitline_ns = 0.40;
    arch->timing.inner_bank.sense_amp_ns = 0.20;
    arch->timing.inner_bank.column_mux_ns = 0.15;
    arch->timing.inner_bank.subarray_output_drv_ns = 0.12;
    arch->timing.inner_bank.local_io_ns = 0.22;
    arch->timing.inner_bank.htree_horizontal_ns = 0.35;
    arch->timing.inner_bank.htree_vertical_ns = 0.35;
    arch->timing.inner_bank.global_io_ns = 0.30;
    arch->timing.inner_bank.bank_output_drv_ns = 0.15;

    arch->timing.inner_bank.verification_status = VerificationStatus::INFERRED;
    arch->timing.inner_bank.source =
        "CACTI 6.5 analytical model (external/mcpat/cacti/), "
        "22nm process, 8MB L3 cache configuration";

    // Total: ~2.74ns inner-bank (matches 2.5-3.0ns access time)

    // ===== ENERGY (CACTI-derived) =====

    arch->energy.subarray_energy_pJ = 0.5;
    arch->energy.mat_energy_pJ = 0.8;
    arch->energy.bank_energy_pJ = 1.2;
    arch->energy.chip_energy_pJ = 2.0;

    arch->energy.subarray_energy_per_byte = 0.1;  // Very low!
    arch->energy.bank_energy_per_byte = 0.2;
    arch->energy.chip_energy_per_byte = 0.3;

    arch->energy.subarray_leakage_mw = 0.5;
    arch->energy.bank_leakage_mw = 8.0;   // 16 mats × 4 subarrays
    arch->energy.chip_leakage_mw = 64.0;  // 8 banks

    arch->energy.energy_source = "CACTI 6.5, 22nm process";

    // ===== DATAPATH =====

    arch->datapath.subarray_local_io_bits = 128;  // Wide local I/O
    arch->datapath.mat_io_bits = 64;
    arch->datapath.bank_io_bits = 64;
    arch->datapath.chip_io_bits = 512;   // Wide chip-level (to CPU)

    arch->datapath.verification_status = VerificationStatus::INFERRED;
    arch->datapath.source = "CACTI 6.5 cache modeling";

    return arch;
}

//=============================================================================
// Example: 16MB LLC (14nm)
//=============================================================================

inline std::unique_ptr<SRAMArchitecture> createSRAM_LLC_16MB_14nm() {
    auto arch = std::make_unique<SRAMArchitecture>("SRAM-LLC-16MB", "SRAM");
    arch->process_node = "14nm";

    // ===== ORGANIZATION =====

    arch->organization.banks_per_chip = 16;
    arch->organization.chip_size_mb = 16;
    arch->organization.bank_size_kb = 1024;

    arch->organization.mats_per_bank_rows = 4;
    arch->organization.mats_per_bank_cols = 4;
    arch->organization.mat_size_kb = 64;

    arch->organization.subarrays_per_mat = 4;
    arch->organization.subarray_size_kb = 16;

    arch->organization.rows_per_subarray = 512;
    arch->organization.cols_per_subarray = 256;

    // ===== TIMING (14nm - faster than 22nm) =====

    arch->timing.clock_freq_ghz = 3.5;

    arch->timing.subarray_access_ns = 2.0;   // Faster process
    arch->timing.mat_access_ns = 2.4;
    arch->timing.bank_access_ns = 2.8;
    arch->timing.chip_access_ns = 3.2;

    // Inner-bank breakdown (14nm, scaled from 22nm)
    arch->timing.inner_bank.row_decoder_ns = 0.15;
    arch->timing.inner_bank.wordline_ns = 0.25;
    arch->timing.inner_bank.bitline_ns = 0.32;
    arch->timing.inner_bank.sense_amp_ns = 0.16;
    arch->timing.inner_bank.column_mux_ns = 0.12;
    arch->timing.inner_bank.subarray_output_drv_ns = 0.10;
    arch->timing.inner_bank.local_io_ns = 0.18;
    arch->timing.inner_bank.htree_horizontal_ns = 0.28;
    arch->timing.inner_bank.htree_vertical_ns = 0.28;
    arch->timing.inner_bank.global_io_ns = 0.24;
    arch->timing.inner_bank.bank_output_drv_ns = 0.12;

    arch->timing.inner_bank.verification_status = VerificationStatus::INFERRED;
    arch->timing.inner_bank.source =
        "CACTI 6.5 scaled to 14nm process, 16MB LLC configuration";

    // Total: ~2.20ns inner-bank

    // ===== ENERGY (14nm - lower than 22nm) =====

    arch->energy.subarray_energy_pJ = 0.3;
    arch->energy.mat_energy_pJ = 0.5;
    arch->energy.bank_energy_pJ = 0.8;
    arch->energy.chip_energy_pJ = 1.5;

    arch->energy.subarray_energy_per_byte = 0.08;
    arch->energy.bank_energy_per_byte = 0.15;
    arch->energy.chip_energy_per_byte = 0.25;

    arch->energy.subarray_leakage_mw = 0.3;
    arch->energy.bank_leakage_mw = 5.0;
    arch->energy.chip_leakage_mw = 80.0;  // More banks but better process

    arch->energy.energy_source = "CACTI 6.5, 14nm process";

    // ===== DATAPATH =====

    arch->datapath.subarray_local_io_bits = 128;
    arch->datapath.mat_io_bits = 64;
    arch->datapath.bank_io_bits = 64;
    arch->datapath.chip_io_bits = 512;

    arch->datapath.verification_status = VerificationStatus::INFERRED;
    arch->datapath.source = "CACTI 6.5 cache modeling, 14nm";

    return arch;
}

} // namespace memory
} // namespace pimid

#endif // PIMID_SRAM_ARCHITECTURE_H
