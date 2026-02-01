/**
 * @file internal_dram_network.cpp
 * @brief Implementation of Internal DRAM Network Model
 */

#include "memory/internal_dram_network.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace pimid {

InternalDRAMNetwork::InternalDRAMNetwork(
    const std::string& dram_type,
    std::shared_ptr<NetworkModel> network_model)
    : dram_type_(dram_type),
      num_subarrays_per_bank_(0),
      num_banks_per_bg_(0),
      num_bg_per_chip_(0),
      num_chips_per_rank_(0),
      use_custom_switch_config_(false),
      subarray_queue_limit_(DEFAULT_QUEUE_DEPTH_LIMIT),
      bank_queue_limit_(DEFAULT_QUEUE_DEPTH_LIMIT),
      bg_queue_limit_(DEFAULT_QUEUE_DEPTH_LIMIT),
      chip_queue_limit_(DEFAULT_QUEUE_DEPTH_LIMIT),
      external_network_model_(network_model),
      garnet_subarray_network_(nullptr),
      garnet_bank_network_(nullptr),
      garnet_bg_network_(nullptr),
      garnet_chip_network_(nullptr),
      use_garnet_models_(false),
      current_cycle_(0),
      total_packets_sent_(0),
      total_packets_completed_(0),
      total_bytes_transferred_(0),
      total_network_latency_(0),
      subarray_network_accesses_(0),
      bank_network_accesses_(0),
      bg_network_accesses_(0),
      chip_network_accesses_(0) {
}

void InternalDRAMNetwork::initialize(int num_subarrays_per_bank,
                                     int num_banks_per_bg,
                                     int num_bg_per_chip,
                                     int num_chips_per_rank) {
    num_subarrays_per_bank_ = num_subarrays_per_bank;
    num_banks_per_bg_ = num_banks_per_bg;
    num_bg_per_chip_ = num_bg_per_chip;
    num_chips_per_rank_ = num_chips_per_rank;

    // Configure network based on memory type (DRAM and NVM)
    if (dram_type_ == "SRAM") {
        configureSRAMNetwork();
    } else if (dram_type_ == "STT-MRAM" || dram_type_ == "STTMRAM" || dram_type_ == "MRAM") {
        configureSTTMRAMNetwork();
    } else if (dram_type_ == "PCM" || dram_type_ == "PRAM") {
        configurePCMNetwork();
    } else if (dram_type_ == "ReRAM" || dram_type_ == "RERAM") {
        configureReRAMNetwork();
    } else if (dram_type_ == "DDR3") {
        configureDDR3Network();
    } else if (dram_type_ == "DDR4" || dram_type_ == "DDR4-RVRR" || dram_type_ == "DDR4-VRR") {
        configureDDR4Network();
    } else if (dram_type_ == "DDR5" || dram_type_ == "DDR5-RVRR" || dram_type_ == "DDR5-VRR") {
        configureDDR5Network();
    } else if (dram_type_ == "LPDDR5") {
        configureLPDDR5Network();
    } else if (dram_type_ == "GDDR6") {
        configureGDDR6Network();
    } else if (dram_type_ == "HBM") {
        configureHBMNetwork();
    } else if (dram_type_ == "HBM2") {
        configureHBM2Network();
    } else if (dram_type_ == "HBM3") {
        configureHBM3Network();
    } else {
        std::cerr << "WARNING: Unknown memory type " << dram_type_
                  << ", using DDR4 defaults\n";
        configureDDR4Network();
    }

    resetStats();

    std::cout << "Internal DRAM Network Initialized (" << dram_type_ << "):\n";
    std::cout << "  Subarray network: " << subarray_network_config_.link_width_bits
              << " bits, " << subarray_network_config_.bandwidth_GBs << " GB/s\n";
    std::cout << "  Bank network:     " << bank_network_config_.link_width_bits
              << " bits, " << bank_network_config_.bandwidth_GBs << " GB/s\n";
    std::cout << "  BankGroup network:" << bg_network_config_.link_width_bits
              << " bits, " << bg_network_config_.bandwidth_GBs << " GB/s\n";
    std::cout << "  Chip network:     " << chip_network_config_.link_width_bits
              << " bits, " << chip_network_config_.bandwidth_GBs << " GB/s\n";
}

void InternalDRAMNetwork::configureDDR4Network() {
    // DDR4 has NARROW internal paths!

    // Subarray network (within bank): Uses GSA (256 bits) but serialized
    subarray_network_config_.link_width_bits = 64;  // Prefetch width
    subarray_network_config_.frequency_GHz = 1.2;    // DDR4-2400
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 5;    // Short distance
    subarray_network_config_.topology = "crossbar"; // Within bank

    // Bank network (within bank group): NARROW 8-16 bit paths
    bank_network_config_.link_width_bits = 8;       // Bank serialization bottleneck
    bank_network_config_.frequency_GHz = 1.2;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 10;       // Medium distance
    bank_network_config_.topology = "bus";          // Shared bus

    // Bank group network (within chip): Slightly wider
    bg_network_config_.link_width_bits = 16;
    bg_network_config_.frequency_GHz = 1.2;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 20;         // Longer distance
    bg_network_config_.topology = "bus";

    // Chip network (within rank): Via external pins (slow!)
    chip_network_config_.link_width_bits = 8;        // x8 device I/O
    chip_network_config_.frequency_GHz = 1.2;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 50;        // Must go off-chip!
    chip_network_config_.topology = "point-to-point";
}

void InternalDRAMNetwork::configureDDR5Network() {
    // DDR5 has wider prefetch but similar bank bottleneck

    subarray_network_config_.link_width_bits = 128;  // 16n prefetch
    subarray_network_config_.frequency_GHz = 1.6;    // DDR5-3200
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 5;
    subarray_network_config_.topology = "crossbar";

    bank_network_config_.link_width_bits = 16;       // Slightly wider than DDR4
    bank_network_config_.frequency_GHz = 1.6;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 10;
    bank_network_config_.topology = "bus";

    bg_network_config_.link_width_bits = 32;
    bg_network_config_.frequency_GHz = 1.6;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 20;
    bg_network_config_.topology = "bus";

    chip_network_config_.link_width_bits = 8;
    chip_network_config_.frequency_GHz = 1.6;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 50;
    chip_network_config_.topology = "point-to-point";
}

void InternalDRAMNetwork::configureHBM2Network() {
    // HBM2 has WIDE internal paths via TSV!

    subarray_network_config_.link_width_bits = 256;  // Wide column I/O
    subarray_network_config_.frequency_GHz = 1.0;    // HBM2
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 3;     // Short vertical distance
    subarray_network_config_.topology = "crossbar";

    // CRITICAL: HBM2 has WIDE bank paths (64 bits) via TSV!
    bank_network_config_.link_width_bits = 64;       // 8x wider than DDR4!
    bank_network_config_.frequency_GHz = 1.0;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 5;         // TSV is fast
    bank_network_config_.topology = "crossbar";      // 3D crossbar via TSV

    bg_network_config_.link_width_bits = 128;
    bg_network_config_.frequency_GHz = 1.0;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 8;
    bg_network_config_.topology = "crossbar";

    chip_network_config_.link_width_bits = 128;      // Wide channel
    chip_network_config_.frequency_GHz = 1.0;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 10;        // On-interposer
    chip_network_config_.topology = "crossbar";
}

void InternalDRAMNetwork::configureHBM3Network() {
    // HBM3 is even wider and faster

    subarray_network_config_.link_width_bits = 512;
    subarray_network_config_.frequency_GHz = 1.8;
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 3;
    subarray_network_config_.topology = "crossbar";

    bank_network_config_.link_width_bits = 128;      // Even wider
    bank_network_config_.frequency_GHz = 1.8;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 5;
    bank_network_config_.topology = "crossbar";

    bg_network_config_.link_width_bits = 256;
    bg_network_config_.frequency_GHz = 1.8;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 8;
    bg_network_config_.topology = "crossbar";

    chip_network_config_.link_width_bits = 128;
    chip_network_config_.frequency_GHz = 1.8;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 10;
    chip_network_config_.topology = "crossbar";
}

void InternalDRAMNetwork::configureDDR3Network() {
    // DDR3 has narrower internal paths than DDR4

    // Subarray network (within bank): 8n prefetch (32 bits internal)
    subarray_network_config_.link_width_bits = 32;  // 4-byte prefetch
    subarray_network_config_.frequency_GHz = 0.8;    // DDR3-1600
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 6;    // Slightly slower
    subarray_network_config_.topology = "crossbar";

    // Bank network (within rank): Very narrow paths
    bank_network_config_.link_width_bits = 8;       // 8-bit bank port
    bank_network_config_.frequency_GHz = 0.8;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 12;       // Older tech, slower
    bank_network_config_.topology = "bus";

    // Bank group network: DDR3 has no bank groups, use rank-level
    bg_network_config_.link_width_bits = 16;
    bg_network_config_.frequency_GHz = 0.8;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 25;
    bg_network_config_.topology = "bus";

    // Chip network (within rank): Via external pins
    chip_network_config_.link_width_bits = 8;        // x8 device I/O
    chip_network_config_.frequency_GHz = 0.8;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 60;        // Off-chip is slow
    chip_network_config_.topology = "point-to-point";
}

