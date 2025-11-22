/**
 * @file pe_plugin.cpp
 * @brief Implementation of Processing Element (PE) plugin system for PIMID
 *
 * This file provides implementations for three example PE plugins:
 * 1. ScalarPEPlugin - Simple scalar processing element
 * 2. VectorPEPlugin - SIMD/vector processing element
 * 3. MatrixPEPlugin - Matrix/tensor processing element
 *
 * Each plugin demonstrates how to create custom PE architectures with
 * different computational capabilities.
 */

#include "plugin/pe_plugin.h"
#include "address_translation/pe_placement.h"
#include <sstream>

namespace pimid {
namespace plugin {

//=============================================================================
// ScalarPEPlugin Implementation
//=============================================================================

/**
 * @brief Constructor for ScalarPEPlugin
 *
 * Creates a simple scalar processing element with basic arithmetic capabilities.
 */
ScalarPEPlugin::ScalarPEPlugin()
    : PEPluginBase(PluginMetadata{
        "ScalarPE",                      // name
        "1.0.0",                         // version
        "PIMID Team",                    // author
        "Simple scalar processing element", // description
        PluginType::PE_TYPE              // type
    })
{
    // Initialize default architecture
    arch_.name = "ScalarPE";
    arch_.capabilities = {
        PECapability::INTEGER_ARITHMETIC,
        PECapability::FLOATING_POINT,
        PECapability::LOGIC_OPS
    };
    arch_.num_registers = 32;
    arch_.register_width_bits = 64;
    arch_.vector_width = 1;  // Scalar
    arch_.pipeline_stages = 5;
    arch_.frequency_mhz = 1000.0;  // 1 GHz
    arch_.area_mm2 = 0.5;
    arch_.power_mw = 50.0;
}

/**
 * @brief Initialize the scalar PE plugin
 *
 * @param config Configuration parameters
 * @return true if initialization succeeded
 */
bool ScalarPEPlugin::initialize(const std::map<std::string, std::string>& config) {
    config_ = config;

    // Read configuration parameters
    arch_.num_registers = getConfigValueAs<uint32_t>("num_registers", 32);
    arch_.register_width_bits = getConfigValueAs<uint32_t>("register_width_bits", 64);
    arch_.pipeline_stages = getConfigValueAs<uint32_t>("pipeline_stages", 5);
    arch_.frequency_mhz = getConfigValueAs<double>("frequency_mhz", 1000.0);
    arch_.area_mm2 = getConfigValueAs<double>("area_mm2", 0.5);
    arch_.power_mw = getConfigValueAs<double>("power_mw", 50.0);

    initialized_ = true;
    return true;
}

void ScalarPEPlugin::shutdown() {
    initialized_ = false;
}

bool ScalarPEPlugin::validateConfig(
    const std::map<std::string, std::string>& config,
    std::vector<std::string>& errors) const {

    if (!PluginBase::validateConfig(config, errors)) {
        return false;
    }

    // Validate positive values
    auto validate_positive = [&](const std::string& key) {
        auto it = config.find(key);
        if (it != config.end()) {
            try {
                double value = std::stod(it->second);
                if (value <= 0.0) {
                    errors.push_back(key + " must be positive");
                }
            } catch (...) {
                errors.push_back(key + " must be a valid number");
            }
        }
    };

    validate_positive("frequency_mhz");
    validate_positive("area_mm2");
    validate_positive("power_mw");

    return errors.empty();
}

std::vector<std::string> ScalarPEPlugin::getRequiredParameters() const {
    return {};  // All parameters are optional
}

std::vector<std::string> ScalarPEPlugin::getOptionalParameters() const {
    return {
        "num_registers",
        "register_width_bits",
        "pipeline_stages",
        "frequency_mhz",
        "area_mm2",
        "power_mw"
    };
}

std::map<std::string, std::string> ScalarPEPlugin::getParameterDescriptions() const {
    return {
        {"num_registers", "Number of general-purpose registers (default: 32)"},
        {"register_width_bits", "Register width in bits (default: 64)"},
        {"pipeline_stages", "Pipeline depth (default: 5)"},
        {"frequency_mhz", "Operating frequency in MHz (default: 1000)"},
        {"area_mm2", "PE area in mm² (default: 0.5)"},
        {"power_mw", "Average power consumption in mW (default: 50)"}
    };
}

std::unique_ptr<ProcessingElement> ScalarPEPlugin::createPE(
    uint32_t pe_id,
    PEPlacementManager* placement_manager) {

    return std::make_unique<ProcessingElement>(pe_id, arch_);
}

//=============================================================================
// VectorPEPlugin Implementation
//=============================================================================

/**
 * @brief Constructor for VectorPEPlugin
 *
 * Creates a SIMD/vector processing element with parallel execution capabilities.
 */
VectorPEPlugin::VectorPEPlugin()
    : PEPluginBase(PluginMetadata{
        "VectorPE",                      // name
        "1.0.0",                         // version
        "PIMID Team",                    // author
        "SIMD/vector processing element", // description
        PluginType::PE_TYPE              // type
    })
{
    // Initialize default architecture
    arch_.name = "VectorPE";
    arch_.capabilities = {
        PECapability::INTEGER_ARITHMETIC,
        PECapability::FLOATING_POINT,
        PECapability::VECTOR_OPS,
        PECapability::LOGIC_OPS
    };
    arch_.num_registers = 32;
    arch_.register_width_bits = 512;  // Wide registers for SIMD
    arch_.vector_width = 8;  // Process 8 elements in parallel
    arch_.pipeline_stages = 6;
    arch_.frequency_mhz = 1000.0;  // 1 GHz
    arch_.area_mm2 = 2.0;  // Larger than scalar
    arch_.power_mw = 150.0;  // Higher power
}

bool VectorPEPlugin::initialize(const std::map<std::string, std::string>& config) {
    config_ = config;

    // Read configuration parameters
    arch_.num_registers = getConfigValueAs<uint32_t>("num_registers", 32);
    arch_.register_width_bits = getConfigValueAs<uint32_t>("register_width_bits", 512);
    arch_.vector_width = getConfigValueAs<uint32_t>("vector_width", 8);
    arch_.pipeline_stages = getConfigValueAs<uint32_t>("pipeline_stages", 6);
    arch_.frequency_mhz = getConfigValueAs<double>("frequency_mhz", 1000.0);
    arch_.area_mm2 = getConfigValueAs<double>("area_mm2", 2.0);
    arch_.power_mw = getConfigValueAs<double>("power_mw", 150.0);

    initialized_ = true;
    return true;
}

void VectorPEPlugin::shutdown() {
    initialized_ = false;
}

bool VectorPEPlugin::validateConfig(
    const std::map<std::string, std::string>& config,
    std::vector<std::string>& errors) const {

    if (!PluginBase::validateConfig(config, errors)) {
        return false;
    }

    // Validate vector width is a power of 2
    auto it = config.find("vector_width");
    if (it != config.end()) {
        try {
            uint32_t width = std::stoul(it->second);
            if (width == 0 || (width & (width - 1)) != 0) {
                errors.push_back("vector_width must be a power of 2");
            }
        } catch (...) {
            errors.push_back("vector_width must be a valid integer");
        }
    }

    return errors.empty();
}

std::vector<std::string> VectorPEPlugin::getRequiredParameters() const {
    return {};  // All parameters are optional
}

std::vector<std::string> VectorPEPlugin::getOptionalParameters() const {
    return {
        "num_registers",
        "register_width_bits",
        "vector_width",
        "pipeline_stages",
        "frequency_mhz",
        "area_mm2",
        "power_mw"
    };
}

std::map<std::string, std::string> VectorPEPlugin::getParameterDescriptions() const {
    return {
        {"num_registers", "Number of vector registers (default: 32)"},
        {"register_width_bits", "Vector register width in bits (default: 512)"},
        {"vector_width", "SIMD width (elements processed in parallel, default: 8)"},
        {"pipeline_stages", "Pipeline depth (default: 6)"},
        {"frequency_mhz", "Operating frequency in MHz (default: 1000)"},
        {"area_mm2", "PE area in mm² (default: 2.0)"},
        {"power_mw", "Average power consumption in mW (default: 150)"}
    };
}

std::unique_ptr<ProcessingElement> VectorPEPlugin::createPE(
    uint32_t pe_id,
    PEPlacementManager* placement_manager) {

    return std::make_unique<ProcessingElement>(pe_id, arch_);
}

//=============================================================================
// MatrixPEPlugin Implementation
//=============================================================================

/**
 * @brief Constructor for MatrixPEPlugin
 *
 * Creates a matrix/tensor processing element optimized for matrix operations.
 */
MatrixPEPlugin::MatrixPEPlugin()
    : PEPluginBase(PluginMetadata{
        "MatrixPE",                      // name
        "1.0.0",                         // version
        "PIMID Team",                    // author
        "Matrix/tensor processing element", // description
        PluginType::PE_TYPE              // type
    }),
    matrix_size_(16)  // 16x16 matrix unit
{
    // Initialize default architecture
    arch_.name = "MatrixPE";
    arch_.capabilities = {
        PECapability::INTEGER_ARITHMETIC,
        PECapability::FLOATING_POINT,
        PECapability::MATRIX_OPS,
        PECapability::VECTOR_OPS
    };
    arch_.num_registers = 64;  // More registers for matrix operations
    arch_.register_width_bits = 256;
    arch_.vector_width = 16;  // Matrix dimension
    arch_.pipeline_stages = 8;
    arch_.frequency_mhz = 1000.0;  // 1 GHz
    arch_.area_mm2 = 5.0;  // Large area for matrix unit
    arch_.power_mw = 300.0;  // High power
}

bool MatrixPEPlugin::initialize(const std::map<std::string, std::string>& config) {
    config_ = config;

    // Read configuration parameters
    matrix_size_ = getConfigValueAs<uint32_t>("matrix_size", 16);
    arch_.vector_width = matrix_size_;

    arch_.num_registers = getConfigValueAs<uint32_t>("num_registers", 64);
    arch_.register_width_bits = getConfigValueAs<uint32_t>("register_width_bits", 256);
    arch_.pipeline_stages = getConfigValueAs<uint32_t>("pipeline_stages", 8);
    arch_.frequency_mhz = getConfigValueAs<double>("frequency_mhz", 1000.0);
    arch_.area_mm2 = getConfigValueAs<double>("area_mm2", 5.0);
    arch_.power_mw = getConfigValueAs<double>("power_mw", 300.0);

    initialized_ = true;
    return true;
}

void MatrixPEPlugin::shutdown() {
    initialized_ = false;
}

bool MatrixPEPlugin::validateConfig(
    const std::map<std::string, std::string>& config,
    std::vector<std::string>& errors) const {

    if (!PluginBase::validateConfig(config, errors)) {
        return false;
    }

    // Validate matrix size is a power of 2
    auto it = config.find("matrix_size");
    if (it != config.end()) {
        try {
            uint32_t size = std::stoul(it->second);
            if (size == 0 || (size & (size - 1)) != 0) {
                errors.push_back("matrix_size must be a power of 2");
            }
            if (size < 4 || size > 64) {
                errors.push_back("matrix_size must be between 4 and 64");
            }
        } catch (...) {
            errors.push_back("matrix_size must be a valid integer");
        }
    }

    return errors.empty();
}

std::vector<std::string> MatrixPEPlugin::getRequiredParameters() const {
    return {};  // All parameters are optional
}

std::vector<std::string> MatrixPEPlugin::getOptionalParameters() const {
    return {
        "num_registers",
        "register_width_bits",
        "matrix_size",
        "pipeline_stages",
        "frequency_mhz",
        "area_mm2",
        "power_mw"
    };
}

std::map<std::string, std::string> MatrixPEPlugin::getParameterDescriptions() const {
    return {
        {"num_registers", "Number of matrix registers (default: 64)"},
        {"register_width_bits", "Register width in bits (default: 256)"},
        {"matrix_size", "Matrix unit dimension (e.g., 16 for 16x16, default: 16)"},
        {"pipeline_stages", "Pipeline depth (default: 8)"},
        {"frequency_mhz", "Operating frequency in MHz (default: 1000)"},
        {"area_mm2", "PE area in mm² (default: 5.0)"},
        {"power_mw", "Average power consumption in mW (default: 300)"}
    };
}

std::unique_ptr<ProcessingElement> MatrixPEPlugin::createPE(
    uint32_t pe_id,
    PEPlacementManager* placement_manager) {

    return std::make_unique<ProcessingElement>(pe_id, arch_);
}

} // namespace plugin
} // namespace pimid
