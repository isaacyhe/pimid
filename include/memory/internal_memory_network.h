/**
 * @file internal_memory_network.h
 * @brief Generic Internal Memory Network Model for All Memory Technologies
 *
 * This extends the DRAM internal network concept to ALL memory technologies:
 * - SRAM (scratchpad/cache with banks)
 * - STT-MRAM (non-volatile with similar hierarchy)
 * - PCM (Phase Change Memory)
 * - ReRAM (Resistive RAM)
 * - DRAM (already supported)
 *
 * UNIVERSAL REQUIREMENT:
 * All memory technologies with bank-level, subarray-level, or chip-level PEs
 * need internal network modeling for data communication within the memory.
 *
 * ARCHITECTURE:
 * - Subarray-to-Subarray: Network within bank
 * - Bank-to-Bank: Network within chip
 * - Chip-to-Chip: Network within system (for multi-chip memories)
 *
 * NETWORK TOPOLOGY:
 * Uses GARNET H-tree with configurable VN/VC/router pipeline:
 * - Virtual Networks (VN): Message classes (read/write, request/response)
 * - Virtual Channels (VC): Deadlock avoidance within VN
 * - Router Pipeline: MINIMAL/SIMPLE/REDUCED/FULL complexity
 */

#ifndef PIMID_INTERNAL_MEMORY_NETWORK_H
#define PIMID_INTERNAL_MEMORY_NETWORK_H

#include "memory/internal_dram_network.h"  // Reuse existing infrastructure
#include <string>
#include <memory>

namespace pimid {

/**
 * Memory Technology Types
 */
enum class MemoryTechnologyType {
    SRAM,
    DRAM,
    STT_MRAM,
    PCM,
    RERAM
};

/* 1.11.57 (latent D066 + D067): createInternalMemoryNetwork() is REMOVED.
 * It forwarded a TOTAL bank count into createInternalDRAMNetwork()'s
 * banks-per-bank-group parameter (4x too large on DDR4) and its
 * technology switch printed "<tech> configuration applied" from cases that
 * applied nothing. It had no callers; see internal_memory_network.cpp for the
 * full account. Call createInternalDRAMNetwork() directly, with the
 * PER-BANK-GROUP bank count, as src/main.cpp and RamulatorWrapper do.
 *
 * The two helpers below are unused as of 1.11.57 -- nothing in the tree
 * includes this header -- and are kept because they are correct and because
 * getMemoryTechnologyType() refuses an unknown technology by throwing rather
 * than classifying it by exclusion. */

/**
 * Get memory technology type from string
 */
MemoryTechnologyType getMemoryTechnologyType(const std::string& tech_name);

/**
 * Get human-readable name for memory technology
 */
std::string getMemoryTechnologyName(MemoryTechnologyType type);

} // namespace pimid

#endif // PIMID_INTERNAL_MEMORY_NETWORK_H