void InternalDRAMNetwork::configureLPDDR5Network() {
    // LPDDR5 optimized for mobile: Wide I/O, low power

    // Subarray network: 16n prefetch (128 bits)
    subarray_network_config_.link_width_bits = 128;  // Wide prefetch
    subarray_network_config_.frequency_GHz = 1.6;    // LPDDR5-6400
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 4;     // Optimized for mobile
    subarray_network_config_.topology = "crossbar";

    // Bank network: Wider than DDR5 for bandwidth
    bank_network_config_.link_width_bits = 16;       // 16-bit paths
    bank_network_config_.frequency_GHz = 1.6;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 8;         // Low latency
    bank_network_config_.topology = "bus";

    // Bank group network
    bg_network_config_.link_width_bits = 32;
    bg_network_config_.frequency_GHz = 1.6;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 15;
    bg_network_config_.topology = "bus";

    // Chip network: Mobile uses on-package connections
    chip_network_config_.link_width_bits = 16;       // x16 only for LPDDR5
    chip_network_config_.frequency_GHz = 1.6;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 30;        // PoP is faster than off-chip
    chip_network_config_.topology = "point-to-point";
}

void InternalDRAMNetwork::configureGDDR6Network() {
    // GDDR6 for graphics: VERY wide paths, high bandwidth

    // Subarray network: Extremely wide for graphics bandwidth
    subarray_network_config_.link_width_bits = 256;  // Wide graphics paths
    subarray_network_config_.frequency_GHz = 2.0;    // GDDR6-16000 (2 GHz)
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 4;     // Optimized for throughput
    subarray_network_config_.topology = "crossbar";

    // Bank network: Wide paths for graphics workloads
    bank_network_config_.link_width_bits = 32;       // 32-bit bank ports
    bank_network_config_.frequency_GHz = 2.0;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 6;         // Graphics optimized
    bank_network_config_.topology = "bus";

    // Bank group network: Graphics needs wide buses
    bg_network_config_.link_width_bits = 64;
    bg_network_config_.frequency_GHz = 2.0;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 10;
    bg_network_config_.topology = "bus";

    // Chip network: GDDR6 has dual channel per chip
    chip_network_config_.link_width_bits = 16;       // x8 or x16
    chip_network_config_.frequency_GHz = 2.0;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 20;        // On-board traces
    chip_network_config_.topology = "point-to-point";
}

//=============================================================================
// Non-Volatile Memory (NVM) Configurations
//=============================================================================

void InternalDRAMNetwork::configureSRAMNetwork() {
    // SRAM: On-chip memory with very fast, wide interconnects

    // Subarray network: Very wide on-chip buses
    subarray_network_config_.link_width_bits = 128;  // Wide on-chip paths
    subarray_network_config_.frequency_GHz = 2.5;    // High frequency on-chip
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 1;     // Very low latency
    subarray_network_config_.topology = "crossbar";

    // Bank network: Still on-chip, fast
    bank_network_config_.link_width_bits = 64;       // 64-bit on-chip
    bank_network_config_.frequency_GHz = 2.5;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 2;         // Fast on-chip
    bank_network_config_.topology = "crossbar";

    // Bank group network: On-chip crossbar
    bg_network_config_.link_width_bits = 64;
    bg_network_config_.frequency_GHz = 2.5;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 3;
    bg_network_config_.topology = "crossbar";

    // Chip network: On-chip, very fast
    chip_network_config_.link_width_bits = 32;
    chip_network_config_.frequency_GHz = 2.5;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 4;
    chip_network_config_.topology = "crossbar";
}

void InternalDRAMNetwork::configureSTTMRAMNetwork() {
    // STT-MRAM: Non-volatile with asymmetric read/write
    // Write latency >> read latency due to MTJ switching

    // Subarray network: Moderate width
    subarray_network_config_.link_width_bits = 64;   // Moderate width
    subarray_network_config_.frequency_GHz = 1.5;    // STT-MRAM frequency
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 3;     // Read latency
    subarray_network_config_.topology = "crossbar";

    // Bank network: Similar to DRAM but with NVM characteristics
    bank_network_config_.link_width_bits = 16;       // 16-bit paths
    bank_network_config_.frequency_GHz = 1.5;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 6;         // Moderate latency
    bank_network_config_.topology = "bus";

    // Bank group network
    bg_network_config_.link_width_bits = 32;
    bg_network_config_.frequency_GHz = 1.5;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 12;
    bg_network_config_.topology = "bus";

    // Chip network
    chip_network_config_.link_width_bits = 16;
    chip_network_config_.frequency_GHz = 1.5;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 25;
    chip_network_config_.topology = "point-to-point";
}

void InternalDRAMNetwork::configurePCMNetwork() {
    // PCM: Phase Change Memory with high write latency
    // Write >> Read due to crystallization/amorphization

    // Subarray network: Moderate width
    subarray_network_config_.link_width_bits = 64;   // Moderate width
    subarray_network_config_.frequency_GHz = 1.2;    // PCM frequency
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 4;     // Read latency
    subarray_network_config_.topology = "crossbar";

    // Bank network: Similar organization to DRAM
    bank_network_config_.link_width_bits = 16;       // 16-bit paths
    bank_network_config_.frequency_GHz = 1.2;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 8;         // Moderate latency
    bank_network_config_.topology = "bus";

    // Bank group network
    bg_network_config_.link_width_bits = 32;
    bg_network_config_.frequency_GHz = 1.2;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 16;
    bg_network_config_.topology = "bus";

    // Chip network
    chip_network_config_.link_width_bits = 16;
    chip_network_config_.frequency_GHz = 1.2;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 32;
    chip_network_config_.topology = "point-to-point";
}

void InternalDRAMNetwork::configureReRAMNetwork() {
    // ReRAM: Resistive RAM with moderate asymmetry
    // Faster writes than PCM, but still slower than reads

    // Subarray network: Moderate width
    subarray_network_config_.link_width_bits = 64;   // Moderate width
    subarray_network_config_.frequency_GHz = 1.4;    // ReRAM frequency
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 3;     // Good read latency
    subarray_network_config_.topology = "crossbar";

    // Bank network: Similar to STT-MRAM
    bank_network_config_.link_width_bits = 16;       // 16-bit paths
    bank_network_config_.frequency_GHz = 1.4;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 7;         // Moderate latency
    bank_network_config_.topology = "bus";

    // Bank group network
    bg_network_config_.link_width_bits = 32;
    bg_network_config_.frequency_GHz = 1.4;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 14;
    bg_network_config_.topology = "bus";

    // Chip network
    chip_network_config_.link_width_bits = 16;
    chip_network_config_.frequency_GHz = 1.4;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 28;
    chip_network_config_.topology = "point-to-point";
}

void InternalDRAMNetwork::configureHBMNetwork() {
    // HBM Gen 1: First generation 3D-stacked memory

    // Subarray network: Wide but not as wide as HBM2
    subarray_network_config_.link_width_bits = 256;  // HBM Gen 1 width
    subarray_network_config_.frequency_GHz = 1.0;    // HBM-1Gbps
    subarray_network_config_.bandwidth_GBs =
        (subarray_network_config_.link_width_bits / 8.0) *
        subarray_network_config_.frequency_GHz;
    subarray_network_config_.latency_cycles = 4;     // TSV benefits
    subarray_network_config_.topology = "crossbar";

    // Bank network: TSV-enabled wide paths
    bank_network_config_.link_width_bits = 64;       // 64-bit via TSV
    bank_network_config_.frequency_GHz = 1.0;
    bank_network_config_.bandwidth_GBs =
        (bank_network_config_.link_width_bits / 8.0) *
        bank_network_config_.frequency_GHz;
    bank_network_config_.latency_cycles = 6;         // 3D stacking
    bank_network_config_.topology = "crossbar";

    // Bank group network
    bg_network_config_.link_width_bits = 128;
    bg_network_config_.frequency_GHz = 1.0;
    bg_network_config_.bandwidth_GBs =
        (bg_network_config_.link_width_bits / 8.0) *
        bg_network_config_.frequency_GHz;
    bg_network_config_.latency_cycles = 10;
    bg_network_config_.topology = "crossbar";

    // Chip network: Via interposer
    chip_network_config_.link_width_bits = 128;      // Wide channel
    chip_network_config_.frequency_GHz = 1.0;
    chip_network_config_.bandwidth_GBs =
        (chip_network_config_.link_width_bits / 8.0) *
        chip_network_config_.frequency_GHz;
    chip_network_config_.latency_cycles = 12;        // On-interposer
    chip_network_config_.topology = "crossbar";
}

