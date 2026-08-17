/**
 * @file internal_memory_network.cpp
 * @brief Memory-technology naming helpers.
 *
 * 1.11.57 (latent D066 + D067): this file's factory function,
 * createInternalMemoryNetwork(), is DELETED. What is left is the technology
 * enum and its two string conversions.
 *
 * D066 -- IT PASSED THE WRONG ARGUMENT. Its third parameter was named
 * num_banks and meant the TOTAL bank count -- its own validation proved that
 * reading, rejecting configurations where num_banks % num_bank_groups != 0 --
 * and it forwarded that total straight into createInternalDRAMNetwork()'s
 * third parameter, which is num_banks_per_bg (internal_dram_network.h). On a
 * DDR4 part that is 16 passed where 4 belongs, so every bank-level endpoint
 * count and every bank-to-bank distance in the resulting network would have
 * been four times too large. The live callers do it correctly -- src/main.cpp
 * and RamulatorWrapper::initializePIMComponents both pass
 * banks_per_bank_group -- which is exactly why this copy was never noticed.
 *
 * D067 -- IT ANNOUNCED CONFIGURATION IT DID NOT PERFORM. The switch that
 * followed printed "[InternalMemoryNetwork] STT-MRAM configuration applied",
 * "PCM configuration applied", "ReRAM configuration applied" and so on, each
 * sitting over a comment saying the configuration was "already handled"
 * elsewhere. No case did anything at all. A log line asserting that a
 * technology-specific configuration was applied, printed by a branch that
 * applies nothing, is a false statement in the run record.
 *
 * Deleted rather than repaired because it had NO CALLERS anywhere in src/ or
 * include/ -- nothing in the tree even includes this header -- so there is no
 * behaviour to preserve and no number to re-derive. Anyone who needs a
 * technology-generic factory should call createInternalDRAMNetwork() directly
 * with the per-bank-group count, as the two live sites do.
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

} // namespace pimid
