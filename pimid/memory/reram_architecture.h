/**
 * @file reram_architecture.h
 * @brief ReRAM (Resistive RAM / Memristor) Architecture Specifications for PIM
 *
 * KEY CHARACTERISTICS:
 * - Non-volatile
 * - Fast writes (5-20ns, much better than PCM!)
 * - Moderate read latency (4-7ns)
 * - Crossbar structure (IDEAL for analog computing!)
 * - Good endurance (10^10-10^12 cycles)
 *
 * SOURCES:
 * - NVSim (pimid/external/nvsim/)
 * - ISAAC, PRIME ReRAM papers
 * - INNER_BANK_TIMING_ALL_MEMORY_TECH.md
 *
 * VERSION: 1.0
 */

#ifndef PIMID_RERAM_ARCHITECTURE_H
#define PIMID_RERAM_ARCHITECTURE_H

#include <string>
#include <memory>

namespace pimid {
namespace memory {

enum class VerificationStatus { VERIFIED, INFERRED, ESTIMATED, UNKNOWN };

struct ReRAMOrganization {
    int banks_per_chip;
    int bank_rows, bank_cols;
    size_t chip_size_mb, bank_size_kb;
    int subarrays_per_bank;
    size_t subarray_size_kb;
    int crossbar_rows, crossbar_cols;  // Crossbar array size
};

struct ReRAMInnerBankTiming {
    // READ path
    double row_decoder_ns, wordline_ns;
    double bitline_ns;             // Resistance sensing
    double sense_amp_ns;
    double column_mux_ns, subarray_output_drv_ns;
    double local_io_ns;
    double htree_horizontal_ns, htree_vertical_ns;
    double global_io_ns, bank_output_drv_ns;

    // WRITE path (fast!)
    double set_pulse_ns;           // Form conductive filament
    double reset_pulse_ns;         // Rupture filament

    // ANALOG compute (special for ReRAM!)
    double analog_multiply_ns;     // Matrix-vector multiply in crossbar
    double analog_accumulate_ns;   // Column accumulation

    VerificationStatus verification_status;
    std::string source;

    double getTotalReadLatency() const {
        return row_decoder_ns + wordline_ns + bitline_ns + sense_amp_ns +
               column_mux_ns + subarray_output_drv_ns + local_io_ns +
               htree_horizontal_ns + htree_vertical_ns + global_io_ns +
               bank_output_drv_ns;
    }

    double getTotalWriteLatency() const {
        return row_decoder_ns + wordline_ns + set_pulse_ns;  // Fast!
    }

    double getAnalogComputeLatency() const {
        // Crossbar analog compute (very fast!)
        return analog_multiply_ns + analog_accumulate_ns;
    }
};

struct ReRAMTiming {
    double clock_freq_ghz;
    double subarray_read_ns, bank_read_ns, chip_read_ns;
    double subarray_write_ns, bank_write_ns, chip_write_ns;
    double analog_compute_ns;      // Analog crossbar operation
    ReRAMInnerBankTiming inner_bank;
};

struct ReRAMEnergy {
    double subarray_read_energy_pJ, bank_read_energy_pJ, chip_read_energy_pJ;
    double subarray_write_energy_pJ, bank_write_energy_pJ, chip_write_energy_pJ;
    double analog_compute_energy_pJ;  // Very low!
    double read_energy_per_byte, write_energy_per_byte;
    double subarray_leakage_mw, bank_leakage_mw, chip_leakage_mw;
    std::string energy_source;
};

struct ReRAMEndurance {
    double write_cycles;           // 10^10-10^12 typical
    double retention_years;
    bool mlc_support;              // Multi-level cell
    bool analog_capable;           // Analog computing support
    std::string endurance_source;
};

struct ReRAMDatapath {
    int subarray_local_io_bits, bank_io_bits, chip_io_bits;
    int crossbar_analog_bits;      // Analog compute resolution
    VerificationStatus verification_status;
    std::string source;
};

class ReRAMArchitecture {
public:
    std::string name, technology, process_node;
    ReRAMOrganization organization;
    ReRAMTiming timing;
    ReRAMEnergy energy;
    ReRAMEndurance endurance;
    ReRAMDatapath datapath;

