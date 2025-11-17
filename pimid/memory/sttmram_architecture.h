/**
 * @file sttmram_architecture.h
 * @brief STT-MRAM Architecture Specifications for PIM
 *
 * This file contains STT-MRAM (Spin-Transfer Torque MRAM) architectural
 * parameters based on NVSim models and research papers.
 *
 * HIERARCHY:
 * Chip → Bank → Subarray → MTJ Cell Array
 *
 * KEY CHARACTERISTICS:
 * - Non-volatile (retains data without power)
 * - Asymmetric R/W (read: 3-7ns, write: 13-27ns)
 * - No refresh needed
 * - Good endurance (10^12-10^15 cycles)
 * - MTJ (Magnetic Tunnel Junction) cells
 *
 * SOURCES:
 * - NVSim (pimid/external/nvsim/)
 * - SMART STT-MRAM paper (MEMSYS 2019)
 * - INNER_BANK_TIMING_ALL_MEMORY_TECH.md
 *
 * VERSION: 1.0
 * DATE: November 2024
 */

#ifndef PIMID_STTMRAM_ARCHITECTURE_H
#define PIMID_STTMRAM_ARCHITECTURE_H

#include <string>
#include <memory>
#include <iostream>

namespace pimid {
namespace memory {

//=============================================================================
// Verification Status
//=============================================================================

enum class VerificationStatus {
    VERIFIED,
    INFERRED,
    ESTIMATED,
    UNKNOWN
};

//=============================================================================
// STT-MRAM Hierarchy
//=============================================================================

/**
 * @brief STT-MRAM Hierarchy
 *
 * STT-MRAM can be standalone chip or embedded:
 *
 * Level 1: Chip
 *   - 32-64 banks (organized as grid, e.g., 8x8)
 *   - Global row decoder
 *   - H-tree network
 *
 * Level 2: Bank
 *   - Multiple subarrays
 *   - Local row decoder
 *   - WL (Wordline) drivers
 *   - SL/BL switch matrix
 *
 * Level 3: Subarray
 *   - MTJ cell array
 *   - Sense amplifiers
 *   - Column decoder
 *
 * Note: No bank groups (unlike DRAM)
 */
struct STTMRAMOrganization {
    // Chip level
    int banks_per_chip;           // 32-64 typical
    int bank_rows;                // Grid organization (e.g., 8x8)
    int bank_cols;
    size_t chip_size_mb;

    // Bank level
    int subarrays_per_bank;       // 4-16 typical
    size_t bank_size_kb;

    // Subarray level (mat)
    int wordlines_per_subarray;   // Rows (e.g., 512-1024)
    int bitlines_per_subarray;    // Columns (e.g., 1024-2048)
    size_t subarray_size_kb;

    int getBanksPerChip() const {
        return bank_rows * bank_cols;
    }
};

//=============================================================================
// STT-MRAM Inner-Bank Timing
//=============================================================================

/**
 * @brief STT-MRAM Inner-Bank Datapath Timing
 *
 * Based on NVSim models and SMART STT-MRAM architecture.
 *
 * CRITICAL: Read and write have VERY different latencies!
 * - Read: 3-7ns (sensing MTJ resistance)
 * - Write: 13-27ns (MTJ switching + verify)
 */
struct STTMRAMInnerBankTiming {
    // ===== READ PATH =====

    // Row path
    double row_decoder_ns;        // Row address decode
    double wordline_ns;           // WL activation

    // Column path (READ)
    double bitline_read_ns;       // MTJ resistance sensing
    double sense_amp_ns;          // Amplify small signal
    double column_mux_ns;         // Column selection
    double subarray_output_drv_ns;

    // Inner-bank datapath
    double local_io_ns;           // Local data lines
    double htree_horizontal_ns;   // H-tree routing
    double htree_vertical_ns;
    double global_io_ns;          // Bank-level I/O
    double bank_output_drv_ns;

    // ===== WRITE PATH =====

    double bitline_write_ns;      // Set WL + apply write voltage
    double mtj_switching_ns;      // MTJ state switching (SLOW!)
    double write_verify_ns;       // Read back to verify

    // Verification
    VerificationStatus verification_status;
    std::string source;

    // Derived totals
    double getReadRowPath() const {
        return row_decoder_ns + wordline_ns;
    }

