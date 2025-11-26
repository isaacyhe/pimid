/**
 * @file test_subarray_pim_comparison.cpp
 * @brief Comprehensive comparison of per-subarray PIM across all memory technologies
 *
 * This test simulates per-subarray Processing Elements (ALUs) for:
 * - SRAM (cache-level PIM)
 * - STT-MRAM (non-volatile PIM)
 * - PCM (read-only PIM)
 * - ReRAM (analog compute PIM)
 *
 * Workloads tested:
 * 1. Vector Addition (R+R→W)
 * 2. Vector Multiplication (R+R→W)
 * 3. Dot Product (R+R→accumulate)
 * 4. Matrix-Vector Multiply (special for ReRAM analog)
 *
 * Metrics measured:
 * - Total latency (ns)
 * - Energy consumption (pJ)
 * - Throughput (GOPS)
 * - Energy efficiency (pJ/op)
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>
#include <cmath>
#include <chrono>

#include "memory_models/include/memory_model.h"
#include "memory_models/include/sram_model.h"
#include "memory_models/include/sttmram_model.h"
#include "memory_models/include/pcm_model.h"
#include "memory_models/include/reram_model.h"

using namespace pimid;

//=============================================================================
// Test Configuration
//=============================================================================

struct PIMWorkloadConfig {
    uint64_t vector_size;           // Number of elements
    uint32_t num_subarrays;         // Number of parallel subarrays
    uint32_t operations_per_element; // ALU operations per element
    std::string workload_name;
};

// Workload configurations
const PIMWorkloadConfig VECTOR_ADD = {
    .vector_size = 1024 * 1024,  // 1M elements
    .num_subarrays = 16,
    .operations_per_element = 1,  // 1 ADD
    .workload_name = "Vector Addition (A+B)"
};

const PIMWorkloadConfig VECTOR_MUL = {
    .vector_size = 1024 * 1024,
    .num_subarrays = 16,
    .operations_per_element = 1,  // 1 MUL
    .workload_name = "Vector Multiply (A*B)"
};

const PIMWorkloadConfig DOT_PRODUCT = {
    .vector_size = 1024 * 1024,
    .num_subarrays = 16,
    .operations_per_element = 2,  // 1 MUL + 1 ADD
    .workload_name = "Dot Product (A·B)"
};

const PIMWorkloadConfig MATRIX_VECTOR = {
    .vector_size = 256 * 256,    // 256x256 matrix
    .num_subarrays = 16,
    .operations_per_element = 256,  // 256 MAC operations per row
    .workload_name = "Matrix-Vector Multiply (Ax)"
};

//=============================================================================
// Performance Metrics
//=============================================================================

struct PIMPerformanceMetrics {
    std::string technology_name;
    std::string workload_name;

    // Timing
    double total_latency_ns;
    double subarray_latency_ns;
    double read_latency_ns;
    double write_latency_ns;
    double compute_latency_ns;

    // Energy
    double total_energy_pj;
    double read_energy_pj;
    double write_energy_pj;
    double compute_energy_pj;

    // Throughput
    double throughput_gops;
    double bandwidth_gbs;

    // Efficiency
    double energy_per_op_pj;

    // Special features
    bool used_analog_compute;
    std::string notes;
};

//=============================================================================
// PIM Operation Simulator
//=============================================================================

class SubarrayPIMSimulator {
public:
    SubarrayPIMSimulator(std::shared_ptr<MemoryModel> model, const std::string& tech_name)
        : model_(model), tech_name_(tech_name) {}

    PIMPerformanceMetrics simulateWorkload(const PIMWorkloadConfig& config) {
        PIMPerformanceMetrics metrics;
        metrics.technology_name = tech_name_;
        metrics.workload_name = config.workload_name;
        metrics.used_analog_compute = false;

        std::cout << "\n--- Simulating " << config.workload_name
                  << " on " << tech_name_ << " ---" << std::endl;

        // Calculate elements per subarray
        uint64_t elements_per_subarray = config.vector_size / config.num_subarrays;

        // Get timing parameters
        double read_lat_ns = getReadLatency();
        double write_lat_ns = getWriteLatency();
        double compute_lat_ns = getComputeLatency(config.operations_per_element);

        std::cout << "Read latency: " << read_lat_ns << " ns" << std::endl;
        std::cout << "Write latency: " << write_lat_ns << " ns" << std::endl;
        std::cout << "Compute latency: " << compute_lat_ns << " ns/op" << std::endl;

        // Simulate per-subarray PIM operation
        // Each subarray processes elements_per_subarray elements in parallel

        double subarray_operation_time = 0.0;

        // For most workloads: Read A, Read B, Compute, Write Result
        if (!isAnalogCapable() || config.workload_name.find("Matrix") == std::string::npos) {
            // Standard digital compute path
            double read_a_time = read_lat_ns;
            double read_b_time = read_lat_ns;
            double compute_time = compute_lat_ns * config.operations_per_element;
            double write_time = needsWrite(config.workload_name) ? write_lat_ns : 0.0;

            // Pipeline: Read A -> Read B -> Compute -> Write (overlapped if possible)
            subarray_operation_time = read_a_time + read_b_time + compute_time + write_time;

            metrics.read_latency_ns = read_a_time + read_b_time;
            metrics.write_latency_ns = write_time;
            metrics.compute_latency_ns = compute_time;
        } else {
            // ReRAM analog compute for matrix-vector multiply!
            metrics.used_analog_compute = true;
            double analog_lat = getAnalogComputeLatency();

            std::cout << "*** USING ANALOG COMPUTE: " << analog_lat << " ns ***" << std::endl;

            subarray_operation_time = analog_lat;
            metrics.compute_latency_ns = analog_lat;
            metrics.read_latency_ns = 0.0;  // Analog compute doesn't need separate reads
            metrics.write_latency_ns = 0.0; // Output is analog voltage
            metrics.notes = "Used analog crossbar compute";
        }

        metrics.subarray_latency_ns = subarray_operation_time;

        // Total latency = time for all elements to be processed by subarrays
        // All subarrays work in parallel
        uint64_t iterations = elements_per_subarray;
        metrics.total_latency_ns = subarray_operation_time * iterations;

        std::cout << "Subarray operation time: " << subarray_operation_time << " ns" << std::endl;
        std::cout << "Iterations per subarray: " << iterations << std::endl;
        std::cout << "Total latency: " << metrics.total_latency_ns << " ns" << std::endl;

        // Calculate energy
        double read_energy_per_byte = model_->getReadEnergy();  // nJ or pJ
        double write_energy_per_byte = model_->getWriteEnergy();

        // Convert to pJ if in nJ
        if (read_energy_per_byte > 100.0) {
            read_energy_per_byte *= 1000.0; // nJ to pJ
            write_energy_per_byte *= 1000.0;
        }

        uint64_t bytes_per_element = 8; // Assume 64-bit elements
        uint64_t total_reads = config.vector_size * 2; // Read A and B
        uint64_t total_writes = needsWrite(config.workload_name) ? config.vector_size : 0;

        if (metrics.used_analog_compute) {
            // ReRAM analog compute has very low energy
            metrics.compute_energy_pj = getAnalogComputeEnergy() * config.vector_size;
            metrics.read_energy_pj = 0.0;  // Built into analog compute
            metrics.write_energy_pj = 0.0;
        } else {
            metrics.read_energy_pj = total_reads * bytes_per_element * read_energy_per_byte;
            metrics.write_energy_pj = total_writes * bytes_per_element * write_energy_per_byte;
            metrics.compute_energy_pj = estimateComputeEnergy(config.operations_per_element, config.vector_size);
        }

        metrics.total_energy_pj = metrics.read_energy_pj + metrics.write_energy_pj + metrics.compute_energy_pj;

        // Calculate throughput
        uint64_t total_operations = config.vector_size * config.operations_per_element;
        metrics.throughput_gops = (total_operations / metrics.total_latency_ns);  // GOPS

        uint64_t total_bytes = total_reads * bytes_per_element + total_writes * bytes_per_element;
        metrics.bandwidth_gbs = (total_bytes / metrics.total_latency_ns);  // GB/s

        // Calculate efficiency
        metrics.energy_per_op_pj = metrics.total_energy_pj / total_operations;

        return metrics;
    }

private:
    std::shared_ptr<MemoryModel> model_;
    std::string tech_name_;

    double getReadLatency() {
        // Try to get subarray-level latency if available
        if (auto sram = std::dynamic_pointer_cast<SRAMModel>(model_)) {
            return sram->getSubarrayReadLatency();
        } else if (auto mram = std::dynamic_pointer_cast<STTMRAMModel>(model_)) {
            return mram->getSubarrayReadLatency();
        } else if (auto pcm = std::dynamic_pointer_cast<PCMModel>(model_)) {
            return pcm->getSubarrayReadLatency();
        } else if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model_)) {
            return reram->getSubarrayReadLatency();
        }
        return 10.0; // Default
    }

    double getWriteLatency() {
        if (auto sram = std::dynamic_pointer_cast<SRAMModel>(model_)) {
            return sram->getSubarrayReadLatency(); // SRAM has symmetric R/W
        } else if (auto mram = std::dynamic_pointer_cast<STTMRAMModel>(model_)) {
            return mram->getSubarrayWriteLatency();
        } else if (auto pcm = std::dynamic_pointer_cast<PCMModel>(model_)) {
            return pcm->getSubarraySetWriteLatency(); // Very slow!
        } else if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model_)) {
            return reram->getSubarrayWriteLatency();
        }
        return 10.0;
    }

    double getComputeLatency(uint32_t ops_per_element) {
        // Assume simple ALU operations (ADD, MUL, etc.)
        // Modern ALUs can do 1 op per cycle at ~1-2 GHz
        double alu_latency_per_op = 0.5; // ns (2 GHz clock)
        return alu_latency_per_op;
    }

    bool isAnalogCapable() {
        if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model_)) {
            return reram->supportsAnalogCompute();
        }
        return false;
    }

    double getAnalogComputeLatency() {
        if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model_)) {
            return reram->getAnalogComputeLatency();
        }
        return 1000.0; // Should never reach here
    }

    double getAnalogComputeEnergy() {
        if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model_)) {
            return reram->getAnalogComputeEnergy();
        }
        return 100.0;
    }

    bool needsWrite(const std::string& workload) {
        // Dot product doesn't need to write intermediate results to memory
        return workload.find("Dot Product") == std::string::npos;
    }

    double estimateComputeEnergy(uint32_t ops_per_element, uint64_t num_elements) {
        // Energy for ALU operations
        // Modern ALUs: ~1-10 pJ per operation
        double energy_per_alu_op = 5.0; // pJ
        return energy_per_alu_op * ops_per_element * num_elements;
    }
};

//=============================================================================
// Results Display
//=============================================================================

void printMetricsTable(const std::vector<PIMPerformanceMetrics>& all_metrics,
                       const std::string& workload_name) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "WORKLOAD: " << workload_name << std::endl;
    std::cout << "========================================" << std::endl;

    // Header
    std::cout << std::left << std::setw(12) << "Technology"
              << std::right << std::setw(15) << "Latency (μs)"
              << std::setw(15) << "Throughput"
              << std::setw(15) << "Energy (μJ)"
              << std::setw(15) << "Energy/Op (pJ)"
              << std::endl;
    std::cout << std::string(72, '-') << std::endl;

    // Find baseline (SRAM) for comparison
    double baseline_latency = 0.0;
    for (const auto& m : all_metrics) {
        if (m.technology_name == "SRAM") {
            baseline_latency = m.total_latency_ns;
            break;
        }
    }

    // Print results
    for (const auto& m : all_metrics) {
        std::cout << std::left << std::setw(12) << m.technology_name
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(15) << (m.total_latency_ns / 1000.0); // ns to μs

        if (baseline_latency > 0) {
            double speedup = baseline_latency / m.total_latency_ns;
            std::cout << std::setw(15) << (m.throughput_gops) << " GOPS";
        }

        std::cout << std::setw(15) << (m.total_energy_pj / 1e6) // pJ to μJ
                  << std::setw(15) << m.energy_per_op_pj;

        if (m.used_analog_compute) {
            std::cout << " (ANALOG!)";
        }

        std::cout << std::endl;
    }

    std::cout << std::endl;

    // Detailed breakdown for each technology
    std::cout << "--- Detailed Timing Breakdown ---" << std::endl;
    for (const auto& m : all_metrics) {
        std::cout << "\n" << m.technology_name << ":" << std::endl;
        std::cout << "  Subarray operation: " << m.subarray_latency_ns << " ns" << std::endl;

        if (!m.used_analog_compute) {
            std::cout << "    - Read:    " << m.read_latency_ns << " ns" << std::endl;
            std::cout << "    - Compute: " << m.compute_latency_ns << " ns" << std::endl;
            std::cout << "    - Write:   " << m.write_latency_ns << " ns" << std::endl;
        } else {
            std::cout << "    - Analog compute: " << m.compute_latency_ns << " ns (crossbar!)" << std::endl;
        }

        std::cout << "  Energy breakdown:" << std::endl;
        std::cout << "    - Read:    " << (m.read_energy_pj / 1e6) << " μJ" << std::endl;
        std::cout << "    - Compute: " << (m.compute_energy_pj / 1e6) << " μJ" << std::endl;
        std::cout << "    - Write:   " << (m.write_energy_pj / 1e6) << " μJ" << std::endl;

        if (!m.notes.empty()) {
            std::cout << "  Note: " << m.notes << std::endl;
        }
    }
}

void printComparativeSummary(const std::vector<std::vector<PIMPerformanceMetrics>>& all_workload_results) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "COMPARATIVE SUMMARY: PER-SUBARRAY PIM" << std::endl;
    std::cout << "========================================" << std::endl;

    std::cout << "\n--- Best Technology for Each Workload ---" << std::endl;

    for (const auto& workload_results : all_workload_results) {
        if (workload_results.empty()) continue;

        std::string workload = workload_results[0].workload_name;

        // Find best latency
        auto best_latency = std::min_element(workload_results.begin(), workload_results.end(),
            [](const auto& a, const auto& b) { return a.total_latency_ns < b.total_latency_ns; });

        // Find best energy
        auto best_energy = std::min_element(workload_results.begin(), workload_results.end(),
            [](const auto& a, const auto& b) { return a.energy_per_op_pj < b.energy_per_op_pj; });

        std::cout << "\n" << workload << ":" << std::endl;
        std::cout << "  Fastest:        " << best_latency->technology_name
                  << " (" << (best_latency->total_latency_ns / 1000.0) << " μs)" << std::endl;
        std::cout << "  Most Efficient: " << best_energy->technology_name
                  << " (" << best_energy->energy_per_op_pj << " pJ/op)" << std::endl;
    }

    std::cout << "\n--- Technology Recommendations ---" << std::endl;
    std::cout << "SRAM:     Best for general-purpose PIM, fastest overall" << std::endl;
    std::cout << "STT-MRAM: Best for non-volatile PIM, persistent state" << std::endl;
    std::cout << "PCM:      Best for READ-ONLY workloads (very slow writes!)" << std::endl;
    std::cout << "ReRAM:    Best for MATRIX operations with analog compute!" << std::endl;
}

//=============================================================================
// Main Test
//=============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Per-Subarray PIM Comparison Test" << std::endl;
    std::cout << "Testing: SRAM, STT-MRAM, PCM, ReRAM" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // Create memory models
        auto sram = std::make_shared<SRAMModel>("config.yaml");
        auto mram = std::make_shared<STTMRAMModel>("config.yaml");
        auto pcm = std::make_shared<PCMModel>("config.yaml");
        auto reram = std::make_shared<ReRAMModel>("config.yaml");

        // Initialize
        sram->initialize();
        mram->initialize();
        pcm->initialize();
        reram->initialize();

        // Create simulators
        SubarrayPIMSimulator sram_sim(sram, "SRAM");
        SubarrayPIMSimulator mram_sim(mram, "STT-MRAM");
        SubarrayPIMSimulator pcm_sim(pcm, "PCM");
        SubarrayPIMSimulator reram_sim(reram, "ReRAM");

        // Test configurations
        std::vector<PIMWorkloadConfig> workloads = {
            VECTOR_ADD,
            VECTOR_MUL,
            DOT_PRODUCT,
            MATRIX_VECTOR
        };

        std::vector<std::vector<PIMPerformanceMetrics>> all_results;

        // Run all workloads on all technologies
        for (const auto& workload : workloads) {
            std::vector<PIMPerformanceMetrics> workload_results;

            workload_results.push_back(sram_sim.simulateWorkload(workload));
            workload_results.push_back(mram_sim.simulateWorkload(workload));
            workload_results.push_back(pcm_sim.simulateWorkload(workload));
            workload_results.push_back(reram_sim.simulateWorkload(workload));

            printMetricsTable(workload_results, workload.workload_name);
            all_results.push_back(workload_results);
        }

        // Print comparative summary
        printComparativeSummary(all_results);

        std::cout << "\n========================================" << std::endl;
        std::cout << "TEST COMPLETED SUCCESSFULLY!" << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
