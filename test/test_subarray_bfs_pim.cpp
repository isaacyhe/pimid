/**
 * @file test_subarray_bfs_pim.cpp
 * @brief Subarray-level PIM comparison for BFS workload across all memory technologies
 *
 * Tests Breadth-First Search (BFS) graph traversal with per-subarray ALUs.
 *
 * Configuration:
 * - PE Type: Simple ALU per subarray
 * - Subarrays per bank: 4
 * - Memory Technologies: SRAM, STT-MRAM, PCM, ReRAM, DRAM
 *
 * BFS Workload Characteristics:
 * - Graph traversal (irregular access patterns)
 * - Read vertex data from adjacency list
 * - Check and update visited flags
 * - Conditional operations (is visited?)
 * - Queue management
 *
 * PIM Operations per vertex visit:
 * 1. Read vertex ID from queue
 * 2. Read adjacency list (neighbors)
 * 3. For each neighbor:
 *    - Read visited flag
 *    - Compare (is visited?)
 *    - If not visited: Write visited flag, Enqueue neighbor
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
// BFS Workload Configuration
//=============================================================================

struct BFSWorkload {
    uint32_t num_vertices;
    uint32_t avg_degree;        // Average edges per vertex
    uint32_t num_banks;
    uint32_t subarrays_per_bank;
    std::string workload_name;
};

// BFS on 256K vertex graph, average degree 16
const BFSWorkload BFS_256K = {
    .num_vertices = 256 * 1024,
    .avg_degree = 16,
    .num_banks = 8,
    .subarrays_per_bank = 4,
    .workload_name = "BFS (256K vertices, deg=16)"
};

//=============================================================================
// BFS Performance Metrics
//=============================================================================

struct BFSMetrics {
    std::string technology_name;
    std::string workload_name;

    // Timing
    double vertex_processing_time_ns;
    double neighbor_processing_time_ns;
    double total_latency_ns;

    // Operations breakdown
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t total_compares;

    // Energy
    double total_energy_pj;
    double energy_per_vertex_pj;

    // Throughput
    double vertices_per_second;
    double edges_per_second;
};

//=============================================================================
// Subarray BFS Simulator
//=============================================================================

class SubarrayBFSSimulator {
public:
    SubarrayBFSSimulator(std::shared_ptr<MemoryModel> model, const std::string& tech_name)
        : model_(model), tech_name_(tech_name) {}

    BFSMetrics simulateBFS(const BFSWorkload& config) {
        BFSMetrics metrics;
        metrics.technology_name = tech_name_;
        metrics.workload_name = config.workload_name;

        std::cout << "\n--- Simulating BFS on " << tech_name_ << " ---" << std::endl;
        std::cout << "Vertices: " << config.num_vertices << std::endl;
        std::cout << "Average degree: " << config.avg_degree << std::endl;
        std::cout << "Subarrays per bank: " << config.subarrays_per_bank << std::endl;

        // Get memory timing
        double read_lat_ns = getSubarrayReadLatency();
        double write_lat_ns = getSubarrayWriteLatency();

        std::cout << "Subarray read: " << read_lat_ns << " ns" << std::endl;
        std::cout << "Subarray write: " << write_lat_ns << " ns" << std::endl;

        // Simple ALU operations (no instruction overhead)
        double alu_compare_ns = 0.3;  // Fast comparison
        double alu_add_ns = 0.5;      // Address calculation

        // BFS operations PER VERTEX:
        // 1. Read vertex ID from queue (1 read)
        // 2. Read adjacency list pointer (1 read)
        // 3. For each neighbor (avg_degree):
        //    a. Read neighbor ID (1 read)
        //    b. Read visited flag (1 read)
        //    c. Compare if visited (1 compare)
        //    d. If not visited (~50% probability):
        //       - Write visited flag (1 write)
        //       - Enqueue neighbor (1 write)

        // Step 1-2: Read vertex and adjacency pointer
        double read_vertex = read_lat_ns * 2;

        // Step 3: Process neighbors
        double per_neighbor_time = 0.0;
        per_neighbor_time += read_lat_ns;        // Read neighbor ID
        per_neighbor_time += read_lat_ns;        // Read visited flag
        per_neighbor_time += alu_compare_ns;     // Compare
        per_neighbor_time += write_lat_ns * 0.5; // Write if not visited (50% prob)
        per_neighbor_time += write_lat_ns * 0.5; // Enqueue if not visited (50% prob)

        double process_neighbors = per_neighbor_time * config.avg_degree;

        // Total time per vertex
        double vertex_time = read_vertex + process_neighbors;

        metrics.vertex_processing_time_ns = read_vertex;
        metrics.neighbor_processing_time_ns = process_neighbors;

        std::cout << "  Vertex read time: " << read_vertex << " ns" << std::endl;
        std::cout << "  Neighbor processing: " << process_neighbors << " ns" << std::endl;
        std::cout << "  Total per vertex: " << vertex_time << " ns" << std::endl;

        // Total latency: Process all vertices
        // Vertices distributed across subarrays, all work in parallel
        uint32_t total_subarrays = config.num_banks * config.subarrays_per_bank;
        uint64_t vertices_per_subarray = config.num_vertices / total_subarrays;

        metrics.total_latency_ns = vertex_time * vertices_per_subarray;

        std::cout << "Total subarrays: " << total_subarrays << std::endl;
        std::cout << "Vertices per subarray: " << vertices_per_subarray << std::endl;
        std::cout << "Total latency: " << (metrics.total_latency_ns / 1e6) << " ms" << std::endl;

        // Count operations
        metrics.total_reads = config.num_vertices * (2 + config.avg_degree * 2);  // Vertex + neighbors
        metrics.total_writes = config.num_vertices * config.avg_degree * 0.5 * 2; // 50% write visited + enqueue
        metrics.total_compares = config.num_vertices * config.avg_degree;

        // Energy calculation
        double read_energy = model_->getReadEnergy();
        double write_energy = model_->getWriteEnergy();

        // Convert nJ to pJ if needed
        if (read_energy > 100.0) {
            read_energy *= 1000.0;
            write_energy *= 1000.0;
        }

        uint64_t bytes_per_element = 8;
        double alu_energy_per_op = 2.0;  // pJ for simple ALU

        metrics.total_energy_pj = (metrics.total_reads * bytes_per_element * read_energy) +
                                  (metrics.total_writes * bytes_per_element * write_energy) +
                                  (metrics.total_compares * alu_energy_per_op);

        metrics.energy_per_vertex_pj = metrics.total_energy_pj / config.num_vertices;

        // Throughput
        metrics.vertices_per_second = (config.num_vertices / metrics.total_latency_ns) * 1e9;
        metrics.edges_per_second = (config.num_vertices * config.avg_degree / metrics.total_latency_ns) * 1e9;

        return metrics;
    }

private:
    std::shared_ptr<MemoryModel> model_;
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
            // DRAM subarray read = tRCD + tCAS (simplified)
            return 13.32;  // ns (DDR4-2400)
        }
        return 10.0;
    }

    double getSubarrayWriteLatency() {
        if (auto sram = std::dynamic_pointer_cast<SRAMModel>(model_)) {
            return sram->getSubarrayReadLatency();  // Symmetric
        } else if (auto mram = std::dynamic_pointer_cast<STTMRAMModel>(model_)) {
            return mram->getSubarrayWriteLatency();
        } else if (auto pcm = std::dynamic_pointer_cast<PCMModel>(model_)) {
            return pcm->getSubarraySetWriteLatency();
        } else if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model_)) {
            return reram->getSubarrayWriteLatency();
        } else if (auto dram = std::dynamic_pointer_cast<DRAMModel>(model_)) {
            // DRAM write = tRCD + tCAS + tWR (simplified)
            return 15.0;  // ns
        }
        return 10.0;
    }
};

//=============================================================================
// Results Display
//=============================================================================

void printBFSResults(const std::vector<BFSMetrics>& results) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "SUBARRAY-LEVEL BFS RESULTS" << std::endl;
    std::cout << "PE: Simple ALU per subarray" << std::endl;
    std::cout << "========================================" << std::endl;

    // Header
    std::cout << std::left << std::setw(12) << "Technology"
              << std::right << std::setw(15) << "Latency (ms)"
              << std::setw(18) << "Vertices/sec (M)"
              << std::setw(18) << "Energy/Vtx (nJ)"
              << std::endl;
    std::cout << std::string(63, '-') << std::endl;

    // Find baseline (SRAM or DRAM)
    double baseline_latency = results[0].total_latency_ns;

    for (const auto& m : results) {
        std::cout << std::left << std::setw(12) << m.technology_name
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(15) << (m.total_latency_ns / 1e6)  // ns to ms
                  << std::setw(18) << (m.vertices_per_second / 1e6)  // vertices/sec to M vertices/sec
                  << std::setw(18) << (m.energy_per_vertex_pj / 1000.0);  // pJ to nJ

        double speedup = baseline_latency / m.total_latency_ns;
        std::cout << "  (" << std::setprecision(2) << speedup << "x)" << std::endl;
    }

    std::cout << "\n--- Detailed Breakdown ---" << std::endl;
    for (const auto& m : results) {
        std::cout << "\n" << m.technology_name << ":" << std::endl;
        std::cout << "  Vertex processing: " << m.vertex_processing_time_ns << " ns" << std::endl;
        std::cout << "  Neighbor processing: " << m.neighbor_processing_time_ns << " ns" << std::endl;
        std::cout << "  Total per vertex: "
                  << (m.vertex_processing_time_ns + m.neighbor_processing_time_ns) << " ns" << std::endl;
        std::cout << "  Operations:" << std::endl;
        std::cout << "    Reads:    " << m.total_reads << std::endl;
        std::cout << "    Writes:   " << m.total_writes << std::endl;
        std::cout << "    Compares: " << m.total_compares << std::endl;
        std::cout << "  Edges/sec: " << (m.edges_per_second / 1e6) << " M edges/sec" << std::endl;
    }
}

//=============================================================================
// Main Test
//=============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Subarray-Level BFS PIM Test" << std::endl;
    std::cout << "PE: Simple ALU per subarray" << std::endl;
    std::cout << "Subarrays per bank: 4" << std::endl;
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

        // Create simulators
        SubarrayBFSSimulator sram_sim(sram, "SRAM");
        SubarrayBFSSimulator mram_sim(mram, "STT-MRAM");
        SubarrayBFSSimulator pcm_sim(pcm, "PCM");
        SubarrayBFSSimulator reram_sim(reram, "ReRAM");
        SubarrayBFSSimulator dram_sim(dram, "DRAM");

        // Run BFS workload
        std::vector<BFSMetrics> results;
        results.push_back(sram_sim.simulateBFS(BFS_256K));
        results.push_back(mram_sim.simulateBFS(BFS_256K));
        results.push_back(pcm_sim.simulateBFS(BFS_256K));
        results.push_back(reram_sim.simulateBFS(BFS_256K));
        results.push_back(dram_sim.simulateBFS(BFS_256K));

        printBFSResults(results);

        std::cout << "\n========================================" << std::endl;
        std::cout << "TEST COMPLETED!" << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
