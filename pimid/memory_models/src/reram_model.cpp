#include "reram_model.h"
#include "nvsim_wrapper.h"
#include "memory/reram_architecture.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace pimid {

//=============================================================================
// ReRAMModel Implementation
//=============================================================================

ReRAMModel::ReRAMModel(const std::string& config_path)
    : MemoryModel(MemoryTechnology::RERAM, config_path)
    , nvsim_wrapper_(nullptr)
    , reram_arch_(nullptr)
    , total_reads_(0)
    , total_writes_(0)
    , total_analog_ops_(0)
    , write_cycles_(0)
    , read_energy_(0.0)
    , write_energy_(0.0)
    , analog_compute_energy_(0.0)
    , leakage_power_(0.0)
    , area_mm2_(0.0)
    , current_cycle_(0)
    , capacity_(0)
    , bandwidth_(0)
    , endurance_(0) {

    // Initialize default ReRAM configuration
    reram_config_.capacity = 256ULL * 1024 * 1024;  // 256MB
    reram_config_.banks = 16;
    reram_config_.read_write_ports = 1;
    reram_config_.tech_node_nm = 32;
    reram_config_.read_latency = 7;      // 7 cycles (moderate)
    reram_config_.write_latency = 15;    // 15 cycles (fast!)
    reram_config_.analog_compute_latency = 3;  // 3 cycles (VERY fast analog!)
    reram_config_.endurance = 1e11;      // 10^11 writes (good)
    reram_config_.analog_capable = true;
    reram_config_.is_pim_enabled = true;
}

void ReRAMModel::initialize() {
    std::cout << "[ReRAMModel] Initializing ReRAM model..." << std::endl;
    loadConfig(config_path_);

    // Calculate derived parameters
    capacity_ = reram_config_.capacity;
    bandwidth_ = capacity_ / 50;  // Good bandwidth
    endurance_ = reram_config_.endurance;

    // Initialize ReRAM architecture with inner-bank timing (NEW!)
    if (reram_config_.analog_capable) {
        // Analog-capable ReRAM
        reram_arch_ = memory::createReRAM_2MB_32nm_Analog();
        std::cout << "[ReRAMModel] Using 2MB 32nm Analog-capable architecture specs" << std::endl;
        std::cout << "[ReRAMModel] ANALOG COMPUTE ENABLED!" << std::endl;
    } else {
        // Digital-only ReRAM
        reram_arch_ = memory::createReRAM_8MB_22nm_Digital();
        std::cout << "[ReRAMModel] Using 8MB 22nm Digital architecture specs" << std::endl;
    }

    std::cout << "[ReRAMModel] Inner-bank read latency: "
              << reram_arch_->timing.inner_bank.getTotalReadLatency() << " ns" << std::endl;
    std::cout << "[ReRAMModel] Inner-bank write latency: "
              << reram_arch_->timing.inner_bank.getTotalWriteLatency() << " ns" << std::endl;

    if (reram_arch_->hasAnalogCompute()) {
        std::cout << "[ReRAMModel] Analog compute latency: "
                  << reram_arch_->timing.inner_bank.getAnalogComputeLatency() << " ns (FAST!)" << std::endl;
    }

    // Use architecture energy values
    read_energy_ = reram_arch_->energy.read_energy_per_byte;
    write_energy_ = reram_arch_->energy.write_energy_per_byte;
    analog_compute_energy_ = reram_arch_->energy.analog_compute_energy_pJ;
    leakage_power_ = reram_arch_->energy.chip_leakage_mw / 1000.0;

    std::cout << "[ReRAMModel] Configuration:" << std::endl;
    std::cout << "  Capacity: " << (reram_config_.capacity / (1024.0 * 1024)) << " MB" << std::endl;
    std::cout << "  Banks: " << reram_config_.banks << std::endl;
    std::cout << "  Technology: " << reram_config_.tech_node_nm << " nm" << std::endl;
    std::cout << "  Read Latency: " << reram_config_.read_latency << " cycles" << std::endl;
    std::cout << "  Write Latency: " << reram_config_.write_latency << " cycles (fast!)" << std::endl;
    std::cout << "  Analog Compute Latency: " << reram_config_.analog_compute_latency
              << " cycles (VERY fast!)" << std::endl;
    std::cout << "  Endurance: " << reram_config_.endurance << " writes" << std::endl;
    std::cout << "  Analog Capable: " << (reram_config_.analog_capable ? "Yes" : "No") << std::endl;
    std::cout << "  Read Energy: " << read_energy_ << " pJ/byte" << std::endl;
    std::cout << "  Write Energy: " << write_energy_ << " pJ/byte" << std::endl;
    std::cout << "  Analog Compute Energy: " << analog_compute_energy_ << " pJ (very low!)" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "[ReRAMModel] Initialization complete" << std::endl;
}

