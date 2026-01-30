/**
 * @file dram_architecture.cpp
 * @brief Implementation of DRAM architecture utilities
 */

#include "dram_architecture.h"
#include <iostream>
#include <iomanip>

namespace pimid {
namespace memory {

void DRAMArchitecture::printSummary() const {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::setw(58) << std::left << (name + " Architecture") << "║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "Technology: " << technology << "\n\n";

    // Port Bitwidths (CRITICAL SECTION!)
    std::cout << "═══ Internal Port Bitwidths (CRITICAL for PIM!) ═══\n";
    std::cout << std::setw(30) << "  Bank port:"
              << std::setw(6) << ports.bank_port_bits << " bits  "
              << "(" << std::fixed << std::setprecision(2) << getBankBandwidth() << " GB/s @ "
              << timing.clock_freq_mhz << " MHz)\n";
    std::cout << std::setw(30) << "  Bank Group port:"
              << std::setw(6) << ports.bank_group_port_bits << " bits  "
              << "(" << getBankGroupBandwidth() << " GB/s)\n";
    std::cout << std::setw(30) << "  Chip internal:"
              << std::setw(6) << ports.chip_internal_bits << " bits\n";
    std::cout << std::setw(30) << "  Chip I/O:"
              << std::setw(6) << ports.chip_io_bits << " bits  "
              << "(" << getChipIOBandwidth() << " GB/s @ "
              << timing.data_rate_mtps << " MT/s)\n";
    std::cout << std::setw(30) << "  Rank data bus:"
              << std::setw(6) << ports.rank_data_bits << " bits  "
              << "(" << getRankBandwidth() << " GB/s) ← FIRST WIDE!\n";
    std::cout << std::setw(30) << "  Channel data bus:"
              << std::setw(6) << ports.channel_data_bits << " bits  "
              << "(" << getChannelBandwidth() << " GB/s)\n\n";

    // Organization
    std::cout << "═══ Physical Organization ═══\n";
    std::cout << std::setw(30) << "  Subarrays per bank:" << organization.subarrays_per_bank << "\n";
    std::cout << std::setw(30) << "  Banks per bank group:" << organization.banks_per_bank_group << "\n";
    std::cout << std::setw(30) << "  Bank groups per chip:" << organization.bank_groups_per_chip << "\n";
    std::cout << std::setw(30) << "  Chips per rank:" << organization.chips_per_rank << "\n";
    std::cout << std::setw(30) << "  Ranks per channel:" << organization.ranks_per_channel << "\n";
    std::cout << "\n";
    std::cout << std::setw(30) << "  Subarray size:" << organization.subarray_size_kb << " KB\n";
    std::cout << std::setw(30) << "  Bank size:" << organization.bank_size_mb << " MB\n";
    std::cout << std::setw(30) << "  Chip size:" << organization.chip_size_mb << " MB\n";
    std::cout << std::setw(30) << "  Rank size:" << organization.rank_size_gb << " GB\n";
    std::cout << std::setw(30) << "  Channel capacity:" << organization.channel_capacity_gb << " GB\n\n";

    // Timing
    std::cout << "═══ Timing Parameters ═══\n";
    std::cout << std::setw(30) << "  Clock frequency:" << timing.clock_freq_mhz << " MHz\n";
    std::cout << std::setw(30) << "  Data rate:" << timing.data_rate_mtps << " MT/s\n";
    std::cout << std::setw(30) << "  tRCD:" << timing.tRCD_ns << " ns\n";
    std::cout << std::setw(30) << "  tCAS:" << timing.tCAS_ns << " ns\n";
    std::cout << std::setw(30) << "  tRP:" << timing.tRP_ns << " ns\n";
    std::cout << std::setw(30) << "  tRAS:" << timing.tRAS_ns << " ns\n\n";

    std::cout << "═══ Hierarchical Access Latencies ═══\n";
    std::cout << std::setw(30) << "  Subarray access:" << timing.subarray_access_ns << " ns\n";
    std::cout << std::setw(30) << "  Bank access:" << timing.bank_access_ns << " ns\n";
    std::cout << std::setw(30) << "  Bank group access:" << timing.bank_group_access_ns << " ns\n";
    std::cout << std::setw(30) << "  Chip access:" << timing.chip_access_ns << " ns\n";
    std::cout << std::setw(30) << "  Rank access:" << timing.rank_access_ns << " ns\n";
    std::cout << std::setw(30) << "  Channel access:" << timing.channel_access_ns << " ns\n\n";

    // Energy
    std::cout << "═══ Data Movement Energy (pJ/byte) ═══\n";
    std::cout << std::setw(30) << "  Subarray:" << energy.subarray_energy_pJ << " pJ/byte\n";
    std::cout << std::setw(30) << "  Bank:" << energy.bank_energy_pJ << " pJ/byte\n";
    std::cout << std::setw(30) << "  Bank group:" << energy.bank_group_energy_pJ << " pJ/byte\n";
    std::cout << std::setw(30) << "  Chip:" << energy.chip_energy_pJ << " pJ/byte\n";
    std::cout << std::setw(30) << "  Rank:" << energy.rank_energy_pJ << " pJ/byte\n";
    std::cout << std::setw(30) << "  Channel:" << energy.channel_energy_pJ << " pJ/byte\n\n";

    // Key insights for PIM
    std::cout << "═══ PIM Implications ═══\n";
    if (ports.bank_port_bits < 32) {
        std::cout << "  ⚠️  NARROW internal ports (" << ports.bank_port_bits << "-bit banks)\n";
        std::cout << "      → Fine-grained PIM (bank/subarray) will be bandwidth-limited!\n";
        std::cout << "      → Rank-level PIM recommended (" << ports.rank_data_bits << "-bit wide)\n";
    } else if (ports.bank_port_bits >= 64) {
        std::cout << "  ✅  WIDE internal ports (" << ports.bank_port_bits << "-bit banks)\n";
        std::cout << "      → Fine-grained PIM (bank-level) is viable!\n";
        std::cout << "      → " << getBankBandwidth() << " GB/s per bank\n";
    } else {
        std::cout << "  ⚡  MODERATE internal ports (" << ports.bank_port_bits << "-bit banks)\n";
        std::cout << "      → Bank-level PIM may work for some workloads\n";
    }

    double internal_vs_external = getBankBandwidth() / getChipIOBandwidth();
    if (internal_vs_external < 0.5) {
        std::cout << "  ⚠️  Internal BW << External I/O ("
                  << std::fixed << std::setprecision(1) << internal_vs_external * 100 << "%)\n";
        std::cout << "      → Internal bandwidth is the bottleneck!\n";
    }

    std::cout << "\n";
}

} // namespace memory
} // namespace pimid