    ReRAMArchitecture(const std::string& name, const std::string& tech)
        : name(name), technology(tech) {}

    bool isSuitableForPIM() const {
        // Excellent for PIM, especially analog compute!
        return endurance.analog_capable;
    }

    bool hasAnalogCompute() const {
        return endurance.analog_capable;
    }
};

// Example: 2MB ReRAM (32nm, analog-capable)
inline std::unique_ptr<ReRAMArchitecture> createReRAM_2MB_32nm_Analog() {
    auto arch = std::make_unique<ReRAMArchitecture>("ReRAM-2MB-Analog", "ReRAM");
    arch->process_node = "32nm";

    arch->organization.banks_per_chip = 16;
    arch->organization.bank_rows = 4; arch->organization.bank_cols = 4;
    arch->organization.chip_size_mb = 2;
    arch->organization.bank_size_kb = 128;
    arch->organization.subarrays_per_bank = 8;
    arch->organization.subarray_size_kb = 16;
    arch->organization.crossbar_rows = 256;  // Crossbar array
    arch->organization.crossbar_cols = 256;

    arch->timing.clock_freq_ghz = 1.0;
    arch->timing.subarray_read_ns = 5.0;
    arch->timing.bank_read_ns = 6.0;
    arch->timing.chip_read_ns = 7.0;
    arch->timing.subarray_write_ns = 12.0;  // Fast writes!
    arch->timing.bank_write_ns = 14.0;
    arch->timing.chip_write_ns = 16.0;
    arch->timing.analog_compute_ns = 3.0;  // Very fast analog compute!

    arch->timing.inner_bank.row_decoder_ns = 0.50;
    arch->timing.inner_bank.wordline_ns = 0.70;
    arch->timing.inner_bank.bitline_ns = 1.20;
    arch->timing.inner_bank.sense_amp_ns = 0.90;
    arch->timing.inner_bank.column_mux_ns = 0.28;
    arch->timing.inner_bank.subarray_output_drv_ns = 0.22;
    arch->timing.inner_bank.local_io_ns = 0.28;
    arch->timing.inner_bank.htree_horizontal_ns = 0.42;
    arch->timing.inner_bank.htree_vertical_ns = 0.42;
    arch->timing.inner_bank.global_io_ns = 0.48;
    arch->timing.inner_bank.bank_output_drv_ns = 0.22;
    arch->timing.inner_bank.set_pulse_ns = 10.0;   // Fast!
    arch->timing.inner_bank.reset_pulse_ns = 8.0;
    arch->timing.inner_bank.analog_multiply_ns = 2.0;     // Analog crossbar
    arch->timing.inner_bank.analog_accumulate_ns = 1.0;  // Column sum
    arch->timing.inner_bank.verification_status = VerificationStatus::INFERRED;
    arch->timing.inner_bank.source = "NVSim 32nm ReRAM, ISAAC/PRIME papers";

    arch->energy.subarray_read_energy_pJ = 1.0;
    arch->energy.bank_read_energy_pJ = 1.8;
    arch->energy.chip_read_energy_pJ = 3.5;
    arch->energy.subarray_write_energy_pJ = 8.0;   // Moderate
    arch->energy.bank_write_energy_pJ = 12.0;
    arch->energy.chip_write_energy_pJ = 20.0;
    arch->energy.analog_compute_energy_pJ = 0.5;   // Very low!
    arch->energy.read_energy_per_byte = 0.18;
    arch->energy.write_energy_per_byte = 1.6;
    arch->energy.subarray_leakage_mw = 0.02;
    arch->energy.bank_leakage_mw = 0.16;
    arch->energy.chip_leakage_mw = 2.56;
    arch->energy.energy_source = "NVSim ReRAM, ISAAC paper";

    arch->endurance.write_cycles = 1e11;  // Good endurance
    arch->endurance.retention_years = 10.0;
    arch->endurance.mlc_support = true;
    arch->endurance.analog_capable = true;  // ANALOG COMPUTE!
    arch->endurance.endurance_source = "ReRAM literature, ISAAC";

    arch->datapath.subarray_local_io_bits = 128;   // Wide for analog
    arch->datapath.bank_io_bits = 64;
    arch->datapath.chip_io_bits = 128;
    arch->datapath.crossbar_analog_bits = 8;  // 8-bit analog resolution
    arch->datapath.verification_status = VerificationStatus::INFERRED;
    arch->datapath.source = "NVSim ReRAM, analog compute modeling";

    return arch;
}

// Example: 8MB ReRAM (22nm, digital)
inline std::unique_ptr<ReRAMArchitecture> createReRAM_8MB_22nm_Digital() {
    auto arch = std::make_unique<ReRAMArchitecture>("ReRAM-8MB-Digital", "ReRAM");
    arch->process_node = "22nm";

    arch->organization.banks_per_chip = 32;
    arch->organization.bank_rows = 8; arch->organization.bank_cols = 4;
    arch->organization.chip_size_mb = 8;
    arch->organization.bank_size_kb = 256;
    arch->organization.subarrays_per_bank = 8;
    arch->organization.subarray_size_kb = 32;
    arch->organization.crossbar_rows = 512;
    arch->organization.crossbar_cols = 512;

    arch->timing.clock_freq_ghz = 1.2;
    arch->timing.subarray_read_ns = 4.0;   // Faster than 32nm
    arch->timing.bank_read_ns = 5.0;
    arch->timing.chip_read_ns = 6.0;
    arch->timing.subarray_write_ns = 10.0;
    arch->timing.bank_write_ns = 12.0;
    arch->timing.chip_write_ns = 14.0;
    arch->timing.analog_compute_ns = 0.0;  // Digital only

    arch->timing.inner_bank.row_decoder_ns = 0.38;
    arch->timing.inner_bank.wordline_ns = 0.55;
    arch->timing.inner_bank.bitline_ns = 0.95;
    arch->timing.inner_bank.sense_amp_ns = 0.72;
    arch->timing.inner_bank.column_mux_ns = 0.22;
    arch->timing.inner_bank.subarray_output_drv_ns = 0.18;
    arch->timing.inner_bank.local_io_ns = 0.22;
    arch->timing.inner_bank.htree_horizontal_ns = 0.35;
    arch->timing.inner_bank.htree_vertical_ns = 0.35;
    arch->timing.inner_bank.global_io_ns = 0.38;
    arch->timing.inner_bank.bank_output_drv_ns = 0.18;
    arch->timing.inner_bank.set_pulse_ns = 8.0;
    arch->timing.inner_bank.reset_pulse_ns = 6.0;
    arch->timing.inner_bank.analog_multiply_ns = 0.0;  // Not supported
    arch->timing.inner_bank.analog_accumulate_ns = 0.0;
    arch->timing.inner_bank.verification_status = VerificationStatus::INFERRED;
    arch->timing.inner_bank.source = "NVSim 22nm ReRAM, digital configuration";

    arch->energy.subarray_read_energy_pJ = 0.7;
    arch->energy.bank_read_energy_pJ = 1.2;
    arch->energy.chip_read_energy_pJ = 2.5;
    arch->energy.subarray_write_energy_pJ = 6.0;
    arch->energy.bank_write_energy_pJ = 9.0;
    arch->energy.chip_write_energy_pJ = 15.0;
    arch->energy.analog_compute_energy_pJ = 0.0;
    arch->energy.read_energy_per_byte = 0.12;
    arch->energy.write_energy_per_byte = 1.2;
    arch->energy.subarray_leakage_mw = 0.01;
    arch->energy.bank_leakage_mw = 0.08;
    arch->energy.chip_leakage_mw = 2.56;
    arch->energy.energy_source = "NVSim 22nm ReRAM";

    arch->endurance.write_cycles = 1e12;  // Better at 22nm
    arch->endurance.retention_years = 10.0;
    arch->endurance.mlc_support = false;  // SLC for reliability
    arch->endurance.analog_capable = false;  // Digital only
    arch->endurance.endurance_source = "ReRAM literature";

    arch->datapath.subarray_local_io_bits = 64;
    arch->datapath.bank_io_bits = 64;
    arch->datapath.chip_io_bits = 128;
    arch->datapath.crossbar_analog_bits = 0;  // Digital
    arch->datapath.verification_status = VerificationStatus::INFERRED;
    arch->datapath.source = "NVSim ReRAM";

    return arch;
}

} // namespace memory
} // namespace pimid

#endif // PIMID_RERAM_ARCHITECTURE_H
