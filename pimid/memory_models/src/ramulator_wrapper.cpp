#include "ramulator_wrapper.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <yaml-cpp/yaml.h>

// Include Ramulator headers
#include "base/base.h"
#include "base/request.h"
#include "memory_system/memory_system.h"
#include "base/factory.h"

namespace pimid {

RamulatorWrapper::RamulatorWrapper(const std::string& config_path)
    : config_path_(config_path),
      capacity_(0),
      bandwidth_(0),
      channels_(1),
      ranks_per_channel_(1),
      banks_per_rank_(8),
      total_reads_(0),
      total_writes_(0),
      row_hits_(0),
      row_misses_(0),
      row_conflicts_(0),
      cached_read_energy_(0.0),
      cached_write_energy_(0.0),
      cached_leakage_power_(0.0),
      last_energy_update_(0),
      current_cycle_(0),
      pim_enabled_(false),
      dram_type_("DDR4"),
      dram_arch_(nullptr),
      bandwidth_tracker_(nullptr),
      internal_network_(nullptr),
      pim_plugin_(nullptr) {
}

RamulatorWrapper::~RamulatorWrapper() {
    if (ramulator_memory_system_) {
        ramulator_memory_system_->finalize();
    }
}

void RamulatorWrapper::initialize() {
    parseConfiguration();
    createRamulatorInstance();
    current_cycle_ = 0;
    resetStats();
}

void RamulatorWrapper::loadConfig(const std::string& config_path) {
    config_path_ = config_path;
    parseConfiguration();
}

void RamulatorWrapper::parseConfiguration() {
    // Try to load PIMID configuration and convert to Ramulator format
    if (config_path_.empty()) {
        // Create default DDR4 configuration
        config_yaml_ = R"(
MemorySystem:
  Frontend: GEM5
  DRAM: DDR4

DDR4:
  standard: DDR4
  organization:
    channel: 1
    rank: 1
    bank: 8
    row: 65536
    column: 1024
  timing:
    preset: DDR4_2400R
  power:
    VDD: 1.2
    IDD0: 48
    IDD2N: 34
    IDD3N: 38
    IDD4W: 125
    IDD4R: 135
    IDD5: 150
)";
        channels_ = 1;
        ranks_per_channel_ = 1;
        banks_per_rank_ = 8;
        capacity_ = 8ULL * 1024 * 1024 * 1024;  // 8 GB default
        bandwidth_ = 19200;  // 19.2 GB/s for DDR4-2400
    } else {
        // Load configuration from file
        std::ifstream config_file(config_path_);
        if (config_file.is_open()) {
            std::stringstream buffer;
            buffer << config_file.rdbuf();
            config_yaml_ = buffer.str();

            try {
                YAML::Node config = YAML::Load(config_yaml_);

                // Parse basic parameters
                if (config["dram"]) {
                    auto dram = config["dram"];
                    if (dram["channels"]) channels_ = dram["channels"].as<uint32_t>();
                    if (dram["ranks"]) ranks_per_channel_ = dram["ranks"].as<uint32_t>();
                    if (dram["banks"]) banks_per_rank_ = dram["banks"].as<uint32_t>();
                    if (dram["capacity"]) capacity_ = dram["capacity"].as<uint64_t>();
                    if (dram["bandwidth"]) bandwidth_ = dram["bandwidth"].as<uint64_t>();
                }
            } catch (const YAML::Exception& e) {
                std::cerr << "Warning: Failed to parse YAML config: " << e.what() << std::endl;
                std::cerr << "Using default DDR4 configuration" << std::endl;
            }
        }
    }
}

void RamulatorWrapper::createRamulatorInstance() {
    try {
        // Parse YAML configuration for Ramulator
        YAML::Node config = YAML::Load(config_yaml_);

        // Create Ramulator memory system using factory
        auto memory_system_raw = Ramulator::Factory::create_memory_system(config);
        ramulator_memory_system_ = std::shared_ptr<Ramulator::IMemorySystem>(memory_system_raw);

        if (!ramulator_memory_system_) {
            std::cerr << "Failed to create Ramulator memory system" << std::endl;
            std::cerr << "Falling back to simplified timing model" << std::endl;
            return;
        }

        std::cout << "Ramulator2 DRAM simulator initialized successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception creating Ramulator instance: " << e.what() << std::endl;
        std::cerr << "Falling back to simplified timing model" << std::endl;
        ramulator_memory_system_.reset();
    }
}

