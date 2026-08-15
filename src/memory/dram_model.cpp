#include "memory/dram_model.h"
#include "memory/ramulator_wrapper.h"
#include "memory/architecture_extractor.h"
#include "config/config_parser.h"
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
    : MemoryModel(MemoryTechnology::DDR4, config_path)
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

    // Create Ramulator wrapper instance
    ramulator_instance_ = std::make_unique<RamulatorWrapper>(config_path);
}

DRAMModel::~DRAMModel() {
    // Destructor defined here where RamulatorWrapper is complete
}

void DRAMModel::initialize() {
    std::cout << "[DRAMModel] Initializing DRAM model with Ramulator2..." << std::endl;
    loadConfig(config_path_);

    // Initialize Ramulator wrapper FIRST
    if (ramulator_instance_) {
        ramulator_instance_->initialize();
        capacity_ = ramulator_instance_->getCapacity();
        bandwidth_ = ramulator_instance_->getBandwidth();
        std::cout << "[DRAMModel] Using Ramulator2 for cycle-accurate DRAM simulation" << std::endl;
    }

    // Initialize DRAM architecture with inner-bank timing
    // PRIORITY: Extract from Ramulator if available, otherwise use factory defaults
    if (ramulator_instance_) {
        // Extract architecture directly from Ramulator (PREFERRED!)
        dram_arch_ = memory::extractDRAMArchitecture(*ramulator_instance_,
            "DRAM-Ramulator-Extracted");

        if (dram_arch_) {
            std::cout << "[DRAMModel] Architecture EXTRACTED from Ramulator2" << std::endl;
        } else {
            std::cerr << "[DRAMModel] Ramulator extraction failed, using factory defaults" << std::endl;
        }
    }

    // Fallback to factory defaults if extraction failed
    if (!dram_arch_) {
        // Select factory architecture based on config standard
        if (dram_config_.standard == "DDR5" || dram_config_.standard == "DDR5-4800") {
            dram_arch_ = memory::createDDR5_4800_Verified();
            std::cout << "[DRAMModel] Using factory DDR5-4800 specs (hard-coded)" << std::endl;
        } else if (dram_config_.standard == "HBM2") {
            dram_arch_ = memory::createHBM2_Verified();
            std::cout << "[DRAMModel] Using factory HBM2 specs (hard-coded)" << std::endl;
        } else if (dram_config_.standard == "HBM3") {
            dram_arch_ = memory::createHBM3_Verified();
            std::cout << "[DRAMModel] Using factory HBM3 specs (hard-coded)" << std::endl;
        } else {
            // Default to DDR4-2400
            dram_arch_ = memory::createDDR4_2400_Verified();
            std::cout << "[DRAMModel] Using factory DDR4-2400 specs (hard-coded)" << std::endl;
        }
    }

    // Print architecture details
    if (dram_arch_) {
        std::cout << "[DRAMModel] Inner-bank datapath delay: "
                  << dram_arch_->timing.inner_bank.getTotalInnerBankDelay() << " ns" << std::endl;
        std::cout << "[DRAMModel] Architecture source: "
                  << dram_arch_->timing.inner_bank.source << std::endl;
    }

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
    std::cout << "[DRAMModel] Loading configuration from: " << config_path << std::endl;

    // Parse YAML configuration file
    config::ConfigParser parser;
    std::map<std::string, std::string> config;

    if (!parser.parseFile(config_path, config)) {
        std::cerr << "[DRAMModel] Warning: Failed to parse config file, using defaults" << std::endl;
        return;
    }

    // Extract DRAM standard and speed
    if (config.find("dram.standard") != config.end()) {
        dram_config_.standard = config["dram.standard"];
    }
    if (config.find("dram.speed_grade") != config.end()) {
        // Speed grade is stored but primarily managed by Ramulator
    }

    // Extract organization parameters
    if (config.find("dram.organization.channels") != config.end()) {
        try {
            dram_config_.channels = std::stoi(config["dram.organization.channels"]);
        } catch (...) {
            std::cerr << "[DRAMModel] Warning: Invalid channels value" << std::endl;
        }
    }

    if (config.find("dram.organization.ranks_per_channel") != config.end()) {
        try {
            dram_config_.ranks_per_channel = std::stoi(config["dram.organization.ranks_per_channel"]);
        } catch (...) {
            std::cerr << "[DRAMModel] Warning: Invalid ranks_per_channel value" << std::endl;
        }
    }

    if (config.find("dram.organization.banks_per_chip") != config.end()) {
        try {
            dram_config_.banks_per_rank = std::stoi(config["dram.organization.banks_per_chip"]);
        } catch (...) {
            std::cerr << "[DRAMModel] Warning: Invalid banks_per_chip value" << std::endl;
        }
    }

    // Extract timing parameters (all in memory clock cycles)
    if (config.find("dram.timing.tCL") != config.end()) {
        try {
            dram_config_.tCL = std::stoi(config["dram.timing.tCL"]);
        } catch (...) {
            std::cerr << "[DRAMModel] Warning: Invalid tCL value" << std::endl;
        }
    }

    if (config.find("dram.timing.tRCD") != config.end()) {
        try {
            dram_config_.tRCD = std::stoi(config["dram.timing.tRCD"]);
        } catch (...) {
            std::cerr << "[DRAMModel] Warning: Invalid tRCD value" << std::endl;
        }
    }

    if (config.find("dram.timing.tRP") != config.end()) {
        try {
            dram_config_.tRP = std::stoi(config["dram.timing.tRP"]);
        } catch (...) {
            std::cerr << "[DRAMModel] Warning: Invalid tRP value" << std::endl;
        }
    }

    if (config.find("dram.timing.tRAS") != config.end()) {
        try {
            dram_config_.tRAS = std::stoi(config["dram.timing.tRAS"]);
        } catch (...) {
            std::cerr << "[DRAMModel] Warning: Invalid tRAS value" << std::endl;
        }
    }

    // Calculate capacity based on organization
    // This is a simplified calculation; Ramulator will provide exact values
    if (config.find("dram.organization.rows_per_bank") != config.end() &&
        config.find("dram.organization.columns_per_row") != config.end() &&
        config.find("dram.organization.chips_per_rank") != config.end()) {
        try {
            uint64_t rows = std::stoull(config["dram.organization.rows_per_bank"]);
            uint64_t cols = std::stoull(config["dram.organization.columns_per_row"]);
            uint64_t chips = std::stoull(config["dram.organization.chips_per_rank"]);

            // Capacity = channels * ranks * banks * rows * cols * chip_width (bytes)
            // Assuming 8-bit chip width (x8 device)
            dram_config_.capacity = dram_config_.channels *
                                   dram_config_.ranks_per_channel *
                                   dram_config_.banks_per_rank *
                                   rows * cols * chips;
        } catch (...) {
            std::cerr << "[DRAMModel] Warning: Could not calculate capacity from organization" << std::endl;
        }
    }

    // Extract bandwidth if specified, otherwise calculate from speed grade
    if (config.find("channels.bandwidth_per_channel_gbs") != config.end()) {
        try {
            double bw_per_channel = std::stod(config["channels.bandwidth_per_channel_gbs"]);
            dram_config_.bandwidth = static_cast<uint64_t>(
                bw_per_channel * dram_config_.channels * 1024 * 1024 * 1024
            );
        } catch (...) {
            std::cerr << "[DRAMModel] Warning: Invalid bandwidth value" << std::endl;
        }
    }

    // Row buffer policy
    if (config.find("dram.row_buffer.policy") != config.end()) {
        std::string policy = config["dram.row_buffer.policy"];
        std::cout << "[DRAMModel] Row buffer policy: " << policy << std::endl;
        // Row buffer policy would be passed to Ramulator
    }

    // Ramulator-specific settings
    if (config.find("dram.ramulator.config_file") != config.end()) {
        std::string ramulator_config = config["dram.ramulator.config_file"];
        std::cout << "[DRAMModel] Ramulator config: " << ramulator_config << std::endl;
        // Pass to Ramulator wrapper during initialization
    }

    std::cout << "[DRAMModel] Configuration loaded successfully" << std::endl;
    std::cout << "[DRAMModel] DRAM Standard: " << dram_config_.standard << std::endl;
    std::cout << "[DRAMModel] Channels: " << dram_config_.channels << std::endl;
    std::cout << "[DRAMModel] Ranks per channel: " << dram_config_.ranks_per_channel << std::endl;
    std::cout << "[DRAMModel] Banks per rank: " << dram_config_.banks_per_rank << std::endl;
}

