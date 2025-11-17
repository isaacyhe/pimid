#include "sram_model.h"
#include "cacti_wrapper.h"
#include "memory/sram_architecture.h"
#include "config/config_parser.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace pimid {

//=============================================================================
// SRAMModel Implementation (CACTI Integration)
//=============================================================================

SRAMModel::SRAMModel(const std::string& config_path)
    : MemoryModel(MemoryTechnology::SRAM, config_path)
    , cacti_wrapper_(nullptr)
    , total_reads_(0)
    , total_writes_(0)
    , total_accesses_(0)
    , read_energy_(0.0)
    , write_energy_(0.0)
    , leakage_power_(0.0)
    , area_mm2_(0.0)
    , current_cycle_(0)
    , capacity_(0)
    , bandwidth_(0) {

    // Initialize default SRAM configuration
    sram_config_.capacity = 256 * 1024;  // 256 KB
    sram_config_.line_size = 64;         // 64 bytes
    sram_config_.associativity = 8;      // 8-way set associative
    sram_config_.banks = 8;
    sram_config_.read_write_ports = 1;
    sram_config_.read_ports = 0;
    sram_config_.write_ports = 0;
    sram_config_.tech_node_nm = 22;      // 22nm technology
    sram_config_.access_time = 2;        // 2 cycles
}

void SRAMModel::initialize() {
    std::cout << "[SRAMModel] Initializing SRAM model with CACTI..." << std::endl;
    loadConfig(config_path_);

    // Calculate derived parameters
    capacity_ = sram_config_.capacity;
    bandwidth_ = capacity_ * 2; // Simplified: 2x capacity per second

#ifdef HAVE_CACTI
    try {
        // Create CACTI configuration
        CACTIWrapper::SRAMConfig cacti_config;
        cacti_config.capacity_bytes = sram_config_.capacity;
        cacti_config.line_size = sram_config_.line_size;
        cacti_config.associativity = sram_config_.associativity;
        cacti_config.banks = sram_config_.banks;
        cacti_config.read_write_ports = sram_config_.read_write_ports;
        cacti_config.read_ports = sram_config_.read_ports;
        cacti_config.write_ports = sram_config_.write_ports;
        cacti_config.tech_node_nm = sram_config_.tech_node_nm;
        cacti_config.is_cache = true;

        // Create and initialize CACTI wrapper
        cacti_wrapper_ = std::make_unique<CACTIWrapper>(cacti_config);
        cacti_wrapper_->initialize();

        // Extract CACTI results
        if (cacti_wrapper_->isValid()) {
            read_energy_ = cacti_wrapper_->getDynamicReadEnergy();
            write_energy_ = cacti_wrapper_->getDynamicWriteEnergy();
            leakage_power_ = cacti_wrapper_->getLeakagePower() / 1000.0; // Convert mW to W
            area_mm2_ = cacti_wrapper_->getArea();

            // Update access time from CACTI (assuming 1 GHz clock)
            double freq_hz = 1e9;
            sram_config_.access_time = cacti_wrapper_->getAccessLatencyCycles(freq_hz);

            std::cout << "[SRAMModel] Using CACTI-generated parameters" << std::endl;
        } else {
            std::cerr << "[SRAMModel] CACTI failed: " << cacti_wrapper_->getErrorMessage() << std::endl;
            std::cerr << "[SRAMModel] Falling back to default values" << std::endl;
            useFallbackValues();
        }
    } catch (const std::exception& e) {
        std::cerr << "[SRAMModel] CACTI exception: " << e.what() << std::endl;
        std::cerr << "[SRAMModel] Falling back to default values" << std::endl;
        useFallbackValues();
    }
#else
    std::cout << "[SRAMModel] CACTI not available, using default values" << std::endl;
    useFallbackValues();
#endif

    std::cout << "[SRAMModel] Configuration:" << std::endl;
    std::cout << "  Capacity: " << (sram_config_.capacity / 1024) << " KB" << std::endl;
    std::cout << "  Line Size: " << sram_config_.line_size << " bytes" << std::endl;
    std::cout << "  Associativity: " << sram_config_.associativity << "-way" << std::endl;
    std::cout << "  Banks: " << sram_config_.banks << std::endl;
    std::cout << "  Ports (RW): " << sram_config_.read_write_ports << std::endl;
    std::cout << "  Technology: " << sram_config_.tech_node_nm << " nm" << std::endl;
    std::cout << "  Access Time: " << sram_config_.access_time << " cycles" << std::endl;
    std::cout << "  Area: " << area_mm2_ << " mm^2" << std::endl;
    std::cout << "  Read Energy: " << read_energy_ << " nJ" << std::endl;
    std::cout << "  Write Energy: " << write_energy_ << " nJ" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    // Initialize SRAM architecture with inner-bank timing (NEW!)
    if (sram_config_.capacity <= 512 * 1024) {
        // Small cache: use 8MB L3 22nm configuration
        sram_arch_ = memory::createSRAM_L3_8MB_22nm();
        std::cout << "[SRAMModel] Using 8MB L3 22nm architecture specs" << std::endl;
    } else {
        // Large cache: use 16MB LLC 14nm configuration
        sram_arch_ = memory::createSRAM_LLC_16MB_14nm();
        std::cout << "[SRAMModel] Using 16MB LLC 14nm architecture specs" << std::endl;
    }

    std::cout << "[SRAMModel] Inner-bank datapath latency: "
              << sram_arch_->timing.inner_bank.getTotalInnerBank() << " ns" << std::endl;

    std::cout << "[SRAMModel] Initialization complete" << std::endl;
}