bool RamulatorWrapper::send(Address addr, MemoryRequestType type,
                             std::function<void(Address)> callback) {
    // Track statistics
    if (type == MemoryRequestType::READ) {
        total_reads_++;
    } else if (type == MemoryRequestType::WRITE) {
        total_writes_++;
    }

    // If Ramulator is available, use it
    if (ramulator_memory_system_) {
        Ramulator::Request req = createRamulatorRequest(addr, type);

        // Set up callback to track completion
        req.callback = [this, addr, callback](Ramulator::Request& completed_req) {
            handleRequestCompletion(completed_req);
            if (callback) {
                callback(addr);
            }
        };

        bool accepted = ramulator_memory_system_->send(req);

        if (accepted && callback) {
            // Track pending request
            pending_requests_.push_back({addr, type, current_cycle_, callback});
        }

        return accepted;
    } else {
        // Fallback: accept all requests immediately
        if (callback) {
            callback(addr);
        }
        return true;
    }
}

bool RamulatorWrapper::canAccept() const {
    if (ramulator_memory_system_) {
        // Ramulator checks internally if it can accept more requests
        // We assume it can if queue is not full (checked in send())
        return true;
    }
    return true;
}

void RamulatorWrapper::tick() {
    current_cycle_++;

    if (ramulator_memory_system_) {
        ramulator_memory_system_->tick();
    }

    // Tick PIM components
    if (pim_enabled_ && pim_plugin_) {
        pim_plugin_->tick();
    }
}

Ramulator::Request RamulatorWrapper::createRamulatorRequest(
    Address addr, MemoryRequestType type) {

    int req_type = (type == MemoryRequestType::READ) ?
                   Ramulator::Request::Type::Read :
                   Ramulator::Request::Type::Write;

    return Ramulator::Request(static_cast<Ramulator::Addr_t>(addr), req_type);
}

void RamulatorWrapper::handleRequestCompletion(Ramulator::Request& req) {
    // Update statistics based on completed request
    // Row buffer statistics would come from Ramulator's internal stats
    // This is a placeholder for now
}

double RamulatorWrapper::getReadEnergy() const {
    updateEnergyMetrics();
    return cached_read_energy_;
}

double RamulatorWrapper::getWriteEnergy() const {
    updateEnergyMetrics();
    return cached_write_energy_;
}

double RamulatorWrapper::getActivationEnergy() const {
    // Energy for row activations (ACT command)
    // Based on DRAM power model from literature and DRAM architecture
    //
    // DDR4 typical values from NVIDIA-HPCA17, DAS-MICRO15:
    //   - Row activation: ~2-3 nJ per activation
    //   - Includes: wordline driver, bitline precharge, sense amp settling
    //
    // HBM2: Lower due to shorter bitlines and TSV architecture (~1-1.5 nJ)

    double activation_energy_per_op_nJ = 2.5;  // Default DDR4 value

    // Use actual energy from DRAM architecture if available
    if (dram_arch_) {
        // Bank energy includes activation + column access
        // Estimate activation is ~60% of total bank energy
        activation_energy_per_op_nJ = dram_arch_->energy.bank_energy_pJ * 0.6 / 1000.0;
    }

    // Estimate number of row activations
    // In worst case, every access causes activation (0% row hit rate)
    // In best case, only row misses cause activation
    uint64_t estimated_activations = row_misses_ + row_conflicts_;
    if (estimated_activations == 0) {
        // If no statistics, assume activations proportional to total accesses
        // with typical row buffer locality (assume 50% hit rate)
        estimated_activations = (total_reads_ + total_writes_) / 2;
    }

    return estimated_activations * activation_energy_per_op_nJ;
}