bool InternalDRAMNetwork::sendPacket(const InternalNetworkPacket& packet) {
    // Determine which network level is needed
    NetworkLevel level = determineNetworkLevel(packet.source_bank, packet.dest_bank,
                                              packet.source_subarray, packet.dest_subarray);

    // Get the appropriate network config and GARNET model
    InternalNetworkLink* link_config = nullptr;
    std::queue<InternalNetworkPacket>* queue = nullptr;
    std::shared_ptr<NetworkModel> garnet_network = nullptr;

    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            link_config = &subarray_network_config_;
            queue = &subarray_network_queue_;
            garnet_network = garnet_subarray_network_;
            subarray_network_accesses_++;
            break;
        case NetworkLevel::BANK_NETWORK:
            link_config = &bank_network_config_;
            queue = &bank_network_queue_;
            garnet_network = garnet_bank_network_;
            bank_network_accesses_++;
            break;
        case NetworkLevel::BANK_GROUP_NETWORK:
            link_config = &bg_network_config_;
            queue = &bg_network_queue_;
            garnet_network = garnet_bg_network_;
            bg_network_accesses_++;
            break;
        case NetworkLevel::CHIP_NETWORK:
            link_config = &chip_network_config_;
            queue = &chip_network_queue_;
            garnet_network = garnet_chip_network_;
            chip_network_accesses_++;
            break;
    }

    // Use GARNET cycle-accurate simulation if enabled
    if (use_garnet_models_ && garnet_network) {
        // Convert to NetworkPacket for GARNET
        uint32_t src_node = 0, dst_node = 0;

        // Map bank/subarray IDs to network node IDs based on level
        switch (level) {
            case NetworkLevel::SUBARRAY_NETWORK:
                src_node = static_cast<uint32_t>(packet.source_subarray);
                dst_node = static_cast<uint32_t>(packet.dest_subarray);
                break;
            case NetworkLevel::BANK_NETWORK:
                src_node = static_cast<uint32_t>(packet.source_bank % num_banks_per_bg_);
                dst_node = static_cast<uint32_t>(packet.dest_bank % num_banks_per_bg_);
                break;
            case NetworkLevel::BANK_GROUP_NETWORK:
                src_node = static_cast<uint32_t>((packet.source_bank / num_banks_per_bg_) % num_bg_per_chip_);
                dst_node = static_cast<uint32_t>((packet.dest_bank / num_banks_per_bg_) % num_bg_per_chip_);
                break;
            case NetworkLevel::CHIP_NETWORK:
                src_node = static_cast<uint32_t>((packet.source_bank / (num_banks_per_bg_ * num_bg_per_chip_)) % num_chips_per_rank_);
                dst_node = static_cast<uint32_t>((packet.dest_bank / (num_banks_per_bg_ * num_bg_per_chip_)) % num_chips_per_rank_);
                break;
        }

        // Check if GARNET can accept the packet
        if (!garnet_network->canInject(src_node)) {
            // Network congested - packet rejected
            return false;
        }

        // Create GARNET NetworkPacket
        NetworkPacket net_packet(
            src_node,
            dst_node,
            PacketType::DATA,
            static_cast<uint32_t>(packet.data_bytes),
            packet.packet_id,
            current_cycle_
        );

        // Inject into GARNET network
        garnet_network->injectPacket(net_packet);

        // Track packet in inflight list (GARNET handles actual timing)
        InternalNetworkPacket timed_packet = packet;
        timed_packet.injection_time = current_cycle_;
        timed_packet.completion_time = 0;  // Will be set when packet arrives
        timed_packet.completed = false;
        inflight_packets_.push_back(timed_packet);

        total_packets_sent_++;
        total_bytes_transferred_ += packet.data_bytes;
        return true;
    }

    // Fall back to analytical model
    uint64_t transfer_time = calculateTransferTime(*link_config, packet.data_bytes);
    uint64_t completion_time = current_cycle_ + link_config->latency_cycles + transfer_time;

    // Create packet copy with timing info
    InternalNetworkPacket timed_packet = packet;
    timed_packet.injection_time = current_cycle_;
    timed_packet.completion_time = completion_time;
    timed_packet.completed = false;

    // Add to in-flight packets
    inflight_packets_.push_back(timed_packet);

    total_packets_sent_++;
    total_bytes_transferred_ += packet.data_bytes;

    return true;
}

void InternalDRAMNetwork::tick() {
    current_cycle_++;

    // Tick GARNET networks if enabled
    if (use_garnet_models_) {
        if (garnet_subarray_network_) garnet_subarray_network_->tick();
        if (garnet_bank_network_) garnet_bank_network_->tick();
        if (garnet_bg_network_) garnet_bg_network_->tick();
        if (garnet_chip_network_) garnet_chip_network_->tick();

        // Check for arrived packets in GARNET networks
        processGarnetArrivedPackets();
    }

    processInflightPackets();
}

uint64_t InternalDRAMNetwork::getTransferLatency(NetworkLevel level,
                                                 int source_id, int dest_id,
                                                 uint64_t data_bytes) {
    InternalNetworkLink* link_config = nullptr;

    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            link_config = &subarray_network_config_;
            break;
        case NetworkLevel::BANK_NETWORK:
            link_config = &bank_network_config_;
            break;
        case NetworkLevel::BANK_GROUP_NETWORK:
            link_config = &bg_network_config_;
            break;
        case NetworkLevel::CHIP_NETWORK:
            link_config = &chip_network_config_;
            break;
    }

    uint64_t transfer_time = calculateTransferTime(*link_config, data_bytes);
    return link_config->latency_cycles + transfer_time;
}

bool InternalDRAMNetwork::canAcceptPacket(NetworkLevel level) {
    size_t current_depth = getQueueDepth(level);
    size_t limit = getQueueLimit(level);

    // If limit is 0, queue is unlimited
    if (limit == 0) {
        return true;
    }

    return current_depth < limit;
}

void InternalDRAMNetwork::setQueueLimit(NetworkLevel level, size_t limit) {
    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            subarray_queue_limit_ = limit;
            break;
        case NetworkLevel::BANK_NETWORK:
            bank_queue_limit_ = limit;
            break;
        case NetworkLevel::BANK_GROUP_NETWORK:
            bg_queue_limit_ = limit;
            break;
        case NetworkLevel::CHIP_NETWORK:
            chip_queue_limit_ = limit;
            break;
    }
}

void InternalDRAMNetwork::setQueueLimits(size_t subarray_limit, size_t bank_limit,
                                          size_t bg_limit, size_t chip_limit) {
    subarray_queue_limit_ = subarray_limit;
    bank_queue_limit_ = bank_limit;
    bg_queue_limit_ = bg_limit;
    chip_queue_limit_ = chip_limit;
}

size_t InternalDRAMNetwork::getQueueDepth(NetworkLevel level) const {
    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            return subarray_network_queue_.size();
        case NetworkLevel::BANK_NETWORK:
            return bank_network_queue_.size();
        case NetworkLevel::BANK_GROUP_NETWORK:
            return bg_network_queue_.size();
        case NetworkLevel::CHIP_NETWORK:
            return chip_network_queue_.size();
        default:
            return 0;
    }
}

size_t InternalDRAMNetwork::getQueueLimit(NetworkLevel level) const {
    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            return subarray_queue_limit_;
        case NetworkLevel::BANK_NETWORK:
            return bank_queue_limit_;
        case NetworkLevel::BANK_GROUP_NETWORK:
            return bg_queue_limit_;
        case NetworkLevel::CHIP_NETWORK:
            return chip_queue_limit_;
        default:
            return 0;
    }
}

NetworkLevel InternalDRAMNetwork::determineNetworkLevel(int source_bank, int dest_bank,
                                                       int source_subarray, int dest_subarray) {
    // If within same bank, use subarray network
    if (source_bank == dest_bank) {
        return NetworkLevel::SUBARRAY_NETWORK;
    }

    // If within same bank group (banks 0-3 vs 4-7 in typical config)
    int source_bg = source_bank / num_banks_per_bg_;
    int dest_bg = dest_bank / num_banks_per_bg_;
    if (source_bg == dest_bg) {
        return NetworkLevel::BANK_NETWORK;
    }

    // If within same chip
    int source_chip = source_bank / (num_banks_per_bg_ * num_bg_per_chip_);
    int dest_chip = dest_bank / (num_banks_per_bg_ * num_bg_per_chip_);
    if (source_chip == dest_chip) {
        return NetworkLevel::BANK_GROUP_NETWORK;
    }

    // Different chips - need chip network
    return NetworkLevel::CHIP_NETWORK;
}

