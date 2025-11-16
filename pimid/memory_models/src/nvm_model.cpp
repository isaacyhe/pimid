#include "nvm_model.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace pimid {

//=============================================================================
// NVMModel Implementation (NVSim Integration)
//=============================================================================

NVMModel::NVMModel(const std::string& config_path)
    : MemoryModel(MemoryTechnology::STT_MRAM, config_path)
    , nvsim_instance_(nullptr)
    , total_reads_(0)
    , total_writes_(0)
    , write_cycles_(0)
    , read_energy_(0.0)
    , write_energy_(0.0)
    , leakage_power_(0.0)
    , area_mm2_(0.0)
    , current_cycle_(0)
    , capacity_(0)
    , bandwidth_(0)
    , endurance_(0) {

    // Initialize default NVM configuration (STT-MRAM)
    nvm_config_.cell_type = "STT-MRAM";
    nvm_config_.capacity = 1ULL * 1024 * 1024 * 1024;  // 1GB
    nvm_config_.banks = 8;
    nvm_config_.read_write_ports = 1;
    nvm_config_.tech_node_nm = 22;      // 22nm technology
    nvm_config_.read_latency = 10;      // 10 cycles (faster than DRAM)
    nvm_config_.write_latency = 50;     // 50 cycles (slower write)
    nvm_config_.endurance = 1e15;       // 10^15 writes (high for STT-MRAM)
    nvm_config_.is_pim_enabled = true;  // Support in-memory compute
}

void NVMModel::initialize() {
    std::cout << "[NVMModel] Initializing NVM model..." << std::endl;
    loadConfig(config_path_);

    // Calculate derived parameters
    capacity_ = nvm_config_.capacity;
    bandwidth_ = capacity_ / 100; // Simplified bandwidth model
    endurance_ = nvm_config_.endurance;

    // TODO: Initialize NVSim instance when integrated
    // nvsim_instance_ = new NVSimWrapper(nvm_config_);
    // After NVSim runs, it will populate:
    // - read_energy_
    // - write_energy_
    // - leakage_power_
    // - area_mm2_

    // Default energy values based on cell type
    if (nvm_config_.cell_type == "STT-MRAM") {
        read_energy_ = 0.3;      // nJ per read (lower than DRAM)
        write_energy_ = 5.0;     // nJ per write (higher due to spin torque)
        leakage_power_ = 0.01;   // W (very low leakage)
        area_mm2_ = 15.0;        // mm^2
    } else if (nvm_config_.cell_type == "PCM") {
        read_energy_ = 1.0;      // nJ per read
        write_energy_ = 20.0;    // nJ per write (high write energy)
        leakage_power_ = 0.02;   // W
        area_mm2_ = 12.0;        // mm^2
    } else if (nvm_config_.cell_type == "ReRAM") {
        read_energy_ = 0.5;      // nJ per read
        write_energy_ = 8.0;     // nJ per write
        leakage_power_ = 0.015;  // W
        area_mm2_ = 10.0;        // mm^2
    }

    std::cout << "[NVMModel] Configuration:" << std::endl;
    std::cout << "  Cell Type: " << nvm_config_.cell_type << std::endl;
    std::cout << "  Capacity: " << (nvm_config_.capacity / (1024.0 * 1024 * 1024)) << " GB" << std::endl;
    std::cout << "  Banks: " << nvm_config_.banks << std::endl;
    std::cout << "  Ports (RW): " << nvm_config_.read_write_ports << std::endl;
    std::cout << "  Technology: " << nvm_config_.tech_node_nm << " nm" << std::endl;
    std::cout << "  Read Latency: " << nvm_config_.read_latency << " cycles" << std::endl;
    std::cout << "  Write Latency: " << nvm_config_.write_latency << " cycles" << std::endl;
    std::cout << "  Endurance: " << nvm_config_.endurance << " writes" << std::endl;
    std::cout << "  PIM Enabled: " << (nvm_config_.is_pim_enabled ? "Yes" : "No") << std::endl;
    std::cout << "  Area: " << area_mm2_ << " mm^2" << std::endl;
    std::cout << "  Read Energy: " << read_energy_ << " nJ" << std::endl;
    std::cout << "  Write Energy: " << write_energy_ << " nJ" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "[NVMModel] Initialization complete" << std::endl;
}

void NVMModel::loadConfig(const std::string& config_path) {
    // TODO: Parse YAML configuration file
    // For now, use default configuration
    // When YAML parsing is available:
    // - Parse memory_config.yaml
    // - Extract NVM-specific parameters
    // - Update nvm_config_ structure

    std::cout << "[NVMModel] Loading configuration from: " << config_path << std::endl;
    std::cout << "[NVMModel] Using default " << nvm_config_.cell_type << " configuration" << std::endl;
}

Cycle NVMModel::access(const MemoryRequest& req) {
    Cycle latency;

    // NVM has asymmetric read/write latency
    if (req.type == MemoryRequestType::READ) {
        latency = nvm_config_.read_latency;
        total_reads_++;
    } else if (req.type == MemoryRequestType::WRITE) {
        latency = nvm_config_.write_latency;
        total_writes_++;
        write_cycles_++;

        // Track endurance
        updateEndurance(req.addr);

        // Check if endurance limit reached
        if (write_cycles_ > endurance_) {
            std::cerr << "[NVMModel] WARNING: Endurance limit exceeded! ("
                     << write_cycles_ << " > " << endurance_ << ")" << std::endl;
        }
    } else if (req.type == MemoryRequestType::ATOMIC) {
        // Atomic operations: read + modify + write
        latency = nvm_config_.read_latency + nvm_config_.write_latency;
        total_reads_++;
        total_writes_++;
        write_cycles_++;
    }

    // Add queuing delay
    if (!pending_requests_.empty()) {
        latency += pending_requests_.size();
    }

    // Add to pending requests
    pending_requests_.push(req);

    // Schedule completion callback
    if (completion_callback_) {
        Cycle completion_cycle = current_cycle_ + latency;
        // Callback will be invoked at completion_cycle
    }

    return latency;
}