double RamulatorWrapper::getPrechargeEnergy() const {
    // Energy for row precharges (PRE command)
    // Based on DRAM power model from literature
    //
    // DDR4 typical values:
    //   - Precharge: ~1.5-2 nJ per precharge
    //   - Includes: bitline discharge, sense amp reset
    //
    // HBM2: Lower due to TSV architecture (~0.8-1 nJ)

    double precharge_energy_per_op_nJ = 1.8;  // Default DDR4 value

    // Use actual energy from DRAM architecture if available
    if (dram_arch_) {
        // Estimate precharge is ~40% of total bank energy
        precharge_energy_per_op_nJ = dram_arch_->energy.bank_energy_pJ * 0.4 / 1000.0;
    }

    // Estimate number of precharges (roughly equal to activations)
    uint64_t estimated_precharges = row_misses_ + row_conflicts_;
    if (estimated_precharges == 0) {
        // If no statistics, assume precharges proportional to total accesses
        estimated_precharges = (total_reads_ + total_writes_) / 2;
    }

    return estimated_precharges * precharge_energy_per_op_nJ;
}

double RamulatorWrapper::getRefreshEnergy() const {
    // Energy for DRAM refresh operations
    // Based on JEDEC specs and literature
    //
    // DDR4 refresh parameters:
    //   - tREFI = 7.8µs (average refresh interval)
    //   - Each refresh activates one row per bank
    //   - Energy per refresh ≈ activation energy
    //
    // Refresh energy = (num_refreshes) × (energy_per_refresh) × (num_banks)

    double refresh_energy_per_row_nJ = 2.0;  // Default: similar to activation

    // Use DRAM architecture energy if available
    if (dram_arch_) {
        // Refresh energy is similar to activation (same operation, different trigger)
        refresh_energy_per_row_nJ = dram_arch_->energy.bank_energy_pJ * 0.6 / 1000.0;
    }

    // Calculate number of refresh cycles
    // tREFI for DDR4 = 7.8µs = 7800ns
    // At 1ns cycle time (typical for modeling), tREFI = 7800 cycles
    // At actual DRAM clock (1.2GHz = 0.833ns), tREFI = 9360 cycles

    double clock_period_ns = 1.0;  // Default 1ns modeling cycle
    if (dram_arch_) {
        clock_period_ns = 1000.0 / dram_arch_->timing.clock_freq_mhz;
    }

    double tREFI_cycles = 7800.0 / clock_period_ns;  // 7.8µs refresh interval
    uint64_t refresh_count = current_cycle_ / static_cast<uint64_t>(tREFI_cycles);

    // Total refresh energy = refreshes × banks × energy_per_refresh
    uint32_t total_banks = banks_per_rank_ * ranks_per_channel_ * channels_;
    return refresh_count * total_banks * refresh_energy_per_row_nJ;
}

double RamulatorWrapper::getLeakagePower() const {
    updateEnergyMetrics();
    return cached_leakage_power_;
}

double RamulatorWrapper::getTotalEnergy() const {
    return getReadEnergy() + getWriteEnergy() +
           getActivationEnergy() + getPrechargeEnergy() +
           getRefreshEnergy() + (getLeakagePower() * current_cycle_ / 1000000.0);
}

