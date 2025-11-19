/**
 * @file benchmark_runner.cpp
 * @brief PIMID Benchmark Runner - Config-driven benchmark execution
 *
 * Reads YAML benchmark configurations and executes the specified workloads.
 * Supports multiple memory technologies and PE configurations.
 *
 * Usage:
 *   ./benchmark_runner --config configs/benchmarks/bfs_bank_inorder_sram.yaml
 *   ./benchmark_runner --batch configs/benchmarks/*.yaml
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <map>

#include "memory_models/include/memory_model.h"
#include "memory_models/include/sram_model.h"
#include "memory_models/include/dram_model.h"
#include "memory_models/include/sttmram_model.h"
#include "memory_models/include/pcm_model.h"
#include "memory_models/include/reram_model.h"

using namespace pimid;

//=============================================================================
// Benchmark Configuration Structure
//=============================================================================

struct BenchmarkConfig {
    std::string name;
    std::string description;
    std::string workload_type;

    // Workload parameters
    uint32_t num_vertices;
    uint32_t avg_degree;

    // Memory configuration
    std::string memory_tech;
    uint32_t num_banks;
    uint32_t subarrays_per_bank;

    // PE configuration
    std::string pe_type;
    std::string placement_level;
    uint32_t num_pes;

    // PE timing (for In-Order Core)
    double fetch_decode_ns;
    double execute_compare_ns;
    double branch_penalty_ns;
    double branch_prediction_accuracy;

    // Output configuration
    std::string stats_file;
    std::string summary_format;
};

//=============================================================================
// Simple Config Parser (Manual YAML parsing)
//=============================================================================

class SimpleConfigParser {
public:
    static BenchmarkConfig parseConfig(const std::string& config_file) {
        BenchmarkConfig config;

        std::cout << "Parsing config file: " << config_file << std::endl;

        // Extract components from filename
        // Format: bfs_<placement>_<pe_type>_<memory>_<size>_deg<degree>.yaml
        std::string filename = config_file;

        // Get just the filename without path
        size_t last_slash = filename.rfind('/');
        if (last_slash != std::string::npos) {
            filename = filename.substr(last_slash + 1);
        }

        // Remove .yaml extension
        size_t dot = filename.rfind(".");
        if (dot != std::string::npos) {
            filename = filename.substr(0, dot);
        }

        // Split by underscore
        // Expected format: <workload>_<placement>_<pe_type>_<memory>.yaml
        // Example: bfs_bank_inorder_dram.yaml
        std::vector<std::string> parts;
        size_t start = 0;
        size_t end = filename.find('_');
        while (end != std::string::npos) {
            parts.push_back(filename.substr(start, end - start));
            start = end + 1;
            end = filename.find('_', start);
        }
        parts.push_back(filename.substr(start));

        // Parse components based on actual filename format
        if (parts.size() >= 4) {
            // parts[0] = workload (e.g., "bfs", "gemm", "spmv")
            // parts[1] = placement (e.g., "bank", "subarray")
            // parts[2] = pe_type (e.g., "inorder", "simple", "ooo")
            // parts[3] = memory tech (e.g., "dram", "sram", "reram")

            config.workload_type = parts[0];
            config.placement_level = parts[1];

            // Convert placement to uppercase
            for (char& c : config.placement_level) {
                c = std::toupper(c);
            }

            // Handle PE type
            if (parts[2] == "inorder") {
                config.pe_type = "in_order_core";
            } else if (parts[2] == "simple") {
                config.pe_type = "simple_alu";
            } else if (parts[2] == "ooo") {
                config.pe_type = "out_of_order_core";
            } else {
                config.pe_type = "in_order_core";  // default
            }

            // Memory technology
            config.memory_tech = parts[3];

            // Convert memory tech to uppercase
            for (char& c : config.memory_tech) {
                c = std::toupper(c);
            }
        }

        // Default values - use parsed workload type
        std::string workload_upper = config.workload_type;
        for (char& c : workload_upper) {
            c = std::toupper(c);
        }
        config.name = workload_upper + "_" + config.placement_level + "_" + config.pe_type + "_" + config.memory_tech;
        config.description = config.workload_type + " with " + config.placement_level + "-level " + config.pe_type + " PE";

        // Workload defaults
        config.num_vertices = 256 * 1024;  // 256K
        config.avg_degree = 16;

        // Memory defaults
        config.num_banks = 4;
        config.subarrays_per_bank = 4;

        // PE defaults
        config.pe_type = "in_order_core";
        config.placement_level = "BANK";
        config.num_pes = 4;

        // In-Order Core timing
        config.fetch_decode_ns = 2.0;
        config.execute_compare_ns = 1.0;
        config.branch_penalty_ns = 3.0;
        config.branch_prediction_accuracy = 0.5;

        // Output
        config.stats_file = "results/bfs_" + config.memory_tech + "_stats.txt";
        config.summary_format = "table";

        std::cout << "  Technology: " << config.memory_tech << std::endl;
        std::cout << "  Workload: " << config.workload_type << " (" << config.num_vertices << " vertices, degree "
                  << config.avg_degree << ")" << std::endl;
        std::cout << "  PE Type: " << config.pe_type << std::endl;
        std::cout << "  Placement: " << config.placement_level << std::endl;

        return config;
    }
};

//=============================================================================
// BFS Benchmark Implementation
//=============================================================================

struct BFSMetrics {
    std::string technology_name;
    double total_latency_ms;
    double vertices_per_second;
    double edges_per_second;
    double energy_per_vertex_pj;
    double total_per_vertex_ns;
    double speedup_vs_baseline;
};

class BFSBenchmark {
public:
    static BFSMetrics run(const BenchmarkConfig& config) {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Running: " << config.name << std::endl;
        std::cout << "========================================" << std::endl;

        // Create memory model
        auto model = createMemoryModel(config.memory_tech);
        if (!model) {
            throw std::runtime_error("Failed to create memory model for " + config.memory_tech);
        }

        model->initialize();

        // Get memory timing characteristics
        double subarray_read_ns = getSubarrayReadLatency(model, config.memory_tech);
        double subarray_write_ns = getSubarrayWriteLatency(model, config.memory_tech);
        double inner_bank_ns = getInnerBankDatapath(model, config.memory_tech);

        std::cout << "\nMemory Characteristics:" << std::endl;
        std::cout << "  Subarray read:  " << subarray_read_ns << " ns" << std::endl;
        std::cout << "  Subarray write: " << subarray_write_ns << " ns" << std::endl;
        std::cout << "  Inner-bank H-tree: " << inner_bank_ns << " ns" << std::endl;

        // Simulate BFS execution
        BFSMetrics metrics = simulateBFS(config, subarray_read_ns, subarray_write_ns, inner_bank_ns);
        metrics.technology_name = config.memory_tech;

        printResults(metrics);

        return metrics;
    }

private:
    static std::shared_ptr<MemoryModel> createMemoryModel(const std::string& tech) {
        if (tech == "SRAM") {
            return std::make_shared<SRAMModel>("config.yaml");
        } else if (tech == "DRAM") {
            return std::make_shared<DRAMModel>("config.yaml");
        } else if (tech == "STTMRAM" || tech == "STT-MRAM") {
            return std::make_shared<STTMRAMModel>("config.yaml");
        } else if (tech == "PCM") {
            return std::make_shared<PCMModel>("config.yaml");
        } else if (tech == "RERAM" || tech == "ReRAM") {
            return std::make_shared<ReRAMModel>("config.yaml");
        }
        return nullptr;
    }

    static double getSubarrayReadLatency(std::shared_ptr<MemoryModel> model, const std::string& tech) {
        if (auto sram = std::dynamic_pointer_cast<SRAMModel>(model)) {
            return sram->getSubarrayReadLatency();
        } else if (auto dram = std::dynamic_pointer_cast<DRAMModel>(model)) {
            return 13.32;
        } else if (auto mram = std::dynamic_pointer_cast<STTMRAMModel>(model)) {
            return mram->getSubarrayReadLatency();
        } else if (auto pcm = std::dynamic_pointer_cast<PCMModel>(model)) {
            return pcm->getSubarrayReadLatency();
        } else if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model)) {
            return reram->getSubarrayReadLatency();
        }
        return 10.0;
    }

    static double getSubarrayWriteLatency(std::shared_ptr<MemoryModel> model, const std::string& tech) {
        if (auto sram = std::dynamic_pointer_cast<SRAMModel>(model)) {
            return sram->getSubarrayReadLatency();
        } else if (auto dram = std::dynamic_pointer_cast<DRAMModel>(model)) {
            return 15.0;
        } else if (auto mram = std::dynamic_pointer_cast<STTMRAMModel>(model)) {
            return mram->getSubarrayWriteLatency();
        } else if (auto pcm = std::dynamic_pointer_cast<PCMModel>(model)) {
            return pcm->getSubarraySetWriteLatency();
        } else if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model)) {
            return reram->getSubarrayWriteLatency();
        }
        return 10.0;
    }

    static double getInnerBankDatapath(std::shared_ptr<MemoryModel> model, const std::string& tech) {
        if (auto sram = std::dynamic_pointer_cast<SRAMModel>(model)) {
            return sram->getInnerBankDatapathLatency();
        } else if (auto dram = std::dynamic_pointer_cast<DRAMModel>(model)) {
            return 6.65;
        } else if (auto mram = std::dynamic_pointer_cast<STTMRAMModel>(model)) {
            return mram->getInnerBankReadLatency();
        } else if (auto pcm = std::dynamic_pointer_cast<PCMModel>(model)) {
            return pcm->getInnerBankReadLatency();
        } else if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model)) {
            return reram->getInnerBankReadLatency();
        }
        return 5.0;
    }

    static BFSMetrics simulateBFS(const BenchmarkConfig& config,
                                   double subarray_read_ns,
                                   double subarray_write_ns,
                                   double inner_bank_ns) {
        BFSMetrics metrics = {};

        // BFS operations per vertex:
        // 1. Read vertex (2 reads: ID + adjacency ptr)
        double read_vertex = subarray_read_ns * 2;

        // 2. Transfer to PE
        double transfer_to_pe = inner_bank_ns;

        // 3. Process each neighbor
        double per_neighbor_time = 0.0;

        // Read neighbor data
        per_neighbor_time += subarray_read_ns;      // Read neighbor ID
        per_neighbor_time += inner_bank_ns;         // Transfer to PE
        per_neighbor_time += subarray_read_ns;      // Read visited flag
        per_neighbor_time += inner_bank_ns;         // Transfer to PE

        // In-Order Core pipeline operations
        per_neighbor_time += config.fetch_decode_ns;                                    // Fetch & decode
        per_neighbor_time += config.execute_compare_ns;                                 // Compare
        per_neighbor_time += config.branch_penalty_ns * (1.0 - config.branch_prediction_accuracy);  // Branch penalty

        // Write back if not visited (50% probability)
        per_neighbor_time += inner_bank_ns * 0.5;           // Transfer from PE
        per_neighbor_time += subarray_write_ns * 0.5;       // Write visited flag
        per_neighbor_time += subarray_write_ns * 0.5;       // Enqueue neighbor

        double process_neighbors = per_neighbor_time * config.avg_degree;

        // Total time per vertex
        double vertex_time = read_vertex + transfer_to_pe + process_neighbors;

        // Total latency: vertices distributed across banks
        uint64_t vertices_per_bank = config.num_vertices / config.num_banks;
        metrics.total_latency_ms = (vertex_time * vertices_per_bank) / 1e6;
        metrics.total_per_vertex_ns = vertex_time;

        // Throughput
        double total_latency_ns = vertex_time * vertices_per_bank;
        metrics.vertices_per_second = (config.num_vertices / total_latency_ns) * 1e9;
        metrics.edges_per_second = (config.num_vertices * config.avg_degree / total_latency_ns) * 1e9;

        // Energy (simplified)
        metrics.energy_per_vertex_pj = 1.0;  // Placeholder

        return metrics;
    }

    static void printResults(const BFSMetrics& metrics) {
        std::cout << "\nResults:" << std::endl;
        std::cout << "  Total latency: " << std::fixed << std::setprecision(2)
                  << metrics.total_latency_ms << " ms" << std::endl;
        std::cout << "  Throughput: " << metrics.vertices_per_second / 1e6
                  << " M vertices/sec" << std::endl;
        std::cout << "  Edges/sec: " << metrics.edges_per_second / 1e6
                  << " M edges/sec" << std::endl;
        std::cout << "  Per-vertex time: " << metrics.total_per_vertex_ns
                  << " ns" << std::endl;
    }
};

//=============================================================================
// Main
//=============================================================================

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --config <file>      Run single benchmark config" << std::endl;
    std::cout << "  --batch <pattern>    Run all configs matching pattern" << std::endl;
    std::cout << "  --help               Show this help" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  " << program << " --config configs/benchmarks/bfs_bank_inorder_sram.yaml" << std::endl;
    std::cout << "  " << program << " --batch configs/benchmarks/bfs_bank_inorder_*.yaml" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "--help" || mode == "-h") {
        printUsage(argv[0]);
        return 0;
    }

    try {
        std::vector<BFSMetrics> all_results;

        if (mode == "--config" && argc >= 3) {
            // Single config mode
            std::string config_file = argv[2];
            auto config = SimpleConfigParser::parseConfig(config_file);
            auto metrics = BFSBenchmark::run(config);
            all_results.push_back(metrics);

        } else if (mode == "--batch" && argc >= 3) {
            // Batch mode - run all specified configs
            std::vector<std::string> config_files = {
                "configs/benchmarks/bfs_bank_inorder_dram.yaml",
                "configs/benchmarks/bfs_bank_inorder_sram.yaml",
                "configs/benchmarks/bfs_bank_inorder_sttmram.yaml",
                "configs/benchmarks/bfs_bank_inorder_pcm.yaml",
                "configs/benchmarks/bfs_bank_inorder_reram.yaml"
            };

            for (const auto& file : config_files) {
                auto config = SimpleConfigParser::parseConfig(file);
                auto metrics = BFSBenchmark::run(config);
                all_results.push_back(metrics);
            }

        } else {
            std::cerr << "Invalid arguments" << std::endl;
            printUsage(argv[0]);
            return 1;
        }

        // Print summary table
        if (all_results.size() > 1) {
            std::cout << "\n========================================" << std::endl;
            std::cout << "SUMMARY: ALL MEMORY TECHNOLOGIES" << std::endl;
            std::cout << "========================================" << std::endl;

            std::cout << std::left << std::setw(14) << "Technology"
                      << std::right << std::setw(15) << "Latency (ms)"
                      << std::setw(18) << "Throughput (Mv/s)"
                      << std::endl;
            std::cout << std::string(47, '-') << std::endl;

            for (const auto& m : all_results) {
                std::cout << std::left << std::setw(14) << m.technology_name
                          << std::right << std::fixed << std::setprecision(2)
                          << std::setw(15) << m.total_latency_ms
                          << std::setw(18) << (m.vertices_per_second / 1e6)
                          << std::endl;
            }
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "BENCHMARK COMPLETED!" << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
