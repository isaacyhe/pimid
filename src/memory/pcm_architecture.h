/**
 * @file pcm_architecture.h  
 * @brief PCM (Phase-Change Memory) Architecture Specifications for PIM
 *
 * KEY CHARACTERISTICS:
 * - Non-volatile
 * - VERY slow writes (SET: 50-150ns, RESET: 10-50ns)
 * - Moderate read latency (6-12ns)
 * - 3D stackable (e.g., Intel Optane)
 * - Bus routing (some designs, not always H-tree)
 *
 * SOURCES:
 * - NVSim (external/nvsim/)
 * - JSSC 2007 Samsung PCM paper
 * - INNER_BANK_TIMING_ALL_MEMORY_TECH.md
 *
 * VERSION: 1.0
 */

#ifndef PIMID_PCM_ARCHITECTURE_H
#define PIMID_PCM_ARCHITECTURE_H

#include <string>
#include <memory>
#include "sram_architecture.h"  // For VerificationStatus enum

namespace pimid {
namespace memory {

// VerificationStatus is defined in sram_architecture.h

struct PCMOrganization {
    int banks_per_chip;
    int bank_rows, bank_cols;
    size_t chip_size_mb, bank_size_kb;
    int mats_per_bank;
    size_t mat_size_kb;
    int wordlines_per_mat, bitlines_per_mat;
};

struct PCMInnerBankTiming {
    // READ path
    double row_decoder_ns, wordline_ns;
    double bitline_read_ns;        // Resistance sensing (SLOW)
    double sense_amp_external_ns;  // External to mat!
    double column_mux_ns, mat_output_drv_ns;
    double bus_horizontal_ns;      // Bus routing (not H-tree)
    double bus_vertical_ns;
    double global_io_ns, bank_output_drv_ns;

    // WRITE path (VERY SLOW!)
    double set_pulse_ns;           // Crystallize (50-150ns)
    double reset_pulse_ns;         // Amorphize (10-50ns)

    VerificationStatus verification_status;
    std::string source;

    double getTotalReadLatency() const {
        return row_decoder_ns + wordline_ns + bitline_read_ns +
               sense_amp_external_ns + column_mux_ns + mat_output_drv_ns +
               bus_horizontal_ns + bus_vertical_ns + global_io_ns +
               bank_output_drv_ns;
    }

    double getTotalSetWriteLatency() const {
        return row_decoder_ns + wordline_ns + set_pulse_ns;
    }

    double getTotalResetWriteLatency() const {
        return row_decoder_ns + wordline_ns + reset_pulse_ns;
    }
};

struct PCMTiming {
    double clock_freq_ghz;
    double subarray_read_ns, bank_read_ns, chip_read_ns;
    double subarray_set_ns, bank_set_ns, chip_set_ns;
    double subarray_reset_ns, bank_reset_ns, chip_reset_ns;
    PCMInnerBankTiming inner_bank;
};

struct PCMEnergy {
    double subarray_read_energy_pJ, bank_read_energy_pJ, chip_read_energy_pJ;
    double subarray_set_energy_pJ, bank_set_energy_pJ, chip_set_energy_pJ;
    double subarray_reset_energy_pJ, bank_reset_energy_pJ, chip_reset_energy_pJ;
    double read_energy_per_byte, write_energy_per_byte;
    double subarray_leakage_mw, bank_leakage_mw, chip_leakage_mw;
    std::string energy_source;
};

struct PCMEndurance {
    double write_cycles;           // 10^8-10^9 typical
    double retention_years;
    bool mlc_support;              // Multi-level cell
    std::string endurance_source;
};

struct PCMDatapath {
    int mat_io_bits, bank_io_bits, chip_io_bits;
    VerificationStatus verification_status;
    std::string source;
};

class PCMArchitecture {
public:
    std::string name, technology, process_node;
    PCMOrganization organization;
    PCMTiming timing;
    PCMEnergy energy;
    PCMEndurance endurance;
    PCMDatapath datapath;

    PCMArchitecture(const std::string& name, const std::string& tech)
        : name(name), technology(tech) {}

