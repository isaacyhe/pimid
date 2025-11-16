#ifndef PIMID_PE_PLACEMENT_H
#define PIMID_PE_PLACEMENT_H

#include "common/types.h"
#include <vector>
#include <map>

namespace pimid {

/**
 * Memory hierarchy structure
 */
struct MemoryHierarchy {
    uint32_t num_subarrays_per_bank;
    uint32_t num_banks_per_chip;
    uint32_t num_chips_per_rank;
    uint32_t num_ranks;
    bool has_logic_die;  // For HBM/HMC

    MemoryHierarchy() : num_subarrays_per_bank(0), num_banks_per_chip(0),
                        num_chips_per_rank(0), num_ranks(0),
                        has_logic_die(false) {}
};

/**
 * Data bus constraints based on PE placement level
 */
struct PEBusConstraints {
    uint64_t data_bus_width_bits;      // Width of data bus in bits
    uint64_t max_bandwidth_gbps;        // Maximum bandwidth in GB/s
    uint64_t row_buffer_size_bytes;     // Row buffer size (for subarray level)
    uint32_t shared_bus_pes;            // Number of PEs sharing this bus
    bool has_dedicated_bus;             // True if PE has dedicated data path

    PEBusConstraints() : data_bus_width_bits(64), max_bandwidth_gbps(25),
                         row_buffer_size_bytes(8192), shared_bus_pes(1),
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

    // Capabilities
    uint32_t num_cores;
    uint32_t frequency_mhz;
    bool has_l1_cache;
    uint32_t l1_size_kb;

    PEDescriptor() : pe_id(0), level(PEPlacementLevel::RANK),
                     subarray_id(0), bank_id(0), chip_id(0), rank_id(0),
                     on_logic_die(false), addr_base(0), addr_limit(0),
                     num_cores(1), frequency_mhz(1000),
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
                       AddressingMode mode);

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

} // namespace pimid

#endif // PIMID_PE_PLACEMENT_H
