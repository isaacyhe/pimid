/**
 * @file pim_request_payload.h
 * @brief PIM Request Payload for Ramulator Integration
 *
 * This file defines the PIM-specific metadata that extends Ramulator's Request
 * to support Processing-In-Memory operations with detailed bandwidth tracking.
 *
 * KEY DESIGN PRINCIPLES:
 * 1. NON-INTRUSIVE: Uses Request::m_payload without modifying Ramulator core
 * 2. HIERARCHY-AWARE: Tracks which DRAM level the PIM operation targets
 * 3. BANDWIDTH-AWARE: Applies internal port bitwidth constraints
 * 4. NETWORK-AWARE: Enables internal DRAM network modeling
 */

#ifndef PIMID_PIM_REQUEST_PAYLOAD_H
#define PIMID_PIM_REQUEST_PAYLOAD_H

#include <cstdint>
#include <functional>
#include <vector>

namespace pimid {

/**
 * @brief PIM Granularity - Where processing happens in DRAM hierarchy
 *
 * This defines the DRAM level at which PIM units are placed.
 * Each level has DIFFERENT internal bandwidth limits!
 */
enum class PIMGranularity {
    CPU = 0,           // No PIM, traditional CPU processing
    MEMORY_CONTROLLER, // PIM at memory controller (rank-level access)
    RANK,              // PIM at rank level (64-bit DDR4 interface)
    CHIP,              // PIM at chip level (8-bit x8 device I/O)
    BANK_GROUP,        // PIM at bank group level (~16-bit internal)
    BANK,              // PIM at bank level (8-bit internal serialization)
    SUBARRAY           // PIM at subarray level (256-bit GSA, but serialized out)
};

/**
 * @brief PIM Operation Type
 */
enum class PIMOperationType {
    NORMAL_READ,       // Traditional DRAM read
    NORMAL_WRITE,      // Traditional DRAM write
    PIM_COMPUTE,       // In-memory computation
    PIM_GATHER,        // Gather data from multiple banks (needs network!)
    PIM_SCATTER,       // Scatter data to multiple banks (needs network!)
    PIM_REDUCE,        // Reduction across banks (needs network!)
    PIM_BROADCAST      // Broadcast to multiple banks (needs network!)
};

/**
 * @brief Data Locality Classification
 *
 * Classifies whether data is local to the PIM unit or requires network access
 */
enum class DataLocality {
    LOCAL,              // Data is in PE's local memory (no network needed)
    REMOTE_SAME_BG,     // Data in same bank group, different bank (needs bank network)
    REMOTE_SAME_CHIP,   // Data in same chip, different BG (needs BG network)
    REMOTE_SAME_RANK,   // Data in same rank, different chip (needs chip network)
    REMOTE_EXTERNAL     // Data outside rank (needs rank interface)
};

/**
 * @brief Internal DRAM Network Transfer
 *
 * When PIM operations need to move data INSIDE the DRAM chip,
 * we need to model the internal network (e.g., bank-to-bank transfers).
 */
struct InternalDRAMTransfer {
    int source_bank;           // Source bank ID (-1 if external)
    int dest_bank;             // Destination bank ID (-1 if external)
    int source_subarray;       // Source subarray ID
    int dest_subarray;         // Destination subarray ID
    int source_bank_group;     // Source bank group ID
    int dest_bank_group;       // Destination bank group ID
    int source_chip;           // Source chip ID
    int dest_chip;             // Destination chip ID
    uint64_t transfer_bytes;   // Amount of data to transfer
    uint64_t network_latency;  // Network latency (cycles)
    DataLocality locality;     // Type of data access
    bool requires_network;     // Does this need internal network?
};

/**
 * @brief PIM Request Payload
 *
 * This is attached to Ramulator's Request::m_payload to track
 * PIM-specific information throughout the memory system.
 */
struct PIMRequestPayload {
    // PIM Granularity
    PIMGranularity granularity;

