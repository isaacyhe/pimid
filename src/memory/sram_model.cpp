#include "memory/sram_model.h"
#include "memory/cacti_wrapper.h"
#include "memory/sram_architecture.h"
#include "memory/architecture_extractor.h"
#include "config/config_parser.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <yaml-cpp/yaml.h>

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
    cacti_config.temperature = sram_config_.temperature_k;   // 1.11.52 (D055)
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

    // Initialize SRAM architecture with inner-bank timing
    // PRIORITY: Extract from CACTI if available, otherwise use factory defaults
#ifdef HAVE_CACTI
    if (cacti_wrapper_ && cacti_wrapper_->isValid()) {
        // Extract architecture directly from CACTI results (PREFERRED!)
        sram_arch_ = memory::extractSRAMArchitecture(*cacti_wrapper_,
            "SRAM-CACTI-Extracted", 3.0);

        if (sram_arch_) {
            std::cout << "[SRAMModel] Architecture EXTRACTED from CACTI 7.0" << std::endl;
        } else {
            std::cerr << "[SRAMModel] CACTI extraction failed, using factory defaults" << std::endl;
        }
    }
#endif

    /* 1.11.24: the hand-written factory defaults are GONE. They were a
     * fallback for "tool extraction failed", and that fallback is exactly
     * what let fabricated numbers reach a result while looking tool-sourced:
     * 678 literal assignments across 19 create*() factories, none of them
     * derivable from anything. A technology whose tool binding fails must
     * REFUSE, not quietly report invented specs.
     *
     * This is the vendorArrayFraction() discipline applied to a whole
     * model: absence is reported, never filled. */
    if (!sram_arch_) {
        throw std::runtime_error(
            "[SRAMModel] CACTI characterization failed and there is no fallback. "
            "The hand-written default specs were removed in 1.11.24 because "
            "they were unsourced and indistinguishable from tool output. "
            "Fix the CACTI configuration rather than pricing this run from "
            "invented numbers.");
    }

    std::cout << "[SRAMModel] Inner-bank datapath latency: "
              << sram_arch_->timing.inner_bank.getTotalInnerBank() << " ns" << std::endl;
    std::cout << "[SRAMModel] Architecture source: "
              << sram_arch_->timing.inner_bank.source << std::endl;

    std::cout << "[SRAMModel] Initialization complete" << std::endl;
}

void SRAMModel::loadConfig(const std::string& config_path) {
    std::cout << "[SRAMModel] Loading configuration from: " << config_path << std::endl;

    try {
        YAML::Node config = YAML::LoadFile(config_path);

        if (!config["sram"]) {
            std::cout << "[SRAMModel] No 'sram' section found, using default configuration" << std::endl;
            return;
        }

        YAML::Node sram = config["sram"];

        // Load capacity
        if (sram["capacity_mb"]) {
            uint64_t capacity_mb = sram["capacity_mb"].as<uint64_t>();
            sram_config_.capacity = capacity_mb * 1024 * 1024;
        }

        // Load organization
        if (sram["organization"]) {
            auto org = sram["organization"];
            if (org["banks"]) {
                sram_config_.banks = org["banks"].as<uint32_t>();
            }
            if (org["read_write_ports"]) {
                sram_config_.read_write_ports = org["read_write_ports"].as<uint32_t>();
            }
            if (org["read_ports"]) {
                sram_config_.read_ports = org["read_ports"].as<uint32_t>();
            }
            if (org["write_ports"]) {
                sram_config_.write_ports = org["write_ports"].as<uint32_t>();
            }
        }

        // Load cache configuration
        if (sram["cache"]) {
            auto cache = sram["cache"];
            if (cache["line_size_bytes"]) {
                sram_config_.line_size = cache["line_size_bytes"].as<uint32_t>();
            }
            if (cache["associativity"]) {
                sram_config_.associativity = cache["associativity"].as<uint32_t>();
            }
        }

        // Load timing
        if (sram["timing"]) {
            auto timing = sram["timing"];
            if (timing["read_latency_cycles"]) {
                sram_config_.access_time = timing["read_latency_cycles"].as<uint32_t>();
            }
        }

        // Load power parameters
        if (sram["power"]) {
            auto power = sram["power"];
            if (power["tech_node_nm"]) {
                sram_config_.tech_node_nm = power["tech_node_nm"].as<uint32_t>();
            }
            // Load energy values (will be overridden by CACTI if available)
            if (power["read_energy_nj"]) {
                read_energy_ = power["read_energy_nj"].as<double>();
            }
            if (power["write_energy_nj"]) {
                write_energy_ = power["write_energy_nj"].as<double>();
            }
            if (power["leakage_power_mw"]) {
                leakage_power_ = power["leakage_power_mw"].as<double>() / 1000.0;  // Convert mW to W
            }
        }

        std::cout << "[SRAMModel] Successfully loaded configuration from YAML" << std::endl;
        std::cout << "[SRAMModel] Capacity: " << (sram_config_.capacity / 1024) << " KB" << std::endl;

    } catch (const YAML::Exception& e) {
        std::cerr << "[SRAMModel] YAML parsing error: " << e.what() << std::endl;
        std::cerr << "[SRAMModel] Using default configuration" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[SRAMModel] Error loading config: " << e.what() << std::endl;
        std::cerr << "[SRAMModel] Using default configuration" << std::endl;
    }
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

    return latency;
}

