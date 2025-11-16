#include "dram_model.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace pimid {

//=============================================================================
// DRAMModel Implementation (Ramulator Integration)
//=============================================================================

DRAMModel::DRAMModel(const std::string& config_path)
    : MemoryModel(MemoryTechnology::DRAM, config_path)
    , ramulator_instance_(nullptr)
    , total_reads_(0)
    , total_writes_(0)
    , row_hits_(0)
    , row_misses_(0)
    , row_conflicts_(0)
    , read_energy_(0.0)
    , write_energy_(0.0)
    , leakage_power_(0.0)
    , activation_energy_(0.0)
    , precharge_energy_(0.0)
    , current_cycle_(0) {

    // Initialize default DRAM configuration
    dram_config_.standard = "DDR4";
    dram_config_.org = "4Gb_x8";
    dram_config_.channels = 1;
    dram_config_.ranks_per_channel = 1;
    dram_config_.banks_per_rank = 8;
    dram_config_.capacity = 4ULL * 1024 * 1024 * 1024; // 4GB
    dram_config_.bandwidth = 25600ULL * 1024 * 1024;    // 25.6 GB/s

    // DDR4-2400 timing parameters (in cycles at 1.2 GHz)
    dram_config_.tCL = 17;   // CAS latency
    dram_config_.tRCD = 17;  // RAS to CAS delay
    dram_config_.tRP = 17;   // Row precharge time
    dram_config_.tRAS = 39;  // Row active time
}

void DRAMModel::initialize() {
    std::cout << "[DRAMModel] Initializing DRAM model..." << std::endl;
    loadConfig(config_path_);

    // TODO: Initialize Ramulator instance when integrated
    // ramulator_instance_ = new RamulatorWrapper(dram_config_);

    std::cout << "[DRAMModel] Configuration:" << std::endl;
    std::cout << "  Standard: " << dram_config_.standard << std::endl;
    std::cout << "  Organization: " << dram_config_.org << std::endl;
    std::cout << "  Channels: " << dram_config_.channels << std::endl;
    std::cout << "  Ranks/Channel: " << dram_config_.ranks_per_channel << std::endl;
    std::cout << "  Banks/Rank: " << dram_config_.banks_per_rank << std::endl;
    std::cout << "  Capacity: " << (dram_config_.capacity / (1024.0 * 1024 * 1024)) << " GB" << std::endl;
    std::cout << "  Bandwidth: " << (dram_config_.bandwidth / (1024.0 * 1024 * 1024)) << " GB/s" << std::endl;
    std::cout << "  tCL: " << dram_config_.tCL << " cycles" << std::endl;
    std::cout << "  tRCD: " << dram_config_.tRCD << " cycles" << std::endl;
    std::cout << "  tRP: " << dram_config_.tRP << " cycles" << std::endl;
    std::cout << "  tRAS: " << dram_config_.tRAS << " cycles" << std::endl;
    std::cout << "[DRAMModel] Initialization complete" << std::endl;
}

void DRAMModel::loadConfig(const std::string& config_path) {
    // TODO: Parse YAML configuration file
    // For now, use default configuration
    // When YAML parsing is available:
    // - Parse memory_config.yaml
    // - Extract DRAM-specific parameters
    // - Update dram_config_ structure

    std::cout << "[DRAMModel] Loading configuration from: " << config_path << std::endl;
    std::cout << "[DRAMModel] Using default DDR4-2400 configuration" << std::endl;
}

Cycle DRAMModel::access(const MemoryRequest& req) {
    // Calculate access latency
    Cycle latency = calculateLatency(req);

    // Update statistics
    if (req.type == MemoryRequestType::READ) {
        total_reads_++;
        read_energy_ += 2.5;  // nJ per read (typical DDR4)
    } else if (req.type == MemoryRequestType::WRITE) {
        total_writes_++;
        write_energy_ += 3.0;  // nJ per write (typical DDR4)
    }

    // Update row buffer state
    updateRowBuffer(req.addr);

    // Add to pending requests queue
    pending_requests_.push(req);

    // Schedule completion callback
    if (completion_callback_) {
        Cycle completion_cycle = current_cycle_ + latency;
        // Callback will be invoked at completion_cycle
    }

    return latency;
}

bool DRAMModel::canAccept(const MemoryRequest& req) {
    // Check if request queue has space
    const size_t MAX_PENDING_REQUESTS = 64;
    return pending_requests_.size() < MAX_PENDING_REQUESTS;
}

void DRAMModel::tick() {
    current_cycle_++;

    // TODO: When Ramulator is integrated, call ramulator->tick()

    // Process pending requests
    if (!pending_requests_.empty()) {
        // Simple model: process one request per tick if ready
        pending_requests_.pop();
    }

    // Accumulate leakage energy
    leakage_power_ = 1.5; // W (typical DDR4 idle power)
}

