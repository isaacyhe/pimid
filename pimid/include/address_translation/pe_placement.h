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

    // Helper functions
    void buildLookupTables();
    bool isAddressInRange(Address addr, const PEDescriptor& pe) const;
};

} // namespace pimid

#endif // PIMID_PE_PLACEMENT_H
