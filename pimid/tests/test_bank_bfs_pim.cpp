/**
 * @file test_bank_bfs_pim.cpp
 * @brief Bank-level PIM comparison for BFS workload with ALU and In-Order Core
 *
 * Tests Breadth-First Search (BFS) with bank-level processing elements.
 *
 * Configuration:
 * - PE Types: Simple ALU, then In-Order Core
 * - Banks per chip: 4
 * - Subarrays per bank: 4
 * - Memory Technologies: SRAM, STT-MRAM, PCM, ReRAM, DRAM
 *
 * BFS at Bank Level:
 * - PE shared across all subarrays in bank
 * - Data transfers via inner-bank network
 * - ALU: Fast but limited (no branches)
 * - In-Order Core: Full control flow support
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>

#include "memory_models/include/memory_model.h"
#include "memory_models/include/sram_model.h"
#include "memory_models/include/sttmram_model.h"
#include "memory_models/include/pcm_model.h"
#include "memory_models/include/reram_model.h"
#include "memory_models/include/dram_model.h"

using namespace pimid;

//=============================================================================
// Processing Element Types
//=============================================================================

enum class PEType {
    SIMPLE_ALU,
    IN_ORDER_CORE
};

struct PEConfig {
    PEType type;
    std::string name;

    // Timing
    double fetch_decode_ns;
    double compare_ns;
    double branch_ns;
    double add_ns;
    double load_ns;

    // Energy
    double fetch_decode_energy_pj;
    double alu_energy_pj;
};

PEConfig createSimpleALU() {
    PEConfig pe;
    pe.type = PEType::SIMPLE_ALU;
    pe.name = "Simple ALU";
    pe.fetch_decode_ns = 0.0;   // No instruction overhead
    pe.compare_ns = 0.3;
    pe.branch_ns = 0.0;         // Not supported
    pe.add_ns = 0.5;
    pe.load_ns = 0.5;
    pe.fetch_decode_energy_pj = 0.0;
    pe.alu_energy_pj = 2.0;
    return pe;
}

PEConfig createInOrderCore() {
    PEConfig pe;
    pe.type = PEType::IN_ORDER_CORE;
    pe.name = "In-Order Core";
    pe.fetch_decode_ns = 2.0;   // Instruction processing
    pe.compare_ns = 1.0;
    pe.branch_ns = 3.0;         // Branch penalty
    pe.add_ns = 1.0;
    pe.load_ns = 1.0;
    pe.fetch_decode_energy_pj = 10.0;
    pe.alu_energy_pj = 5.0;
    return pe;
}

//=============================================================================
// BFS Workload
//=============================================================================

struct BFSWorkload {
    uint32_t num_vertices;
    uint32_t avg_degree;
    uint32_t num_banks;
    uint32_t subarrays_per_bank;
    std::string workload_name;
};

const BFSWorkload BFS_256K = {
    .num_vertices = 256 * 1024,
    .avg_degree = 16,
    .num_banks = 4,
    .subarrays_per_bank = 4,
    .workload_name = "BFS (256K vertices, deg=16)"
};

//=============================================================================
// BFS Metrics
//=============================================================================

struct BankBFSMetrics {
    std::string technology_name;
    std::string pe_name;
    std::string workload_name;

    // Timing
    double subarray_read_ns;
    double inner_bank_transfer_ns;
    double pe_compute_ns;
    double pe_branch_overhead_ns;
    double bank_write_ns;
    double total_per_vertex_ns;
    double total_latency_ns;

    // Operations
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t total_compares;
    uint64_t total_branches;

    // Energy
    double memory_energy_pj;
    double transfer_energy_pj;
    double pe_energy_pj;
    double total_energy_pj;
    double energy_per_vertex_pj;

    // Throughput
    double vertices_per_second;
    double edges_per_second;
};

//=============================================================================
// Bank-Level BFS Simulator
//=============================================================================

class BankBFSSimulator {
public:
    BankBFSSimulator(std::shared_ptr<MemoryModel> model,
                     const PEConfig& pe_config,
                     const std::string& tech_name)
        : model_(model), pe_config_(pe_config), tech_name_(tech_name) {}

    BankBFSMetrics simulateBFS(const BFSWorkload& config) {
        BankBFSMetrics metrics;
        metrics.technology_name = tech_name_;
        metrics.pe_name = pe_config_.name;
        metrics.workload_name = config.workload_name;

        std::cout << "\n--- Simulating BFS on " << tech_name_
                  << " with " << pe_config_.name << " ---" << std::endl;

        // Get memory timing
        double subarray_read_ns = getSubarrayReadLatency();
        double subarray_write_ns = getSubarrayWriteLatency();
        double inner_bank_ns = getInnerBankDatapath();

        std::cout << "Subarray read: " << subarray_read_ns << " ns" << std::endl;
        std::cout << "Subarray write: " << subarray_write_ns << " ns" << std::endl;
        std::cout << "Inner-bank datapath: " << inner_bank_ns << " ns" << std::endl;

        // BFS operations at bank-level PE:
        // For each vertex:
        //   1. Read vertex ID (subarray → bank PE)
        //   2. Read adjacency list
        //   3. For each neighbor:
        //      a. Read neighbor ID
        //      b. Read visited flag
        //      c. Compare (is visited?) - needs PE
        //      d. Conditional branch - needs PE (In-Order Core)
        //      e. If not visited: write flag, enqueue

        // STEP 1: Read vertex data from subarray
        double read_vertex = subarray_read_ns * 2;  // Vertex ID + adjacency ptr

        // STEP 2: Transfer to bank PE
        double transfer_to_pe = inner_bank_ns;

        // STEP 3: Process each neighbor at PE
        double per_neighbor_time = 0.0;

        // Read neighbor data
        per_neighbor_time += subarray_read_ns;     // Read neighbor ID
        per_neighbor_time += inner_bank_ns;        // Transfer to PE
        per_neighbor_time += subarray_read_ns;     // Read visited flag
        per_neighbor_time += inner_bank_ns;        // Transfer to PE

        // PE operations
        if (pe_config_.type == PEType::IN_ORDER_CORE) {
            // In-Order Core can do conditional branch
            per_neighbor_time += pe_config_.fetch_decode_ns;  // Instruction fetch
            per_neighbor_time += pe_config_.compare_ns;       // Compare
            per_neighbor_time += pe_config_.branch_ns * 0.5;  // Branch (50% taken)
        } else {
            // Simple ALU must process unconditionally (less efficient!)
            per_neighbor_time += pe_config_.compare_ns;       // Compare
            // No branch - must write speculatively and waste some work
        }

        // Write back if not visited (50% probability)
        per_neighbor_time += inner_bank_ns * 0.5;        // Transfer from PE
        per_neighbor_time += subarray_write_ns * 0.5;    // Write visited flag
        per_neighbor_time += subarray_write_ns * 0.5;    // Enqueue neighbor

        double process_neighbors = per_neighbor_time * config.avg_degree;

        // Total time per vertex
        double vertex_time = read_vertex + transfer_to_pe + process_neighbors;

        metrics.subarray_read_ns = read_vertex;
        metrics.inner_bank_transfer_ns = transfer_to_pe + (inner_bank_ns * config.avg_degree * 2);
        metrics.pe_compute_ns = pe_config_.compare_ns * config.avg_degree;
        metrics.pe_branch_overhead_ns = (pe_config_.type == PEType::IN_ORDER_CORE) ?
                                        (pe_config_.fetch_decode_ns + pe_config_.branch_ns * 0.5) * config.avg_degree : 0.0;
        metrics.bank_write_ns = subarray_write_ns * config.avg_degree * 0.5 * 2;
        metrics.total_per_vertex_ns = vertex_time;

        std::cout << "  Per vertex:" << std::endl;
        std::cout << "    Read: " << read_vertex << " ns" << std::endl;
        std::cout << "    Transfer: " << metrics.inner_bank_transfer_ns << " ns" << std::endl;
        std::cout << "    PE compute: " << metrics.pe_compute_ns << " ns" << std::endl;
        if (pe_config_.type == PEType::IN_ORDER_CORE) {
            std::cout << "    Branch overhead: " << metrics.pe_branch_overhead_ns << " ns" << std::endl;
        }
        std::cout << "    Write: " << metrics.bank_write_ns << " ns" << std::endl;
        std::cout << "    TOTAL: " << vertex_time << " ns" << std::endl;

        // Total latency: vertices distributed across banks
        uint64_t vertices_per_bank = config.num_vertices / config.num_banks;
        metrics.total_latency_ns = vertex_time * vertices_per_bank;

        std::cout << "Vertices per bank: " << vertices_per_bank << std::endl;
        std::cout << "Total latency: " << (metrics.total_latency_ns / 1e6) << " ms" << std::endl;

        // Count operations
        metrics.total_reads = config.num_vertices * (2 + config.avg_degree * 2);
        metrics.total_writes = config.num_vertices * config.avg_degree * 0.5 * 2;
        metrics.total_compares = config.num_vertices * config.avg_degree;
        metrics.total_branches = (pe_config_.type == PEType::IN_ORDER_CORE) ?
                                 config.num_vertices * config.avg_degree : 0;

        // Energy
        double read_energy = model_->getReadEnergy();
        double write_energy = model_->getWriteEnergy();

        if (read_energy > 100.0) {
            read_energy *= 1000.0;
            write_energy *= 1000.0;
        }

        uint64_t bytes_per_element = 8;
        double transfer_energy_per_byte = 0.5;  // Inner-bank network

        metrics.memory_energy_pj = (metrics.total_reads * bytes_per_element * read_energy) +
                                   (metrics.total_writes * bytes_per_element * write_energy);

        metrics.transfer_energy_pj = (metrics.total_reads + metrics.total_writes) *
                                     bytes_per_element * transfer_energy_per_byte;

        uint64_t pe_ops = metrics.total_compares + metrics.total_branches;
        metrics.pe_energy_pj = pe_ops * (pe_config_.fetch_decode_energy_pj + pe_config_.alu_energy_pj);

        metrics.total_energy_pj = metrics.memory_energy_pj + metrics.transfer_energy_pj + metrics.pe_energy_pj;
        metrics.energy_per_vertex_pj = metrics.total_energy_pj / config.num_vertices;

        // Throughput
        metrics.vertices_per_second = (config.num_vertices / metrics.total_latency_ns) * 1e9;
        metrics.edges_per_second = (config.num_vertices * config.avg_degree / metrics.total_latency_ns) * 1e9;

        return metrics;
    }

private:
    std::shared_ptr<MemoryModel> model_;
    PEConfig pe_config_;
    std::string tech_name_;

    double getSubarrayReadLatency() {
        if (auto sram = std::dynamic_pointer_cast<SRAMModel>(model_)) {
            return sram->getSubarrayReadLatency();
        } else if (auto mram = std::dynamic_pointer_cast<STTMRAMModel>(model_)) {
            return mram->getSubarrayReadLatency();
        } else if (auto pcm = std::dynamic_pointer_cast<PCMModel>(model_)) {
            return pcm->getSubarrayReadLatency();
        } else if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model_)) {
            return reram->getSubarrayReadLatency();
        } else if (auto dram = std::dynamic_pointer_cast<DRAMModel>(model_)) {
            return 13.32;  // DDR4
        }
        return 10.0;
    }

    double getSubarrayWriteLatency() {
        if (auto sram = std::dynamic_pointer_cast<SRAMModel>(model_)) {
            return sram->getSubarrayReadLatency();
        } else if (auto mram = std::dynamic_pointer_cast<STTMRAMModel>(model_)) {
            return mram->getSubarrayWriteLatency();
        } else if (auto pcm = std::dynamic_pointer_cast<PCMModel>(model_)) {
            return pcm->getSubarraySetWriteLatency();
        } else if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model_)) {
            return reram->getSubarrayWriteLatency();
        } else if (auto dram = std::dynamic_pointer_cast<DRAMModel>(model_)) {
            return 15.0;
        }
        return 10.0;
    }

    double getInnerBankDatapath() {
        if (auto sram = std::dynamic_pointer_cast<SRAMModel>(model_)) {
            return sram->getInnerBankDatapathLatency();
        } else if (auto mram = std::dynamic_pointer_cast<STTMRAMModel>(model_)) {
            return mram->getInnerBankReadLatency();
        } else if (auto pcm = std::dynamic_pointer_cast<PCMModel>(model_)) {
            return pcm->getInnerBankReadLatency();
        } else if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model_)) {
            return reram->getInnerBankReadLatency();
        } else if (auto dram = std::dynamic_pointer_cast<DRAMModel>(model_)) {
            // DRAM inner-bank (estimated from architecture)
            return 6.65;  // DDR4 H-tree + global I/O
        }
        return 5.0;
    }
};

//=============================================================================
// Results Display
//=============================================================================

void printBankBFSResults(const std::vector<BankBFSMetrics>& results) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "BANK-LEVEL BFS RESULTS" << std::endl;
    std::cout << "Banks: 4, Subarrays/bank: 4" << std::endl;
    std::cout << "========================================" << std::endl;

    // Header
    std::cout << std::left << std::setw(12) << "Technology"
              << std::setw(16) << "PE Type"
              << std::right << std::setw(15) << "Latency (ms)"
              << std::setw(18) << "Vertices/sec (M)"
              << std::setw(18) << "Energy/Vtx (nJ)"
              << std::endl;
    std::cout << std::string(79, '-') << std::endl;

    for (const auto& m : results) {
        std::cout << std::left << std::setw(12) << m.technology_name
                  << std::setw(16) << m.pe_name
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(15) << (m.total_latency_ns / 1e6)
                  << std::setw(18) << (m.vertices_per_second / 1e6)
                  << std::setw(18) << (m.energy_per_vertex_pj / 1000.0)
                  << std::endl;
    }

    std::cout << "\n--- Detailed Breakdown ---" << std::endl;
    for (const auto& m : results) {
        std::cout << "\n" << m.technology_name << " + " << m.pe_name << ":" << std::endl;
        std::cout << "  Timing per vertex:" << std::endl;
        std::cout << "    Subarray read:       " << m.subarray_read_ns << " ns" << std::endl;
        std::cout << "    Inner-bank transfer: " << m.inner_bank_transfer_ns << " ns" << std::endl;
        std::cout << "    PE compute:          " << m.pe_compute_ns << " ns" << std::endl;
        if (m.pe_branch_overhead_ns > 0) {
            std::cout << "    Branch overhead:     " << m.pe_branch_overhead_ns << " ns" << std::endl;
        }
        std::cout << "    Bank write:          " << m.bank_write_ns << " ns" << std::endl;
        std::cout << "    TOTAL:               " << m.total_per_vertex_ns << " ns" << std::endl;

        std::cout << "  Energy breakdown:" << std::endl;
        std::cout << "    Memory:      " << (m.memory_energy_pj / 1e6) << " μJ" << std::endl;
        std::cout << "    Transfer:    " << (m.transfer_energy_pj / 1e6) << " μJ" << std::endl;
        std::cout << "    PE compute:  " << (m.pe_energy_pj / 1e6) << " μJ" << std::endl;

        std::cout << "  Throughput:" << std::endl;
        std::cout << "    Edges/sec: " << (m.edges_per_second / 1e6) << " M edges/sec" << std::endl;
    }
}

//=============================================================================
// Main Test
//=============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Bank-Level BFS PIM Test" << std::endl;
    std::cout << "PE Types: Simple ALU, In-Order Core" << std::endl;
    std::cout << "Banks: 4, Subarrays/bank: 4" << std::endl;
    std::cout << "Technologies: SRAM, STT-MRAM, PCM, ReRAM, DRAM" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // Create memory models
        auto sram = std::make_shared<SRAMModel>("config.yaml");
        auto mram = std::make_shared<STTMRAMModel>("config.yaml");
        auto pcm = std::make_shared<PCMModel>("config.yaml");
        auto reram = std::make_shared<ReRAMModel>("config.yaml");
        auto dram = std::make_shared<DRAMModel>("config.yaml");

        sram->initialize();
        mram->initialize();
        pcm->initialize();
        reram->initialize();
        dram->initialize();

        // Create PE configurations
        auto simple_alu = createSimpleALU();
        auto in_order_core = createInOrderCore();

        std::cout << "\n--- PE Configurations ---" << std::endl;
        std::cout << "Simple ALU: Compare=" << simple_alu.compare_ns
                  << "ns, no branches" << std::endl;
        std::cout << "In-Order Core: Fetch/Decode=" << in_order_core.fetch_decode_ns
                  << "ns, Compare=" << in_order_core.compare_ns
                  << "ns, Branch=" << in_order_core.branch_ns << "ns" << std::endl;

        std::vector<BankBFSMetrics> all_results;

        // Test with both PE types
        for (auto pe_config : {simple_alu, in_order_core}) {
            BankBFSSimulator sram_sim(sram, pe_config, "SRAM");
            BankBFSSimulator mram_sim(mram, pe_config, "STT-MRAM");
            BankBFSSimulator pcm_sim(pcm, pe_config, "PCM");
            BankBFSSimulator reram_sim(reram, pe_config, "ReRAM");
            BankBFSSimulator dram_sim(dram, pe_config, "DRAM");

            all_results.push_back(sram_sim.simulateBFS(BFS_256K));
            all_results.push_back(mram_sim.simulateBFS(BFS_256K));
            all_results.push_back(pcm_sim.simulateBFS(BFS_256K));
            all_results.push_back(reram_sim.simulateBFS(BFS_256K));
            all_results.push_back(dram_sim.simulateBFS(BFS_256K));
        }

        printBankBFSResults(all_results);

        std::cout << "\n========================================" << std::endl;
        std::cout << "TEST COMPLETED!" << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
