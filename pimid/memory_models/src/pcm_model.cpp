#include "pcm_model.h"
#include "nvsim_wrapper.h"
#include "memory/pcm_architecture.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace pimid {

//=============================================================================
// PCMModel Implementation
//=============================================================================

PCMModel::PCMModel(const std::string& config_path)
    : MemoryModel(MemoryTechnology::PCM, config_path)
    , nvsim_wrapper_(nullptr)
    , pcm_arch_(nullptr)
    , total_reads_(0)
    , total_set_writes_(0)
    , total_reset_writes_(0)
    , write_cycles_(0)
    , read_energy_(0.0)
    , write_energy_(0.0)
    , leakage_power_(0.0)
    , area_mm2_(0.0)
    , current_cycle_(0)
    , capacity_(0)
    , bandwidth_(0)
    , endurance_(0) {

    // Initialize default PCM configuration
    pcm_config_.capacity = 1ULL * 1024 * 1024 * 1024;  // 1GB
    pcm_config_.banks = 16;
    pcm_config_.read_write_ports = 1;
    pcm_config_.tech_node_nm = 90;
    pcm_config_.read_latency = 12;         // 12 cycles (moderate)
    pcm_config_.set_write_latency = 100;   // 100 cycles (VERY SLOW!)
    pcm_config_.reset_write_latency = 40;  // 40 cycles (faster than SET)
    pcm_config_.endurance = 1e8;           // 10^8 writes (limited)
    pcm_config_.is_pim_enabled = true;
}

void PCMModel::initialize() {
    std::cout << "[PCMModel] Initializing PCM model..." << std::endl;
    loadConfig(config_path_);

    // Calculate derived parameters
    capacity_ = pcm_config_.capacity;
    bandwidth_ = capacity_ / 200;  // Lower bandwidth due to slow writes
    endurance_ = pcm_config_.endurance;

    // Initialize PCM architecture with inner-bank timing (NEW!)
    pcm_arch_ = memory::createPCM_16MB_90nm();
    std::cout << "[PCMModel] Using 16MB 90nm architecture specs" << std::endl;

    std::cout << "[PCMModel] Inner-bank read latency: "
              << pcm_arch_->timing.inner_bank.getTotalReadLatency() << " ns" << std::endl;
    std::cout << "[PCMModel] Inner-bank SET write latency: "
              << pcm_arch_->timing.inner_bank.getTotalSetWriteLatency() << " ns" << std::endl;
    std::cout << "[PCMModel] Inner-bank RESET write latency: "
              << pcm_arch_->timing.inner_bank.getTotalResetWriteLatency() << " ns" << std::endl;

    // Use architecture energy values
    read_energy_ = pcm_arch_->energy.read_energy_per_byte;
    write_energy_ = pcm_arch_->energy.write_energy_per_byte;  // Average of SET/RESET
    leakage_power_ = pcm_arch_->energy.chip_leakage_mw / 1000.0;

    std::cout << "[PCMModel] Configuration:" << std::endl;
    std::cout << "  Capacity: " << (pcm_config_.capacity / (1024.0 * 1024 * 1024)) << " GB" << std::endl;
    std::cout << "  Banks: " << pcm_config_.banks << std::endl;
    std::cout << "  Technology: " << pcm_config_.tech_node_nm << " nm" << std::endl;
    std::cout << "  Read Latency: " << pcm_config_.read_latency << " cycles" << std::endl;
    std::cout << "  SET Write Latency: " << pcm_config_.set_write_latency << " cycles (SLOW!)" << std::endl;
    std::cout << "  RESET Write Latency: " << pcm_config_.reset_write_latency << " cycles" << std::endl;
    std::cout << "  Endurance: " << pcm_config_.endurance << " writes (limited)" << std::endl;
    std::cout << "  WARNING: PCM only suitable for read-heavy PIM workloads!" << std::endl;
    std::cout << "  Read Energy: " << read_energy_ << " pJ/byte" << std::endl;
    std::cout << "  Write Energy: " << write_energy_ << " pJ/byte (30x read!)" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "[PCMModel] Initialization complete" << std::endl;
}

