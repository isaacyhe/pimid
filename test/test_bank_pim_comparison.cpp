/**
 * @file test_bank_pim_comparison.cpp
 * @brief Bank-level PIM comparison across all memory technologies
 *
 * Compares two Processing Element (PE) configurations at bank level:
 * 1. **In-Order Core** - More capable, can execute complex instructions
 *    - Fetch/decode overhead
 *    - Pipeline stages (5-stage typical)
 *    - ~5-10 cycles per operation
 *    - Can do branches, loads, stores
 *
 * 2. **Simple ALU** - Fast arithmetic operations only
 *    - No instruction overhead
 *    - ~1-2 cycles per operation
 *    - Limited to ADD, MUL, MAC operations
 *
 * Key differences from subarray-level PIM:
 * - PE is SHARED across all subarrays in a bank
 * - Data must move from subarrays to bank-level PE
 * - Inner-bank network latency is critical
 * - Potential contention for shared PE
 *
 * Tests: SRAM, STT-MRAM, PCM, ReRAM
 * Workloads: Vector Add, Vector Mul, Dot Product, Matrix-Vector Multiply
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>

#include "memory_models/include/memory_model.h"
#include "memory_models/include/sram_model.h"
#include "memory_models/include/sttmram_model.h"
#include "memory_models/include/pcm_model.h"
#include "memory_models/include/reram_model.h"

using namespace pimid;

//=============================================================================
// Processing Element (PE) Types
//=============================================================================

enum class PEType {
    IN_ORDER_CORE,  // More capable, but slower
    SIMPLE_ALU      // Fast, but limited
};

struct PEConfiguration {
    PEType type;
    std::string name;

    // Timing (cycles at 1GHz = ns)
    double fetch_decode_ns;      // Instruction fetch/decode overhead
    double add_latency_ns;        // ADD operation
    double mul_latency_ns;        // MUL operation
    double mac_latency_ns;        // Multiply-accumulate
    double branch_latency_ns;     // Branch (only for core)
    double load_latency_ns;       // Load from local buffer

    // Capabilities
    bool supports_branches;
    bool supports_complex_ops;

    // Energy (pJ per operation)
    double fetch_decode_energy_pj;
    double alu_energy_pj;
};

// In-order core configuration (RISC-V style)
PEConfiguration createInOrderCore() {
    PEConfiguration pe;
    pe.type = PEType::IN_ORDER_CORE;
    pe.name = "In-Order Core";

    // 5-stage pipeline: Fetch, Decode, Execute, Memory, WriteBack
    pe.fetch_decode_ns = 2.0;      // 2 cycles for fetch+decode
    pe.add_latency_ns = 1.0;       // 1 cycle in execute stage
    pe.mul_latency_ns = 3.0;       // 3 cycles (unpipelined multiplier)
    pe.mac_latency_ns = 4.0;       // MUL + ADD
    pe.branch_latency_ns = 3.0;    // Branch misprediction penalty
    pe.load_latency_ns = 1.0;      // Load from L0 buffer

    pe.supports_branches = true;
    pe.supports_complex_ops = true;

    pe.fetch_decode_energy_pj = 10.0;  // Instruction processing
    pe.alu_energy_pj = 5.0;

    return pe;
}

// Simple ALU configuration
PEConfiguration createSimpleALU() {
    PEConfiguration pe;
    pe.type = PEType::SIMPLE_ALU;
    pe.name = "Simple ALU";

    // No instruction overhead, just datapath
    pe.fetch_decode_ns = 0.0;      // No instruction fetch
    pe.add_latency_ns = 0.5;       // Very fast ADD
    pe.mul_latency_ns = 1.5;       // Fast MUL (dedicated multiplier)
    pe.mac_latency_ns = 2.0;       // MAC unit
    pe.branch_latency_ns = 0.0;    // No branches
    pe.load_latency_ns = 0.5;      // Simple load

    pe.supports_branches = false;
    pe.supports_complex_ops = false;

    pe.fetch_decode_energy_pj = 0.0;   // No instruction processing
    pe.alu_energy_pj = 2.0;             // Just datapath

    return pe;
}

//=============================================================================
// Workload Configuration
//=============================================================================

struct BankPIMWorkload {
    uint64_t vector_size;
    uint32_t num_banks;
    uint32_t subarrays_per_bank;
    uint32_t operations_per_element;
    std::string workload_name;
};

const BankPIMWorkload VECTOR_ADD_BANK = {
    .vector_size = 1024 * 1024,
    .num_banks = 8,
    .subarrays_per_bank = 16,
    .operations_per_element = 1,
    .workload_name = "Vector Addition (Bank-Level)"
};

const BankPIMWorkload VECTOR_MUL_BANK = {
    .vector_size = 1024 * 1024,
    .num_banks = 8,
    .subarrays_per_bank = 16,
    .operations_per_element = 1,
    .workload_name = "Vector Multiply (Bank-Level)"
};

const BankPIMWorkload DOT_PRODUCT_BANK = {
    .vector_size = 1024 * 1024,
    .num_banks = 8,
    .subarrays_per_bank = 16,
    .operations_per_element = 2,  // MUL + ADD
    .workload_name = "Dot Product (Bank-Level)"
};

const BankPIMWorkload MATRIX_VECTOR_BANK = {
    .vector_size = 256 * 256,
    .num_banks = 8,
    .subarrays_per_bank = 16,
    .operations_per_element = 256,  // 256 MAC per row
    .workload_name = "Matrix-Vector Multiply (Bank-Level)"
};

//=============================================================================
// Performance Metrics
//=============================================================================

struct BankPIMMetrics {
    std::string technology_name;
    std::string pe_name;
    std::string workload_name;

    // Timing breakdown
    double subarray_read_ns;
    double inner_bank_transfer_ns;   // Subarray → Bank PE
    double pe_compute_ns;
    double bank_write_ns;
    double total_latency_ns;

    // Throughput
    double throughput_gops;
    double effective_bandwidth_gbs;

    // Energy breakdown
    double memory_read_energy_pj;
    double data_transfer_energy_pj;
    double pe_compute_energy_pj;
    double memory_write_energy_pj;
    double total_energy_pj;
    double energy_per_op_pj;

    // Special features
    bool used_analog_compute;
    std::string notes;
};

//=============================================================================
// Bank-Level PIM Simulator
//=============================================================================

class BankPIMSimulator {
public:
    BankPIMSimulator(std::shared_ptr<MemoryModel> model,
                     const PEConfiguration& pe_config,
                     const std::string& tech_name)
        : model_(model), pe_config_(pe_config), tech_name_(tech_name) {}

    BankPIMMetrics simulateWorkload(const BankPIMWorkload& config) {
        BankPIMMetrics metrics;
        metrics.technology_name = tech_name_;
        metrics.pe_name = pe_config_.name;
        metrics.workload_name = config.workload_name;
        metrics.used_analog_compute = false;

        std::cout << "\n--- Simulating " << config.workload_name
                  << " on " << tech_name_ << " with " << pe_config_.name << " ---" << std::endl;

        // Elements per bank
        uint64_t elements_per_bank = config.vector_size / config.num_banks;
        uint64_t elements_per_subarray = elements_per_bank / config.subarrays_per_bank;

        std::cout << "Banks: " << config.num_banks << std::endl;
        std::cout << "Subarrays per bank: " << config.subarrays_per_bank << std::endl;
        std::cout << "Elements per bank: " << elements_per_bank << std::endl;
        std::cout << "Elements per subarray: " << elements_per_subarray << std::endl;

        // Check for ReRAM analog compute (special case)
        if (isAnalogCapable() && config.workload_name.find("Matrix") != std::string::npos) {
            return simulateAnalogCompute(config);
        }

        // Get memory timing
        double subarray_read_ns = getSubarrayReadLatency();
        double subarray_write_ns = getSubarrayWriteLatency();
        double inner_bank_ns = getInnerBankDatapathLatency();

        std::cout << "Subarray read: " << subarray_read_ns << " ns" << std::endl;
        std::cout << "Subarray write: " << subarray_write_ns << " ns" << std::endl;
        std::cout << "Inner-bank datapath: " << inner_bank_ns << " ns" << std::endl;

        // Bank-level PIM operation flow:
        // 1. Read operands from subarrays
        // 2. Transfer data through inner-bank network to bank-level PE
        // 3. PE computes result
        // 4. Write result back through inner-bank network to subarray

        // STEP 1: Read from subarrays (can overlap across subarrays)
        double read_time = subarray_read_ns * 2;  // Read A and B

        // STEP 2: Transfer to bank PE via inner-bank network
        // Data travels: subarray → local I/O → H-tree → global I/O → bank PE
        double transfer_to_pe = inner_bank_ns;

        // STEP 3: PE computation
        double pe_time = 0.0;
        if (config.operations_per_element == 1) {
            // Simple operation (ADD or MUL)
            pe_time = pe_config_.fetch_decode_ns +
                      (config.workload_name.find("Add") != std::string::npos
                       ? pe_config_.add_latency_ns
                       : pe_config_.mul_latency_ns);
        } else if (config.operations_per_element == 2) {
            // Dot product: MUL + ADD
            pe_time = pe_config_.fetch_decode_ns + pe_config_.mac_latency_ns;
        } else {
            // Matrix operation: many MACs
            pe_time = pe_config_.fetch_decode_ns +
                      (config.operations_per_element * pe_config_.mac_latency_ns);
        }

        // STEP 4: Write result back (if needed)
        double write_time = needsWrite(config.workload_name) ?
                           (inner_bank_ns + subarray_write_ns) : 0.0;

        // Total time per operation at one bank PE
        double bank_operation_time = read_time + transfer_to_pe + pe_time + write_time;

        std::cout << "  Read time: " << read_time << " ns" << std::endl;
        std::cout << "  Transfer to PE: " << transfer_to_pe << " ns" << std::endl;
        std::cout << "  PE compute: " << pe_time << " ns" << std::endl;
        std::cout << "  Write back: " << write_time << " ns" << std::endl;
        std::cout << "  Bank operation time: " << bank_operation_time << " ns" << std::endl;

        // All banks work in parallel, but PE is shared within each bank
        // So we need to process elements_per_bank operations sequentially at each bank
        metrics.total_latency_ns = bank_operation_time * elements_per_bank;

        // Breakdown
        metrics.subarray_read_ns = read_time;
        metrics.inner_bank_transfer_ns = transfer_to_pe + (needsWrite(config.workload_name) ? inner_bank_ns : 0.0);
        metrics.pe_compute_ns = pe_time;
        metrics.bank_write_ns = needsWrite(config.workload_name) ? subarray_write_ns : 0.0;

        std::cout << "Total latency: " << (metrics.total_latency_ns / 1000.0) << " μs" << std::endl;

        // Energy calculation
        uint64_t bytes_per_element = 8;
        uint64_t total_reads = config.vector_size * 2;
        uint64_t total_writes = needsWrite(config.workload_name) ? config.vector_size : 0;

        double read_energy_per_byte = model_->getReadEnergy();
        double write_energy_per_byte = model_->getWriteEnergy();

        // Convert nJ to pJ if needed
        if (read_energy_per_byte > 100.0) {
            read_energy_per_byte *= 1000.0;
            write_energy_per_byte *= 1000.0;
        }

        metrics.memory_read_energy_pj = total_reads * bytes_per_element * read_energy_per_byte;
        metrics.memory_write_energy_pj = total_writes * bytes_per_element * write_energy_per_byte;

        // Data transfer energy (inner-bank network)
        double transfer_energy_per_byte = 0.5;  // pJ per byte through H-tree
        metrics.data_transfer_energy_pj = (total_reads + total_writes) * bytes_per_element * transfer_energy_per_byte;

        // PE compute energy
        uint64_t total_ops = config.vector_size * config.operations_per_element;
        metrics.pe_compute_energy_pj = total_ops * (pe_config_.fetch_decode_energy_pj + pe_config_.alu_energy_pj);

        metrics.total_energy_pj = metrics.memory_read_energy_pj +
                                  metrics.data_transfer_energy_pj +
                                  metrics.pe_compute_energy_pj +
                                  metrics.memory_write_energy_pj;

        // Throughput
        uint64_t total_operations = total_ops;
        metrics.throughput_gops = total_operations / metrics.total_latency_ns;

        uint64_t total_bytes = (total_reads + total_writes) * bytes_per_element;
        metrics.effective_bandwidth_gbs = total_bytes / metrics.total_latency_ns;

        metrics.energy_per_op_pj = metrics.total_energy_pj / total_operations;

        return metrics;
    }

private:
    std::shared_ptr<MemoryModel> model_;
    PEConfiguration pe_config_;
    std::string tech_name_;

    BankPIMMetrics simulateAnalogCompute(const BankPIMWorkload& config) {
        BankPIMMetrics metrics;
        metrics.technology_name = tech_name_;
        metrics.pe_name = "Analog Crossbar";
        metrics.workload_name = config.workload_name;
        metrics.used_analog_compute = true;
        metrics.notes = "ReRAM analog crossbar compute - no bank PE needed!";

        std::cout << "*** USING ANALOG COMPUTE - No bank PE needed! ***" << std::endl;

        double analog_latency = getAnalogComputeLatency();
        uint64_t elements_per_bank = config.vector_size / config.num_banks;

        // Analog compute happens IN the subarray, no transfer needed!
        metrics.total_latency_ns = analog_latency * elements_per_bank;
        metrics.subarray_read_ns = 0.0;
        metrics.inner_bank_transfer_ns = 0.0;
        metrics.pe_compute_ns = analog_latency;
        metrics.bank_write_ns = 0.0;

        // Energy
        double analog_energy = getAnalogComputeEnergy();
        uint64_t total_ops = config.vector_size * config.operations_per_element;
        metrics.pe_compute_energy_pj = total_ops * analog_energy;
        metrics.memory_read_energy_pj = 0.0;
        metrics.data_transfer_energy_pj = 0.0;
        metrics.memory_write_energy_pj = 0.0;
        metrics.total_energy_pj = metrics.pe_compute_energy_pj;

        metrics.throughput_gops = total_ops / metrics.total_latency_ns;
        metrics.energy_per_op_pj = metrics.total_energy_pj / total_ops;

        return metrics;
    }

    double getSubarrayReadLatency() {
        if (auto sram = std::dynamic_pointer_cast<SRAMModel>(model_)) {
            return sram->getSubarrayReadLatency();
        } else if (auto mram = std::dynamic_pointer_cast<STTMRAMModel>(model_)) {
            return mram->getSubarrayReadLatency();
        } else if (auto pcm = std::dynamic_pointer_cast<PCMModel>(model_)) {
            return pcm->getSubarrayReadLatency();
        } else if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model_)) {
            return reram->getSubarrayReadLatency();
        }
        return 10.0;
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
        }
        return 10.0;
    }

    double getInnerBankDatapathLatency() {
        if (auto sram = std::dynamic_pointer_cast<SRAMModel>(model_)) {
            return sram->getInnerBankDatapathLatency();
        } else if (auto mram = std::dynamic_pointer_cast<STTMRAMModel>(model_)) {
            return mram->getInnerBankReadLatency();
        } else if (auto pcm = std::dynamic_pointer_cast<PCMModel>(model_)) {
            return pcm->getInnerBankReadLatency();
        } else if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model_)) {
            return reram->getInnerBankReadLatency();
        }
        return 5.0;
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
        return 1000.0;
    }

    double getAnalogComputeEnergy() {
        if (auto reram = std::dynamic_pointer_cast<ReRAMModel>(model_)) {
            return reram->getAnalogComputeEnergy();
        }
        return 100.0;
    }

    bool needsWrite(const std::string& workload) {
        return workload.find("Dot Product") == std::string::npos;
    }
};

//=============================================================================
// Results Display
//=============================================================================

void printBankPIMResults(const std::vector<BankPIMMetrics>& results,
                         const std::string& workload_name) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "WORKLOAD: " << workload_name << std::endl;
    std::cout << "========================================" << std::endl;

    // Header
    std::cout << std::left << std::setw(12) << "Technology"
              << std::setw(16) << "PE Type"
              << std::right << std::setw(15) << "Latency (μs)"
              << std::setw(15) << "Throughput"
              << std::setw(15) << "Energy/Op (pJ)"
              << std::endl;
    std::cout << std::string(73, '-') << std::endl;

    for (const auto& m : results) {
        std::cout << std::left << std::setw(12) << m.technology_name
                  << std::setw(16) << m.pe_name
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(15) << (m.total_latency_ns / 1000.0)
                  << std::setw(13) << m.throughput_gops << " G"
                  << std::setw(15) << m.energy_per_op_pj;

        if (m.used_analog_compute) {
            std::cout << " (ANALOG!)";
        }

        std::cout << std::endl;
    }

    std::cout << "\n--- Detailed Breakdown ---" << std::endl;
    for (const auto& m : results) {
        std::cout << "\n" << m.technology_name << " + " << m.pe_name << ":" << std::endl;

        if (!m.used_analog_compute) {
            std::cout << "  Timing:" << std::endl;
            std::cout << "    Subarray read:       " << m.subarray_read_ns << " ns" << std::endl;
            std::cout << "    Inner-bank transfer: " << m.inner_bank_transfer_ns << " ns" << std::endl;
            std::cout << "    PE compute:          " << m.pe_compute_ns << " ns" << std::endl;
            std::cout << "    Bank write:          " << m.bank_write_ns << " ns" << std::endl;

            std::cout << "  Energy:" << std::endl;
            std::cout << "    Memory read:    " << (m.memory_read_energy_pj / 1e6) << " μJ" << std::endl;
            std::cout << "    Data transfer:  " << (m.data_transfer_energy_pj / 1e6) << " μJ" << std::endl;
            std::cout << "    PE compute:     " << (m.pe_compute_energy_pj / 1e6) << " μJ" << std::endl;
            std::cout << "    Memory write:   " << (m.memory_write_energy_pj / 1e6) << " μJ" << std::endl;
        } else {
            std::cout << "  Analog compute: " << m.pe_compute_ns << " ns (in-place!)" << std::endl;
            std::cout << "  Energy: " << (m.total_energy_pj / 1e6) << " μJ (very low!)" << std::endl;
        }

        if (!m.notes.empty()) {
            std::cout << "  Note: " << m.notes << std::endl;
        }
    }
}

//=============================================================================
// Main Test
//=============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Bank-Level PIM Comparison Test" << std::endl;
    std::cout << "PE Types: In-Order Core vs Simple ALU" << std::endl;
    std::cout << "Technologies: SRAM, STT-MRAM, PCM, ReRAM" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // Create memory models
        auto sram = std::make_shared<SRAMModel>("config.yaml");
        auto mram = std::make_shared<STTMRAMModel>("config.yaml");
        auto pcm = std::make_shared<PCMModel>("config.yaml");
        auto reram = std::make_shared<ReRAMModel>("config.yaml");

        sram->initialize();
        mram->initialize();
        pcm->initialize();
        reram->initialize();

        // Create PE configurations
        auto in_order_core = createInOrderCore();
        auto simple_alu = createSimpleALU();

        std::cout << "\n--- Processing Element Configurations ---" << std::endl;
        std::cout << "In-Order Core: Fetch/Decode=" << in_order_core.fetch_decode_ns
                  << "ns, ADD=" << in_order_core.add_latency_ns
                  << "ns, MUL=" << in_order_core.mul_latency_ns << "ns" << std::endl;
        std::cout << "Simple ALU:    No overhead, ADD=" << simple_alu.add_latency_ns
                  << "ns, MUL=" << simple_alu.mul_latency_ns << "ns" << std::endl;

        // Workloads
        std::vector<BankPIMWorkload> workloads = {
            VECTOR_ADD_BANK,
            VECTOR_MUL_BANK,
            DOT_PRODUCT_BANK,
            MATRIX_VECTOR_BANK
        };

        // Run all combinations
        for (const auto& workload : workloads) {
            std::vector<BankPIMMetrics> workload_results;

            // Test each technology with both PE types
            for (auto pe_config : {in_order_core, simple_alu}) {
                BankPIMSimulator sram_sim(sram, pe_config, "SRAM");
                BankPIMSimulator mram_sim(mram, pe_config, "STT-MRAM");
                BankPIMSimulator pcm_sim(pcm, pe_config, "PCM");
                BankPIMSimulator reram_sim(reram, pe_config, "ReRAM");

                workload_results.push_back(sram_sim.simulateWorkload(workload));
                workload_results.push_back(mram_sim.simulateWorkload(workload));
                workload_results.push_back(pcm_sim.simulateWorkload(workload));
                workload_results.push_back(reram_sim.simulateWorkload(workload));
            }

            printBankPIMResults(workload_results, workload.workload_name);
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "BANK-LEVEL PIM TEST COMPLETED!" << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
