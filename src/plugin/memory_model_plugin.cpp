/**
 * @file memory_model_plugin.cpp
 * @brief Implementation of memory model plugin system for PIMID
 *
 * This file provides implementation for the CustomMemoryModelPlugin,
 * which serves as a template for users to create their own memory models.
 *
 * The plugin system allows users to:
 * - Define custom memory technologies (e.g., emerging non-volatile memories)
 * - Specify latency, bandwidth, and power characteristics
 * - Integrate custom memory simulators
 * - Support various memory operations (read, write, atomic)
 */

#include "plugin/memory_model_plugin.h"
#include "memory/memory_model.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace pimid {
namespace plugin {

//=============================================================================
// CustomMemoryModelPlugin Implementation
//=============================================================================

/**
 * @brief Constructor for CustomMemoryModelPlugin
 *
 * Creates a custom memory model plugin with default metadata.
 * This serves as a template for users to create their own memory models.
 */
CustomMemoryModelPlugin::CustomMemoryModelPlugin()
    : MemoryModelPluginBase(PluginMetadata(
        "CustomMemory",                     // name
        "1.0.0",                            // version
        "PIMID Team",                       // author
        "Template for custom memory models",// description
        PluginType::MEMORY_MODEL            // type
    )),
    read_latency_(50),      // Default: 50 cycles
    write_latency_(100),    // Default: 100 cycles
    bandwidth_(25600),      // Default: 25.6 GB/s
    tech_name_("CustomMemory"),
    tech_node_(22)          // Default: 22nm technology node
{
}

/**
 * @brief Initialize the custom memory model plugin
 *
 * @param config Configuration parameters for the memory model
 * @return true if initialization succeeded, false otherwise
 *
 * Configuration parameters:
 * - read_latency: Read access latency in cycles
 * - write_latency: Write access latency in cycles
 * - bandwidth: Maximum bandwidth in MB/s
 * - tech_name: Technology name (e.g., "SRAM", "MRAM", "ReRAM")
 * - tech_node_nm: Technology node in nanometers
 */
bool CustomMemoryModelPlugin::initialize(const std::map<std::string, std::string>& config) {
    config_ = config;

    // Read configuration parameters with defaults
    read_latency_ = getConfigValueAs<Cycle>("read_latency", 50);
    write_latency_ = getConfigValueAs<Cycle>("write_latency", 100);
    bandwidth_ = getConfigValueAs<uint64_t>("bandwidth", 25600);  // MB/s
    tech_name_ = getConfigValue("tech_name", "CustomMemory");
    tech_node_ = getConfigValueAs<uint32_t>("tech_node_nm", 22);

    // Validate parameters
    if (read_latency_ == 0) {
        std::cerr << "Warning: read_latency is 0, using default of 50 cycles" << std::endl;
        read_latency_ = 50;
    }

    if (write_latency_ == 0) {
        std::cerr << "Warning: write_latency is 0, using default of 100 cycles" << std::endl;
        write_latency_ = 100;
    }

    if (bandwidth_ == 0) {
        std::cerr << "Warning: bandwidth is 0, using default of 25600 MB/s" << std::endl;
        bandwidth_ = 25600;
    }

    initialized_ = true;
    return true;
}

/**
 * @brief Shutdown the memory model plugin
 */
void CustomMemoryModelPlugin::shutdown() {
    initialized_ = false;
}

/**
 * @brief Validate configuration parameters
 *
 * @param config Configuration to validate
 * @param errors Vector to store error messages
 * @return true if configuration is valid
 */
bool CustomMemoryModelPlugin::validateConfig(
    const std::map<std::string, std::string>& config,
    std::vector<std::string>& errors) const {

    if (!PluginBase::validateConfig(config, errors)) {
        return false;
    }

    // Validate numeric parameters
    auto validate_positive = [&](const std::string& key, const std::string& name) {
        auto it = config.find(key);
        if (it != config.end()) {
            try {
                int64_t value = std::stoll(it->second);
                if (value <= 0) {
                    errors.push_back(name + " must be positive");
                }
            } catch (...) {
                errors.push_back(name + " must be a valid integer");
            }
        }
    };

    validate_positive("read_latency", "read_latency");
    validate_positive("write_latency", "write_latency");
    validate_positive("bandwidth", "bandwidth");
    validate_positive("tech_node_nm", "tech_node_nm");

    return errors.empty();
}

/**
 * @brief Get list of required configuration parameters
 *
 * @return Vector of required parameter names
 */
std::vector<std::string> CustomMemoryModelPlugin::getRequiredParameters() const {
    // For custom memory, these are all optional with defaults
    return {};
}

/**
 * @brief Get list of optional configuration parameters
 *
 * @return Vector of optional parameter names
 */
std::vector<std::string> CustomMemoryModelPlugin::getOptionalParameters() const {
    return {
        "read_latency",
        "write_latency",
        "bandwidth",
        "tech_name",
        "tech_node_nm",
        "config_file"  // Path to detailed config file
    };
}

/**
 * @brief Get descriptions of all configuration parameters
 *
 * @return Map of parameter names to their descriptions
 */
std::map<std::string, std::string> CustomMemoryModelPlugin::getParameterDescriptions() const {
    return {
        {"read_latency", "Read access latency in cycles (default: 50)"},
        {"write_latency", "Write access latency in cycles (default: 100)"},
        {"bandwidth", "Maximum bandwidth in MB/s (default: 25600)"},
        {"tech_name", "Technology name, e.g., 'SRAM', 'MRAM' (default: 'CustomMemory')"},
        {"tech_node_nm", "Technology node in nanometers (default: 22)"},
        {"config_file", "Path to detailed memory configuration file (optional)"}
    };
}

/**
 * @brief Create a memory model instance
 *
 * This method creates an actual memory model instance that will be used
 * during simulation. Users should override this to create their specific
 * memory model type.
 *
 * @param config_path Path to memory configuration file
 * @return Shared pointer to the created memory model
 */
std::shared_ptr<MemoryModel> CustomMemoryModelPlugin::createMemoryModel(
    const std::string& config_path) {

    // Get the config file path from plugin config if not provided
    std::string actual_config_path = config_path;
    if (actual_config_path.empty()) {
        actual_config_path = getConfigValue("config_file", "");
    }

    // Users would typically create their custom MemoryModel subclass here
    // For now, we return a nullptr as this is just a template
    // Example:
    //   return std::make_shared<CustomMemoryModel>(actual_config_path, config_);

    std::cerr << "CustomMemoryModelPlugin::createMemoryModel() called" << std::endl;
    std::cerr << "  Config path: " << actual_config_path << std::endl;
    std::cerr << "  Read latency: " << read_latency_ << " cycles" << std::endl;
    std::cerr << "  Write latency: " << write_latency_ << " cycles" << std::endl;
    std::cerr << "  Bandwidth: " << bandwidth_ << " MB/s" << std::endl;
    std::cerr << "  Technology: " << tech_name_ << " (" << tech_node_ << "nm)" << std::endl;

    // Return nullptr for template - users should override this
    return nullptr;
}

/**
 * @brief Get typical read latency
 *
 * @return Read latency in cycles
 */
Cycle CustomMemoryModelPlugin::getTypicalReadLatency() const {
    return read_latency_;
}

/**
 * @brief Get typical write latency
 *
 * @return Write latency in cycles
 */
Cycle CustomMemoryModelPlugin::getTypicalWriteLatency() const {
    return write_latency_;
}

/**
 * @brief Get maximum bandwidth
 *
 * @return Maximum bandwidth in bytes per second
 */
uint64_t CustomMemoryModelPlugin::getMaxBandwidth() const {
    // Convert MB/s to bytes/s
    return bandwidth_ * 1024 * 1024;
}

/**
 * @brief Get technology name
 *
 * @return Technology name string
 */
std::string CustomMemoryModelPlugin::getTechnologyName() const {
    return tech_name_;
}

/**
 * @brief Get technology node size
 *
 * @return Technology node in nanometers
 */
uint32_t CustomMemoryModelPlugin::getTechnologyNode() const {
    return tech_node_;
}

} // namespace plugin
} // namespace pimid