Cycle DRAMModel::access(const MemoryRequest& req) {
    // If Ramulator is available, send request to it
    if (ramulator_instance_) {
        // Send request to Ramulator
        bool accepted = ramulator_instance_->send(req.addr, req.type);

        if (!accepted) {
            // Request queue is full, return high latency penalty
            return 1000;
        }

        // Ramulator will track the request
        // Return estimated latency
        return ramulator_instance_->getAverageLatency();
    }

    // Fallback: Calculate access latency
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

    return latency;
}

bool DRAMModel::canAccept(const MemoryRequest& req) {
    // If Ramulator is available, use its queue status
    if (ramulator_instance_) {
        return ramulator_instance_->canAccept();
    }

    // Fallback: Check if request queue has space
    const size_t MAX_PENDING_REQUESTS = 64;
    return pending_requests_.size() < MAX_PENDING_REQUESTS;
}

void DRAMModel::tick() {
    current_cycle_++;

    // Tick Ramulator if available
    if (ramulator_instance_) {
        ramulator_instance_->tick();

        // Update statistics from Ramulator
        total_reads_ = ramulator_instance_->getTotalReads();
        total_writes_ = ramulator_instance_->getTotalWrites();
        row_hits_ = ramulator_instance_->getRowHits();
        row_misses_ = ramulator_instance_->getRowMisses();
        row_conflicts_ = ramulator_instance_->getRowConflicts();

        // Update energy metrics
        read_energy_ = ramulator_instance_->getReadEnergy();
        write_energy_ = ramulator_instance_->getWriteEnergy();
        leakage_power_ = ramulator_instance_->getLeakagePower();
        activation_energy_ = ramulator_instance_->getActivationEnergy();
        precharge_energy_ = ramulator_instance_->getPrechargeEnergy();
    } else {
        // Fallback: simple model
        if (!pending_requests_.empty()) {
            pending_requests_.pop();
        }
        leakage_power_ = 1.5; // W (typical DDR4 idle power)
    }
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

//=============================================================================
// Inner-Bank Timing Queries (NEW!)
//=============================================================================

double DRAMModel::getSubarrayAccessLatency() const {
    if (!dram_arch_) return 0.0;
    return dram_arch_->timing.subarray_access_ns;
}

double DRAMModel::getBankAccessLatency() const {
    if (!dram_arch_) return 0.0;
    return dram_arch_->timing.bank_access_ns;
}

double DRAMModel::getChipAccessLatency() const {
    if (!dram_arch_) return 0.0;
    return dram_arch_->timing.chip_access_ns;
}

double DRAMModel::getInnerBankDatapathDelay() const {
    if (!dram_arch_) return 0.0;
    return dram_arch_->timing.inner_bank.getTotalInnerBankDelay();
}

bool DRAMModel::supportsBankPIM() const {
    // Bank-level PIM is supported but limited by bank serialization
    // Check if architecture indicates reasonable bank bandwidth
    if (!dram_arch_) return false;
    return dram_arch_->bandwidth_limits.bank_effective_bw_GBs > 0.5;  // > 0.5 GB/s
}

bool DRAMModel::supportsSubarrayPIM() const {
    // Subarray-level PIM is always supported (direct row buffer access)
    return true;
}


/* 1.11.24: the memory plugin contract. DRAM is the DRAM-like case: it has
 * every tier. Values come from RamulatorWrapper, which since 1.11.23 composes
 * them from the JEDEC timing rather than returning a hand-written literal. */
double DRAMModel::getTierLatencyNs(Tier tier, Op op) const {
    if (op != Op::READ && op != Op::WRITE) return -1.0;   // no SET/RESET in DRAM
    switch (tier) {
        case Tier::SUBARRAY: return getSubarrayAccessLatency();
        case Tier::BANK:     return getBankAccessLatency();
        case Tier::CHIP:     return getChipAccessLatency();
        default:             return -1.0;   // bankgroup/rank/channel: see below
    }
}
bool DRAMModel::hasTier(Tier tier) const {
    return tier == Tier::SUBARRAY || tier == Tier::BANK ||
           tier == Tier::BANKGROUP || tier == Tier::CHIP ||
           tier == Tier::RANK || tier == Tier::CHANNEL;
}
std::string DRAMModel::tierLatencySource(Tier tier, Op op) const {
    if (getTierLatencyNs(tier, op) < 0.0) return "";
    switch (tier) {
        case Tier::SUBARRAY: return "Ramulator tRCD+tCAS";
        case Tier::BANK:     return "Ramulator tRP+tRCD+tCAS";
        case Tier::CHIP:     return "Ramulator bank+tBurst";
        default:             return "";
    }
}

} // namespace pimid