    double getReadColumnPath() const {
        return bitline_read_ns + sense_amp_ns + column_mux_ns +
               subarray_output_drv_ns;
    }

    double getInnerBankDatapath() const {
        return local_io_ns + htree_horizontal_ns + htree_vertical_ns +
               global_io_ns + bank_output_drv_ns;
    }

    double getTotalReadLatency() const {
        return getReadRowPath() + getReadColumnPath() + getInnerBankDatapath();
    }

    double getTotalWriteLatency() const {
        // Write includes: WL activation + write pulse + MTJ switching + verify
        return row_decoder_ns + wordline_ns + bitline_write_ns +
               mtj_switching_ns + write_verify_ns;
    }

    // For PIM
    double getSubarrayToSubarrayHTree() const {
        return 2.0 * (htree_horizontal_ns + htree_vertical_ns);
    }
};

//=============================================================================
// STT-MRAM Access Timing
//=============================================================================

struct STTMRAMTiming {
    double clock_freq_ghz;

    // READ latencies
    double subarray_read_ns;
    double bank_read_ns;
    double chip_read_ns;

    // WRITE latencies (much slower!)
    double subarray_write_ns;
    double bank_write_ns;
    double chip_write_ns;

    // Inner-bank breakdown
    STTMRAMInnerBankTiming inner_bank;

    // Read/Write asymmetry ratio
    double getWriteToReadRatio() const {
        return subarray_write_ns / subarray_read_ns;
    }
};

//=============================================================================
// STT-MRAM Energy
//=============================================================================

struct STTMRAMEnergy {
    // Read energy (low)
    double subarray_read_energy_pJ;
    double bank_read_energy_pJ;
    double chip_read_energy_pJ;

    // Write energy (HIGH!)
    double subarray_write_energy_pJ;
    double bank_write_energy_pJ;
    double chip_write_energy_pJ;

    // Per-byte energy
    double read_energy_per_byte;
    double write_energy_per_byte;

    // Leakage (very low - non-volatile!)
    double subarray_leakage_mw;
    double bank_leakage_mw;
    double chip_leakage_mw;

    std::string energy_source;

    double getWriteToReadEnergyRatio() const {
        return subarray_write_energy_pJ / subarray_read_energy_pJ;
    }
};

//=============================================================================
// STT-MRAM Endurance
//=============================================================================

struct STTMRAMEndurance {
    double write_cycles;          // 10^12 - 10^15 typical
    double retention_years;       // 10+ years
    bool ecc_required;            // Error correction

    std::string endurance_source;
};

//=============================================================================
// STT-MRAM Datapath
//=============================================================================

struct STTMRAMDatapath {
    int subarray_local_io_bits;
    int bank_io_bits;
    int chip_io_bits;             // External interface

    VerificationStatus verification_status;
    std::string source;
};

//=============================================================================
// Complete STT-MRAM Architecture
//=============================================================================

class STTMRAMArchitecture {
public:
    std::string name;
    std::string technology;
    std::string process_node;

    STTMRAMOrganization organization;
    STTMRAMTiming timing;
    STTMRAMEnergy energy;
    STTMRAMEndurance endurance;
    STTMRAMDatapath datapath;

    // Constructor
    STTMRAMArchitecture(const std::string& name, const std::string& tech)
        : name(name), technology(tech) {}

    // Print summary
    void printSummary() const;

    // PIM-specific calculations
    double getSubarrayReadBandwidth() const {
        return (datapath.subarray_local_io_bits / 8.0) *
               (1000.0 / timing.inner_bank.getTotalReadLatency());  // MB/s
    }

    double getSubarrayWriteBandwidth() const {
        return (datapath.subarray_local_io_bits / 8.0) *
               (1000.0 / timing.inner_bank.getTotalWriteLatency());  // MB/s
    }