    // Operation Type
    PIMOperationType operation;

    // PE (Processing Element) Identification
    int pe_id;                 // Which PE is handling this request
    int num_pes_at_level;      // Total PEs at this DRAM level

    // Bandwidth Tracking
    uint64_t data_bytes;       // Amount of data accessed
    int target_bank;           // Which bank this accesses
    int target_subarray;       // Which subarray this accesses
    int target_bank_group;     // Which bank group this accesses
    int target_chip;           // Which chip this accesses

    // Data Locality and Reach
    uint64_t local_data_bytes;     // Data within PE's local reach (no network)
    uint64_t remote_data_bytes;    // Data requiring network transfers
    double local_data_fraction;    // Fraction of data that is local (0.0 - 1.0)
    DataLocality primary_locality; // Primary data locality classification

    // Data Reach at Each Granularity (in bytes):
    // - Subarray PIM: Can only access ~8-64 MB per subarray locally
    // - Bank PIM: Can access ~128-512 MB per bank locally
    // - BG PIM: Can access ~512 MB - 2 GB per bank group locally
    // - Chip PIM: Can access ~2-8 GB per chip locally
    // - Rank PIM: Can access entire rank (8-64 GB) locally
    // - MC PIM: Can access all ranks through MC
    uint64_t local_capacity_bytes; // How much data this PE can access locally

    // Internal Network Transfers
    std::vector<InternalDRAMTransfer> internal_transfers;

    // Bandwidth Limits (from our verified DRAM architecture specs!)
    int port_bitwidth;         // Port bitwidth at this level (8-bit bank, 64-bit rank, etc.)
    double effective_bw_GBs;   // Effective bandwidth at this level

    // Timing
    uint64_t compute_cycles;   // Cycles for in-memory computation
    uint64_t data_movement_cycles; // Cycles for data movement (limited by port BW!)
    uint64_t network_cycles;   // Cycles for internal network transfers

    // Contention Tracking
    bool bandwidth_limited;    // Is this request bandwidth-limited?
    int concurrent_pes;        // Number of PEs sharing this port

    // Callback for PIM completion
    std::function<void()> pim_completion_callback;

    // Constructor
    PIMRequestPayload()
        : granularity(PIMGranularity::CPU),
          operation(PIMOperationType::NORMAL_READ),
          pe_id(-1),
          num_pes_at_level(0),
          data_bytes(0),
          target_bank(-1),
          target_subarray(-1),
          target_bank_group(-1),
          target_chip(-1),
          local_data_bytes(0),
          remote_data_bytes(0),
          local_data_fraction(1.0),
          primary_locality(DataLocality::LOCAL),
          local_capacity_bytes(0),
          port_bitwidth(64),
          effective_bw_GBs(0.0),
          compute_cycles(0),
          data_movement_cycles(0),
          network_cycles(0),
          bandwidth_limited(false),
          concurrent_pes(1) {}

    /**
     * @brief Check if this request requires internal DRAM network
     */
    bool requiresInternalNetwork() const {
        return operation == PIMOperationType::PIM_GATHER ||
               operation == PIMOperationType::PIM_SCATTER ||
               operation == PIMOperationType::PIM_REDUCE ||
               operation == PIMOperationType::PIM_BROADCAST;
    }

    /**
     * @brief Get human-readable granularity name
     */
    const char* getGranularityName() const {
        switch (granularity) {
            case PIMGranularity::CPU: return "CPU";
            case PIMGranularity::MEMORY_CONTROLLER: return "MemoryController";
            case PIMGranularity::RANK: return "Rank";
            case PIMGranularity::CHIP: return "Chip";
            case PIMGranularity::BANK_GROUP: return "BankGroup";
            case PIMGranularity::BANK: return "Bank";
            case PIMGranularity::SUBARRAY: return "Subarray";
            default: return "Unknown";
        }
    }