bool SRAMModel::canAccept(const MemoryRequest& req) {
    // SRAM typically has smaller queues than DRAM
    const size_t MAX_PENDING_REQUESTS = 16;
    return pending_requests_.size() < MAX_PENDING_REQUESTS;
}

void SRAMModel::tick() {
    current_cycle_++;

#ifdef HAVE_CACTI
    // Re-query CACTI periodically to update energy models based on temperature
    // (temperature changes due to activity can affect leakage)
    if (cacti_wrapper_ && cacti_wrapper_->isValid()) {
        // Update leakage power based on temperature profile (every 100K cycles)
        if (current_cycle_ % 100000 == 0) {
            // CACTI provides activity-based power estimation
            leakage_power_ = cacti_wrapper_->getLeakagePower() / 1000.0;  // mW to W
        }
    }
#endif

    // Process pending requests: latency is accounted for at access() time;
    // tick() just frees capacity by popping one request.
    if (!pending_requests_.empty()) {
        pending_requests_.pop();
    }

    // Leakage power is constant (but scaled by CACTI if available)
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


/* 1.11.24: SRAM is NOT DRAM-like -- no bank groups, no ranks, no channels.
 * Reporting those as absent is the point: a tier that does not exist must not
 * be collapsed onto its neighbour or filled from a multiplier. */
double SRAMModel::getTierLatencyNs(Tier tier, Op op) const {
    if (op != Op::READ && op != Op::WRITE) return -1.0;
    switch (tier) {
        case Tier::SUBARRAY: return getSubarrayReadLatency();
        case Tier::BANK:     return getBankReadLatency();
        case Tier::CHIP:     return getChipReadLatency();
        default:             return -1.0;
    }
}
bool SRAMModel::hasTier(Tier tier) const {
    return tier == Tier::SUBARRAY || tier == Tier::BANK || tier == Tier::CHIP;
}
std::string SRAMModel::tierLatencySource(Tier tier, Op op) const {
    if (getTierLatencyNs(tier, op) < 0.0) return "";
    switch (tier) {
        case Tier::SUBARRAY: return "CACTI component delays";
        case Tier::BANK:     return "CACTI getAccessTime";
        case Tier::CHIP:     return "CACTI getAccessTime + configured net hop";
        default:             return "";
    }
}


/* 1.11.25: characterize the array the RUN configures, not the model's
 * compiled-in default. Must be called before initialize(). */
void SRAMModel::setArrayCapacityBytes(uint64_t bytes) {
    if (bytes > 0) sram_config_.capacity = bytes;
}


/* 1.11.25: characterize the access the RUN performs. Must precede
 * initialize(). 0 keeps the model default. */
/* 1.11.51 (L70): the run's node, replacing the compiled-in default.
 * Must precede initialize(). */
/* 1.11.52 (D055): the run's temperature, replacing the hardcoded 350 K in
 * the tool query below. Must precede initialize(). */
void SRAMModel::setTemperatureK(int k) {
    if (k > 0) sram_config_.temperature_k = static_cast<uint32_t>(k);
}

void SRAMModel::setTechNodeNm(int nm) {
    if (nm > 0) sram_config_.tech_node_nm = nm;
}

void SRAMModel::setAccessWidthBits(uint32_t bits) {
    if (bits >= 8 && bits <= 1024) access_width_bits_ = bits;
}

} // namespace pimid
