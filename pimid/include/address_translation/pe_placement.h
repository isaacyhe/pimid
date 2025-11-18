#ifndef PIMID_PE_PLACEMENT_H
#define PIMID_PE_PLACEMENT_H

#include "common/types.h"
#include <vector>
#include <map>
#include <memory>

namespace pimid {

// Forward declaration for DRAM architecture
namespace memory {
    struct DRAMArchitectureV2;
}

/**
 * Memory hierarchy structure
 * NOTE: All size parameters should come from DRAM architecture configuration,
 * not hard-coded values. See pimid/memory/dram_architecture_v2.h
 */
struct MemoryHierarchy {
    uint32_t num_subarrays_per_bank;
    uint32_t num_banks_per_chip;
    uint32_t num_chips_per_rank;
    uint32_t num_ranks;
    bool has_logic_die;  // For HBM/HMC

    // Memory sizes (in bytes) - should be populated from DRAM architecture
    uint64_t subarray_size_bytes;
    uint64_t bank_size_bytes;
    uint64_t chip_size_bytes;
    uint64_t rank_size_bytes;

    MemoryHierarchy() : num_subarrays_per_bank(0), num_banks_per_chip(0),
                        num_chips_per_rank(0), num_ranks(0),
                        has_logic_die(false),
                        subarray_size_bytes(0), bank_size_bytes(0),
                        chip_size_bytes(0), rank_size_bytes(0) {}
};

/**
 * Data bus constraints based on PE placement level
 * NOTE: Do not use the default constructor values. Always populate from
 * DRAM architecture configuration using createFromDRAMArchitecture().
 */
struct PEBusConstraints {
    uint64_t data_bus_width_bits;      // Width of data bus in bits
    uint64_t max_bandwidth_gbps;        // Maximum bandwidth in GB/s
    uint64_t row_buffer_size_bytes;     // Row buffer size (for subarray level)
    uint32_t shared_bus_pes;            // Number of PEs sharing this bus
    bool has_dedicated_bus;             // True if PE has dedicated data path

    // Default constructor - values should be overridden from DRAM architecture
    PEBusConstraints() : data_bus_width_bits(0), max_bandwidth_gbps(0),
                         row_buffer_size_bytes(0), shared_bus_pes(1),
                         has_dedicated_bus(false) {}
};

/**
 * Address space constraints based on PE placement level
 */
struct PEAddressConstraints {
    Address accessible_base;            // Base of accessible address range
    Address accessible_limit;           // Limit of accessible address range
    uint64_t accessible_size_bytes;     // Size of accessible region
    bool can_access_remote;             // Can access outside local region
    uint32_t remote_access_penalty;     // Cycle penalty for remote access

    PEAddressConstraints() : accessible_base(0), accessible_limit(0),
                            accessible_size_bytes(0), can_access_remote(false),
                            remote_access_penalty(0) {}
};

/**
 * Processing Element descriptor
 */
struct PEDescriptor {
    uint32_t pe_id;
    PEPlacementLevel level;

    // Hierarchical location
    uint32_t subarray_id;
    uint32_t bank_id;
    uint32_t chip_id;
    uint32_t rank_id;
    bool on_logic_die;

    // Address range (for discrete addressing)
    Address addr_base;
    Address addr_limit;

    // Placement-specific constraints
    PEBusConstraints bus_constraints;
    PEAddressConstraints addr_constraints;

    // Capabilities (should be populated from PE configuration)
    uint32_t num_cores;
    uint32_t frequency_mhz;
    bool has_l1_cache;
    uint32_t l1_size_kb;

    // Default constructor - frequency_mhz should be set from configuration
    PEDescriptor() : pe_id(0), level(PEPlacementLevel::RANK),
                     subarray_id(0), bank_id(0), chip_id(0), rank_id(0),
                     on_logic_die(false), addr_base(0), addr_limit(0),
                     num_cores(1), frequency_mhz(0),
                     has_l1_cache(false), l1_size_kb(0) {}
};

/**
 * PE placement manager
 * Handles fine-grained placement of PEs at different memory hierarchy levels
 */
class PEPlacementManager {
public:
    PEPlacementManager(const MemoryHierarchy& hierarchy,
                       PEPlacementLevel level,
                       AddressingMode mode,
                       std::shared_ptr<memory::DRAMArchitectureV2> dram_arch = nullptr);

    // PE registration
    void registerPE(const PEDescriptor& pe);
    void unregisterPE(uint32_t pe_id);

    // PE queries
    const PEDescriptor* getPE(uint32_t pe_id) const;
    std::vector<uint32_t> getPEsAtLevel(PEPlacementLevel level) const;
    std::vector<uint32_t> getPEsForAddress(Address addr) const;

    // Address mapping
    uint32_t getPEForAddress(Address addr) const;
    bool isLocalAddress(uint32_t pe_id, Address addr) const;
    Address translateAddress(uint32_t from_pe, uint32_t to_pe, Address addr) const;

    // Address access validation
    bool canAccess(uint32_t pe_id, Address addr) const;
    bool isRemoteAccess(uint32_t pe_id, Address addr) const;
    uint32_t getAccessPenalty(uint32_t pe_id, Address addr) const;

