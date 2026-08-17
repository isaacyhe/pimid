#ifndef PIMID_COMMON_TYPES_H
#define PIMID_COMMON_TYPES_H

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

namespace pimid {

// Basic types
using Cycle = uint64_t;
using Address = uint64_t;
using ProcessID = uint32_t;
using ThreadID = uint32_t;

// Host core descriptor (used by YAML config parser for system-scope configs)
struct HostCoreConfig {
    uint32_t core_id;
    uint32_t num_threads;
    uint32_t frequency_mhz;
    std::string core_type;  // in-order, out-of-order
    uint32_t pipeline_depth;
    uint32_t issue_width;
};

// Host cache configuration (used by YAML config parser for system-scope configs)
struct HostCacheConfig {
    uint64_t l1i_size_kb;
    uint64_t l1d_size_kb;
    uint64_t l2_size_kb;
    uint64_t l3_size_kb;
    uint32_t l1_line_size;
    uint32_t l1_associativity;
    uint32_t l2_associativity;
    uint32_t l3_associativity;
};

// Memory hierarchy levels for PE placement
enum class PEPlacementLevel {
    SUBARRAY,     // L0
    BANK,         // L1
    BANK_GROUP,   // L2
    CHIP,         // L3
    RANK,         // L4
    LOGIC_DIE,    // L5
    HOST_MC       // No PE-MCs needed -- PEs share host MC directly
};

// Memory technology types (canonical definition -- all DRAM/NVM/SRAM variants)
enum class MemoryTechnology {
    DDR3,
    DDR4,
    DDR5,
    LPDDR5,
    GDDR6,
    HBM2,
    HBM3,
    SRAM,
    STT_MRAM,
    PCM,
    ReRAM,
    UNKNOWN
};

// Addressing mode
enum class AddressingMode {
    UNIFIED,     // Unified address space (host and PIM share)
    DISCRETE     // Discrete address spaces (explicit data movement)
};

// Simulation domain types
enum class SimulationDomain {
    HOST,
    DEVICE
};

// Memory request types
enum class MemoryRequestType {
    READ,
    WRITE,
    ATOMIC
};

// Network packet types
enum class PacketType {
    DATA,
    CONTROL,
    COHERENCE
};

// Configuration structure
struct PIMIDConfig {
    std::string host_config_path;
    std::string device_config_path;
    std::string memory_config_path;
    std::string network_config_path;
    std::string power_config_path;

    MemoryTechnology memory_tech;
    AddressingMode addressing_mode;
    PEPlacementLevel pe_placement_level;

    bool enable_power_modeling;
    bool enable_network_modeling;
    bool enable_coherence;
};

// Memory request structure
struct MemoryRequest {
    Address addr;
    MemoryRequestType type;
    uint64_t size;
    Cycle issue_cycle;
    SimulationDomain domain;
    uint32_t src_id;
    uint32_t flags;  // Request flags (e.g., 0x80 for PIM analog compute)

    MemoryRequest()
        : addr(0), type(MemoryRequestType::READ), size(0), issue_cycle(0),
          domain(SimulationDomain::HOST), src_id(0), flags(0) {}

    MemoryRequest(Address a, MemoryRequestType t, uint64_t s, Cycle c,
                  SimulationDomain d, uint32_t id, uint32_t f = 0)
        : addr(a), type(t), size(s), issue_cycle(c), domain(d), src_id(id), flags(f) {}
};

// Network packet structure
struct NetworkPacket {
    uint32_t src_node;
    uint32_t dst_node;
    PacketType type;
    uint64_t size;
    Address addr;
    Cycle inject_cycle;

    NetworkPacket()
        : src_node(0), dst_node(0), type(PacketType::DATA), size(0),
          addr(0), inject_cycle(0) {}

    NetworkPacket(uint32_t src, uint32_t dst, PacketType t, uint64_t s,
                  Address a, Cycle c)
        : src_node(src), dst_node(dst), type(t), size(s), addr(a),
          inject_cycle(c) {}
};

// Statistics structure
struct PIMIDStats {
    Cycle total_cycles;
    uint64_t total_instructions;
    uint64_t memory_accesses;
    uint64_t network_packets;
    uint64_t total_tasks;
    double total_energy_j;
    double memory_energy_j;
    double compute_energy_j;
    double network_energy_j;

    PIMIDStats() : total_cycles(0), total_instructions(0),
                   memory_accesses(0), network_packets(0), total_tasks(0),
                   total_energy_j(0.0), memory_energy_j(0.0),
                   compute_energy_j(0.0), network_energy_j(0.0) {}
};

} // namespace pimid

#endif // PIMID_COMMON_TYPES_H