uint64_t InternalDRAMNetwork::calculateTransferTime(const InternalNetworkLink& link,
                                                    uint64_t data_bytes) {
    // transfer_time = data_bytes / bandwidth_GBs / (1 / freq_GHz)
    //               = data_bytes / bandwidth_GBs * freq_GHz

    // Safety check: prevent division by zero
    if (link.bandwidth_GBs <= 0.0 || link.frequency_GHz <= 0.0) {
        std::cerr << "ERROR: Invalid link parameters - bandwidth: "
                  << link.bandwidth_GBs << " GB/s, frequency: "
                  << link.frequency_GHz << " GHz" << std::endl;
        // Return conservative estimate: 1 cycle per byte
        return std::max(data_bytes, (uint64_t)1);
    }

    double transfer_time_ns = data_bytes / link.bandwidth_GBs;
    uint64_t transfer_cycles = std::ceil(transfer_time_ns * link.frequency_GHz);
    return std::max(transfer_cycles, (uint64_t)1);
}

void InternalDRAMNetwork::processInflightPackets() {
    auto it = inflight_packets_.begin();
    while (it != inflight_packets_.end()) {
        // For GARNET-managed packets, completion_time is 0 until packet arrives
        if (it->completion_time == 0 && use_garnet_models_) {
            // Packet is being managed by GARNET, skip analytical processing
            ++it;
            continue;
        }

        if (current_cycle_ >= it->completion_time && !it->completed) {
            // Packet completed
            it->completed = true;
            total_packets_completed_++;
            total_network_latency_ += (it->completion_time - it->injection_time);

            // Call callback if exists
            if (it->callback) {
                it->callback();
            }

            // Remove from inflight
            it = inflight_packets_.erase(it);
        } else {
            ++it;
        }
    }
}

void InternalDRAMNetwork::processGarnetArrivedPackets() {
    // Helper to process arrived packets from a GARNET network
    auto process_network = [this](std::shared_ptr<NetworkModel>& network, NetworkLevel level) {
        if (!network) return;

        // Check each destination node for arrived packets
        for (const auto& node : network->getConfig().num_rows > 0 ?
             std::vector<uint32_t>(network->getConfig().num_rows) : std::vector<uint32_t>()) {
            // Note: This is a simplified check. In production, we'd track
            // which destination nodes might have packets.
        }

        // Extract arrived packets - iterate through possible destination nodes
        uint32_t max_nodes = 0;
        switch (level) {
            case NetworkLevel::SUBARRAY_NETWORK:
                max_nodes = num_subarrays_per_bank_;
                break;
            case NetworkLevel::BANK_NETWORK:
                max_nodes = num_banks_per_bg_;
                break;
            case NetworkLevel::BANK_GROUP_NETWORK:
                max_nodes = num_bg_per_chip_;
                break;
            case NetworkLevel::CHIP_NETWORK:
                max_nodes = num_chips_per_rank_;
                break;
        }

        for (uint32_t dst = 0; dst < max_nodes; dst++) {
            while (network->hasArrived(dst)) {
                NetworkPacket arrived = network->extractPacket(dst);

                // Find matching inflight packet and mark as completed
                for (auto& pkt : inflight_packets_) {
                    if (pkt.packet_id == arrived.addr && !pkt.completed && pkt.completion_time == 0) {
                        pkt.completion_time = current_cycle_;
                        pkt.completed = true;
                        total_packets_completed_++;
                        total_network_latency_ += (pkt.completion_time - pkt.injection_time);

                        if (pkt.callback) {
                            pkt.callback();
                        }
                        break;
                    }
                }
            }
        }
    };

    // Process each network level
    process_network(garnet_subarray_network_, NetworkLevel::SUBARRAY_NETWORK);
    process_network(garnet_bank_network_, NetworkLevel::BANK_NETWORK);
    process_network(garnet_bg_network_, NetworkLevel::BANK_GROUP_NETWORK);
    process_network(garnet_chip_network_, NetworkLevel::CHIP_NETWORK);

    // Clean up completed GARNET-managed packets
    inflight_packets_.erase(
        std::remove_if(inflight_packets_.begin(), inflight_packets_.end(),
                      [](const InternalNetworkPacket& pkt) { return pkt.completed; }),
        inflight_packets_.end()
    );
}

void InternalDRAMNetwork::printStats() const {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Internal DRAM Network Statistics (" << dram_type_ << ")                    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "Total Packets:             " << total_packets_sent_ << "\n";
    std::cout << "Completed Packets:         " << total_packets_completed_ << "\n";
    std::cout << "In-Flight Packets:         " << inflight_packets_.size() << "\n";
    std::cout << "Total Bytes Transferred:   " << total_bytes_transferred_ << " B ("
              << total_bytes_transferred_ / (1024.0 * 1024) << " MB)\n";

    if (total_packets_completed_ > 0) {
        std::cout << "Average Network Latency:   "
                  << static_cast<double>(total_network_latency_) / total_packets_completed_
                  << " cycles\n";
    }

    std::cout << "\nNetwork Level Usage:\n";
    std::cout << "────────────────────\n";
    std::cout << "  Subarray network:  " << subarray_network_accesses_ << " transfers\n";
    std::cout << "  Bank network:      " << bank_network_accesses_ << " transfers\n";
    std::cout << "  BankGroup network: " << bg_network_accesses_ << " transfers\n";
    std::cout << "  Chip network:      " << chip_network_accesses_ << " transfers\n";
    std::cout << "\n";
}

void InternalDRAMNetwork::resetStats() {
    total_packets_sent_ = 0;
    total_packets_completed_ = 0;
    total_bytes_transferred_ = 0;
    total_network_latency_ = 0;
    subarray_network_accesses_ = 0;
    bank_network_accesses_ = 0;
    bg_network_accesses_ = 0;
    chip_network_accesses_ = 0;
    inflight_packets_.clear();
}

uint64_t InternalDRAMNetwork::calculateNetworkRequirements(
    int pe_bank, int pe_bg, int pe_chip,
    const std::map<int, uint64_t>& data_distribution,
    std::vector<InternalDRAMTransfer>& transfers) {

    uint64_t total_latency = 0;
    transfers.clear();

    for (const auto& [bank_id, bytes] : data_distribution) {
        if (bank_id == pe_bank) {
            // Local access - no network needed
            continue;
        }

        // Remote access - need network transfer
        InternalDRAMTransfer transfer;
        transfer.source_bank = bank_id;
        transfer.dest_bank = pe_bank;
        transfer.source_subarray = -1; // Not specified
        transfer.dest_subarray = -1;
        transfer.source_bank_group = bank_id / num_banks_per_bg_;
        transfer.dest_bank_group = pe_bg;
        transfer.source_chip = bank_id / (num_banks_per_bg_ * num_bg_per_chip_);
        transfer.dest_chip = pe_chip;
        transfer.transfer_bytes = bytes;
        transfer.requires_network = true;

        // Determine network level and calculate latency
        NetworkLevel level;
        if (transfer.source_bank_group == transfer.dest_bank_group) {
            level = NetworkLevel::BANK_NETWORK;
        } else if (transfer.source_chip == transfer.dest_chip) {
            level = NetworkLevel::BANK_GROUP_NETWORK;
        } else {
            level = NetworkLevel::CHIP_NETWORK;
        }

        uint64_t latency = getTransferLatency(level, bank_id, pe_bank, bytes);
        transfer.network_latency = latency;
        total_latency += latency;

        // Determine locality
        if (transfer.source_bank_group == transfer.dest_bank_group) {
            transfer.locality = DataLocality::REMOTE_SAME_BG;
        } else if (transfer.source_chip == transfer.dest_chip) {
            transfer.locality = DataLocality::REMOTE_SAME_CHIP;
        } else {
            transfer.locality = DataLocality::REMOTE_SAME_RANK;
        }

        transfers.push_back(transfer);
    }

    return total_latency;
}

uint64_t InternalDRAMNetwork::executeGather(
    int pe_bank,
    const std::vector<int>& source_banks,
    uint64_t bytes_per_bank) {

    uint64_t total_latency = 0;

    // Create map of data distribution
    std::map<int, uint64_t> data_dist;
    for (int bank : source_banks) {
        data_dist[bank] = bytes_per_bank;
    }

    // Calculate network requirements
    std::vector<InternalDRAMTransfer> transfers;
    int pe_bg = pe_bank / num_banks_per_bg_;
    int pe_chip = pe_bank / (num_banks_per_bg_ * num_bg_per_chip_);

    total_latency = calculateNetworkRequirements(
        pe_bank, pe_bg, pe_chip, data_dist, transfers);

    return total_latency;
}

