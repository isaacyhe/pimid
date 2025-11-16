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
    uint64_t transfer_bytes;   // Amount of data to transfer
    uint64_t network_latency;  // Network latency (cycles)
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
};

} // namespace pimid

#endif // PIMID_PIM_REQUEST_PAYLOAD_H
