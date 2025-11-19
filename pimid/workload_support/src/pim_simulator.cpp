/**
 * @file pim_simulator.cpp
 * @brief Implementation of PIMID-based PIM simulator for DAC26
 *
 * NOTE: This is an analytical model version with explicit timing/energy constants.
 * Future work: Replace analytical models with actual PIMID component queries.
 * See /home/user/pimid-dev/DAC26_INTEGRATION_ANALYSIS.md for improvement plan.
 */

#include "pim_simulator.h"
// NOTE: Power and memory models not yet integrated - using analytical models
// #include "power_model.h"
// #include "memory_model.h"

#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace dac26 {

// ============================================================================
// ANALYTICAL MODEL CONSTANTS
// These are currently hardcoded but should be replaced with PIMID queries
// ============================================================================

// DRAM Timing Constants (45nm, 1GHz DDR3-like)
// Source: JEDEC DDR3 specifications, tRCD + tCL
// TODO: Replace with Ramulator queries for specific DRAM type
constexpr uint32_t BASE_READ_CYCLES_45NM_1GHZ = 12;   // Row activate + column read
constexpr uint32_t BASE_WRITE_CYCLES_45NM_1GHZ = 15;  // Row activate + column write + precharge

// Network Timing Constants
// TODO: Replace with GARNET network simulator
constexpr uint32_t LIBCOM_SWITCH_LATENCY = 1;      // Direct interconnect: 1 cycle per hop
constexpr uint32_t HTREE_BASE_LATENCY = 2;         // H-tree base overhead

// Compute Timing Constants
// Source: Typical in-order processor pipeline
// TODO: Query from McPAT based on processor configuration
constexpr uint32_t BASE_COMPUTE_CYCLES = 1;        // 1 cycle per simple ALU op

// Energy Constants (pJ @ 45nm, 1GHz)
// Source: CACTI 6.5 for 4KB SRAM array @ 45nm
// TODO: Replace with actual CACTI or McPAT queries
constexpr double BASE_READ_ENERGY_PJ_45NM = 8.0;   // Per 32-bit read
constexpr double BASE_WRITE_ENERGY_PJ_45NM = 12.0; // Per 32-bit write
constexpr double BASE_COMPUTE_ENERGY_PJ_45NM = 2.0; // Per ALU operation

// Interconnect Energy Constants
// Source: LIBCom paper specifications
// TODO: Make configurable or query from interconnect model
constexpr double LIBCOM_ENERGY_FACTOR = 0.55;     // 45% savings (0.55 = 55% of baseline)
constexpr double HTREE_WIRE_ENERGY_BASE = 10.0;   // Base wire energy per hop (pJ)

// Atomic and Synchronization Overheads
// Source: Typical cache coherence protocols
// TODO: Make configurable based on coherence protocol
constexpr uint32_t ATOMIC_OVERHEAD_CYCLES = 2;
constexpr double ATOMIC_ENERGY_MULTIPLIER = 1.5;
constexpr uint32_t BARRIER_SYNC_CYCLES = 10;
constexpr double BARRIER_SYNC_ENERGY_PJ = 5.0;

// ============================================================================

PIMSimulator::PIMSimulator(const PIMConfig& config)
    : config_(config), power_model_(nullptr), memory_model_(nullptr) {
}

PIMSimulator::~PIMSimulator() {
    // Power model cleanup when integrated
    // if (power_model_) {
    //     delete static_cast<PowerModel*>(power_model_);
    // }
}

void PIMSimulator::initialize() {
    std::cout << "\n=== Initializing PIMID Simulator ===" << std::endl;
    std::cout << "Technology: " << config_.tech_node_nm << "nm" << std::endl;
    std::cout << "Frequency: " << config_.frequency_ghz << " GHz" << std::endl;
    std::cout << "Subarrays: " << config_.num_subarrays << std::endl;
    std::cout << "Topology: " << (config_.topology == Topology::LIBCOM ? "LIBCom" : "H-tree Baseline") << std::endl;

    initializePowerModel();
    computeTimingParameters();
    computeEnergyParameters();

    std::cout << "\n--- Computed Parameters ---" << std::endl;
    std::cout << "Local read latency: " << config_.local_read_cycles << " cycles" << std::endl;
    std::cout << "Local write latency: " << config_.local_write_cycles << " cycles" << std::endl;
    std::cout << "Remote access latency: " << config_.remote_access_cycles << " cycles" << std::endl;
    std::cout << "Compute latency: " << config_.compute_cycles << " cycle/op" << std::endl;

    std::cout << "\nLocal read energy: " << config_.local_read_energy_pJ << " pJ" << std::endl;
    std::cout << "Local write energy: " << config_.local_write_energy_pJ << " pJ" << std::endl;
    std::cout << "Remote access energy: " << config_.remote_access_energy_pJ << " pJ" << std::endl;
    std::cout << "Compute energy: " << config_.compute_energy_pJ << " pJ/op" << std::endl;
    std::cout << "================================\n" << std::endl;
}