uint64_t InternalDRAMNetwork::executeScatter(
    int pe_bank,
    const std::vector<int>& dest_banks,
    uint64_t bytes_per_bank) {

    uint64_t total_latency = 0;

    for (int dest_bank : dest_banks) {
        if (dest_bank == pe_bank) continue; // No transfer needed

        NetworkLevel level;
        int pe_bg = pe_bank / num_banks_per_bg_;
        int dest_bg = dest_bank / num_banks_per_bg_;

        if (pe_bg == dest_bg) {
            level = NetworkLevel::BANK_NETWORK;
        } else {
            level = NetworkLevel::BANK_GROUP_NETWORK;
        }

        uint64_t latency = getTransferLatency(level, pe_bank, dest_bank, bytes_per_bank);
        total_latency += latency;
    }

    return total_latency;
}

uint64_t InternalDRAMNetwork::executeReduce(
    const std::vector<int>& source_banks,
    int dest_bank,
    uint64_t bytes_per_bank) {

    // Similar to gather, but with reduction operation overhead
    uint64_t gather_latency = executeGather(dest_bank, source_banks, bytes_per_bank);

    // Add reduction computation overhead (simplified model)
    uint64_t reduction_overhead = source_banks.size() * 10; // 10 cycles per source

    return gather_latency + reduction_overhead;
}

uint64_t InternalDRAMNetwork::executeBroadcast(
    int source_bank,
    const std::vector<int>& dest_banks,
    uint64_t total_bytes) {

    // Broadcast can potentially use multicast if network supports it
    // For now, model as series of point-to-point transfers
    uint64_t bytes_per_dest = total_bytes; // Full copy to each destination

    return executeScatter(source_bank, dest_banks, bytes_per_dest);
}

double InternalDRAMNetwork::getAvailableBandwidth(NetworkLevel level) const {
    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            return subarray_network_config_.bandwidth_GBs;
        case NetworkLevel::BANK_NETWORK:
            return bank_network_config_.bandwidth_GBs;
        case NetworkLevel::BANK_GROUP_NETWORK:
            return bg_network_config_.bandwidth_GBs;
        case NetworkLevel::CHIP_NETWORK:
            return chip_network_config_.bandwidth_GBs;
        default:
            return 0.0;
    }
}

bool InternalDRAMNetwork::inSameBankGroup(int bank1, int bank2) const {
    int bg1 = bank1 / num_banks_per_bg_;
    int bg2 = bank2 / num_banks_per_bg_;
    return bg1 == bg2;
}

bool InternalDRAMNetwork::inSameChip(int bank1, int bank2) const {
    int chip1 = bank1 / (num_banks_per_bg_ * num_bg_per_chip_);
    int chip2 = bank2 / (num_banks_per_bg_ * num_bg_per_chip_);
    return chip1 == chip2;
}

void InternalDRAMNetwork::enableGarnetSimulation(bool enable) {
    use_garnet_models_ = enable;

    if (enable) {
        std::cout << "\n[InternalDRAMNetwork] Enabling GARNET H-tree simulation" << std::endl;
        std::cout << "  This will provide cycle-accurate NoC modeling with:" << std::endl;
        std::cout << "    - Contention and queuing delays" << std::endl;
        std::cout << "    - Router pipeline simulation" << std::endl;
        std::cout << "    - Accurate power/energy modeling" << std::endl;
        std::cout << "    - Virtual channel flow control\n" << std::endl;

        // Create GARNET H-tree network for subarray level (within bank)
        garnet_subarray_network_ = createGarnetHTreeForDRAM(
            NetworkLevel::SUBARRAY_NETWORK,
            num_subarrays_per_bank_,
            subarray_network_config_.link_width_bits,
            subarray_network_config_.latency_cycles,
            subarray_network_config_.bandwidth_GBs);

        // Create GARNET network for bank level (within bank group)
        garnet_bank_network_ = createGarnetHTreeForDRAM(
            NetworkLevel::BANK_NETWORK,
            num_banks_per_bg_,
            bank_network_config_.link_width_bits,
            bank_network_config_.latency_cycles,
            bank_network_config_.bandwidth_GBs);

        // Create GARNET network for bank group level (within chip)
        garnet_bg_network_ = createGarnetHTreeForDRAM(
            NetworkLevel::BANK_GROUP_NETWORK,
            num_bg_per_chip_,
            bg_network_config_.link_width_bits,
            bg_network_config_.latency_cycles,
            bg_network_config_.bandwidth_GBs);

        // Create GARNET network for chip level (within rank)
        garnet_chip_network_ = createGarnetHTreeForDRAM(
            NetworkLevel::CHIP_NETWORK,
            num_chips_per_rank_,
            chip_network_config_.link_width_bits,
            chip_network_config_.latency_cycles,
            chip_network_config_.bandwidth_GBs);

        std::cout << "[InternalDRAMNetwork] GARNET networks created for all levels\n" << std::endl;
    } else {
        std::cout << "[InternalDRAMNetwork] Using analytical network model (faster)" << std::endl;
        garnet_subarray_network_ = nullptr;
        garnet_bank_network_ = nullptr;
        garnet_bg_network_ = nullptr;
        garnet_chip_network_ = nullptr;
    }
}

std::shared_ptr<InternalDRAMNetwork> createInternalDRAMNetwork(
    const std::string& dram_type,
    int num_subarrays_per_bank,
    int num_banks_per_bg,
    int num_bg_per_chip,
    int num_chips_per_rank) {

    auto network = std::make_shared<InternalDRAMNetwork>(dram_type);
    network->initialize(num_subarrays_per_bank, num_banks_per_bg,
                       num_bg_per_chip, num_chips_per_rank);
    return network;
}