    /**
     * @brief Get human-readable operation name
     */
    const char* getOperationName() const {
        switch (operation) {
            case PIMOperationType::NORMAL_READ: return "NormalRead";
            case PIMOperationType::NORMAL_WRITE: return "NormalWrite";
            case PIMOperationType::PIM_COMPUTE: return "PIMCompute";
            case PIMOperationType::PIM_GATHER: return "PIMGather";
            case PIMOperationType::PIM_SCATTER: return "PIMScatter";
            case PIMOperationType::PIM_REDUCE: return "PIMReduce";
            case PIMOperationType::PIM_BROADCAST: return "PIMBroadcast";
            default: return "Unknown";
        }
    }

    /**
     * @brief Get human-readable locality name
     */
    const char* getLocalityName() const {
        switch (primary_locality) {
            case DataLocality::LOCAL: return "Local";
            case DataLocality::REMOTE_SAME_BG: return "Remote-SameBG";
            case DataLocality::REMOTE_SAME_CHIP: return "Remote-SameChip";
            case DataLocality::REMOTE_SAME_RANK: return "Remote-SameRank";
            case DataLocality::REMOTE_EXTERNAL: return "Remote-External";
            default: return "Unknown";
        }
    }

    /**
     * @brief Calculate data locality based on source/dest locations
     */
    static DataLocality calculateLocality(
        [[maybe_unused]] PIMGranularity granularity,
        int pe_bank, int pe_bg, int pe_chip,
        int data_bank, int data_bg, int data_chip) {

        // Same location = local
        if (data_bank == pe_bank && data_bg == pe_bg && data_chip == pe_chip) {
            return DataLocality::LOCAL;
        }

        // Different bank, same BG
        if (data_bg == pe_bg && data_chip == pe_chip) {
            return DataLocality::REMOTE_SAME_BG;
        }

        // Different BG, same chip
        if (data_chip == pe_chip) {
            return DataLocality::REMOTE_SAME_CHIP;
        }

        // Different chip, same rank
        return DataLocality::REMOTE_SAME_RANK;
    }

    /**
     * @brief Get typical local capacity for each PIM granularity
     *
     * This represents how much data a PE can access locally without
     * needing the internal network. Based on typical DRAM configurations:
     * - DDR4: 16 banks, 4 bank groups, 8 chips per rank
     * - Subarray: ~32 MB (16 subarrays per bank, 512 MB / 16)
     * - Bank: ~512 MB (8 GB rank / 16 banks)
     * - Bank Group: ~2 GB (8 GB rank / 4 BGs)
     * - Chip: ~1 GB (8 GB rank / 8 chips)
     * - Rank: ~8-16 GB (entire rank)
     */
    static uint64_t getTypicalLocalCapacity(PIMGranularity granularity) {
        switch (granularity) {
            case PIMGranularity::SUBARRAY:
                return 32ULL * 1024 * 1024;  // 32 MB per subarray
            case PIMGranularity::BANK:
                return 512ULL * 1024 * 1024; // 512 MB per bank
            case PIMGranularity::BANK_GROUP:
                return 2ULL * 1024 * 1024 * 1024; // 2 GB per bank group
            case PIMGranularity::CHIP:
                return 1ULL * 1024 * 1024 * 1024; // 1 GB per chip
            case PIMGranularity::RANK:
            case PIMGranularity::MEMORY_CONTROLLER:
                return 16ULL * 1024 * 1024 * 1024; // 16 GB (full rank)
            case PIMGranularity::CPU:
            default:
                return UINT64_MAX; // CPU can access all memory through cache hierarchy
        }
    }

    /**
     * @brief Calculate network requirement based on access pattern
     */
    bool needsNetwork() const {
        return remote_data_bytes > 0 || requiresInternalNetwork();
    }
};

} // namespace pimid

#endif // PIMID_PIM_REQUEST_PAYLOAD_H