    // Bandwidth and bus constraints
    uint64_t getAvailableBandwidth(uint32_t pe_id) const;
    uint64_t getEffectiveBandwidth(uint32_t pe_id, Cycle current_cycle) const;
    bool hasBusContention(uint32_t pe_id) const;
    std::vector<uint32_t> getSharingPEs(uint32_t pe_id) const;

    // Constraint calculation based on placement level
    void computeConstraints(PEDescriptor& pe) const;
    PEBusConstraints calculateBusConstraints(PEPlacementLevel level,
                                             uint32_t location_id) const;
    PEAddressConstraints calculateAddressConstraints(PEPlacementLevel level,
                                                     const PEDescriptor& pe) const;

    // Hierarchy navigation
    std::vector<uint32_t> getPEsInSubarray(uint32_t bank_id, uint32_t subarray_id) const;
    std::vector<uint32_t> getPEsInBank(uint32_t bank_id) const;
    std::vector<uint32_t> getPEsInChip(uint32_t chip_id) const;
    std::vector<uint32_t> getPEsInRank(uint32_t rank_id) const;
    std::vector<uint32_t> getPEsOnLogicDie() const;

    // Configuration
    const MemoryHierarchy& getHierarchy() const { return hierarchy_; }
    PEPlacementLevel getPlacementLevel() const { return placement_level_; }
    AddressingMode getAddressingMode() const { return addressing_mode_; }

    // Statistics
    uint32_t getTotalPEs() const { return pes_.size(); }
    void printPlacement() const;
    void printConstraints(uint32_t pe_id) const;

private:
    MemoryHierarchy hierarchy_;
    PEPlacementLevel placement_level_;
    AddressingMode addressing_mode_;

    // DRAM architecture (optional - if provided, used for accurate bus constraints)
    std::shared_ptr<memory::DRAMArchitectureV2> dram_arch_;

    // PE storage
    std::map<uint32_t, PEDescriptor> pes_;

    // Lookup tables for fast queries
    std::map<PEPlacementLevel, std::vector<uint32_t>> level_to_pes_;
    std::map<uint32_t, std::vector<uint32_t>> bank_to_pes_;
    std::map<uint32_t, std::vector<uint32_t>> chip_to_pes_;
    std::map<uint32_t, std::vector<uint32_t>> rank_to_pes_;

    // Bus sharing tracking (for bandwidth contention)
    // Maps bus ID to list of PEs sharing that bus
    std::map<uint32_t, std::vector<uint32_t>> subarray_bus_to_pes_;
    std::map<uint32_t, std::vector<uint32_t>> bank_bus_to_pes_;
    std::map<uint32_t, std::vector<uint32_t>> chip_bus_to_pes_;
    std::map<uint32_t, std::vector<uint32_t>> rank_bus_to_pes_;

    // Bus utilization tracking (percentage, 0-100)
    mutable std::map<uint32_t, uint64_t> bus_utilization_;
    mutable Cycle last_update_cycle_;

    // Helper functions
    void buildLookupTables();
    void buildBusSharingTables();
    bool isAddressInRange(Address addr, const PEDescriptor& pe) const;
    uint32_t getBusID(const PEDescriptor& pe) const;
    Address getHierarchyBase(PEPlacementLevel level, const PEDescriptor& pe) const;
    Address getHierarchyLimit(PEPlacementLevel level, const PEDescriptor& pe) const;
};

//=============================================================================
// Factory Functions for Creating Constraints from DRAM Architecture
//=============================================================================

/**
 * Create PEBusConstraints from DRAM architecture configuration
 * This ensures that PE placement uses actual DRAM datapath widths and
 * bandwidths rather than hardcoded values.
 *
 * NOTE: Automatically applies port_width_scale from DRAMArchitectureV2.
 * This allows "what if the bus was 2x wider?" exploratory studies by setting:
 *   dram_arch->port_width_scale = 2.0;
 *
 * Example usage:
 *   auto dram_arch = memory::createDDR4_2400_Verified();
 *   // Optional: Scale bus widths for exploration (default is 1.0)
 *   dram_arch->port_width_scale = 2.0;  // 2x wider buses
 *   PEBusConstraints subarray_constraints =
 *       createPEBusConstraintsFromDRAM(*dram_arch, PEPlacementLevel::SUBARRAY);
 *   // subarray_constraints.data_bus_width_bits will be 2x the base value
 */
namespace memory {
    class DRAMArchitectureV2;  // Forward declaration
}

PEBusConstraints createPEBusConstraintsFromDRAM(
    const memory::DRAMArchitectureV2& dram_arch,
    PEPlacementLevel level);

/**
 * Create MemoryHierarchy from DRAM architecture configuration
 * Populates all size and organization parameters from verified DRAM specs.
 */
MemoryHierarchy createMemoryHierarchyFromDRAM(
    const memory::DRAMArchitectureV2& dram_arch);

} // namespace pimid

#endif // PIMID_PE_PLACEMENT_H