std::shared_ptr<NetworkModel> createGarnetHTreeForDRAM(
    NetworkLevel level,
    int num_nodes,
    int link_width_bits,
    int link_latency_cycles,
    double bandwidth_GBs) {

    // ========================================
    // Input Validation
    // ========================================

    // Validate number of nodes
    if (num_nodes <= 0) {
        throw std::invalid_argument("Number of nodes must be positive (got " +
                                   std::to_string(num_nodes) + ")");
    }

    if (num_nodes > 1024) {
        throw std::invalid_argument("Number of nodes exceeds reasonable limit (got " +
                                   std::to_string(num_nodes) + ", max 1024)");
    }

    // Validate link width
    if (link_width_bits <= 0) {
        throw std::invalid_argument("Link width must be positive (got " +
                                   std::to_string(link_width_bits) + " bits)");
    }

    // Link width should be a power of 2 and at least 8 bits (1 byte)
    if (link_width_bits < 8 || (link_width_bits & (link_width_bits - 1)) != 0) {
        throw std::invalid_argument("Link width must be a power of 2 and >= 8 bits (got " +
                                   std::to_string(link_width_bits) + " bits)");
    }

    if (link_width_bits > 1024) {
        throw std::invalid_argument("Link width exceeds reasonable limit (got " +
                                   std::to_string(link_width_bits) + " bits, max 1024)");
    }

    // Validate link latency
    if (link_latency_cycles < 0) {
        throw std::invalid_argument("Link latency cannot be negative (got " +
                                   std::to_string(link_latency_cycles) + " cycles)");
    }

    if (link_latency_cycles > 1000) {
        throw std::invalid_argument("Link latency exceeds reasonable limit (got " +
                                   std::to_string(link_latency_cycles) + " cycles, max 1000)");
    }

    // Validate bandwidth
    if (bandwidth_GBs < 0.0) {
        throw std::invalid_argument("Bandwidth cannot be negative (got " +
                                   std::to_string(bandwidth_GBs) + " GB/s)");
    }

    if (bandwidth_GBs > 10000.0) {
        throw std::invalid_argument("Bandwidth exceeds reasonable limit (got " +
                                   std::to_string(bandwidth_GBs) + " GB/s, max 10000)");
    }

    // Validate bandwidth consistency with link parameters
    // BW should approximately equal (link_width_bits / 8) * frequency
    // Since we don't have frequency, we'll just check if it's not zero when link_width > 0
    if (link_width_bits > 0 && bandwidth_GBs == 0.0) {
        throw std::invalid_argument("Bandwidth is zero despite non-zero link width (" +
                                   std::to_string(link_width_bits) + " bits)");
    }

    std::cout << "[GARNET H-Tree] Creating H-tree network for ";
    switch (level) {
        case NetworkLevel::SUBARRAY_NETWORK:
            std::cout << "SUBARRAY level";
            break;
        case NetworkLevel::BANK_NETWORK:
            std::cout << "BANK level";
            break;
        case NetworkLevel::BANK_GROUP_NETWORK:
            std::cout << "BANK_GROUP level";
            break;
        case NetworkLevel::CHIP_NETWORK:
            std::cout << "CHIP level";
            break;
    }
    std::cout << " (" << num_nodes << " nodes)" << std::endl;

    // Create network configuration for H-tree
    NetworkConfig config;
    config.topology = NetworkTopology::H_TREE;
    config.routing = RoutingAlgorithm::TREE_BASED;
    config.flow_control = FlowControl::CREDIT_BASED;

    // H-tree is a binary tree, so we need log2(num_nodes) levels
    // For simplicity, we'll model it as having num_nodes leaf nodes
    config.num_rows = num_nodes;  // Number of leaf nodes (subarrays/banks)
    config.num_cols = 1;
    config.num_layers = 1;

    // Virtual Networks (VN) and Virtual Channels (VC) for DRAM
    // VN = message classes (read vs write traffic)
    // VC = deadlock avoidance within each VN
    config.virtual_networks = 2;          // VN 0: Reads, VN 1: Writes
    config.virtual_channels_per_vn = 1;   // 1 VC per VN (no deadlock in tree)
    config.virtual_channels = config.virtual_networks * config.virtual_channels_per_vn;

    // Link parameters from DRAM specs
    config.link_width_bytes = link_width_bits / 8;
    config.link_latency = link_latency_cycles;

    // Router pipeline - DRAM uses MINIMAL (just muxes, no complex routing)
    // This is critical for realistic DRAM modeling!
    config.router_pipeline = RouterPipelineComplexity::MINIMAL;
    config.router_latency = 1;            // 1 cycle for mux switching
    config.enable_router_bypass = true;   // Bypass for single-hop

    // Buffer depths - keep small for DRAM (limited buffering)
    // DRAM has minimal buffering (sense amp latches, column latches)
    config.input_buffer_depth = 2;        // Minimal buffering
    config.output_buffer_depth = 2;

    // Create GARNET model
    auto garnet = std::make_shared<GarnetModel>(config);
    garnet->initialize();

    // Build H-tree topology
    // In an H-tree, each leaf node connects to the root through log2(N) intermediate routers
    // For now, we'll create a simplified model where all leaf nodes are connected

    // Add leaf nodes (these represent subarrays/banks)
    for (int i = 0; i < num_nodes; i++) {
        NetworkNode node(i, PEPlacementLevel::SUBARRAY, i);
        garnet->addNode(node);
    }

    std::cout << "[GARNET H-Tree] Configuration:" << std::endl;
    std::cout << "  Leaf nodes: " << num_nodes << std::endl;
    std::cout << "  Link width: " << config.link_width_bytes << " bytes ("
              << link_width_bits << " bits)" << std::endl;
    std::cout << "  Link latency: " << link_latency_cycles << " cycles" << std::endl;
    std::cout << "  Bandwidth: " << bandwidth_GBs << " GB/s" << std::endl;
    std::cout << "  Virtual Networks (VN): " << config.virtual_networks
              << " (VN0=Read, VN1=Write)" << std::endl;
    std::cout << "  Virtual Channels per VN: " << config.virtual_channels_per_vn << std::endl;
    std::cout << "  Total VCs: " << config.virtual_channels << std::endl;
    std::cout << "  Router pipeline: MINIMAL (1-stage mux, lightweight)" << std::endl;
    std::cout << "  Router latency: " << config.router_latency << " cycle" << std::endl;
    std::cout << "  Buffer depth: " << config.input_buffer_depth
              << " (minimal, DRAM-realistic)" << std::endl;

    return garnet;
}

//=============================================================================
// Switch Calculation Functions
//=============================================================================

int InternalDRAMNetwork::getNumberOfSwitchLevels() const {
    /**
     * Network hierarchy based on DRAM organization:
     * L0: Banks in a BG share one L0 switch → 1 per BG
     * L1: Each BG has 1 L1 switch connecting all its banks → 1 per BG
     * L2: Each chip has 1 L2 switch connecting all its BGs → 1 per chip
     * L3: Each rank has 1 L3 switch connecting all its chips → 1 per rank
     * L4: Each MC has 1 L4 switch connecting ranks in one channel → 1 per channel
     * L5: One root switch connecting all channels → 1 total
     *
     * Total: 6 levels (L0 through L5)
     */
    return 6;
}

int InternalDRAMNetwork::getTotalNumberOfSwitches(int num_channels, int ranks_per_channel) const {
    // Calculate the number of each type of unit in the hierarchy
    int num_ranks = num_channels * ranks_per_channel;
    int num_chips = num_ranks * num_chips_per_rank_;
    int num_bgs = num_chips * num_bg_per_chip_;

    // Calculate switches at each level
    int l0_switches = num_bgs;  // 1 per BG
    int l1_switches = num_bgs;  // 1 per BG
    int l2_switches = num_chips;  // 1 per chip
    int l3_switches = num_ranks;  // 1 per rank
    int l4_switches = num_channels;  // 1 per channel
    int l5_switches = 1;  // 1 root switch

    // Total switches
    int total_switches = l0_switches + l1_switches + l2_switches +
                        l3_switches + l4_switches + l5_switches;

    std::cout << "[InternalDRAMNetwork] Switch hierarchy calculation:" << std::endl;
    std::cout << "  Configuration:" << std::endl;
    std::cout << "    Channels: " << num_channels << std::endl;
    std::cout << "    Ranks per channel: " << ranks_per_channel << std::endl;
    std::cout << "    Chips per rank: " << num_chips_per_rank_ << std::endl;
    std::cout << "    Bank groups per chip: " << num_bg_per_chip_ << std::endl;
    std::cout << "    Banks per BG: " << num_banks_per_bg_ << std::endl;
    std::cout << std::endl;
    std::cout << "  Hierarchy totals:" << std::endl;
    std::cout << "    Total ranks: " << num_ranks << std::endl;
    std::cout << "    Total chips: " << num_chips << std::endl;
    std::cout << "    Total bank groups: " << num_bgs << std::endl;
    std::cout << "    Total banks: " << (num_bgs * num_banks_per_bg_) << std::endl;
    std::cout << std::endl;
    std::cout << "  Switch counts by level:" << std::endl;
    std::cout << "    L0 (BG level): " << l0_switches << " switches" << std::endl;
    std::cout << "    L1 (BG level): " << l1_switches << " switches" << std::endl;
    std::cout << "    L2 (Chip level): " << l2_switches << " switches" << std::endl;
    std::cout << "    L3 (Rank level): " << l3_switches << " switches" << std::endl;
    std::cout << "    L4 (Channel level): " << l4_switches << " switches" << std::endl;
    std::cout << "    L5 (System level): " << l5_switches << " switch" << std::endl;
    std::cout << "    TOTAL: " << total_switches << " switches across "
              << getNumberOfSwitchLevels() << " levels" << std::endl;

    return total_switches;
}

int InternalDRAMNetwork::getNumberOfSwitchesAtLevel(int level, int num_channels, int ranks_per_channel) const {
    if (level < 0 || level > 5) {
        std::cerr << "[InternalDRAMNetwork] ERROR: Invalid switch level " << level
                  << " (valid range: 0-5)" << std::endl;
        return 0;
    }

    // If using custom configuration, return custom values
    if (use_custom_switch_config_) {
        const NetworkLevelConfig* config = nullptr;
        switch (level) {
            case 0: config = &custom_switch_config_.l0_config; break;
            case 1: config = &custom_switch_config_.l1_config; break;
            case 2: config = &custom_switch_config_.l2_config; break;
            case 3: config = &custom_switch_config_.l3_config; break;
            case 4: config = &custom_switch_config_.l4_config; break;
            case 5: config = &custom_switch_config_.l5_config; break;
        }
        if (config && !config->use_default) {
            return config->num_switches;
        }
    }

    // Calculate the number of each type of unit in the hierarchy
    int num_ranks = num_channels * ranks_per_channel;
    int num_chips = num_ranks * num_chips_per_rank_;
    int num_bgs = num_chips * num_bg_per_chip_;

    switch (level) {
        case 0:  // L0: BG level (internal to BG)
            return num_bgs;
        case 1:  // L1: BG level (connecting to chip level)
            return num_bgs;
        case 2:  // L2: Chip level
            return num_chips;
        case 3:  // L3: Rank level
            return num_ranks;
        case 4:  // L4: Channel level
            return num_channels;
        case 5:  // L5: System level (root)
            return 1;
        default:
            return 0;
    }
}

//=============================================================================
// Custom Topology Configuration Functions
//=============================================================================