void PIMSimulator::initializePowerModel() {
    // TODO: Integrate actual PIMID power model when ready
    // For now, using analytical models with explicit constants
    // See constants at top of file and DAC26_INTEGRATION_ANALYSIS.md

    // Future implementation will create actual McPAT model:
    // TechnologyParams tech_params;
    // tech_params.tech_node_nm = config_.tech_node_nm;
    // tech_params.frequency_ghz = config_.frequency_ghz;
    // tech_params.temperature_k = config_.temperature_k;
    // tech_params.core_count = config_.num_subarrays;
    // tech_params.device_type = "HP";
    //
    // power_model_ = new McPATModel(tech_params);
    // static_cast<PowerModel*>(power_model_)->initialize();

    power_model_ = nullptr;  // Not using power model yet - using analytical constants
}

void PIMSimulator::computeTimingParameters() {
    // Use analytical constants defined at top of file
    // See constants section for sources and TODO notes about PIMID integration

    config_.local_read_cycles = BASE_READ_CYCLES_45NM_1GHZ;
    config_.local_write_cycles = BASE_WRITE_CYCLES_45NM_1GHZ;

    // Remote access latency depends on interconnect topology
    if (config_.topology == Topology::LIBCOM) {
        // LIBCom: Direct interconnect with single-hop switch latency
        config_.remote_access_cycles = config_.local_read_cycles + LIBCOM_SWITCH_LATENCY;
    } else {
        // H-tree: Two local accesses (read + write) plus H-tree traversal
        uint32_t htree_latency = computeHTreeLatency(config_.num_subarrays);
        config_.remote_access_cycles = 2 * config_.local_read_cycles + htree_latency;
    }

    config_.compute_cycles = BASE_COMPUTE_CYCLES;
}

void PIMSimulator::computeEnergyParameters() {
    // Use analytical constants defined at top of file
    // Scale energy values for technology node if different from 45nm

    config_.local_read_energy_pJ = scaleTechnologyNode(BASE_READ_ENERGY_PJ_45NM);
    config_.local_write_energy_pJ = scaleTechnologyNode(BASE_WRITE_ENERGY_PJ_45NM);

    // Remote access energy depends on interconnect topology
    if (config_.topology == Topology::LIBCOM) {
        // LIBCom provides 45% energy savings over baseline
        // Baseline = Read + Write = 2× local energy
        double baseline_remote_energy = config_.local_read_energy_pJ * 2.0;
        config_.remote_access_energy_pJ = baseline_remote_energy * LIBCOM_ENERGY_FACTOR;
    } else {
        // H-tree: Two local accesses plus wire energy
        // Wire energy scales with tree depth (logarithmic)
        double htree_wire_energy = HTREE_WIRE_ENERGY_BASE * std::log2(config_.num_subarrays);
        config_.remote_access_energy_pJ = 2.0 * config_.local_read_energy_pJ + htree_wire_energy;
    }

    config_.compute_energy_pJ = scaleTechnologyNode(BASE_COMPUTE_ENERGY_PJ_45NM);
}

uint32_t PIMSimulator::computeHTreeLatency(uint32_t num_subarrays) const {
    // H-tree latency scales logarithmically with number of leaf nodes
    // Formula: BASE_LATENCY + log2(num_subarrays)
    // This models the number of hops from leaf to root and back
    uint32_t tree_depth = 0;
    uint32_t n = num_subarrays;
    while (n > 1) {
        tree_depth++;
        n >>= 1;
    }
    return HTREE_BASE_LATENCY + tree_depth;
}

double PIMSimulator::scaleTechnologyNode(double base_value_45nm) const {
    // Linear scaling approximation for energy with technology node
    // E ∝ (tech_node / 45nm)^2 (approximately)
    double ratio = static_cast<double>(config_.tech_node_nm) / 45.0;
    return base_value_45nm * ratio * ratio;
}

double PIMSimulator::scaleFrequency(double base_value_1ghz) const {
    // Power scales linearly with frequency (P = CV^2f)
    return base_value_1ghz * config_.frequency_ghz;
}