    bool isSuitableForPIM() const {
        // Good for read-heavy PIM workloads
        return timing.getWriteToReadRatio() < 5.0;  // Write < 5x read time
    }
};

//=============================================================================
// Example: 8MB STT-MRAM Cache (22nm)
//=============================================================================

inline std::unique_ptr<STTMRAMArchitecture> createSTTMRAM_8MB_22nm() {
    auto arch = std::make_unique<STTMRAMArchitecture>("STT-MRAM-8MB", "STT-MRAM");
    arch->process_node = "22nm";

    // ===== ORGANIZATION =====

    arch->organization.banks_per_chip = 64;  // 8x8 grid
    arch->organization.bank_rows = 8;
    arch->organization.bank_cols = 8;
    arch->organization.chip_size_mb = 8;
    arch->organization.bank_size_kb = 128;  // 8MB / 64

    arch->organization.subarrays_per_bank = 8;
    arch->organization.subarray_size_kb = 16;  // 128 / 8

    arch->organization.wordlines_per_subarray = 512;
    arch->organization.bitlines_per_subarray = 256;

    // ===== TIMING (NVSim-derived, 22nm) =====

    arch->timing.clock_freq_ghz = 1.0;  // Lower than SRAM

    // READ latencies
    arch->timing.subarray_read_ns = 5.0;
    arch->timing.bank_read_ns = 6.0;
    arch->timing.chip_read_ns = 7.0;

    // WRITE latencies (much higher!)
    arch->timing.subarray_write_ns = 18.0;  // MTJ switching!
    arch->timing.bank_write_ns = 20.0;
    arch->timing.chip_write_ns = 22.0;

    // Inner-bank breakdown (READ)
    arch->timing.inner_bank.row_decoder_ns = 0.45;
    arch->timing.inner_bank.wordline_ns = 0.65;
    arch->timing.inner_bank.bitline_read_ns = 1.10;  // MTJ resistance
    arch->timing.inner_bank.sense_amp_ns = 0.75;     // Small signal
    arch->timing.inner_bank.column_mux_ns = 0.30;
    arch->timing.inner_bank.subarray_output_drv_ns = 0.22;
    arch->timing.inner_bank.local_io_ns = 0.28;
    arch->timing.inner_bank.htree_horizontal_ns = 0.45;
    arch->timing.inner_bank.htree_vertical_ns = 0.45;
    arch->timing.inner_bank.global_io_ns = 0.52;
    arch->timing.inner_bank.bank_output_drv_ns = 0.22;

    // WRITE-specific
    arch->timing.inner_bank.bitline_write_ns = 2.0;   // Apply write voltage
    arch->timing.inner_bank.mtj_switching_ns = 12.0;  // MTJ switching (SLOW!)
    arch->timing.inner_bank.write_verify_ns = 4.0;    // Read-back verify

    arch->timing.inner_bank.verification_status = VerificationStatus::INFERRED;
    arch->timing.inner_bank.source =
        "NVSim (pimid/external/nvsim/), STT-MRAM configuration, "
        "SMART STT-MRAM paper (MEMSYS 2019)";

    // Total READ: ~5.39ns, Total WRITE: ~18.65ns

    // ===== ENERGY (NVSim-derived) =====

    arch->energy.subarray_read_energy_pJ = 0.8;
    arch->energy.bank_read_energy_pJ = 1.5;
    arch->energy.chip_read_energy_pJ = 3.0;

    arch->energy.subarray_write_energy_pJ = 15.0;  // Much higher!
    arch->energy.bank_write_energy_pJ = 20.0;
    arch->energy.chip_write_energy_pJ = 35.0;

    arch->energy.read_energy_per_byte = 0.15;
    arch->energy.write_energy_per_byte = 3.0;  // 20x higher!

    arch->energy.subarray_leakage_mw = 0.01;  // Very low (non-volatile)
    arch->energy.bank_leakage_mw = 0.1;
    arch->energy.chip_leakage_mw = 6.4;

    arch->energy.energy_source = "NVSim, 22nm STT-MRAM";

    // ===== ENDURANCE =====

    arch->endurance.write_cycles = 1e14;  // 10^14 cycles
    arch->endurance.retention_years = 10.0;
    arch->endurance.ecc_required = true;
    arch->endurance.endurance_source = "STT-MRAM literature";

    // ===== DATAPATH =====

    arch->datapath.subarray_local_io_bits = 64;
    arch->datapath.bank_io_bits = 64;
    arch->datapath.chip_io_bits = 128;  // LPDDR3-like interface

    arch->datapath.verification_status = VerificationStatus::INFERRED;
    arch->datapath.source = "NVSim STT-MRAM modeling";

    return arch;
}

//=============================================================================
// Example: Everspin 256Mb STT-MRAM (40nm)
//=============================================================================

inline std::unique_ptr<STTMRAMArchitecture> createSTTMRAM_Everspin_256Mb() {
    auto arch = std::make_unique<STTMRAMArchitecture>("Everspin-256Mb", "STT-MRAM");
    arch->process_node = "40nm";

    // ===== ORGANIZATION (Everspin-like) =====

    arch->organization.banks_per_chip = 32;  // Conservative estimate
    arch->organization.bank_rows = 8;
    arch->organization.bank_cols = 4;
    arch->organization.chip_size_mb = 32;  // 256Mb = 32MB
    arch->organization.bank_size_kb = 1024;

    arch->organization.subarrays_per_bank = 16;
    arch->organization.subarray_size_kb = 64;

    arch->organization.wordlines_per_subarray = 1024;
    arch->organization.bitlines_per_subarray = 512;

    // ===== TIMING (40nm, slower than 22nm) =====

    arch->timing.clock_freq_ghz = 0.8;

    arch->timing.subarray_read_ns = 7.0;   // Slower process
    arch->timing.bank_read_ns = 8.5;
    arch->timing.chip_read_ns = 10.0;

    arch->timing.subarray_write_ns = 25.0;  // Even slower
    arch->timing.bank_write_ns = 27.0;
    arch->timing.chip_write_ns = 30.0;

    // Inner-bank (scaled for 40nm)
    arch->timing.inner_bank.row_decoder_ns = 0.60;
    arch->timing.inner_bank.wordline_ns = 0.90;
    arch->timing.inner_bank.bitline_read_ns = 1.50;
    arch->timing.inner_bank.sense_amp_ns = 1.00;
    arch->timing.inner_bank.column_mux_ns = 0.42;
    arch->timing.inner_bank.subarray_output_drv_ns = 0.30;
    arch->timing.inner_bank.local_io_ns = 0.38;
    arch->timing.inner_bank.htree_horizontal_ns = 0.60;
    arch->timing.inner_bank.htree_vertical_ns = 0.60;
    arch->timing.inner_bank.global_io_ns = 0.70;
    arch->timing.inner_bank.bank_output_drv_ns = 0.30;

    arch->timing.inner_bank.bitline_write_ns = 3.0;
    arch->timing.inner_bank.mtj_switching_ns = 18.0;  // Slower process
    arch->timing.inner_bank.write_verify_ns = 6.0;

    arch->timing.inner_bank.verification_status = VerificationStatus::ESTIMATED;
    arch->timing.inner_bank.source =
        "Estimated from Everspin datasheets and NVSim 40nm scaling";

    // ===== ENERGY =====

    arch->energy.subarray_read_energy_pJ = 1.2;
    arch->energy.bank_read_energy_pJ = 2.0;
    arch->energy.chip_read_energy_pJ = 4.0;

    arch->energy.subarray_write_energy_pJ = 20.0;
    arch->energy.bank_write_energy_pJ = 30.0;
    arch->energy.chip_write_energy_pJ = 50.0;

    arch->energy.read_energy_per_byte = 0.20;
    arch->energy.write_energy_per_byte = 4.0;

    arch->energy.subarray_leakage_mw = 0.02;
    arch->energy.bank_leakage_mw = 0.32;
    arch->energy.chip_leakage_mw = 10.0;

    arch->energy.energy_source = "Everspin datasheets, 40nm";

    // ===== ENDURANCE =====

    arch->endurance.write_cycles = 1e12;  // Lower than advanced processes
    arch->endurance.retention_years = 20.0;  // Excellent retention
    arch->endurance.ecc_required = true;
    arch->endurance.endurance_source = "Everspin specifications";

    // ===== DATAPATH =====

    arch->datapath.subarray_local_io_bits = 64;
    arch->datapath.bank_io_bits = 64;
    arch->datapath.chip_io_bits = 16;  // x16 interface

    arch->datapath.verification_status = VerificationStatus::INFERRED;
    arch->datapath.source = "Everspin product specifications";

    return arch;
}

} // namespace memory
} // namespace pimid

#endif // PIMID_STTMRAM_ARCHITECTURE_H