void InternalDRAMNetwork::setCustomSwitchHierarchy(const SwitchHierarchyConfig& config) {
    custom_switch_config_ = config;
    use_custom_switch_config_ = true;

    std::cout << "[InternalDRAMNetwork] Custom switch hierarchy configured:" << std::endl;

    auto print_level = [](const std::string& name, const NetworkLevelConfig& cfg) {
        if (!cfg.use_default) {
            std::cout << "  " << name << ":" << std::endl;
            std::cout << "    Topology: " << getTopologyName(cfg.topology) << std::endl;
            std::cout << "    Switches: " << cfg.num_switches << std::endl;
            std::cout << "    Ports per switch: " << cfg.ports_per_switch << std::endl;
            std::cout << "    Endpoints: " << cfg.num_endpoints << std::endl;
        }
    };

    print_level("L0 (BG internal)", config.l0_config);
    print_level("L1 (BG to chip)", config.l1_config);
    print_level("L2 (Chip level)", config.l2_config);
    print_level("L3 (Rank level)", config.l3_config);
    print_level("L4 (Channel level)", config.l4_config);
    print_level("L5 (System level)", config.l5_config);
}

SwitchHierarchyConfig InternalDRAMNetwork::getSwitchHierarchyConfig(int num_channels, int ranks_per_channel) const {
    SwitchHierarchyConfig config;
    config.num_channels = num_channels;
    config.ranks_per_channel = ranks_per_channel;

    // Calculate hierarchy
    int num_ranks = num_channels * ranks_per_channel;
    int num_chips = num_ranks * num_chips_per_rank_;
    int num_bgs = num_chips * num_bg_per_chip_;
    int num_banks = num_bgs * num_banks_per_bg_;

    // L0: BG internal (banks within BG)
    config.l0_config.level = 0;
    config.l0_config.topology = TopologyType::CROSSBAR;
    config.l0_config.num_switches = num_bgs;
    config.l0_config.num_endpoints = num_banks_per_bg_;
    config.l0_config.ports_per_switch = calculatePortsPerSwitch(
        config.l0_config.topology, config.l0_config.num_endpoints, 1);

    // L1: BG to chip (BGs within chip)
    config.l1_config.level = 1;
    config.l1_config.topology = TopologyType::BUS;
    config.l1_config.num_switches = num_bgs;
    config.l1_config.num_endpoints = num_bg_per_chip_;
    config.l1_config.ports_per_switch = calculatePortsPerSwitch(
        config.l1_config.topology, config.l1_config.num_endpoints, 1);

    // L2: Chip level (chips within rank)
    config.l2_config.level = 2;
    config.l2_config.topology = TopologyType::BUS;
    config.l2_config.num_switches = num_chips;
    config.l2_config.num_endpoints = num_chips_per_rank_;
    config.l2_config.ports_per_switch = calculatePortsPerSwitch(
        config.l2_config.topology, config.l2_config.num_endpoints, 1);

    // L3: Rank level (ranks within channel)
    config.l3_config.level = 3;
    config.l3_config.topology = TopologyType::BUS;
    config.l3_config.num_switches = num_ranks;
    config.l3_config.num_endpoints = ranks_per_channel;
    config.l3_config.ports_per_switch = calculatePortsPerSwitch(
        config.l3_config.topology, config.l3_config.num_endpoints, 1);

    // L4: Channel level (channels to system)
    config.l4_config.level = 4;
    config.l4_config.topology = TopologyType::CROSSBAR;
    config.l4_config.num_switches = num_channels;
    config.l4_config.num_endpoints = num_channels;
    config.l4_config.ports_per_switch = calculatePortsPerSwitch(
        config.l4_config.topology, config.l4_config.num_endpoints, 1);

    // L5: System level (root switch)
    config.l5_config.level = 5;
    config.l5_config.topology = TopologyType::CROSSBAR;
    config.l5_config.num_switches = 1;
    config.l5_config.num_endpoints = num_channels;
    config.l5_config.ports_per_switch = calculatePortsPerSwitch(
        config.l5_config.topology, config.l5_config.num_endpoints, 1);

    // If custom config exists, overlay it
    if (use_custom_switch_config_) {
        if (!custom_switch_config_.l0_config.use_default) config.l0_config = custom_switch_config_.l0_config;
        if (!custom_switch_config_.l1_config.use_default) config.l1_config = custom_switch_config_.l1_config;
        if (!custom_switch_config_.l2_config.use_default) config.l2_config = custom_switch_config_.l2_config;
        if (!custom_switch_config_.l3_config.use_default) config.l3_config = custom_switch_config_.l3_config;
        if (!custom_switch_config_.l4_config.use_default) config.l4_config = custom_switch_config_.l4_config;
        if (!custom_switch_config_.l5_config.use_default) config.l5_config = custom_switch_config_.l5_config;
    }

    return config;
}

int InternalDRAMNetwork::calculatePortsPerSwitch(TopologyType topology, int num_endpoints, int num_switches) {
    if (num_switches == 0 || num_endpoints == 0) {
        return 0;
    }

    switch (topology) {
        case TopologyType::BUS:
            // Bus: 1 switch connects all endpoints
            return num_endpoints;

        case TopologyType::CROSSBAR:
            // Crossbar: N×N connections
            return num_endpoints;

        case TopologyType::MESH_2D: {
            // 2D Mesh: Each switch has ~4-5 ports (4 directions + local)
            // Total switches = sqrt(N) × sqrt(N)
            int sqrt_n = static_cast<int>(std::ceil(std::sqrt(num_endpoints / num_switches)));
            return 5;  // N, S, E, W, Local
        }

        case TopologyType::TORUS_2D: {
            // 2D Torus: Same as mesh but wrap-around
            return 5;  // N, S, E, W, Local
        }

        case TopologyType::FAT_TREE: {
            // Fat-tree: k-port switches, k/2 up, k/2 down
            // For simplicity, assume balanced tree
            int endpoints_per_switch = (num_endpoints + num_switches - 1) / num_switches;
            return endpoints_per_switch + 2;  // Endpoints + up/down links
        }

        case TopologyType::H_TREE: {
            // H-tree: Binary tree, log2(N) levels
            // Each router has 2-4 ports depending on position
            return 4;  // Parent + 2 children + local (average)
        }

        case TopologyType::CUSTOM:
            // User must specify
            return 0;

        default:
            return num_endpoints;
    }
}

int InternalDRAMNetwork::calculateOptimalSwitchCount(int num_endpoints, int max_ports_per_switch, TopologyType topology) {
    if (num_endpoints == 0 || max_ports_per_switch == 0) {
        return 1;
    }

    switch (topology) {
        case TopologyType::BUS:
        case TopologyType::CROSSBAR:
            // For bus/crossbar, calculate how many switches needed
            // to keep each under the port limit
            return (num_endpoints + max_ports_per_switch - 1) / max_ports_per_switch;

        case TopologyType::MESH_2D:
        case TopologyType::TORUS_2D: {
            // Mesh/Torus: sqrt(N) × sqrt(N) switches
            int sqrt_n = static_cast<int>(std::ceil(std::sqrt(num_endpoints)));
            return sqrt_n * sqrt_n;
        }

        case TopologyType::FAT_TREE: {
            // Fat-tree with k-port switches
            // Number of switches ≈ 5N/k for k-ary fat tree
            int k = max_ports_per_switch;
            return (5 * num_endpoints + k - 1) / k;
        }

        case TopologyType::H_TREE: {
            // H-tree: Binary tree requires (N-1) internal nodes for N leaves
            return num_endpoints - 1;
        }

        case TopologyType::CUSTOM:
            // Must be specified by user
            return 1;

        default:
            return 1;
    }
}

std::string InternalDRAMNetwork::getTopologyName(TopologyType topology) {
    switch (topology) {
        case TopologyType::BUS:        return "Bus";
        case TopologyType::CROSSBAR:   return "Crossbar";
        case TopologyType::MESH_2D:    return "2D Mesh";
        case TopologyType::TORUS_2D:   return "2D Torus";
        case TopologyType::FAT_TREE:   return "Fat-Tree";
        case TopologyType::H_TREE:     return "H-Tree";
        case TopologyType::CUSTOM:     return "Custom";
        default:                        return "Unknown";
    }
}

//=============================================================================
// Factory Functions for Network Creation at Different Hierarchy Levels
//=============================================================================

namespace {
    // Helper to get network parameters based on DRAM type
    struct NetworkParams {
        int link_width_bits;
        int link_latency_cycles;
        double bandwidth_GBs;
    };

    NetworkParams getSubarrayParams(const std::string& dram_type) {
        if (dram_type == "HBM3") return {512, 3, 115.2};
        if (dram_type == "HBM2") return {256, 3, 32.0};
        if (dram_type == "HBM" || dram_type == "HBM1") return {256, 4, 32.0};
        if (dram_type == "DDR5") return {128, 5, 25.6};
        if (dram_type == "GDDR6") return {256, 4, 64.0};
        if (dram_type == "LPDDR5") return {128, 4, 25.6};
        if (dram_type == "SRAM") return {128, 1, 40.0};
        if (dram_type == "STT-MRAM" || dram_type == "STTMRAM") return {64, 3, 12.0};
        if (dram_type == "PCM" || dram_type == "PRAM") return {64, 4, 9.6};
        if (dram_type == "ReRAM" || dram_type == "RERAM") return {64, 3, 11.2};
        // Default: DDR4
        return {64, 5, 9.6};
    }