Cycle DRAMModel::getLatency(MemoryRequestType type) const {
    // Base latency depends on request type
    Cycle base_latency = dram_config_.tCL;

    if (type == MemoryRequestType::WRITE) {
        // Write latency is typically similar to read in modern DRAM
        base_latency = dram_config_.tCL;
    } else if (type == MemoryRequestType::ATOMIC) {
        // Atomic operations require read-modify-write
        base_latency = dram_config_.tCL * 2;
    }

    return base_latency;
}

double DRAMModel::getTotalEnergy() const {
    // Total energy = dynamic energy + leakage energy
    // Leakage energy = leakage_power * time
    double leakage_energy = leakage_power_ * (current_cycle_ / 1e9); // Assuming 1 GHz
    return read_energy_ + write_energy_ + activation_energy_ +
           precharge_energy_ + leakage_energy;
}

void DRAMModel::printStats() const {
    std::cout << "\n=== DRAM Model Statistics ===" << std::endl;
    std::cout << "Total Cycles: " << current_cycle_ << std::endl;
    std::cout << "Total Reads: " << total_reads_ << std::endl;
    std::cout << "Total Writes: " << total_writes_ << std::endl;
    std::cout << "Row Hits: " << row_hits_ << std::endl;
    std::cout << "Row Misses: " << row_misses_ << std::endl;
    std::cout << "Row Conflicts: " << row_conflicts_ << std::endl;

    if (total_reads_ + total_writes_ > 0) {
        double row_hit_rate = static_cast<double>(row_hits_) /
                             (row_hits_ + row_misses_ + row_conflicts_);
        std::cout << "Row Hit Rate: " << (row_hit_rate * 100.0) << "%" << std::endl;
    }

    std::cout << "\nEnergy Consumption:" << std::endl;
    std::cout << "  Read Energy: " << read_energy_ << " nJ" << std::endl;
    std::cout << "  Write Energy: " << write_energy_ << " nJ" << std::endl;
    std::cout << "  Activation Energy: " << activation_energy_ << " nJ" << std::endl;
    std::cout << "  Precharge Energy: " << precharge_energy_ << " nJ" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "  Total Energy: " << getTotalEnergy() << " nJ" << std::endl;
    std::cout << "================================\n" << std::endl;
}

void DRAMModel::resetStats() {
    total_reads_ = 0;
    total_writes_ = 0;
    row_hits_ = 0;
    row_misses_ = 0;
    row_conflicts_ = 0;
    read_energy_ = 0.0;
    write_energy_ = 0.0;
    activation_energy_ = 0.0;
    precharge_energy_ = 0.0;
    current_cycle_ = 0;
}

//=============================================================================
// Private Helper Functions
//=============================================================================

Cycle DRAMModel::calculateLatency(const MemoryRequest& req) {
    Cycle latency;

    // Check row buffer status
    if (isRowHit(req.addr)) {
        // Row buffer hit: only CAS latency
        latency = dram_config_.tCL;
        row_hits_++;
    } else {
        // Row buffer miss or conflict
        auto it = row_buffer_state_.find(req.addr & ~0xFFFULL); // Same row?

        if (it != row_buffer_state_.end()) {
            // Row conflict: precharge + activate + CAS
            latency = dram_config_.tRP + dram_config_.tRCD + dram_config_.tCL;
            row_conflicts_++;
            activation_energy_ += 5.0;  // nJ for activation
            precharge_energy_ += 3.0;   // nJ for precharge
        } else {
            // Row miss: activate + CAS
            latency = dram_config_.tRCD + dram_config_.tCL;
            row_misses_++;
            activation_energy_ += 5.0;  // nJ for activation
        }
    }

    // Add queuing delay (simplified model)
    if (pending_requests_.size() > 0) {
        latency += pending_requests_.size();
    }

    return latency;
}

void DRAMModel::updateRowBuffer(Address addr) {
    // Extract row address (simplified: upper bits)
    Address row_addr = addr & ~0xFFFULL; // Assuming 4KB rows

    // Update row buffer state for this bank
    row_buffer_state_[row_addr] = current_cycle_;
}

bool DRAMModel::isRowHit(Address addr) const {
    // Check if this address is in the currently open row
    Address row_addr = addr & ~0xFFFULL;
    auto it = row_buffer_state_.find(row_addr);

    if (it == row_buffer_state_.end()) {
        return false;
    }

    // Check if row is still open (not closed due to timeout)
    Cycle row_open_time = current_cycle_ - it->second;
    const Cycle MAX_ROW_OPEN_TIME = 1000; // Cycles

    return row_open_time < MAX_ROW_OPEN_TIME;
}

} // namespace pimid