    bool isSuitableForPIM() const {
        // Only for read-heavy workloads!
        return timing.subarray_set_ns / timing.subarray_read_ns > 10.0;
    }
};

// Example: 16MB PCM (90nm, JSSC 2007 design)
inline std::unique_ptr<PCMArchitecture> createPCM_16MB_90nm() {
    auto arch = std::make_unique<PCMArchitecture>("PCM-16MB", "PCM");
    arch->process_node = "90nm";

    arch->organization.banks_per_chip = 32;
    arch->organization.bank_rows = 8; arch->organization.bank_cols = 4;
    arch->organization.chip_size_mb = 16;
    arch->organization.bank_size_kb = 512;
    arch->organization.mats_per_bank = 4;
    arch->organization.mat_size_kb = 128;
    arch->organization.wordlines_per_mat = 1024;
    arch->organization.bitlines_per_mat = 1024;

    arch->timing.clock_freq_ghz = 0.133;  // 133 MHz
    arch->timing.subarray_read_ns = 8.0;
    arch->timing.bank_read_ns = 10.0;
    arch->timing.chip_read_ns = 12.0;
    arch->timing.subarray_set_ns = 100.0;  // Very slow!
    arch->timing.bank_set_ns = 105.0;
    arch->timing.chip_set_ns = 110.0;
    arch->timing.subarray_reset_ns = 30.0;
    arch->timing.bank_reset_ns = 35.0;
    arch->timing.chip_reset_ns = 40.0;

    arch->timing.inner_bank.row_decoder_ns = 0.75;
    arch->timing.inner_bank.wordline_ns = 1.00;
    arch->timing.inner_bank.bitline_read_ns = 2.25;  // Slow resistance read
    arch->timing.inner_bank.sense_amp_external_ns = 1.50;  // External SA
    arch->timing.inner_bank.column_mux_ns = 0.45;
    arch->timing.inner_bank.mat_output_drv_ns = 0.30;
    arch->timing.inner_bank.bus_horizontal_ns = 0.75;  // Bus, not H-tree
    arch->timing.inner_bank.bus_vertical_ns = 0.75;
    arch->timing.inner_bank.global_io_ns = 0.60;
    arch->timing.inner_bank.bank_output_drv_ns = 0.30;
    arch->timing.inner_bank.set_pulse_ns = 100.0;  // Crystallization
    arch->timing.inner_bank.reset_pulse_ns = 30.0;  // Amorphization
    arch->timing.inner_bank.verification_status = VerificationStatus::INFERRED;
    arch->timing.inner_bank.source = "NVSim 90nm PCM, JSSC 2007 Samsung PCM";

    arch->energy.subarray_read_energy_pJ = 2.0;
    arch->energy.bank_read_energy_pJ = 3.5;
    arch->energy.chip_read_energy_pJ = 6.0;
    arch->energy.subarray_set_energy_pJ = 80.0;  // High!
    arch->energy.bank_set_energy_pJ = 100.0;
    arch->energy.chip_set_energy_pJ = 150.0;
    arch->energy.subarray_reset_energy_pJ = 40.0;
    arch->energy.bank_reset_energy_pJ = 55.0;
    arch->energy.chip_reset_energy_pJ = 80.0;
    arch->energy.read_energy_per_byte = 0.50;
    arch->energy.write_energy_per_byte = 15.0;  // 30x read!
    arch->energy.subarray_leakage_mw = 0.05;
    arch->energy.bank_leakage_mw = 0.2;
    arch->energy.chip_leakage_mw = 6.4;
    arch->energy.energy_source = "NVSim PCM model, 90nm";

    arch->endurance.write_cycles = 1e8;  // Limited
    arch->endurance.retention_years = 10.0;
    arch->endurance.mlc_support = true;  // Multi-level cell possible
    arch->endurance.endurance_source = "PCM literature";

    arch->datapath.mat_io_bits = 64;
    arch->datapath.bank_io_bits = 64;
    arch->datapath.chip_io_bits = 64;
    arch->datapath.verification_status = VerificationStatus::INFERRED;
    arch->datapath.source = "NVSim PCM modeling";

    return arch;
}

} // namespace memory
} // namespace pimid

#endif // PIMID_PCM_ARCHITECTURE_H