void PIMSimulator::simulateOperation(PIMOperation op, uint64_t count) {
    switch (op) {
        case PIMOperation::LOCAL_READ:
            results_.local_reads += count;
            results_.memory_cycles += count * config_.local_read_cycles;
            results_.memory_energy_pJ += count * config_.local_read_energy_pJ;
            break;

        case PIMOperation::LOCAL_WRITE:
            results_.local_writes += count;
            results_.memory_cycles += count * config_.local_write_cycles;
            results_.memory_energy_pJ += count * config_.local_write_energy_pJ;
            break;

        case PIMOperation::REMOTE_READ:
        case PIMOperation::REMOTE_WRITE:
            results_.remote_reads += count;
            results_.network_cycles += count * config_.remote_access_cycles;
            results_.network_energy_pJ += count * config_.remote_access_energy_pJ;
            break;

        case PIMOperation::COMPUTE_INT:
        case PIMOperation::COMPUTE_FP:
            results_.compute_ops += count;
            results_.compute_cycles += count * config_.compute_cycles;
            results_.compute_energy_pJ += count * config_.compute_energy_pJ;
            break;

        case PIMOperation::ATOMIC_OP:
            results_.memory_cycles += count * ATOMIC_OVERHEAD_CYCLES;
            results_.memory_energy_pJ += count * config_.local_write_energy_pJ * ATOMIC_ENERGY_MULTIPLIER;
            break;

        case PIMOperation::BARRIER_SYNC:
            results_.network_cycles += BARRIER_SYNC_CYCLES * count;
            results_.network_energy_pJ += BARRIER_SYNC_ENERGY_PJ * count;
            break;
    }

    // Update total cycles (max of parallel operations)
    results_.total_cycles = std::max(results_.compute_cycles,
                                     std::max(results_.memory_cycles, results_.network_cycles));

    // Update total energy (sum of all components)
    results_.total_energy_pJ = results_.compute_energy_pJ +
                               results_.memory_energy_pJ +
                               results_.network_energy_pJ;

    // Calculate execution time
    double cycle_time_ns = 1000.0 / config_.frequency_ghz;  // ns per cycle
    results_.execution_time_ns = results_.total_cycles * cycle_time_ns;
}

void PIMSimulator::simulateMemoryAccess(bool is_local, bool is_read, uint64_t bytes) {
    // Calculate number of word accesses
    uint64_t word_accesses = (bytes * 8 + config_.word_size_bits - 1) / config_.word_size_bits;

    if (is_local) {
        if (is_read) {
            simulateOperation(PIMOperation::LOCAL_READ, word_accesses);
        } else {
            simulateOperation(PIMOperation::LOCAL_WRITE, word_accesses);
        }
    } else {
        if (is_read) {
            simulateOperation(PIMOperation::REMOTE_READ, word_accesses);
        } else {
            simulateOperation(PIMOperation::REMOTE_WRITE, word_accesses);
        }
    }
}

void PIMSimulator::simulateCompute(uint64_t ops) {
    simulateOperation(PIMOperation::COMPUTE_INT, ops);
}

void PIMSimulator::simulateNetworkTransfer(uint32_t source, uint32_t dest, uint64_t bytes) {
    if (source == dest) {
        simulateMemoryAccess(true, true, bytes);
    } else {
        simulateMemoryAccess(false, true, bytes);
    }
}

void PIMSimulator::resetStats() {
    results_ = SimulationResults();
}

void PIMSimulator::printResults() const {
    std::cout << "\n=== PIMID Simulation Results ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);

    std::cout << "\n--- Cycles ---" << std::endl;
    std::cout << "Total cycles: " << results_.total_cycles << std::endl;
    std::cout << "  Compute: " << results_.compute_cycles
              << " (" << (100.0 * results_.compute_cycles / results_.total_cycles) << "%)" << std::endl;
    std::cout << "  Memory: " << results_.memory_cycles
              << " (" << (100.0 * results_.memory_cycles / results_.total_cycles) << "%)" << std::endl;
    std::cout << "  Network: " << results_.network_cycles
              << " (" << (100.0 * results_.network_cycles / results_.total_cycles) << "%)" << std::endl;

    std::cout << "\n--- Memory Operations ---" << std::endl;
    std::cout << "Local reads: " << results_.local_reads << std::endl;
    std::cout << "Local writes: " << results_.local_writes << std::endl;
    std::cout << "Remote reads: " << results_.remote_reads << std::endl;
    std::cout << "Remote writes: " << results_.remote_writes << std::endl;
    std::cout << "Compute ops: " << results_.compute_ops << std::endl;

    std::cout << "\n--- Energy (pJ) ---" << std::endl;
    std::cout << "Total energy: " << results_.total_energy_pJ << " pJ" << std::endl;
    std::cout << "  Compute: " << results_.compute_energy_pJ << " pJ ("
              << (100.0 * results_.compute_energy_pJ / results_.total_energy_pJ) << "%)" << std::endl;
    std::cout << "  Memory: " << results_.memory_energy_pJ << " pJ ("
              << (100.0 * results_.memory_energy_pJ / results_.total_energy_pJ) << "%)" << std::endl;
    std::cout << "  Network: " << results_.network_energy_pJ << " pJ ("
              << (100.0 * results_.network_energy_pJ / results_.total_energy_pJ) << "%)" << std::endl;

    std::cout << "\n--- Performance ---" << std::endl;
    std::cout << "Execution time: " << results_.execution_time_ns << " ns" << std::endl;
    std::cout << "                " << (results_.execution_time_ns / 1000.0) << " us" << std::endl;

    std::cout << "================================\n" << std::endl;
}

} // namespace dac26