bool NVMModel::canAccept(const MemoryRequest& req) {
    // Check request queue capacity
    const size_t MAX_PENDING_REQUESTS = 32;
    if (pending_requests_.size() >= MAX_PENDING_REQUESTS) {
        return false;
    }

    // Check endurance limit for writes
    if (req.type == MemoryRequestType::WRITE ||
        req.type == MemoryRequestType::ATOMIC) {
        if (write_cycles_ >= endurance_) {
            std::cerr << "[NVMModel] Rejecting write: endurance limit reached" << std::endl;
            return false;
        }
    }

    return true;
}

void NVMModel::tick() {
    current_cycle_++;

    // TODO: When NVSim is integrated, update energy models

    // Process pending requests
    if (!pending_requests_.empty()) {
        auto req = pending_requests_.front();

        // Check if request has completed (simplified model)
        // In real implementation, track completion time per request
        pending_requests_.pop();
    }

    // Leakage power is very low for NVM (key advantage)
}

Cycle NVMModel::getLatency(MemoryRequestType type) const {
    switch (type) {
        case MemoryRequestType::READ:
            return nvm_config_.read_latency;
        case MemoryRequestType::WRITE:
            return nvm_config_.write_latency;
        case MemoryRequestType::ATOMIC:
            return nvm_config_.read_latency + nvm_config_.write_latency;
        default:
            return nvm_config_.read_latency;
    }
}

double NVMModel::getTotalEnergy() const {
    // Total energy = dynamic energy + leakage energy
    double dynamic_energy = (total_reads_ * read_energy_) +
                           (total_writes_ * write_energy_);

    // Leakage energy = leakage_power * time
    double leakage_energy = leakage_power_ * (current_cycle_ / 1e9); // Assuming 1 GHz

    return dynamic_energy + leakage_energy;
}

void NVMModel::printStats() const {
    std::cout << "\n=== NVM Model Statistics ===" << std::endl;
    std::cout << "Cell Type: " << nvm_config_.cell_type << std::endl;
    std::cout << "Total Cycles: " << current_cycle_ << std::endl;
    std::cout << "Total Reads: " << total_reads_ << std::endl;
    std::cout << "Total Writes: " << total_writes_ << std::endl;
    std::cout << "Write Cycles (Endurance): " << write_cycles_ << " / " << endurance_ << std::endl;

    if (total_reads_ + total_writes_ > 0) {
        double read_ratio = static_cast<double>(total_reads_) /
                           (total_reads_ + total_writes_);
        double write_ratio = static_cast<double>(total_writes_) /
                            (total_reads_ + total_writes_);
        std::cout << "Read Ratio: " << (read_ratio * 100.0) << "%" << std::endl;
        std::cout << "Write Ratio: " << (write_ratio * 100.0) << "%" << std::endl;

        double endurance_used = (static_cast<double>(write_cycles_) / endurance_) * 100.0;
        std::cout << "Endurance Used: " << endurance_used << "%" << std::endl;
    }

    std::cout << "\nPhysical Characteristics:" << std::endl;
    std::cout << "  Area: " << area_mm2_ << " mm^2" << std::endl;
    std::cout << "  Technology: " << nvm_config_.tech_node_nm << " nm" << std::endl;
    std::cout << "  PIM Capable: " << (nvm_config_.is_pim_enabled ? "Yes" : "No") << std::endl;

    std::cout << "\nLatency:" << std::endl;
    std::cout << "  Read Latency: " << nvm_config_.read_latency << " cycles" << std::endl;
    std::cout << "  Write Latency: " << nvm_config_.write_latency << " cycles" << std::endl;
    std::cout << "  Write/Read Ratio: " <<
        (static_cast<double>(nvm_config_.write_latency) / nvm_config_.read_latency) << "x" << std::endl;

    std::cout << "\nEnergy Consumption:" << std::endl;
    std::cout << "  Read Energy (per access): " << read_energy_ << " nJ" << std::endl;
    std::cout << "  Write Energy (per access): " << write_energy_ << " nJ" << std::endl;
    std::cout << "  Total Read Energy: " << (total_reads_ * read_energy_) << " nJ" << std::endl;
    std::cout << "  Total Write Energy: " << (total_writes_ * write_energy_) << " nJ" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "  Total Energy: " << getTotalEnergy() << " nJ" << std::endl;

    // Energy comparison
    if (total_reads_ + total_writes_ > 0) {
        double avg_energy = getTotalEnergy() / (total_reads_ + total_writes_);
        std::cout << "  Average Energy per Access: " << avg_energy << " nJ" << std::endl;
    }

    std::cout << "================================\n" << std::endl;
}

void NVMModel::resetStats() {
    total_reads_ = 0;
    total_writes_ = 0;
    write_cycles_ = 0;
    current_cycle_ = 0;
}

//=============================================================================
// Private Helper Functions
//=============================================================================

void NVMModel::updateEndurance(Address addr) {
    // Track write endurance per address/cell
    // In a real implementation, maintain per-cell write counters
    // For now, we just increment global counter

    // TODO: Implement per-bank or per-page endurance tracking
    // std::map<Address, uint64_t> write_count_;
    // Implement wear-leveling if needed
}

} // namespace pimid
