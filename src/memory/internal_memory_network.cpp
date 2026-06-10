/**
 * @file internal_memory_network.cpp
 * @brief Implementation of generic internal memory network for all technologies
 */

#include "memory/internal_memory_network.h"
#include <stdexcept>
#include <iostream>

namespace pimid {

MemoryTechnologyType getMemoryTechnologyType(const std::string& tech_name) {
    if (tech_name == "SRAM") return MemoryTechnologyType::SRAM;
    if (tech_name == "DDR3" || tech_name == "DDR4" || tech_name == "DDR5" ||
        tech_name == "DDR4-RVRR" || tech_name == "DDR4-VRR" ||
        tech_name == "DDR5-RVRR" || tech_name == "DDR5-VRR" ||
        tech_name == "LPDDR5" || tech_name == "GDDR6" ||
        tech_name == "HBM2" || tech_name == "HBM3") {
        return MemoryTechnologyType::DRAM;
    }
    if (tech_name == "STT-MRAM" || tech_name == "STTMRAM" || tech_name == "MRAM") {
        return MemoryTechnologyType::STT_MRAM;
    }
    if (tech_name == "PCM" || tech_name == "PRAM") {
        return MemoryTechnologyType::PCM;
    }
    if (tech_name == "ReRAM" || tech_name == "RERAM") {
        return MemoryTechnologyType::RERAM;
    }

    throw std::runtime_error("Unknown memory technology: " + tech_name);
}

std::string getMemoryTechnologyName(MemoryTechnologyType type) {
    switch (type) {
        case MemoryTechnologyType::SRAM: return "SRAM";
        case MemoryTechnologyType::DRAM: return "DRAM";
        case MemoryTechnologyType::STT_MRAM: return "STT-MRAM";
        case MemoryTechnologyType::PCM: return "PCM";
        case MemoryTechnologyType::RERAM: return "ReRAM";
        default: return "Unknown";
    }
}

std::shared_ptr<InternalDRAMNetwork> createInternalMemoryNetwork(
    const std::string& memory_tech,
    int num_subarrays,
    int num_banks,
    int num_bank_groups,
    int num_chips
) {
    // ========================================
    // Input Validation
    // ========================================

    // Validate memory technology string
    if (memory_tech.empty()) {
        throw std::invalid_argument("Memory technology type cannot be empty");
    }

    // Validate positive counts
    if (num_subarrays <= 0) {
        throw std::invalid_argument("Number of subarrays must be positive (got " +
                                   std::to_string(num_subarrays) + ")");
    }

    if (num_banks <= 0) {
        throw std::invalid_argument("Number of banks must be positive (got " +
                                   std::to_string(num_banks) + ")");
    }

    if (num_bank_groups <= 0) {
        throw std::invalid_argument("Number of bank groups must be positive (got " +
                                   std::to_string(num_bank_groups) + ")");
    }

    if (num_chips <= 0) {
        throw std::invalid_argument("Number of chips must be positive (got " +
                                   std::to_string(num_chips) + ")");
    }

    // Validate reasonable upper bounds to catch configuration errors
    const int MAX_SUBARRAYS = 1024;
    const int MAX_BANKS = 256;
    const int MAX_BANK_GROUPS = 64;
    const int MAX_CHIPS = 128;

    if (num_subarrays > MAX_SUBARRAYS) {
        throw std::invalid_argument("Number of subarrays exceeds reasonable limit (got " +
                                   std::to_string(num_subarrays) + ", max " +
                                   std::to_string(MAX_SUBARRAYS) + ")");
    }

    if (num_banks > MAX_BANKS) {
        throw std::invalid_argument("Number of banks exceeds reasonable limit (got " +
                                   std::to_string(num_banks) + ", max " +
                                   std::to_string(MAX_BANKS) + ")");
    }

    if (num_bank_groups > MAX_BANK_GROUPS) {
        throw std::invalid_argument("Number of bank groups exceeds reasonable limit (got " +
                                   std::to_string(num_bank_groups) + ", max " +
                                   std::to_string(MAX_BANK_GROUPS) + ")");
    }

    if (num_chips > MAX_CHIPS) {
        throw std::invalid_argument("Number of chips exceeds reasonable limit (got " +
                                   std::to_string(num_chips) + ", max " +
                                   std::to_string(MAX_CHIPS) + ")");
    }

    // Validate logical consistency: banks should be multiple of bank groups
    if (num_banks % num_bank_groups != 0) {
        throw std::invalid_argument("Number of banks (" + std::to_string(num_banks) +
                                   ") must be evenly divisible by number of bank groups (" +
                                   std::to_string(num_bank_groups) + ")");
    }

    std::cout << "[InternalMemoryNetwork] Creating network for " << memory_tech << std::endl;
    std::cout << "  Organization: " << num_subarrays << " subarrays, "
              << num_banks << " banks, " << num_bank_groups << " bank groups, "
              << num_chips << " chips" << std::endl;

    // Get technology type (may throw std::runtime_error if unknown)
    MemoryTechnologyType tech_type = getMemoryTechnologyType(memory_tech);

    // Create network using existing InternalDRAMNetwork infrastructure
    // (it's generic enough to work for all memory types!)
    auto network = createInternalDRAMNetwork(
        memory_tech,
        num_subarrays,
        num_banks,
        num_bank_groups,
        num_chips
    );

    // Technology-specific configuration adjustments
    switch (tech_type) {
        case MemoryTechnologyType::SRAM:
            std::cout << "[InternalMemoryNetwork] SRAM configuration applied" << std::endl;
            std::cout << "  - Fast on-chip interconnects" << std::endl;
            std::cout << "  - Low latency, high bandwidth" << std::endl;
            // SRAM already configured in createInternalDRAMNetwork
            break;

        case MemoryTechnologyType::STT_MRAM:
            std::cout << "[InternalMemoryNetwork] STT-MRAM configuration applied" << std::endl;
            std::cout << "  - Asymmetric read/write paths" << std::endl;
            std::cout << "  - Write latency >> read latency" << std::endl;
            // STT-MRAM configuration already handled
            break;

        case MemoryTechnologyType::PCM:
            std::cout << "[InternalMemoryNetwork] PCM configuration applied" << std::endl;
            std::cout << "  - Phase change material delays" << std::endl;
            std::cout << "  - Write latency >> read latency" << std::endl;
            // PCM configuration already handled
            break;

        case MemoryTechnologyType::RERAM:
            std::cout << "[InternalMemoryNetwork] ReRAM configuration applied" << std::endl;
            std::cout << "  - Resistive switching delays" << std::endl;
            std::cout << "  - Moderate asymmetry" << std::endl;
            // ReRAM configuration already handled
            break;

        case MemoryTechnologyType::DRAM:
            // Already handled by createInternalDRAMNetwork
            break;
    }

    return network;
}

} // namespace pimid
