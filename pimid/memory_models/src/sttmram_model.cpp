#include "sttmram_model.h"
#include "nvsim_wrapper.h"
#include "memory/sttmram_architecture.h"
#include "config/config_parser.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace pimid {

//=============================================================================
// STTMRAMModel Implementation
//=============================================================================

STTMRAMModel::STTMRAMModel(const std::string& config_path)
    : MemoryModel(MemoryTechnology::STT_MRAM, config_path)
    , nvsim_wrapper_(nullptr)
    , mram_arch_(nullptr)
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

    // Initialize default STT-MRAM configuration
    mram_config_.capacity = 256ULL * 1024 * 1024;  // 256MB
    mram_config_.banks = 8;
    mram_config_.read_write_ports = 1;
    mram_config_.tech_node_nm = 22;
    mram_config_.read_latency = 5;    // 5 cycles (fast read)
    mram_config_.write_latency = 20;  // 20 cycles (slow MTJ switching)
    mram_config_.endurance = 1e15;    // 10^15 writes (very high)
    mram_config_.is_pim_enabled = true;
}

void STTMRAMModel::initialize() {
    std::cout << "[STTMRAMModel] Initializing STT-MRAM model..." << std::endl;
    loadConfig(config_path_);

    // Calculate derived parameters
    capacity_ = mram_config_.capacity;
    bandwidth_ = capacity_ / 100; // Simplified bandwidth model
    endurance_ = mram_config_.endurance;

    // Initialize STT-MRAM architecture with inner-bank timing (NEW!)
    if (mram_config_.capacity <= 512 * 1024 * 1024) {
        // Small MRAM: use Everspin 256Mb configuration
        mram_arch_ = memory::createSTTMRAM_Everspin_256Mb();
        std::cout << "[STTMRAMModel] Using Everspin 256Mb architecture specs" << std::endl;
    } else {
        // Large MRAM: use 8MB cache configuration
        mram_arch_ = memory::createSTTMRAM_8MB_22nm();
        std::cout << "[STTMRAMModel] Using 8MB 22nm architecture specs" << std::endl;
    }

    std::cout << "[STTMRAMModel] Inner-bank read latency: "
              << mram_arch_->timing.inner_bank.getTotalReadLatency() << " ns" << std::endl;
    std::cout << "[STTMRAMModel] Inner-bank write latency: "
              << mram_arch_->timing.inner_bank.getTotalWriteLatency() << " ns" << std::endl;

    // Use architecture energy values
    read_energy_ = mram_arch_->energy.read_energy_per_byte;
    write_energy_ = mram_arch_->energy.write_energy_per_byte;
    leakage_power_ = mram_arch_->energy.chip_leakage_mw / 1000.0;  // mW to W

    // TODO: Initialize NVSim for runtime calculations
    // nvsim_wrapper_ = std::make_unique<NVSimWrapper>(nvsim_config);

    std::cout << "[STTMRAMModel] Configuration:" << std::endl;
    std::cout << "  Capacity: " << (mram_config_.capacity / (1024.0 * 1024)) << " MB" << std::endl;
    std::cout << "  Banks: " << mram_config_.banks << std::endl;
    std::cout << "  Technology: " << mram_config_.tech_node_nm << " nm" << std::endl;
    std::cout << "  Read Latency: " << mram_config_.read_latency << " cycles" << std::endl;
    std::cout << "  Write Latency: " << mram_config_.write_latency << " cycles" << std::endl;
    std::cout << "  Endurance: " << mram_config_.endurance << " writes" << std::endl;
    std::cout << "  PIM Enabled: " << (mram_config_.is_pim_enabled ? "Yes" : "No") << std::endl;
    std::cout << "  Read Energy: " << read_energy_ << " pJ/byte" << std::endl;
    std::cout << "  Write Energy: " << write_energy_ << " pJ/byte" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "[STTMRAMModel] Initialization complete" << std::endl;
}

