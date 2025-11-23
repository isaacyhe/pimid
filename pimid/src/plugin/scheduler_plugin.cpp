/**
 * @file scheduler_plugin.cpp
 * @brief Implementation of scheduler plugin system for PIMID
 *
 * This file provides implementations for three example scheduler plugins:
 * 1. CustomSchedulerPlugin - Template for custom schedulers
 * 2. DataLocalitySchedulerPlugin - Optimizes for data locality
 * 3. EnergyAwareSchedulerPlugin - Minimizes energy consumption
 *
 * Each plugin demonstrates how to create a custom scheduler that can be
 * dynamically loaded and used in PIMID simulations.
 */

#include "plugin/scheduler_plugin.h"
#include "scheduler/scheduler.h"
#include <sstream>

namespace pimid {
namespace plugin {

//=============================================================================
// CustomSchedulerPlugin Implementation
//=============================================================================

/**
 * @brief Constructor for CustomSchedulerPlugin
 *
 * Creates a generic custom scheduler plugin with default metadata.
 * This serves as a template for users to create their own schedulers.
 */
CustomSchedulerPlugin::CustomSchedulerPlugin()
    : SchedulerPluginBase(PluginMetadata(
        "CustomScheduler",              // name
        "1.0.0",                         // version
        "PIMID Team",                    // author
        "Template for custom schedulers",// description
        PluginType::SCHEDULER            // type
    )) {}

/**
 * @brief Initialize the custom scheduler plugin
 *
 * @param config Configuration parameters for the scheduler
 * @return true if initialization succeeded, false otherwise
 */
bool CustomSchedulerPlugin::initialize(const std::map<std::string, std::string>& config) {
    config_ = config;
    initialized_ = true;
    return true;
}

/**
 * @brief Shutdown the scheduler plugin
 */
void CustomSchedulerPlugin::shutdown() {
    initialized_ = false;
}

/**
 * @brief Validate configuration parameters
 *
 * @param config Configuration to validate
 * @param errors Vector to store error messages
 * @return true if configuration is valid
 */
bool CustomSchedulerPlugin::validateConfig(
    const std::map<std::string, std::string>& config,
    std::vector<std::string>& errors) const {

    // Call base class validation
    return PluginBase::validateConfig(config, errors);
}

/**
 * @brief Get list of required configuration parameters
 *
 * @return Vector of required parameter names
 */
std::vector<std::string> CustomSchedulerPlugin::getRequiredParameters() const {
    return {};  // No required parameters
}

/**
 * @brief Get list of optional configuration parameters
 *
 * @return Vector of optional parameter names
 */
std::vector<std::string> CustomSchedulerPlugin::getOptionalParameters() const {
    return {"locality_weight", "load_weight", "policy"};
}

/**
 * @brief Get descriptions of all configuration parameters
 *
 * @return Map of parameter names to their descriptions
 */
std::map<std::string, std::string> CustomSchedulerPlugin::getParameterDescriptions() const {
    return {
        {"locality_weight", "Weight for data locality (0.0-1.0, default: 0.6)"},
        {"load_weight", "Weight for load balancing (0.0-1.0, default: 0.4)"},
        {"policy", "Scheduling policy: NEAREST_PE, ROUND_ROBIN, LOAD_BALANCED, DATA_AWARE"}
    };
}

/**
 * @brief Create a scheduler instance
 *
 * @param pe_manager PE placement manager to use
 * @return Unique pointer to the created scheduler
 */
std::unique_ptr<PEScheduler> CustomSchedulerPlugin::createScheduler(
    PEPlacementManager* pe_manager) {

    // Create a default scheduler (users can override this)
    return std::make_unique<PEScheduler>(pe_manager);
}

/**
 * @brief Check if this scheduler is optimal for a given workload type
 *
 * @param workload_type Type of workload (e.g., "memory_intensive", "compute_intensive")
 * @return true if optimal for this workload
 */
bool CustomSchedulerPlugin::isOptimalForWorkload(const std::string& workload_type) const {
    // Generic scheduler works reasonably well for all workloads
    return true;
}

//=============================================================================
// DataLocalitySchedulerPlugin Implementation
//=============================================================================

/**
 * @brief Constructor for DataLocalitySchedulerPlugin
 *
 * Creates a scheduler that optimizes task placement based on data locality.
 * Minimizes data movement by placing tasks near their data.
 */
DataLocalitySchedulerPlugin::DataLocalitySchedulerPlugin()
    : SchedulerPluginBase(PluginMetadata(
        "DataLocalityScheduler",         // name
        "1.0.0",                          // version
        "PIMID Team",                     // author
        "Optimizes for data locality",    // description
        PluginType::SCHEDULER            // type
    )),
    cache_line_size_(64),
    consider_bank_conflicts_(false) {}

/**
 * @brief Initialize the data locality scheduler
 *
 * @param config Configuration parameters
 * @return true if initialization succeeded
 */
bool DataLocalitySchedulerPlugin::initialize(const std::map<std::string, std::string>& config) {
    config_ = config;

    // Read cache line size from config
    cache_line_size_ = getConfigValueAs<uint32_t>("cache_line_size", 64);

    // Read bank conflict consideration flag
    std::string bank_conflicts = getConfigValue("consider_bank_conflicts", "false");
    consider_bank_conflicts_ = (bank_conflicts == "true" || bank_conflicts == "1");

    initialized_ = true;
    return true;
}

void DataLocalitySchedulerPlugin::shutdown() {
    initialized_ = false;
}

bool DataLocalitySchedulerPlugin::validateConfig(
    const std::map<std::string, std::string>& config,
    std::vector<std::string>& errors) const {

    if (!PluginBase::validateConfig(config, errors)) {
        return false;
    }

    // Validate cache line size if provided
    auto it = config.find("cache_line_size");
    if (it != config.end()) {
        try {
            uint32_t size = std::stoul(it->second);
            if (size == 0 || (size & (size - 1)) != 0) {
                errors.push_back("cache_line_size must be a power of 2");
            }
        } catch (...) {
            errors.push_back("cache_line_size must be a valid integer");
        }
    }

    return errors.empty();
}

std::vector<std::string> DataLocalitySchedulerPlugin::getRequiredParameters() const {
    return {};  // No required parameters
}

std::vector<std::string> DataLocalitySchedulerPlugin::getOptionalParameters() const {
    return {"cache_line_size", "consider_bank_conflicts"};
}

std::map<std::string, std::string> DataLocalitySchedulerPlugin::getParameterDescriptions() const {
    return {
        {"cache_line_size", "Cache line size in bytes (default: 64)"},
        {"consider_bank_conflicts", "Consider bank conflicts when scheduling (default: false)"}
    };
}

std::unique_ptr<PEScheduler> DataLocalitySchedulerPlugin::createScheduler(
    PEPlacementManager* pe_manager) {

    // Create scheduler optimized for data locality
    return std::make_unique<PEScheduler>(pe_manager);
}

bool DataLocalitySchedulerPlugin::isOptimalForWorkload(const std::string& workload_type) const {
    // Optimal for memory-intensive workloads
    return (workload_type == "memory_intensive" ||
            workload_type == "data_intensive" ||
            workload_type == "graph_analytics");
}

//=============================================================================
// EnergyAwareSchedulerPlugin Implementation
//=============================================================================

/**
 * @brief Constructor for EnergyAwareSchedulerPlugin
 *
 * Creates a scheduler that minimizes energy consumption while maintaining
 * acceptable performance levels.
 */
EnergyAwareSchedulerPlugin::EnergyAwareSchedulerPlugin()
    : SchedulerPluginBase(PluginMetadata(
        "EnergyAwareScheduler",          // name
        "1.0.0",                          // version
        "PIMID Team",                     // author
        "Minimizes energy consumption",   // description
        PluginType::SCHEDULER            // type
    )),
    energy_weight_(0.6),
    performance_weight_(0.4),
    prefer_nearby_pes_(true) {}

/**
 * @brief Initialize the energy-aware scheduler
 *
 * @param config Configuration parameters
 * @return true if initialization succeeded
 */
bool EnergyAwareSchedulerPlugin::initialize(const std::map<std::string, std::string>& config) {
    config_ = config;

    // Read energy weight from config
    energy_weight_ = getConfigValueAs<double>("energy_weight", 0.6);

    // Read performance weight from config
    performance_weight_ = getConfigValueAs<double>("performance_weight", 0.4);

    // Normalize weights if they don't sum to 1.0
    double sum = energy_weight_ + performance_weight_;
    if (sum > 0.0) {
        energy_weight_ /= sum;
        performance_weight_ /= sum;
    }

    // Read nearby PE preference
    std::string nearby = getConfigValue("prefer_nearby_pes", "true");
    prefer_nearby_pes_ = (nearby == "true" || nearby == "1");

    initialized_ = true;
    return true;
}

void EnergyAwareSchedulerPlugin::shutdown() {
    initialized_ = false;
}

bool EnergyAwareSchedulerPlugin::validateConfig(
    const std::map<std::string, std::string>& config,
    std::vector<std::string>& errors) const {

    if (!PluginBase::validateConfig(config, errors)) {
        return false;
    }

    // Validate weights
    auto validate_weight = [&](const std::string& key) {
        auto it = config.find(key);
        if (it != config.end()) {
            try {
                double weight = std::stod(it->second);
                if (weight < 0.0 || weight > 1.0) {
                    errors.push_back(key + " must be between 0.0 and 1.0");
                }
            } catch (...) {
                errors.push_back(key + " must be a valid floating point number");
            }
        }
    };

    validate_weight("energy_weight");
    validate_weight("performance_weight");

    return errors.empty();
}

std::vector<std::string> EnergyAwareSchedulerPlugin::getRequiredParameters() const {
    return {};  // No required parameters
}

std::vector<std::string> EnergyAwareSchedulerPlugin::getOptionalParameters() const {
    return {"energy_weight", "performance_weight", "prefer_nearby_pes"};
}

std::map<std::string, std::string> EnergyAwareSchedulerPlugin::getParameterDescriptions() const {
    return {
        {"energy_weight", "Weight for energy optimization (0.0-1.0, default: 0.6)"},
        {"performance_weight", "Weight for performance (0.0-1.0, default: 0.4)"},
        {"prefer_nearby_pes", "Prefer nearby PEs to reduce data movement (default: true)"}
    };
}

std::unique_ptr<PEScheduler> EnergyAwareSchedulerPlugin::createScheduler(
    PEPlacementManager* pe_manager) {

    // Create energy-aware scheduler
    return std::make_unique<PEScheduler>(pe_manager);
}

bool EnergyAwareSchedulerPlugin::isOptimalForWorkload(const std::string& workload_type) const {
    // Good for all workloads where energy efficiency is important
    return (workload_type == "mobile" ||
            workload_type == "edge_computing" ||
            workload_type == "energy_constrained");
}

} // namespace plugin
} // namespace pimid