Cycle RamulatorWrapper::getAverageLatency() const {
    if (total_reads_ + total_writes_ == 0) {
        return 0;
    }

    // Calculate average DRAM latency based on row buffer hit/miss behavior
    // This is a realistic calculation based on DRAM timing parameters
    //
    // Row Hit: tCAS (Column Access Strobe) - ~13-15ns for DDR4-2400
    // Row Miss (Conflict): tRP + tRCD + tCAS - ~40ns for DDR4-2400
    //
    // Formula: avg_latency = hit_rate * hit_latency + miss_rate * miss_latency

    // Get DRAM timing parameters from architecture if available
    // For DDR4-2400: tCAS=13.32ns, tRCD=13.32ns, tRP=13.32ns
    // For HBM2: tCAS=12.5ns, tRCD=12.5ns, tRP=12.5ns
    double tCAS_cycles = 16;   // ~13.32ns @ 1.2GHz for DDR4-2400
    double tRCD_cycles = 16;   // ~13.32ns @ 1.2GHz
    double tRP_cycles = 16;    // ~13.32ns @ 1.2GHz

    // If DRAM architecture is available, use actual timing
    if (dram_arch_) {
        double clock_period_ns = 1000.0 / dram_arch_->timing.clock_freq_mhz;
        tCAS_cycles = std::ceil(dram_arch_->timing.tCAS_ns / clock_period_ns);
        tRCD_cycles = std::ceil(dram_arch_->timing.tRCD_ns / clock_period_ns);
        tRP_cycles = std::ceil(dram_arch_->timing.tRP_ns / clock_period_ns);
    }

    // Calculate row buffer hit rate
    uint64_t total_accesses = total_reads_ + total_writes_;
    double hit_rate = 0.0;

    if (row_hits_ > 0 || row_misses_ > 0 || row_conflicts_ > 0) {
        // Use actual statistics from Ramulator
        hit_rate = static_cast<double>(row_hits_) / total_accesses;
    } else {
        // Conservative estimate: assume 50% hit rate if no statistics available
        hit_rate = 0.5;
    }

    // Row hit latency (just column access)
    Cycle hit_latency = static_cast<Cycle>(tCAS_cycles);

    // Row miss/conflict latency (precharge + activate + column)
    Cycle miss_latency = static_cast<Cycle>(tRP_cycles + tRCD_cycles + tCAS_cycles);

    // Weighted average latency
    double avg_latency = hit_rate * hit_latency + (1.0 - hit_rate) * miss_latency;

    return static_cast<Cycle>(avg_latency);
}

void RamulatorWrapper::updateEnergyMetrics() const {
    if (current_cycle_ == last_energy_update_) {
        return;  // Already updated this cycle
    }

    // Energy per memory access from DRAM architecture and literature
    // Based on NVIDIA-HPCA17, DAS-MICRO15, and JEDEC power specs
    //
    // DDR4 typical values at 1.2V (from literature):
    //   Read:  2.0-2.5 nJ per access (activate + column read + precharge)
    //   Write: 2.5-3.0 nJ per access (higher due to write recovery)
    //
    // HBM2 typical values (from papers):
    //   Read:  1.0-1.5 nJ per access (TSV reduces energy)
    //   Write: 1.2-1.8 nJ per access

    double read_energy_per_access = 2.5;   // Default DDR4 value (nJ)
    double write_energy_per_access = 3.0;  // Default DDR4 value (nJ)
    double leakage_power_per_gb = 0.8;     // Default DDR4 value (mW per GB)

    // Use actual energy from DRAM architecture if available
    if (dram_arch_) {
        // Bank energy (pJ/byte) includes activation + column access + data transfer
        // Convert to nJ per 64-byte access (typical cache line)
        const double BYTES_PER_ACCESS = 64.0;
        read_energy_per_access = dram_arch_->energy.bank_energy_pJ * BYTES_PER_ACCESS / 1000.0;

        // Write energy is typically 15-20% higher than read
        write_energy_per_access = read_energy_per_access * 1.2;

        // Leakage power scales with capacity
        // DDR4: ~80-100 mW/GB, HBM2: ~50-60 mW/GB (better due to lower voltage)
        if (dram_arch_->technology == "HBM2" || dram_arch_->technology == "HBM3") {
            leakage_power_per_gb = 0.55;  // Lower for HBM
        } else {
            leakage_power_per_gb = 0.85;  // DDR4/DDR5
        }
    }

    // Calculate total energy
    cached_read_energy_ = total_reads_ * read_energy_per_access;
    cached_write_energy_ = total_writes_ * write_energy_per_access;

    // Leakage power (mW) = capacity (GB) × leakage_per_GB
    double capacity_gb = capacity_ / (1024.0 * 1024.0 * 1024.0);
    cached_leakage_power_ = capacity_gb * leakage_power_per_gb;

    last_energy_update_ = current_cycle_;
}