    NetworkParams getBankParams(const std::string& dram_type) {
        if (dram_type == "HBM3") return {128, 5, 28.8};
        if (dram_type == "HBM2") return {64, 5, 8.0};
        if (dram_type == "HBM" || dram_type == "HBM1") return {64, 6, 8.0};
        if (dram_type == "DDR5") return {16, 10, 3.2};
        if (dram_type == "GDDR6") return {32, 6, 8.0};
        if (dram_type == "LPDDR5") return {16, 8, 3.2};
        if (dram_type == "SRAM") return {64, 2, 20.0};
        if (dram_type == "STT-MRAM" || dram_type == "STTMRAM") return {16, 6, 3.0};
        if (dram_type == "PCM" || dram_type == "PRAM") return {16, 8, 2.4};
        if (dram_type == "ReRAM" || dram_type == "RERAM") return {16, 7, 2.8};
        // Default: DDR4
        return {8, 10, 1.2};
    }

    NetworkParams getBankGroupParams(const std::string& dram_type) {
        if (dram_type == "HBM3") return {256, 8, 57.6};
        if (dram_type == "HBM2") return {128, 8, 16.0};
        if (dram_type == "HBM" || dram_type == "HBM1") return {128, 10, 16.0};
        if (dram_type == "DDR5") return {32, 20, 6.4};
        if (dram_type == "GDDR6") return {64, 10, 16.0};
        if (dram_type == "LPDDR5") return {32, 15, 6.4};
        if (dram_type == "SRAM") return {64, 3, 20.0};
        if (dram_type == "STT-MRAM" || dram_type == "STTMRAM") return {32, 12, 6.0};
        if (dram_type == "PCM" || dram_type == "PRAM") return {32, 16, 4.8};
        if (dram_type == "ReRAM" || dram_type == "RERAM") return {32, 14, 5.6};
        // Default: DDR4
        return {16, 20, 2.4};
    }

    NetworkParams getChipParams(const std::string& dram_type) {
        if (dram_type == "HBM3") return {128, 10, 28.8};
        if (dram_type == "HBM2") return {128, 10, 16.0};
        if (dram_type == "HBM" || dram_type == "HBM1") return {128, 12, 16.0};
        if (dram_type == "DDR5") return {8, 50, 1.6};
        if (dram_type == "GDDR6") return {16, 20, 4.0};
        if (dram_type == "LPDDR5") return {16, 30, 3.2};
        if (dram_type == "SRAM") return {32, 4, 10.0};
        if (dram_type == "STT-MRAM" || dram_type == "STTMRAM") return {16, 25, 3.0};
        if (dram_type == "PCM" || dram_type == "PRAM") return {16, 32, 2.4};
        if (dram_type == "ReRAM" || dram_type == "RERAM") return {16, 28, 2.8};
        // Default: DDR4
        return {8, 50, 1.2};
    }
}

std::shared_ptr<NetworkModel> createSubarrayNetwork(
    const std::string& dram_type,
    int num_subarrays,
    bool use_garnet) {

    if (use_garnet) {
        auto params = getSubarrayParams(dram_type);
        return createGarnetHTreeForDRAM(
            NetworkLevel::SUBARRAY_NETWORK,
            num_subarrays,
            params.link_width_bits,
            params.link_latency_cycles,
            params.bandwidth_GBs
        );
    }

    // Create simple analytical model via GarnetModel with H-tree topology
    NetworkConfig config;
    config.topology = NetworkTopology::H_TREE;
    config.routing = RoutingAlgorithm::TREE_BASED;
    config.num_rows = num_subarrays;
    config.num_cols = 1;
    config.router_pipeline = RouterPipelineComplexity::MINIMAL;
    config.router_latency = 1;

    auto params = getSubarrayParams(dram_type);
    config.link_width_bytes = params.link_width_bits / 8;
    config.link_latency = params.link_latency_cycles;

    auto model = std::make_shared<GarnetModel>(config);
    model->initialize();
    return model;
}

std::shared_ptr<NetworkModel> createBankNetwork(
    const std::string& dram_type,
    int num_banks,
    bool use_garnet) {

    if (use_garnet) {
        auto params = getBankParams(dram_type);
        return createGarnetHTreeForDRAM(
            NetworkLevel::BANK_NETWORK,
            num_banks,
            params.link_width_bits,
            params.link_latency_cycles,
            params.bandwidth_GBs
        );
    }

    // Create simple model
    NetworkConfig config;
    config.topology = NetworkTopology::CROSSBAR;  // Banks often use bus/crossbar
    config.routing = RoutingAlgorithm::MINIMAL;
    config.num_rows = num_banks;
    config.num_cols = 1;
    config.router_pipeline = RouterPipelineComplexity::SIMPLE;
    config.router_latency = 2;

    auto params = getBankParams(dram_type);
    config.link_width_bytes = params.link_width_bits / 8;
    config.link_latency = params.link_latency_cycles;

    auto model = std::make_shared<GarnetModel>(config);
    model->initialize();
    return model;
}

std::shared_ptr<NetworkModel> createBankGroupNetwork(
    const std::string& dram_type,
    int num_bank_groups,
    bool use_garnet) {

    if (use_garnet) {
        auto params = getBankGroupParams(dram_type);
        return createGarnetHTreeForDRAM(
            NetworkLevel::BANK_GROUP_NETWORK,
            num_bank_groups,
            params.link_width_bits,
            params.link_latency_cycles,
            params.bandwidth_GBs
        );
    }

    // Create simple model
    NetworkConfig config;
    config.topology = NetworkTopology::CROSSBAR;
    config.routing = RoutingAlgorithm::MINIMAL;
    config.num_rows = num_bank_groups;
    config.num_cols = 1;
    config.router_pipeline = RouterPipelineComplexity::SIMPLE;
    config.router_latency = 2;

    auto params = getBankGroupParams(dram_type);
    config.link_width_bytes = params.link_width_bits / 8;
    config.link_latency = params.link_latency_cycles;

    auto model = std::make_shared<GarnetModel>(config);
    model->initialize();
    return model;
}

std::shared_ptr<NetworkModel> createChipNetwork(
    const std::string& dram_type,
    int num_chips,
    bool use_garnet) {

    if (use_garnet) {
        auto params = getChipParams(dram_type);
        return createGarnetHTreeForDRAM(
            NetworkLevel::CHIP_NETWORK,
            num_chips,
            params.link_width_bits,
            params.link_latency_cycles,
            params.bandwidth_GBs
        );
    }

    // Create simple model - chips typically use point-to-point or crossbar
    NetworkConfig config;
    config.topology = NetworkTopology::CROSSBAR;
    config.routing = RoutingAlgorithm::MINIMAL;
    config.num_rows = num_chips;
    config.num_cols = 1;
    config.router_pipeline = RouterPipelineComplexity::REDUCED;
    config.router_latency = 3;

    auto params = getChipParams(dram_type);
    config.link_width_bytes = params.link_width_bits / 8;
    config.link_latency = params.link_latency_cycles;

    auto model = std::make_shared<GarnetModel>(config);
    model->initialize();
    return model;
}

std::shared_ptr<InternalDRAMNetwork> createHierarchicalNetwork(
    const std::string& dram_type,
    int num_subarrays_per_bank,
    int num_banks_per_bg,
    int num_bg_per_chip,
    int num_chips_per_rank,
    bool use_garnet) {

    std::cout << "\n[Factory] Creating hierarchical network for " << dram_type << ":" << std::endl;
    std::cout << "  Subarrays per bank: " << num_subarrays_per_bank << std::endl;
    std::cout << "  Banks per BG: " << num_banks_per_bg << std::endl;
    std::cout << "  BGs per chip: " << num_bg_per_chip << std::endl;
    std::cout << "  Chips per rank: " << num_chips_per_rank << std::endl;
    std::cout << "  GARNET mode: " << (use_garnet ? "enabled (cycle-accurate)" : "disabled (analytical)") << std::endl;

    auto network = std::make_shared<InternalDRAMNetwork>(dram_type);
    network->initialize(num_subarrays_per_bank, num_banks_per_bg,
                       num_bg_per_chip, num_chips_per_rank);

    if (use_garnet) {
        network->enableGarnetSimulation(true);
    }

    std::cout << "[Factory] Hierarchical network created successfully\n" << std::endl;
    return network;
}

} // namespace pimid