void STTMRAMModel::loadConfig(const std::string& config_path) {
    std::cout << "[STTMRAMModel] Loading configuration from: " << config_path << std::endl;

    config::ConfigParser parser;
    std::map<std::string, std::string> config;

    if (!parser.parseFile(config_path, config)) {
        std::cerr << "[STTMRAMModel] Warning: Failed to parse config, using defaults" << std::endl;
        return;
    }

    // Extract STT-MRAM capacity
    if (config.find("stt_mram.capacity_mb") != config.end()) {
        try {
            uint64_t capacity_mb = std::stoull(config["stt_mram.capacity_mb"]);
            mram_config_.capacity = capacity_mb * 1024 * 1024;
        } catch (...) {}
    }

    // Extract organization
    if (config.find("stt_mram.organization.num_arrays") != config.end()) {
        try {
            mram_config_.banks = std::stoi(config["stt_mram.organization.num_arrays"]);
        } catch (...) {}
    }

    // Extract timing
    if (config.find("stt_mram.timing.read_latency_ns") != config.end()) {
        try {
            double read_ns = std::stod(config["stt_mram.timing.read_latency_ns"]);
            mram_config_.read_latency = static_cast<Cycle>(read_ns);
        } catch (...) {}
    }

    if (config.find("stt_mram.timing.write_latency_ns") != config.end()) {
        try {
            double write_ns = std::stod(config["stt_mram.timing.write_latency_ns"]);
            mram_config_.write_latency = static_cast<Cycle>(write_ns);
        } catch (...) {}
    }

    // Extract reliability
    if (config.find("stt_mram.reliability.write_endurance") != config.end()) {
        try {
            mram_config_.endurance = std::stoull(config["stt_mram.reliability.write_endurance"]);
        } catch (...) {}
    }

    // Extract technology
    if (config.find("stt_mram.power.tech_node_nm") != config.end()) {
        try {
            mram_config_.tech_node_nm = std::stoi(config["stt_mram.power.tech_node_nm"]);
        } catch (...) {}
    }

    std::cout << "[STTMRAMModel] Configuration loaded successfully" << std::endl;
}

Cycle STTMRAMModel::access(const MemoryRequest& req) {
    Cycle latency;

    // STT-MRAM has asymmetric read/write latency
    if (req.type == MemoryRequestType::READ) {
        latency = mram_config_.read_latency;
        total_reads_++;
    } else if (req.type == MemoryRequestType::WRITE) {
        latency = mram_config_.write_latency;  // MTJ switching dominates!
        total_writes_++;
        write_cycles_++;

        // Track endurance
        updateEndurance(req.addr);

        if (write_cycles_ > endurance_) {
            std::cerr << "[STTMRAMModel] WARNING: Endurance limit exceeded!" << std::endl;
        }
    } else if (req.type == MemoryRequestType::ATOMIC) {
        latency = mram_config_.read_latency + mram_config_.write_latency;
        total_reads_++;
        total_writes_++;
        write_cycles_++;
    }

    // Add queuing delay
    if (!pending_requests_.empty()) {
        latency += pending_requests_.size();
    }

    pending_requests_.push(req);

    if (completion_callback_) {
        Cycle completion_cycle = current_cycle_ + latency;
        // Callback will be invoked at completion_cycle
    }

    return latency;
}

