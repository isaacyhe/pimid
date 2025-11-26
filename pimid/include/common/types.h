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

// Memory hierarchy levels for PE placement
enum class PEPlacementLevel {
    SUBARRAY,
    BANK,
    CHIP,
    RANK,
    LOGIC_DIE
};

// Memory technology types
enum class MemoryTechnology {
    DRAM,        // via Ramulator
    SRAM,        // via CACTI
    STT_MRAM,    // via NVSim
    PCM,         // Phase Change Memory
    RERAM,       // Resistive RAM
    HBM,         // High Bandwidth Memory
    HBM2,        // HBM2
    DDR4,        // DDR4
    DDR5         // DDR5
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

    MemoryRequest(Address a, MemoryRequestType t, uint64_t s, Cycle c,
                  SimulationDomain d, uint32_t id)
        : addr(a), type(t), size(s), issue_cycle(c), domain(d), src_id(id) {}
};

// Network packet structure
struct NetworkPacket {
    uint32_t src_node;
    uint32_t dst_node;
    PacketType type;
    uint64_t size;
    Address addr;
    Cycle inject_cycle;

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
    double total_energy_j;
    double memory_energy_j;
    double compute_energy_j;
    double network_energy_j;

    PIMIDStats() : total_cycles(0), total_instructions(0),
                   memory_accesses(0), network_packets(0),
                   total_energy_j(0.0), memory_energy_j(0.0),
                   compute_energy_j(0.0), network_energy_j(0.0) {}
};

} // namespace pimid

#endif // PIMID_COMMON_TYPES_H