void PCMModel::loadConfig(const std::string& config_path) {
    std::cout << "[PCMModel] Loading configuration from: " << config_path << std::endl;
    std::cout << "[PCMModel] Using default 1GB PCM configuration" << std::endl;
}

Cycle PCMModel::access(const MemoryRequest& req) {
    Cycle latency;

    // PCM has VERY asymmetric read/write latency
    if (req.type == MemoryRequestType::READ) {
        latency = pcm_config_.read_latency;
        total_reads_++;
    } else if (req.type == MemoryRequestType::WRITE) {
        // Use SET latency (conservative estimate)
        latency = pcm_config_.set_write_latency;  // VERY SLOW!
        total_set_writes_++;
        write_cycles_++;

        updateEndurance(req.addr);

        if (write_cycles_ > endurance_) {
            std::cerr << "[PCMModel] WARNING: Endurance limit exceeded!" << std::endl;
        }
    } else if (req.type == MemoryRequestType::ATOMIC) {
        latency = pcm_config_.read_latency + pcm_config_.set_write_latency;
        total_reads_++;
        total_set_writes_++;
        write_cycles_++;
    }

    // Add queuing delay
    if (!pending_requests_.empty()) {
        latency += pending_requests_.size() * 2;  // Higher penalty
    }

    pending_requests_.push(req);

    if (completion_callback_) {
        Cycle completion_cycle = current_cycle_ + latency;
    }

    return latency;
}