void ReRAMModel::loadConfig(const std::string& config_path) {
    std::cout << "[ReRAMModel] Loading configuration from: " << config_path << std::endl;
    std::cout << "[ReRAMModel] Using default 256MB ReRAM configuration" << std::endl;
}

Cycle ReRAMModel::access(const MemoryRequest& req) {
    Cycle latency;

    // ReRAM has moderate read/write latency (better than PCM!)
    if (req.type == MemoryRequestType::READ) {
        latency = reram_config_.read_latency;
        total_reads_++;
    } else if (req.type == MemoryRequestType::WRITE) {
        latency = reram_config_.write_latency;  // Fast writes!
        total_writes_++;
        write_cycles_++;

        updateEndurance(req.addr);

        if (write_cycles_ > endurance_) {
            std::cerr << "[ReRAMModel] WARNING: Endurance limit exceeded!" << std::endl;
        }
    } else if (req.type == MemoryRequestType::ATOMIC) {
        // Check if this is an analog compute operation
        // TODO: Extend MemoryRequest to support PIM operation types
        if (reram_config_.analog_capable) {
            // Analog compute is VERY fast!
            latency = reram_config_.analog_compute_latency;
            total_analog_ops_++;
        } else {
            latency = reram_config_.read_latency + reram_config_.write_latency;
            total_reads_++;
            total_writes_++;
            write_cycles_++;
        }
    }

    // Add queuing delay
    if (!pending_requests_.empty()) {
        latency += pending_requests_.size();
    }

    pending_requests_.push(req);

    if (completion_callback_) {
        Cycle completion_cycle = current_cycle_ + latency;
    }

    return latency;
}

bool ReRAMModel::canAccept(const MemoryRequest& req) {
    const size_t MAX_PENDING_REQUESTS = 32;
    if (pending_requests_.size() >= MAX_PENDING_REQUESTS) {
        return false;
    }

    // Check endurance for writes
    if ((req.type == MemoryRequestType::WRITE ||
         req.type == MemoryRequestType::ATOMIC) &&
        write_cycles_ >= endurance_) {
        return false;
    }

    return true;
}

void ReRAMModel::tick() {
    current_cycle_++;

    if (!pending_requests_.empty()) {
        pending_requests_.pop();
    }
}

Cycle ReRAMModel::getLatency(MemoryRequestType type) const {
    switch (type) {
        case MemoryRequestType::READ:
            return reram_config_.read_latency;
        case MemoryRequestType::WRITE:
            return reram_config_.write_latency;
        case MemoryRequestType::ATOMIC:
            if (reram_config_.analog_capable) {
                return reram_config_.analog_compute_latency;
            } else {
                return reram_config_.read_latency + reram_config_.write_latency;
            }
        default:
            return reram_config_.read_latency;
    }
}

double ReRAMModel::getTotalEnergy() const {
    double dynamic_energy = (total_reads_ * read_energy_) +
                           (total_writes_ * write_energy_) +
                           (total_analog_ops_ * analog_compute_energy_);
    double leakage_energy = leakage_power_ * (current_cycle_ / 1e9);
    return dynamic_energy + leakage_energy;
}