void SRAMModel::loadConfig(const std::string& config_path) {
    std::cout << "[SRAMModel] Loading configuration from: " << config_path << std::endl;

    config::ConfigParser parser;
    std::map<std::string, std::string> config;

    if (!parser.parseFile(config_path, config)) {
        std::cerr << "[SRAMModel] Warning: Failed to parse config, using defaults" << std::endl;
        return;
    }

    // Extract SRAM capacity
    if (config.find("sram.capacity_mb") != config.end()) {
        try {
            uint64_t capacity_mb = std::stoull(config["sram.capacity_mb"]);
            sram_config_.capacity = capacity_mb * 1024 * 1024;
        } catch (...) {}
    }

    // Extract organization
    if (config.find("sram.organization.num_arrays") != config.end()) {
        try {
            sram_config_.banks = std::stoi(config["sram.organization.num_arrays"]);
        } catch (...) {}
    }

    // Extract timing
    if (config.find("sram.timing.access_time_ns") != config.end()) {
        try {
            double access_ns = std::stod(config["sram.timing.access_time_ns"]);
            sram_config_.access_time = static_cast<Cycle>(access_ns * 1.0); // Assume 1GHz
        } catch (...) {}
    }

    // Extract technology
    if (config.find("sram.power.tech_node_nm") != config.end()) {
        try {
            sram_config_.tech_node_nm = std::stoi(config["sram.power.tech_node_nm"]);
        } catch (...) {}
    }

    // Extract CACTI settings
    if (config.find("sram.cacti.line_size_bytes") != config.end()) {
        try {
            sram_config_.line_size = std::stoi(config["sram.cacti.line_size_bytes"]);
        } catch (...) {}
    }

    if (config.find("sram.cacti.associativity") != config.end()) {
        try {
            sram_config_.associativity = std::stoi(config["sram.cacti.associativity"]);
        } catch (...) {}
    }

    std::cout << "[SRAMModel] Configuration loaded successfully" << std::endl;
}