bool STTMRAMModel::canAccept(const MemoryRequest& req) {
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

void STTMRAMModel::tick() {
    current_cycle_++;

    // Process pending requests
    if (!pending_requests_.empty()) {
        pending_requests_.pop();
    }
}

Cycle STTMRAMModel::getLatency(MemoryRequestType type) const {
    switch (type) {
        case MemoryRequestType::READ:
            return mram_config_.read_latency;
        case MemoryRequestType::WRITE:
            return mram_config_.write_latency;
        case MemoryRequestType::ATOMIC:
            return mram_config_.read_latency + mram_config_.write_latency;
        default:
            return mram_config_.read_latency;
    }
}

double STTMRAMModel::getTotalEnergy() const {
    double dynamic_energy = (total_reads_ * read_energy_) +
                           (total_writes_ * write_energy_);
    double leakage_energy = leakage_power_ * (current_cycle_ / 1e9);
    return dynamic_energy + leakage_energy;
}

void STTMRAMModel::printStats() const {
    std::cout << "\n=== STT-MRAM Model Statistics ===" << std::endl;
    std::cout << "Total Cycles: " << current_cycle_ << std::endl;
    std::cout << "Total Reads: " << total_reads_ << std::endl;
    std::cout << "Total Writes: " << total_writes_ << std::endl;
    std::cout << "Write Cycles (Endurance): " << write_cycles_ << " / " << endurance_ << std::endl;

    if (total_reads_ + total_writes_ > 0) {
        double read_ratio = static_cast<double>(total_reads_) / (total_reads_ + total_writes_);
        double write_ratio = static_cast<double>(total_writes_) / (total_reads_ + total_writes_);
        std::cout << "Read Ratio: " << (read_ratio * 100.0) << "%" << std::endl;
        std::cout << "Write Ratio: " << (write_ratio * 100.0) << "%" << std::endl;

        double endurance_used = (static_cast<double>(write_cycles_) / endurance_) * 100.0;
        std::cout << "Endurance Used: " << endurance_used << "%" << std::endl;
    }

    std::cout << "\nLatency (Inner-Bank Timing):" << std::endl;
    std::cout << "  Subarray Read: " << getSubarrayReadLatency() << " ns" << std::endl;
    std::cout << "  Bank Read: " << getBankReadLatency() << " ns" << std::endl;
    std::cout << "  Chip Read: " << getChipReadLatency() << " ns" << std::endl;
    std::cout << "  Subarray Write: " << getSubarrayWriteLatency() << " ns" << std::endl;
    std::cout << "  Bank Write: " << getBankWriteLatency() << " ns" << std::endl;
    std::cout << "  Chip Write: " << getChipWriteLatency() << " ns" << std::endl;

    std::cout << "\nEnergy Consumption:" << std::endl;
    std::cout << "  Read Energy (per byte): " << read_energy_ << " pJ" << std::endl;
    std::cout << "  Write Energy (per byte): " << write_energy_ << " pJ" << std::endl;
    std::cout << "  Total Read Energy: " << (total_reads_ * read_energy_) << " pJ" << std::endl;
    std::cout << "  Total Write Energy: " << (total_writes_ * write_energy_) << " pJ" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "  Total Energy: " << getTotalEnergy() << " pJ" << std::endl;
    std::cout << "================================\n" << std::endl;
}

void STTMRAMModel::resetStats() {
    total_reads_ = 0;
    total_writes_ = 0;
    write_cycles_ = 0;
    current_cycle_ = 0;
}

void STTMRAMModel::updateEndurance(Address addr) {
    // TODO: Implement per-cell endurance tracking
    // For now, just increment global counter
}

//=============================================================================
// Inner-Bank Timing Queries (NEW!)
//=============================================================================

double STTMRAMModel::getSubarrayReadLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.subarray_read_ns;
}

double STTMRAMModel::getBankReadLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.bank_read_ns;
}

double STTMRAMModel::getChipReadLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.chip_read_ns;
}

double STTMRAMModel::getSubarrayWriteLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.subarray_write_ns;
}

double STTMRAMModel::getBankWriteLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.bank_write_ns;
}

double STTMRAMModel::getChipWriteLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.chip_write_ns;
}

double STTMRAMModel::getInnerBankReadLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.inner_bank.getTotalReadLatency();
}

double STTMRAMModel::getInnerBankWriteLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.inner_bank.getTotalWriteLatency();
}

bool STTMRAMModel::supportsBankPIM() const {
    if (!mram_arch_) return false;
    return mram_arch_->isSuitableForPIM();
}

bool STTMRAMModel::supportsSubarrayPIM() const {
    // STT-MRAM supports subarray-level PIM (fast reads, persistent state)
    return true;
}

} // namespace pimid