void RamulatorWrapper::printStats() const {
    std::cout << "\n=== Ramulator DRAM Statistics ===" << std::endl;
    std::cout << "Total Reads:     " << total_reads_ << std::endl;
    std::cout << "Total Writes:    " << total_writes_ << std::endl;
    std::cout << "Row Hits:        " << row_hits_ << std::endl;
    std::cout << "Row Misses:      " << row_misses_ << std::endl;
    std::cout << "Row Conflicts:   " << row_conflicts_ << std::endl;

    if (total_reads_ + total_writes_ > 0) {
        double hit_rate = static_cast<double>(row_hits_) /
                         (total_reads_ + total_writes_) * 100.0;
        std::cout << "Row Hit Rate:    " << hit_rate << "%" << std::endl;
    }

    std::cout << "\n=== Energy Metrics ===" << std::endl;
    std::cout << "Read Energy:     " << getReadEnergy() << " nJ" << std::endl;
    std::cout << "Write Energy:    " << getWriteEnergy() << " nJ" << std::endl;
    std::cout << "Activation:      " << getActivationEnergy() << " nJ" << std::endl;
    std::cout << "Precharge:       " << getPrechargeEnergy() << " nJ" << std::endl;
    std::cout << "Refresh:         " << getRefreshEnergy() << " nJ" << std::endl;
    std::cout << "Leakage Power:   " << getLeakagePower() << " mW" << std::endl;
    std::cout << "Total Energy:    " << getTotalEnergy() << " nJ" << std::endl;

    // If Ramulator is active, print its statistics
    if (ramulator_memory_system_) {
        std::cout << "\n=== Ramulator Detailed Statistics ===" << std::endl;
        // Ramulator's finalize() prints statistics
        // We don't call it here to avoid ending simulation
    }
}

void RamulatorWrapper::resetStats() {
    total_reads_ = 0;
    total_writes_ = 0;
    row_hits_ = 0;
    row_misses_ = 0;
    row_conflicts_ = 0;
    cached_read_energy_ = 0.0;
    cached_write_energy_ = 0.0;
    cached_leakage_power_ = 0.0;
    last_energy_update_ = 0;
    pending_requests_.clear();

    // Reset PIM stats
    if (pim_enabled_ && pim_plugin_) {
        pim_plugin_->resetStats();
    }
}

// ============================================================================
// PIM-Specific Methods
// ============================================================================

void RamulatorWrapper::enablePIMSupport(const std::string& dram_type) {
    if (pim_enabled_) {
        std::cout << "PIM support already enabled!\n";
        return;
    }

    dram_type_ = dram_type;
    pim_enabled_ = true;

    std::cout << "Enabling PIM support for " << dram_type_ << "...\n";

    // Create DRAM architecture based on type
    if (dram_type_ == "DDR4") {
        dram_arch_ = pimid::memory::createDDR4_2400_Verified();
    } else if (dram_type_ == "DDR5") {
        // TODO: Add DDR5 verified specs
        std::cerr << "DDR5 specs not yet implemented, using DDR4\n";
        dram_arch_ = pimid::memory::createDDR4_2400_Verified();
    } else if (dram_type_ == "HBM2") {
        dram_arch_ = pimid::memory::createHBM2_Verified();
    } else if (dram_type_ == "HBM3") {
        // TODO: Add HBM3 verified specs
        std::cerr << "HBM3 specs not yet implemented, using HBM2\n";
        dram_arch_ = pimid::memory::createHBM2_Verified();
    } else {
        std::cerr << "Unknown DRAM type: " << dram_type_ << ", using DDR4\n";
        dram_arch_ = pimid::memory::createDDR4_2400_Verified();
    }

    // Initialize PIM components
    initializePIMComponents();

    std::cout << "PIM support enabled!\n";
}