bool PCMModel::canAccept(const MemoryRequest& req) {
    const size_t MAX_PENDING_REQUESTS = 16;  // Smaller queue due to slow writes
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

void PCMModel::tick() {
    current_cycle_++;

    if (!pending_requests_.empty()) {
        pending_requests_.pop();
    }
}

Cycle PCMModel::getLatency(MemoryRequestType type) const {
    switch (type) {
        case MemoryRequestType::READ:
            return pcm_config_.read_latency;
        case MemoryRequestType::WRITE:
            return pcm_config_.set_write_latency;  // Conservative (SET)
        case MemoryRequestType::ATOMIC:
            return pcm_config_.read_latency + pcm_config_.set_write_latency;
        default:
            return pcm_config_.read_latency;
    }
}

double PCMModel::getTotalEnergy() const {
    double dynamic_energy = (total_reads_ * read_energy_) +
                           ((total_set_writes_ + total_reset_writes_) * write_energy_);
    double leakage_energy = leakage_power_ * (current_cycle_ / 1e9);
    return dynamic_energy + leakage_energy;
}

void PCMModel::printStats() const {
    std::cout << "\n=== PCM Model Statistics ===" << std::endl;
    std::cout << "Total Cycles: " << current_cycle_ << std::endl;
    std::cout << "Total Reads: " << total_reads_ << std::endl;
    std::cout << "Total SET Writes: " << total_set_writes_ << std::endl;
    std::cout << "Total RESET Writes: " << total_reset_writes_ << std::endl;
    std::cout << "Total Writes: " << (total_set_writes_ + total_reset_writes_) << std::endl;
    std::cout << "Write Cycles (Endurance): " << write_cycles_ << " / " << endurance_ << std::endl;

    uint64_t total_ops = total_reads_ + total_set_writes_ + total_reset_writes_;
    if (total_ops > 0) {
        double read_ratio = static_cast<double>(total_reads_) / total_ops;
        double write_ratio = static_cast<double>(total_set_writes_ + total_reset_writes_) / total_ops;
        std::cout << "Read Ratio: " << (read_ratio * 100.0) << "%" << std::endl;
        std::cout << "Write Ratio: " << (write_ratio * 100.0) << "%" << std::endl;

        if (write_ratio > 0.2) {
            std::cout << "WARNING: High write ratio detected! PCM is best for read-heavy workloads." << std::endl;
        }

        double endurance_used = (static_cast<double>(write_cycles_) / endurance_) * 100.0;
        std::cout << "Endurance Used: " << endurance_used << "%" << std::endl;
    }

    std::cout << "\nLatency (Inner-Bank Timing):" << std::endl;
    std::cout << "  Subarray Read: " << getSubarrayReadLatency() << " ns" << std::endl;
    std::cout << "  Bank Read: " << getBankReadLatency() << " ns" << std::endl;
    std::cout << "  Chip Read: " << getChipReadLatency() << " ns" << std::endl;
    std::cout << "  Subarray SET Write: " << getSubarraySetWriteLatency() << " ns (SLOW!)" << std::endl;
    std::cout << "  Bank SET Write: " << getBankSetWriteLatency() << " ns" << std::endl;
    std::cout << "  Chip SET Write: " << getChipSetWriteLatency() << " ns" << std::endl;

    std::cout << "\nEnergy Consumption:" << std::endl;
    std::cout << "  Read Energy (per byte): " << read_energy_ << " pJ" << std::endl;
    std::cout << "  Write Energy (per byte): " << write_energy_ << " pJ (30x read!)" << std::endl;
    std::cout << "  Total Read Energy: " << (total_reads_ * read_energy_) << " pJ" << std::endl;
    std::cout << "  Total Write Energy: "
              << ((total_set_writes_ + total_reset_writes_) * write_energy_) << " pJ" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "  Total Energy: " << getTotalEnergy() << " pJ" << std::endl;
    std::cout << "================================\n" << std::endl;
}

void PCMModel::resetStats() {
    total_reads_ = 0;
    total_set_writes_ = 0;
    total_reset_writes_ = 0;
    write_cycles_ = 0;
    current_cycle_ = 0;
}

void PCMModel::updateEndurance(Address addr) {
    // TODO: Per-cell endurance tracking with wear-leveling
}

//=============================================================================
// Inner-Bank Timing Queries (NEW!)
//=============================================================================

double PCMModel::getSubarrayReadLatency() const {
    if (!pcm_arch_) return 0.0;
    return pcm_arch_->timing.subarray_read_ns;
}

double PCMModel::getBankReadLatency() const {
    if (!pcm_arch_) return 0.0;
    return pcm_arch_->timing.bank_read_ns;
}

double PCMModel::getChipReadLatency() const {
    if (!pcm_arch_) return 0.0;
    return pcm_arch_->timing.chip_read_ns;
}

double PCMModel::getSubarraySetWriteLatency() const {
    if (!pcm_arch_) return 0.0;
    return pcm_arch_->timing.subarray_set_ns;
}

double PCMModel::getBankSetWriteLatency() const {
    if (!pcm_arch_) return 0.0;
    return pcm_arch_->timing.bank_set_ns;
}

double PCMModel::getChipSetWriteLatency() const {
    if (!pcm_arch_) return 0.0;
    return pcm_arch_->timing.chip_set_ns;
}

double PCMModel::getSubarrayResetWriteLatency() const {
    if (!pcm_arch_) return 0.0;
    return pcm_arch_->timing.subarray_reset_ns;
}

double PCMModel::getBankResetWriteLatency() const {
    if (!pcm_arch_) return 0.0;
    return pcm_arch_->timing.bank_reset_ns;
}

double PCMModel::getChipResetWriteLatency() const {
    if (!pcm_arch_) return 0.0;
    return pcm_arch_->timing.chip_reset_ns;
}

double PCMModel::getInnerBankReadLatency() const {
    if (!pcm_arch_) return 0.0;
    return pcm_arch_->timing.inner_bank.getTotalReadLatency();
}

bool PCMModel::supportsBankPIM() const {
    if (!pcm_arch_) return false;
    return pcm_arch_->isSuitableForPIM();
}

bool PCMModel::supportsSubarrayPIM() const {
    // PCM supports subarray PIM, but ONLY for read-heavy workloads
    return true;
}

} // namespace pimid
