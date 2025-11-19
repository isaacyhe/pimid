/**
 * @file test_bank_inorder_bfs_pim.cpp
 * @brief Bank-level PIM with In-Order Core PE for BFS across all memory technologies
 *
 * Tests Breadth-First Search (BFS) with bank-level In-Order Core processing elements.
 *
 * Configuration:
 * - PE Type: In-Order Core (5-stage pipeline with branch support)
 * - Banks per chip: 4
 * - PEs: 4 (one per bank)
 * - Subarrays per bank: 4
 * - Memory Technologies: DRAM, SRAM, STT-MRAM, PCM, ReRAM
 *
 * BFS at Bank Level:
 * - PE shared across all subarrays in bank
 * - Data transfers via inner-bank H-tree network
 * - In-Order Core: Full control flow support with branch prediction
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
// In-Order Core PE Configuration
//=============================================================================

struct InOrderCoreConfig {
    std::string name;

    // 5-stage pipeline timing
    double fetch_decode_ns;      // IF + ID stages
    double execute_compare_ns;   // EX stage (compare)
    double execute_alu_ns;       // EX stage (ALU ops)
    double memory_access_ns;     // MEM stage (load/store)
    double writeback_ns;         // WB stage
    double branch_penalty_ns;    // Branch misprediction penalty

    // Energy per operation
    double fetch_decode_energy_pj;
    double alu_energy_pj;
    double branch_energy_pj;
};

InOrderCoreConfig createInOrderCore() {
    InOrderCoreConfig pe;
    pe.name = "In-Order Core (5-stage)";
    pe.fetch_decode_ns = 2.0;        // Instruction fetch and decode
    pe.execute_compare_ns = 1.0;     // Compare operation
    pe.execute_alu_ns = 1.0;         // ALU operation
    pe.memory_access_ns = 1.0;       // Load/Store
    pe.writeback_ns = 0.5;           // Register writeback
    pe.branch_penalty_ns = 3.0;      // Branch penalty (pipeline flush)
    pe.fetch_decode_energy_pj = 10.0;
    pe.alu_energy_pj = 5.0;
    pe.branch_energy_pj = 8.0;
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
    std::string workload_name;

    // Timing breakdown (per vertex)
    double subarray_read_ns;
    double inner_bank_transfer_ns;
    double pe_compute_ns;
    double pe_branch_overhead_ns;
    double bank_write_ns;
    double total_per_vertex_ns;

    // Total timing
    double total_latency_ns;
    double total_latency_ms;

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

    // Speedup relative to SRAM
    double speedup_vs_sram;
};

//=============================================================================
// Bank-Level BFS Simulator with In-Order Core
//=============================================================================

class BankInOrderBFSSimulator {
public:
    BankInOrderBFSSimulator(std::shared_ptr<MemoryModel> model,
                            const InOrderCoreConfig& pe_config,
                            const std::string& tech_name)
        : model_(model), pe_config_(pe_config), tech_name_(tech_name) {}

    BankBFSMetrics simulateBFS(const BFSWorkload& config) {
        BankBFSMetrics metrics;
        metrics.technology_name = tech_name_;
        metrics.workload_name = config.workload_name;

        std::cout << "\n--- Simulating BFS on " << tech_name_
                  << " with " << pe_config_.name << " ---" << std::endl;

        // Get memory timing
        double subarray_read_ns = getSubarrayReadLatency();
        double subarray_write_ns = getSubarrayWriteLatency();
        double inner_bank_ns = getInnerBankDatapath();

        std::cout << "Memory Characteristics:" << std::endl;
        std::cout << "  Subarray read:  " << std::setw(8) << std::fixed
                  << std::setprecision(2) << subarray_read_ns << " ns" << std::endl;
        std::cout << "  Subarray write: " << std::setw(8) << subarray_write_ns << " ns" << std::endl;
        std::cout << "  Inner-bank H-tree: " << std::setw(5) << inner_bank_ns << " ns" << std::endl;

        // BFS operations at bank-level PE with In-Order Core:
        // For each vertex:
        //   1. Read vertex ID and adjacency pointer (from subarray)
        //   2. Transfer to bank PE via H-tree
        //   3. For each neighbor (avg_degree):
        //      a. Read neighbor ID (subarray → PE)
        //      b. Read visited flag (subarray → PE)
        //      c. Compare: if (visited == 0) - PE operation
        //      d. Branch: conditional execution - PE pipeline
        //      e. If not visited (50% prob):
        //         - Write visited flag
        //         - Enqueue neighbor

        // STEP 1: Read vertex data from subarray
        double read_vertex = subarray_read_ns * 2;  // Vertex ID + adjacency ptr

        // STEP 2: Transfer to bank PE via H-tree
        double transfer_to_pe = inner_bank_ns;

        // STEP 3: Process each neighbor at PE
        double per_neighbor_time = 0.0;

        // Read neighbor data (2 reads: neighbor ID + visited flag)
        per_neighbor_time += subarray_read_ns;     // Read neighbor ID
        per_neighbor_time += inner_bank_ns;        // Transfer to PE
        per_neighbor_time += subarray_read_ns;     // Read visited flag
        per_neighbor_time += inner_bank_ns;        // Transfer to PE

        // In-Order Core pipeline operations
        per_neighbor_time += pe_config_.fetch_decode_ns;      // Fetch & decode compare instruction
        per_neighbor_time += pe_config_.execute_compare_ns;   // Compare visited flag
        per_neighbor_time += pe_config_.branch_penalty_ns * 0.5;  // Branch penalty (50% misprediction)

        // Write back if not visited (50% probability)
        per_neighbor_time += inner_bank_ns * 0.5;           // Transfer from PE (conditional)
        per_neighbor_time += subarray_write_ns * 0.5;       // Write visited flag (conditional)
        per_neighbor_time += subarray_write_ns * 0.5;       // Enqueue neighbor (conditional)

        double process_neighbors = per_neighbor_time * config.avg_degree;

        // Total time per vertex
        double vertex_time = read_vertex + transfer_to_pe + process_neighbors;

        metrics.subarray_read_ns = read_vertex;
        metrics.inner_bank_transfer_ns = transfer_to_pe + (inner_bank_ns * config.avg_degree * 2);
        metrics.pe_compute_ns = pe_config_.execute_compare_ns * config.avg_degree;
        metrics.pe_branch_overhead_ns = (pe_config_.fetch_decode_ns + pe_config_.branch_penalty_ns * 0.5) * config.avg_degree;
        metrics.bank_write_ns = subarray_write_ns * config.avg_degree * 0.5 * 2;
        metrics.total_per_vertex_ns = vertex_time;

        std::cout << "Per-Vertex Timing Breakdown:" << std::endl;
        std::cout << "  Subarray read:       " << std::setw(8) << read_vertex << " ns" << std::endl;
        std::cout << "  H-tree transfer:     " << std::setw(8) << metrics.inner_bank_transfer_ns << " ns" << std::endl;
        std::cout << "  PE compare:          " << std::setw(8) << metrics.pe_compute_ns << " ns" << std::endl;
        std::cout << "  Branch overhead:     " << std::setw(8) << metrics.pe_branch_overhead_ns << " ns" << std::endl;
        std::cout << "  Subarray write:      " << std::setw(8) << metrics.bank_write_ns << " ns" << std::endl;
        std::cout << "  TOTAL per vertex:    " << std::setw(8) << vertex_time << " ns" << std::endl;

        // Total latency: vertices distributed across 4 banks
        uint64_t vertices_per_bank = config.num_vertices / config.num_banks;
        metrics.total_latency_ns = vertex_time * vertices_per_bank;
        metrics.total_latency_ms = metrics.total_latency_ns / 1e6;

        std::cout << "Overall Performance:" << std::endl;
        std::cout << "  Vertices per bank: " << vertices_per_bank << std::endl;
        std::cout << "  Total latency: " << std::fixed << std::setprecision(2)
                  << metrics.total_latency_ms << " ms" << std::endl;

        // Count operations
        metrics.total_reads = config.num_vertices * (2 + config.avg_degree * 2);
        metrics.total_writes = config.num_vertices * config.avg_degree * 0.5 * 2;
        metrics.total_compares = config.num_vertices * config.avg_degree;
        metrics.total_branches = config.num_vertices * config.avg_degree;

        // Energy calculation
        double read_energy = model_->getReadEnergy();
        double write_energy = model_->getWriteEnergy();

        // Convert to pJ if in nJ
        if (read_energy > 100.0) {
            read_energy *= 1000.0;
            write_energy *= 1000.0;
        }

        uint64_t bytes_per_element = 8;
        double transfer_energy_per_byte = 0.5;  // Inner-bank H-tree network

        metrics.memory_energy_pj = (metrics.total_reads * bytes_per_element * read_energy) +
                                   (metrics.total_writes * bytes_per_element * write_energy);

        metrics.transfer_energy_pj = (metrics.total_reads + metrics.total_writes) *
                                     bytes_per_element * transfer_energy_per_byte;

        uint64_t pe_ops = metrics.total_compares + metrics.total_branches;
        metrics.pe_energy_pj = pe_ops * (pe_config_.fetch_decode_energy_pj +
                                         pe_config_.alu_energy_pj +
                                         pe_config_.branch_energy_pj);

        metrics.total_energy_pj = metrics.memory_energy_pj + metrics.transfer_energy_pj + metrics.pe_energy_pj;
        metrics.energy_per_vertex_pj = metrics.total_energy_pj / config.num_vertices;

        // Throughput
        metrics.vertices_per_second = (config.num_vertices / metrics.total_latency_ns) * 1e9;
        metrics.edges_per_second = (config.num_vertices * config.avg_degree / metrics.total_latency_ns) * 1e9;

        std::cout << "  Throughput: " << std::fixed << std::setprecision(2)
                  << (metrics.vertices_per_second / 1e6) << " M vertices/sec" << std::endl;

        return metrics;
    }

private:
    std::shared_ptr<MemoryModel> model_;
    InOrderCoreConfig pe_config_;
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
            // DDR4-2400: tRCD + tCAS = effective read latency
            const double DDR4_2400_SUBARRAY_READ_NS = 13.32;  // ns
            return DDR4_2400_SUBARRAY_READ_NS;
        }
        const double DEFAULT_SUBARRAY_READ_NS = 10.0;
        return DEFAULT_SUBARRAY_READ_NS;
    }

    double getSubarrayWriteLatency() {
        if (auto sram = std::dynamic_pointer_cast<SRAMModel>(model_)) {
            return sram->getSubarrayReadLatency();  // SRAM symmetric
        } else if (auto mram = std::dynamic_pointer_cast<STTMRAMModel>(model_)) {
            return mram->getSubarrayWriteLatency();
        } else if (auto pcm = std::dynamic_pointer_cast<PCMModel>(model_)) {
            return pcm->getSubarraySetWriteLatency();
        } else if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model_)) {
            return reram->getSubarrayWriteLatency();
        } else if (auto dram = std::dynamic_pointer_cast<DRAMModel>(model_)) {
            // DDR4-2400: Write latency including tWR
            const double DDR4_2400_SUBARRAY_WRITE_NS = 15.0;  // ns
            return DDR4_2400_SUBARRAY_WRITE_NS;
        }
        const double DEFAULT_SUBARRAY_WRITE_NS = 10.0;
        return DEFAULT_SUBARRAY_WRITE_NS;
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
            // DDR4-2400: H-tree + global I/O latency
            const double DDR4_2400_INNER_BANK_NS = 6.65;  // ns
            return DDR4_2400_INNER_BANK_NS;
        }
        const double DEFAULT_INNER_BANK_NS = 5.0;
        return DEFAULT_INNER_BANK_NS;
    }
};

//=============================================================================
// Results Display
//=============================================================================

void printComparisonTable(const std::vector<BankBFSMetrics>& results) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "BANK-LEVEL PIM: IN-ORDER CORE PE" << std::endl;
    std::cout << "Configuration: 4 Banks, 4 PEs (1 per bank)" << std::endl;
    std::cout << "========================================" << std::endl;

    // Header
    std::cout << "\n" << std::left << std::setw(14) << "Technology"
              << std::right << std::setw(15) << "Latency (ms)"
              << std::setw(18) << "Throughput (Mv/s)"
              << std::setw(18) << "Energy/Vtx (nJ)"
              << std::setw(15) << "Speedup"
              << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    // Find SRAM baseline for speedup
    double sram_latency_ms = 0.0;
    for (const auto& m : results) {
        if (m.technology_name == "SRAM") {
            sram_latency_ms = m.total_latency_ms;
            break;
        }
    }

    for (const auto& m : results) {
        double speedup = (sram_latency_ms > 0) ? (sram_latency_ms / m.total_latency_ms) : 1.0;

        std::cout << std::left << std::setw(14) << m.technology_name
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(15) << m.total_latency_ms
                  << std::setw(18) << (m.vertices_per_second / 1e6)
                  << std::setw(18) << (m.energy_per_vertex_pj / 1000.0)
                  << std::setw(15) << speedup << "x"
                  << std::endl;
    }

    std::cout << "\n--- Detailed Per-Vertex Timing ---" << std::endl;
    std::cout << std::left << std::setw(14) << "Technology"
              << std::right << std::setw(12) << "Read (ns)"
              << std::setw(12) << "Transfer"
              << std::setw(12) << "PE Comp"
              << std::setw(12) << "Branch"
              << std::setw(12) << "Write"
              << std::setw(12) << "Total"
              << std::endl;
    std::cout << std::string(86, '-') << std::endl;

    for (const auto& m : results) {
        std::cout << std::left << std::setw(14) << m.technology_name
                  << std::right << std::fixed << std::setprecision(1)
                  << std::setw(12) << m.subarray_read_ns
                  << std::setw(12) << m.inner_bank_transfer_ns
                  << std::setw(12) << m.pe_compute_ns
                  << std::setw(12) << m.pe_branch_overhead_ns
                  << std::setw(12) << m.bank_write_ns
                  << std::setw(12) << m.total_per_vertex_ns
                  << std::endl;
    }

    std::cout << "\n--- Energy Breakdown ---" << std::endl;
    for (const auto& m : results) {
        std::cout << "\n" << m.technology_name << ":" << std::endl;
        std::cout << "  Memory energy:   " << std::setw(10) << std::fixed << std::setprecision(2)
                  << (m.memory_energy_pj / 1e6) << " μJ" << std::endl;
        std::cout << "  Transfer energy: " << std::setw(10)
                  << (m.transfer_energy_pj / 1e6) << " μJ" << std::endl;
        std::cout << "  PE energy:       " << std::setw(10)
                  << (m.pe_energy_pj / 1e6) << " μJ" << std::endl;
        std::cout << "  Total energy:    " << std::setw(10)
                  << (m.total_energy_pj / 1e6) << " μJ" << std::endl;
        std::cout << "  Throughput:      " << std::setw(10)
                  << (m.edges_per_second / 1e6) << " M edges/sec" << std::endl;
    }
}

//=============================================================================
// Main Test
//=============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Bank-Level BFS PIM Test" << std::endl;
    std::cout << "PE Type: In-Order Core (5-stage pipeline)" << std::endl;
    std::cout << "Banks: 4, PEs: 4 (1 per bank)" << std::endl;
    std::cout << "Subarrays per bank: 4" << std::endl;
    std::cout << "Technologies: DRAM, SRAM, STT-MRAM, PCM, ReRAM" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // Create memory models
        auto dram = std::make_shared<DRAMModel>("config.yaml");
        auto sram = std::make_shared<SRAMModel>("config.yaml");
        auto mram = std::make_shared<STTMRAMModel>("config.yaml");
        auto pcm = std::make_shared<PCMModel>("config.yaml");
        auto reram = std::make_shared<ReRAMModel>("config.yaml");

        dram->initialize();
        sram->initialize();
        mram->initialize();
        pcm->initialize();
        reram->initialize();

        // Create In-Order Core PE configuration
        auto in_order_core = createInOrderCore();

        std::cout << "\n--- In-Order Core PE Configuration ---" << std::endl;
        std::cout << "Fetch/Decode:  " << in_order_core.fetch_decode_ns << " ns" << std::endl;
        std::cout << "Execute (CMP): " << in_order_core.execute_compare_ns << " ns" << std::endl;
        std::cout << "Branch penalty: " << in_order_core.branch_penalty_ns << " ns" << std::endl;

        std::vector<BankBFSMetrics> all_results;

        // Test all memory technologies with In-Order Core
        BankInOrderBFSSimulator dram_sim(dram, in_order_core, "DRAM");
        BankInOrderBFSSimulator sram_sim(sram, in_order_core, "SRAM");
        BankInOrderBFSSimulator mram_sim(mram, in_order_core, "STT-MRAM");
        BankInOrderBFSSimulator pcm_sim(pcm, in_order_core, "PCM");
        BankInOrderBFSSimulator reram_sim(reram, in_order_core, "ReRAM");

        all_results.push_back(dram_sim.simulateBFS(BFS_256K));
        all_results.push_back(sram_sim.simulateBFS(BFS_256K));
        all_results.push_back(mram_sim.simulateBFS(BFS_256K));
        all_results.push_back(pcm_sim.simulateBFS(BFS_256K));
        all_results.push_back(reram_sim.simulateBFS(BFS_256K));

        printComparisonTable(all_results);

        std::cout << "\n========================================" << std::endl;
        std::cout << "TEST COMPLETED!" << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
