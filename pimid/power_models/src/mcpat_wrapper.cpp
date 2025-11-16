#include "mcpat_wrapper.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <stdexcept>

// Note: Full McPAT integration requires XML parsing
// This is a simplified wrapper with placeholder implementations

namespace pimid {

//=============================================================================
// McPATWrapper Implementation
//=============================================================================

McPATWrapper::McPATWrapper(const SystemConfig& config)
    : config_(config)
    , mcpat_parser_(nullptr)
    , mcpat_processor_(nullptr)
    , total_cycles_(0)
    , busy_cycles_(0)
    , total_instructions_(0)
    , l1_reads_(0)
    , l1_writes_(0)
    , l2_reads_(0)
    , l2_writes_(0)
    , l3_reads_(0)
    , l3_writes_(0)
    , initialized_(false)
    , valid_(false)
    , power_computed_(false)
    , error_message_("")
{
}

McPATWrapper::~McPATWrapper() {
    // Clean up McPAT objects if allocated
    if (mcpat_processor_) {
        // Note: McPAT cleanup would go here
        mcpat_processor_ = nullptr;
    }
    if (mcpat_parser_) {
        mcpat_parser_ = nullptr;
    }
}

void McPATWrapper::initialize() {
    if (initialized_) {
        std::cerr << "[McPATWrapper] Warning: Already initialized" << std::endl;
        return;
    }

    validateConfiguration();
    if (!valid_) {
        throw std::runtime_error("[McPATWrapper] Invalid configuration: " + error_message_);
    }

    try {
        createMcPATInput();
        initialized_ = true;

        std::cout << "[McPATWrapper] Initialized with:" << std::endl;
        std::cout << "  Cores: " << config_.num_cores << std::endl;
        std::cout << "  Core Clock: " << config_.core_clock_mhz << " MHz" << std::endl;
        std::cout << "  L1I/L1D: " << (config_.l1i_size_bytes/1024) << "/"
                  << (config_.l1d_size_bytes/1024) << " KB" << std::endl;
        std::cout << "  L2: " << (config_.l2_size_bytes/1024) << " KB" << std::endl;
        std::cout << "  L3: " << (config_.l3_size_bytes/(1024*1024)) << " MB" << std::endl;
        std::cout << "  Technology: " << config_.tech_node_nm << " nm" << std::endl;

    } catch (const std::exception& e) {
        valid_ = false;
        error_message_ = std::string("Initialization failed: ") + e.what();
        throw;
    }
}

void McPATWrapper::reconfigure(const SystemConfig& config) {
    config_ = config;
    initialized_ = false;
    power_computed_ = false;
    initialize();
}

void McPATWrapper::validateConfiguration() {
    valid_ = true;
    error_message_ = "";

    if (config_.num_cores < 1 || config_.num_cores > 1024) {
        valid_ = false;
        error_message_ = "Number of cores out of range (1-1024)";
        return;
    }

    if (config_.core_clock_mhz < 100 || config_.core_clock_mhz > 10000) {
        valid_ = false;
        error_message_ = "Core clock out of range (100-10000 MHz)";
        return;
    }

    if (config_.tech_node_nm < 7 || config_.tech_node_nm > 90) {
        valid_ = false;
        error_message_ = "Technology node out of range (7nm - 90nm)";
        return;
    }
}

void McPATWrapper::setTotalCycles(uint64_t cycles) {
    total_cycles_ = cycles;
    power_computed_ = false;
}

void McPATWrapper::setBusyCycles(uint64_t cycles) {
    busy_cycles_ = cycles;
    power_computed_ = false;
}

void McPATWrapper::setTotalInstructions(uint64_t instructions) {
    total_instructions_ = instructions;
    power_computed_ = false;
}

void McPATWrapper::setL1Accesses(uint64_t reads, uint64_t writes) {
    l1_reads_ = reads;
    l1_writes_ = writes;
    power_computed_ = false;
}

void McPATWrapper::setL2Accesses(uint64_t reads, uint64_t writes) {
    l2_reads_ = reads;
    l2_writes_ = writes;
    power_computed_ = false;
}

void McPATWrapper::setL3Accesses(uint64_t reads, uint64_t writes) {
    l3_reads_ = reads;
    l3_writes_ = writes;
    power_computed_ = false;
}

void McPATWrapper::createMcPATInput() {
    std::cout << "[McPATWrapper] Creating McPAT configuration" << std::endl;
    // In full implementation, this would generate XML configuration
    // or parse existing XML file
    valid_ = true;
}

void McPATWrapper::runMcPAT() {
    std::cout << "[McPATWrapper] Running McPAT power analysis..." << std::endl;
    // Full implementation would call McPAT's analysis functions
    valid_ = true;
}

void McPATWrapper::computePower() {
    if (!initialized_) {
        throw std::runtime_error("[McPATWrapper] Not initialized");
    }

    runMcPAT();
    extractResults();
    power_computed_ = true;

    std::cout << "[McPATWrapper] Power analysis complete" << std::endl;
    std::cout << "  Total Power: " << system_power_.total_power << " W" << std::endl;
}

void McPATWrapper::extractResults() {
    // Extract results from McPAT analysis
    // This is a simplified placeholder implementation

    // Calculate approximate power based on technology and frequency
    double tech_factor = 90.0 / config_.tech_node_nm;  // Power scales with tech node
    double freq_factor = config_.core_clock_mhz / 1000.0;  // GHz

    // Core power (dynamic + leakage)
    PowerMetrics core_power;
    double core_dynamic_per_core = 2.0 * freq_factor * tech_factor;  // ~2W per core at 22nm, 1GHz
    core_power.runtime_dynamic = core_dynamic_per_core * config_.num_cores;
    core_power.subthreshold_leakage = 0.5 * config_.num_cores * tech_factor;
    core_power.gate_leakage = 0.1 * config_.num_cores * tech_factor;
    core_power.total_leakage = core_power.subthreshold_leakage + core_power.gate_leakage;
    core_power.total_dynamic = core_power.runtime_dynamic;
    core_power.total_power = core_power.total_dynamic + core_power.total_leakage;
    component_power_[ComponentType::CORE] = core_power;

    // L1 Cache power
    PowerMetrics l1_power;
    l1_power.runtime_dynamic = 0.3 * config_.num_cores;
    l1_power.total_leakage = 0.1 * config_.num_cores;
    l1_power.total_power = l1_power.runtime_dynamic + l1_power.total_leakage;
    component_power_[ComponentType::L1_CACHE] = l1_power;

    // L2 Cache power
    PowerMetrics l2_power;
    l2_power.runtime_dynamic = 0.5 * config_.num_cores;
    l2_power.total_leakage = 0.2 * config_.num_cores;
    l2_power.total_power = l2_power.runtime_dynamic + l2_power.total_leakage;
    component_power_[ComponentType::L2_CACHE] = l2_power;

    // L3 Cache power (shared)
    PowerMetrics l3_power;
    double l3_size_mb = config_.l3_size_bytes / (1024.0 * 1024.0);
    l3_power.runtime_dynamic = 0.1 * l3_size_mb;
    l3_power.total_leakage = 0.05 * l3_size_mb;
    l3_power.total_power = l3_power.runtime_dynamic + l3_power.total_leakage;
    component_power_[ComponentType::L3_CACHE] = l3_power;

    // Memory Controller
    PowerMetrics mc_power;
    mc_power.runtime_dynamic = 1.0 * config_.num_memory_controllers;
    mc_power.total_leakage = 0.2 * config_.num_memory_controllers;
    mc_power.total_power = mc_power.runtime_dynamic + mc_power.total_leakage;
    component_power_[ComponentType::MEMORY_CONTROLLER] = mc_power;

    // NoC power
    PowerMetrics noc_power;
    if (config_.has_noc) {
        noc_power.runtime_dynamic = 0.5 * config_.num_cores;
        noc_power.total_leakage = 0.1 * config_.num_cores;
        noc_power.total_power = noc_power.runtime_dynamic + noc_power.total_leakage;
    }
    component_power_[ComponentType::NOC] = noc_power;

    // System total
    system_power_.runtime_dynamic = 0;
    system_power_.total_leakage = 0;
    for (const auto& pair : component_power_) {
        system_power_.runtime_dynamic += pair.second.runtime_dynamic;
        system_power_.total_leakage += pair.second.total_leakage;
    }
    system_power_.total_dynamic = system_power_.runtime_dynamic;
    system_power_.total_power = system_power_.total_dynamic + system_power_.total_leakage;
}

//=============================================================================
// Query functions
//=============================================================================

McPATWrapper::PowerMetrics McPATWrapper::getComponentPower(ComponentType component) const {
    auto it = component_power_.find(component);
    if (it != component_power_.end()) {
        return it->second;
    }
    return PowerMetrics();
}

McPATWrapper::PowerMetrics McPATWrapper::getSystemPower() const {
    return system_power_;
}

double McPATWrapper::getCorePower() const {
    return getComponentPower(ComponentType::CORE).total_power;
}

double McPATWrapper::getCachePower() const {
    double total = 0.0;
    total += getComponentPower(ComponentType::L1_CACHE).total_power;
    total += getComponentPower(ComponentType::L2_CACHE).total_power;
    total += getComponentPower(ComponentType::L3_CACHE).total_power;
    return total;
}

double McPATWrapper::getMemoryControllerPower() const {
    return getComponentPower(ComponentType::MEMORY_CONTROLLER).total_power;
}

double McPATWrapper::getNoCPower() const {
    return getComponentPower(ComponentType::NOC).total_power;
}

double McPATWrapper::getComponentArea(ComponentType component) const {
    // Placeholder area calculations (mm^2)
    switch (component) {
        case ComponentType::CORE:
            return 10.0 * config_.num_cores;
        case ComponentType::L1_CACHE:
            return 2.0 * config_.num_cores;
        case ComponentType::L2_CACHE:
            return 4.0 * config_.num_cores;
        case ComponentType::L3_CACHE:
            return config_.l3_size_bytes / (1024.0 * 1024.0);  // ~1 mm^2 per MB
        case ComponentType::MEMORY_CONTROLLER:
            return 5.0 * config_.num_memory_controllers;
        case ComponentType::NOC:
            return config_.has_noc ? (2.0 * config_.num_cores) : 0.0;
        default:
            return 0.0;
    }
}

double McPATWrapper::getTotalArea() const {
    double total = 0.0;
    total += getComponentArea(ComponentType::CORE);
    total += getComponentArea(ComponentType::L1_CACHE);
    total += getComponentArea(ComponentType::L2_CACHE);
    total += getComponentArea(ComponentType::L3_CACHE);
    total += getComponentArea(ComponentType::MEMORY_CONTROLLER);
    total += getComponentArea(ComponentType::NOC);
    return total;
}

double McPATWrapper::getPeakPower() const {
    // Peak power is typically 1.5-2x average power
    return system_power_.total_power * 1.8;
}

double McPATWrapper::getEnergyForPeriod(double time_seconds) const {
    return system_power_.total_power * time_seconds;  // Joules
}

bool McPATWrapper::isValid() const {
    return valid_;
}

std::string McPATWrapper::getErrorMessage() const {
    return error_message_;
}

void McPATWrapper::printDetailedResults() const {
    if (!power_computed_) {
        std::cout << "[McPATWrapper] Power not yet computed" << std::endl;
        return;
    }

    std::cout << "\n=== McPAT Power Analysis Results ===" << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Cores: " << config_.num_cores << " @ " << config_.core_clock_mhz << " MHz" << std::endl;
    std::cout << "  Technology: " << config_.tech_node_nm << " nm" << std::endl;

    printComponentBreakdown();

    std::cout << "\nSystem Totals:" << std::endl;
    std::cout << "  Total Dynamic Power: " << system_power_.total_dynamic << " W" << std::endl;
    std::cout << "  Total Leakage Power: " << system_power_.total_leakage << " W" << std::endl;
    std::cout << "  Total Power: " << system_power_.total_power << " W" << std::endl;
    std::cout << "  Peak Power: " << getPeakPower() << " W" << std::endl;
    std::cout << "  Total Area: " << getTotalArea() << " mm^2" << std::endl;
    std::cout << "=====================================\n" << std::endl;
}

void McPATWrapper::printComponentBreakdown() const {
    std::cout << "\nComponent Power Breakdown:" << std::endl;

    auto print_component = [](const std::string& name, const PowerMetrics& power) {
        std::cout << "  " << name << ":" << std::endl;
        std::cout << "    Dynamic: " << power.runtime_dynamic << " W" << std::endl;
        std::cout << "    Leakage: " << power.total_leakage << " W" << std::endl;
        std::cout << "    Total: " << power.total_power << " W" << std::endl;
    };

    print_component("Cores", getComponentPower(ComponentType::CORE));
    print_component("L1 Caches", getComponentPower(ComponentType::L1_CACHE));
    print_component("L2 Caches", getComponentPower(ComponentType::L2_CACHE));
    print_component("L3 Cache", getComponentPower(ComponentType::L3_CACHE));
    print_component("Memory Controllers", getComponentPower(ComponentType::MEMORY_CONTROLLER));
    if (config_.has_noc) {
        print_component("NoC", getComponentPower(ComponentType::NOC));
    }
}

} // namespace pimid