void ReRAMModel::printStats() const {
    std::cout << "\n=== ReRAM Model Statistics ===" << std::endl;
    std::cout << "Total Cycles: " << current_cycle_ << std::endl;
    std::cout << "Total Reads: " << total_reads_ << std::endl;
    std::cout << "Total Writes: " << total_writes_ << std::endl;
    std::cout << "Total Analog Compute Ops: " << total_analog_ops_ << std::endl;
    std::cout << "Write Cycles (Endurance): " << write_cycles_ << " / " << endurance_ << std::endl;

    uint64_t total_ops = total_reads_ + total_writes_ + total_analog_ops_;
    if (total_ops > 0) {
        double read_ratio = static_cast<double>(total_reads_) / total_ops;
        double write_ratio = static_cast<double>(total_writes_) / total_ops;
        double analog_ratio = static_cast<double>(total_analog_ops_) / total_ops;
        std::cout << "Read Ratio: " << (read_ratio * 100.0) << "%" << std::endl;
        std::cout << "Write Ratio: " << (write_ratio * 100.0) << "%" << std::endl;
        std::cout << "Analog Compute Ratio: " << (analog_ratio * 100.0) << "%" << std::endl;

        double endurance_used = (static_cast<double>(write_cycles_) / endurance_) * 100.0;
        std::cout << "Endurance Used: " << endurance_used << "%" << std::endl;
    }

    std::cout << "\nLatency (Inner-Bank Timing):" << std::endl;
    std::cout << "  Subarray Read: " << getSubarrayReadLatency() << " ns" << std::endl;
    std::cout << "  Bank Read: " << getBankReadLatency() << " ns" << std::endl;
    std::cout << "  Chip Read: " << getChipReadLatency() << " ns" << std::endl;
    std::cout << "  Subarray Write: " << getSubarrayWriteLatency() << " ns (fast!)" << std::endl;
    std::cout << "  Bank Write: " << getBankWriteLatency() << " ns" << std::endl;
    std::cout << "  Chip Write: " << getChipWriteLatency() << " ns" << std::endl;

    if (reram_config_.analog_capable) {
        std::cout << "  Analog Compute: " << getAnalogComputeLatency() << " ns (VERY fast!)" << std::endl;
    }

    std::cout << "\nEnergy Consumption:" << std::endl;
    std::cout << "  Read Energy (per byte): " << read_energy_ << " pJ" << std::endl;
    std::cout << "  Write Energy (per byte): " << write_energy_ << " pJ" << std::endl;
    std::cout << "  Analog Compute Energy (per op): " << analog_compute_energy_ << " pJ (very low!)" << std::endl;
    std::cout << "  Total Read Energy: " << (total_reads_ * read_energy_) << " pJ" << std::endl;
    std::cout << "  Total Write Energy: " << (total_writes_ * write_energy_) << " pJ" << std::endl;
    std::cout << "  Total Analog Compute Energy: " << (total_analog_ops_ * analog_compute_energy_) << " pJ" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "  Total Energy: " << getTotalEnergy() << " pJ" << std::endl;
    std::cout << "================================\n" << std::endl;
}

void ReRAMModel::resetStats() {
    total_reads_ = 0;
    total_writes_ = 0;
    total_analog_ops_ = 0;
    write_cycles_ = 0;
    current_cycle_ = 0;
}

void ReRAMModel::updateEndurance(Address addr) {
    // TODO: Per-cell endurance tracking
}

//=============================================================================
// Inner-Bank Timing Queries (NEW!)
//=============================================================================

double ReRAMModel::getSubarrayReadLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.subarray_read_ns;
}

double ReRAMModel::getBankReadLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.bank_read_ns;
}

double ReRAMModel::getChipReadLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.chip_read_ns;
}

double ReRAMModel::getSubarrayWriteLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.subarray_write_ns;
}

double ReRAMModel::getBankWriteLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.bank_write_ns;
}

double ReRAMModel::getChipWriteLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.chip_write_ns;
}

double ReRAMModel::getInnerBankReadLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.inner_bank.getTotalReadLatency();
}

double ReRAMModel::getInnerBankWriteLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.inner_bank.getTotalWriteLatency();
}

double ReRAMModel::getAnalogComputeLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.inner_bank.getAnalogComputeLatency();
}

double ReRAMModel::getAnalogComputeEnergy() const {
    return analog_compute_energy_;
}

bool ReRAMModel::supportsAnalogCompute() const {
    if (!reram_arch_) return false;
    return reram_arch_->hasAnalogCompute();
}

bool ReRAMModel::supportsBankPIM() const {
    if (!reram_arch_) return false;
    return reram_arch_->isSuitableForPIM();
}

bool ReRAMModel::supportsSubarrayPIM() const {
    // ReRAM supports subarray PIM, especially for analog compute!
    return true;
}

} // namespace pimid