Cycle SRAMModel::access(const MemoryRequest& req) {
    // SRAM has fixed access time
    Cycle latency = sram_config_.access_time;

    // Update statistics and energy
    total_accesses_++;
    if (req.type == MemoryRequestType::READ) {
        total_reads_++;
        // Energy already accumulated per access
    } else if (req.type == MemoryRequestType::WRITE) {
        total_writes_++;
        // Energy already accumulated per access
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

bool SRAMModel::canAccept(const MemoryRequest& req) {
    // SRAM typically has smaller queues than DRAM
    const size_t MAX_PENDING_REQUESTS = 16;
    return pending_requests_.size() < MAX_PENDING_REQUESTS;
}

void SRAMModel::tick() {
    current_cycle_++;

    // TODO: When CACTI is integrated, update dynamic energy
    // For now, accumulate energy based on current activity

    // Process pending requests
    if (!pending_requests_.empty()) {
        auto req = pending_requests_.front();

        // Check if request has completed (simplified model)
        // In real implementation, track completion time per request
        pending_requests_.pop();
    }

    // Leakage power is constant
}

Cycle SRAMModel::getLatency(MemoryRequestType type) const {
    // SRAM has fixed latency regardless of request type
    return sram_config_.access_time;
}

double SRAMModel::getTotalEnergy() const {
    // Total energy = dynamic energy + leakage energy
    double dynamic_energy = (total_reads_ * read_energy_) +
                           (total_writes_ * write_energy_);

    // Leakage energy = leakage_power * time
    double leakage_energy = leakage_power_ * (current_cycle_ / 1e9); // Assuming 1 GHz

    return dynamic_energy + leakage_energy;
}

void SRAMModel::printStats() const {
    std::cout << "\n=== SRAM Model Statistics ===" << std::endl;
    std::cout << "Total Cycles: " << current_cycle_ << std::endl;
    std::cout << "Total Accesses: " << total_accesses_ << std::endl;
    std::cout << "Total Reads: " << total_reads_ << std::endl;
    std::cout << "Total Writes: " << total_writes_ << std::endl;

    if (total_accesses_ > 0) {
        double read_ratio = static_cast<double>(total_reads_) / total_accesses_;
        double write_ratio = static_cast<double>(total_writes_) / total_accesses_;
        std::cout << "Read Ratio: " << (read_ratio * 100.0) << "%" << std::endl;
        std::cout << "Write Ratio: " << (write_ratio * 100.0) << "%" << std::endl;
    }

    std::cout << "\nPhysical Characteristics:" << std::endl;
    std::cout << "  Area: " << area_mm2_ << " mm^2" << std::endl;
    std::cout << "  Technology: " << sram_config_.tech_node_nm << " nm" << std::endl;

    std::cout << "\nEnergy Consumption:" << std::endl;
    std::cout << "  Read Energy (per access): " << read_energy_ << " nJ" << std::endl;
    std::cout << "  Write Energy (per access): " << write_energy_ << " nJ" << std::endl;
    std::cout << "  Total Read Energy: " << (total_reads_ * read_energy_) << " nJ" << std::endl;
    std::cout << "  Total Write Energy: " << (total_writes_ * write_energy_) << " nJ" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "  Total Energy: " << getTotalEnergy() << " nJ" << std::endl;
    std::cout << "================================\n" << std::endl;
}

void SRAMModel::resetStats() {
    total_reads_ = 0;
    total_writes_ = 0;
    total_accesses_ = 0;
    current_cycle_ = 0;
}

void SRAMModel::useFallbackValues() {
    // Default energy values (typical for 22nm SRAM)
    read_energy_ = 0.5;    // nJ per access
    write_energy_ = 0.8;   // nJ per access
    leakage_power_ = 0.05; // W
    area_mm2_ = 2.5;       // mm^2
}

//=============================================================================
// Inner-Bank Timing Queries (NEW!)
//=============================================================================

double SRAMModel::getSubarrayReadLatency() const {
    if (!sram_arch_) return 0.0;
    return sram_arch_->timing.subarray_access_ns;
}

double SRAMModel::getBankReadLatency() const {
    if (!sram_arch_) return 0.0;
    return sram_arch_->timing.bank_access_ns;
}

double SRAMModel::getChipReadLatency() const {
    if (!sram_arch_) return 0.0;
    return sram_arch_->timing.chip_access_ns;
}

double SRAMModel::getInnerBankDatapathLatency() const {
    if (!sram_arch_) return 0.0;
    return sram_arch_->timing.inner_bank.getTotalInnerBank();
}

bool SRAMModel::supportsBankPIM() const {
    // SRAM supports bank-level PIM
    return true;
}

bool SRAMModel::supportsSubarrayPIM() const {
    // SRAM supports subarray-level PIM (fast local operations)
    return true;
}

} // namespace pimid