void RamulatorWrapper::initializePIMComponents() {
    if (!dram_arch_) {
        std::cerr << "ERROR: DRAM architecture not initialized!\n";
        return;
    }

    // Create bandwidth tracker
    bandwidth_tracker_ = std::make_shared<PIMBandwidthTracker>(dram_arch_);

    // Create PIM controller plugin
    pim_plugin_ = std::make_shared<PIMControllerPlugin>(dram_arch_, dram_type_);

    // Initialize with DRAM organization
    // Use organization from Ramulator if available, otherwise use defaults
    int num_subarrays = 16;  // Typical
    int num_bank_groups = dram_arch_->organization.bank_groups_per_chip;
    int num_banks = dram_arch_->organization.banks_per_bank_group * num_bank_groups;

    bandwidth_tracker_->initialize(
        channels_,
        ranks_per_channel_,
        num_bank_groups,
        num_banks,
        num_subarrays
    );

    pim_plugin_->initialize(
        channels_,
        ranks_per_channel_,
        num_bank_groups,
        num_banks,
        num_subarrays
    );

    // Create internal network
    internal_network_ = createInternalDRAMNetwork(
        dram_type_,
        num_subarrays,
        dram_arch_->organization.banks_per_bank_group,
        num_bank_groups,
        8  // chips per rank (typical)
    );

    std::cout << "PIM components initialized:\n";
    std::cout << "  Channels: " << channels_ << "\n";
    std::cout << "  Ranks: " << ranks_per_channel_ << "\n";
    std::cout << "  Bank Groups: " << num_bank_groups << "\n";
    std::cout << "  Banks: " << num_banks << "\n";
    std::cout << "  Subarrays: " << num_subarrays << "\n";
}

bool RamulatorWrapper::sendPIM(Address addr, MemoryRequestType type,
                              PIMRequestPayload* pim_payload,
                              std::function<void(Address)> callback) {
    if (!pim_enabled_) {
        std::cerr << "ERROR: PIM support not enabled! Call enablePIMSupport() first.\n";
        return false;
    }

    if (!pim_payload) {
        std::cerr << "ERROR: PIM payload is null!\n";
        return false;
    }

    // Create Ramulator request with PIM payload
    Ramulator::Request req = createPIMRequest(addr, type, pim_payload);

    // Set up callback
    req.callback = [this, addr, callback, pim_payload](Ramulator::Request& completed_req) {
        handleRequestCompletion(completed_req);

        // Calculate total latency including PIM-specific components
        uint64_t total_latency = completed_req.depart - completed_req.arrive;
        total_latency += pim_payload->data_movement_cycles;
        total_latency += pim_payload->network_cycles;

        // Call user callback
        if (callback) {
            callback(addr);
        }

        // Call PIM completion callback
        if (pim_payload->pim_completion_callback) {
            pim_payload->pim_completion_callback();
        }
    };

    // Send to Ramulator
    if (ramulator_memory_system_) {
        bool accepted = ramulator_memory_system_->send(req);
        if (accepted && callback) {
            pending_requests_.push_back({addr, type, current_cycle_, callback});
        }
        return accepted;
    }

    // Fallback
    if (callback) {
        callback(addr);
    }
    return true;
}

Ramulator::Request RamulatorWrapper::createPIMRequest(
    Address addr, MemoryRequestType type, PIMRequestPayload* pim_payload) {

    int req_type = (type == MemoryRequestType::READ) ?
                   Ramulator::Request::Type::Read :
                   Ramulator::Request::Type::Write;

    Ramulator::Request req(static_cast<Ramulator::Addr_t>(addr), req_type);

    // Attach PIM payload
    req.m_payload = static_cast<void*>(pim_payload);

    return req;
}

void RamulatorWrapper::registerPE(PIMGranularity granularity, int pe_id, int target_bank) {
    if (!pim_enabled_) {
        std::cerr << "WARNING: PIM support not enabled!\n";
        return;
    }

    if (pim_plugin_) {
        pim_plugin_->registerPE(granularity, pe_id, target_bank);
    }
}

double RamulatorWrapper::getBandwidthLimit(PIMGranularity granularity) const {
    if (pim_enabled_ && pim_plugin_) {
        return pim_plugin_->getBandwidthLimit(granularity);
    }
    return 0.0;
}

int RamulatorWrapper::getPortBitwidth(PIMGranularity granularity) const {
    if (pim_enabled_ && pim_plugin_) {
        return pim_plugin_->getPortBitwidth(granularity);
    }
    return 0;
}

double RamulatorWrapper::getEffectiveBandwidthPerPE(PIMGranularity granularity,
                                                   int target_id) const {
    if (pim_enabled_ && pim_plugin_) {
        return pim_plugin_->getEffectiveBandwidthPerPE(granularity, target_id);
    }
    return 0.0;
}

} // namespace pimid
