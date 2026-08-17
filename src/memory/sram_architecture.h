/**
 * @file sram_architecture.h
 * @brief SRAM Architecture Specifications for PIM
 *
 * This file contains SRAM architectural parameters based on CACTI models.
 * SRAM is typically used for on-chip caches (L1/L2/L3/LLC).
 *
 * HIERARCHY:
 * Chip -> Bank -> Mat -> Subarray -> 6T Cell Array
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

/* 1.11.24: the hand-written create*() default factories that used to
 * live in this header are DELETED. They existed only as a fallback for
 * "tool characterization failed", and that fallback let unsourced specs
 * reach a result indistinguishable from a real CACTI/NVSim read. The
 * model now REFUSES when its tool binding fails. The structs below are
 * the plugin CONTRACT -- what a memory technology must report -- and
 * are filled by the extractor from the tool, never by hand. */


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

    /* 1.11.23: CACTI's cycle time is the RANDOM CYCLE TIME -- it includes
     * precharge and restore and is a throughput bound, not an access latency.
     * It was previously assigned to bank_access_ns, where it does not belong.
     * Kept here, correctly named, so the number is still available without
     * being mistaken for a latency. */
    double cycle_time_ns = 0.0;

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


//=============================================================================
// Example: 16MB LLC (14nm)
//=============================================================================


} // namespace memory
} // namespace pimid

#endif // PIMID_SRAM_ARCHITECTURE_H
