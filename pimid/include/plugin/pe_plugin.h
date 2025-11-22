/**
 * @file pe_plugin.h
 * @brief Processing Element (PE) plugin interface for PIMID
 *
 * This file defines the plugin interface for creating custom Processing Element
 * architectures in PIMID. Users can define:
 * - Custom PE instruction sets
 * - PE computational capabilities
 * - PE power and area characteristics
 * - Custom accelerator designs
 */

#ifndef PIMID_PE_PLUGIN_H
#define PIMID_PE_PLUGIN_H

#include "plugin/plugin_interface.h"
#include "common/types.h"
#include <memory>
#include <vector>

namespace pimid {

// Forward declarations
class PEPlacementManager;

/**
 * @brief PE computational capability flags
 */
enum class PECapability {
    INTEGER_ARITHMETIC,     // Basic integer operations (add, sub, mul, div)
    FLOATING_POINT,         // Floating-point operations
    VECTOR_OPS,            // SIMD/vector operations
    MATRIX_OPS,            // Matrix multiplication, etc.
    LOGIC_OPS,             // Boolean and bitwise operations
    ATOMIC_OPS,            // Atomic read-modify-write
    CUSTOM_ISA             // Custom instruction set
};

/**
 * @brief PE architecture specification
 */
struct PEArchitecture {
    std::string name;                           // PE name (e.g., "VectorPE", "MatrixPE")
    std::vector<PECapability> capabilities;     // Supported operations
    uint32_t num_registers;                     // Number of registers
    uint32_t register_width_bits;               // Register width in bits
    uint32_t vector_width;                      // SIMD width (1 = scalar)
    uint32_t pipeline_stages;                   // Pipeline depth
    double frequency_mhz;                       // Operating frequency
    double area_mm2;                            // Area in mm²
    double power_mw;                            // Average power in mW

    PEArchitecture()
        : name("GenericPE"),
          num_registers(32),
          register_width_bits(64),
          vector_width(1),
          pipeline_stages(5),
          frequency_mhz(1000.0),
          area_mm2(1.0),
          power_mw(100.0) {}
};

/**
 * @brief Abstract PE instance
 *
 * Represents a single PE instance in the system. Users can subclass this
 * to define custom PE behavior and state.
 */
class ProcessingElement {
public:
    ProcessingElement(uint32_t pe_id, const PEArchitecture& arch)
        : pe_id_(pe_id), arch_(arch), busy_(false), current_cycle_(0) {}

    virtual ~ProcessingElement() = default;

    // PE identification
    uint32_t getID() const { return pe_id_; }
    const PEArchitecture& getArchitecture() const { return arch_; }

    // PE state
    virtual bool isBusy() const { return busy_; }
    virtual void setBusy(bool busy) { busy_ = busy; }

    // Cycle tracking
    virtual Cycle getCurrentCycle() const { return current_cycle_; }
    virtual void tick(Cycle cycle) { current_cycle_ = cycle; }

    // Capability queries
    virtual bool hasCapability(PECapability cap) const {
        for (const auto& c : arch_.capabilities) {
            if (c == cap) return true;
        }
        return false;
    }

    // Operation execution (override for custom behavior)
    virtual Cycle getOperationLatency(const std::string& op_name) const {
        // Default latencies
        if (op_name == "add" || op_name == "sub") return 1;
        if (op_name == "mul") return 3;
        if (op_name == "div") return 10;
        if (op_name == "load" || op_name == "store") return 1;
        return 1;  // Default
    }

    virtual double getOperationEnergy(const std::string& op_name) const {
        // Default energy in pJ (picojoules)
        if (op_name == "add" || op_name == "sub") return 0.1;
        if (op_name == "mul") return 0.3;
        if (op_name == "div") return 1.0;
        return 0.1;  // Default
    }

protected:
    uint32_t pe_id_;
    PEArchitecture arch_;
    bool busy_;
    Cycle current_cycle_;
};

namespace plugin {

/**
 * @brief PE plugin interface
 *
 * Allows users to create custom Processing Element architectures
 */
class IPEPlugin : public IPlugin {
public:
    virtual ~IPEPlugin() = default;

    // Create PE instance
    virtual std::unique_ptr<ProcessingElement> createPE(
        uint32_t pe_id,
        PEPlacementManager* placement_manager) = 0;

    // PE architecture specification
    virtual PEArchitecture getArchitecture() const = 0;

    // PE characteristics
    virtual bool isVectorPE() const = 0;           // SIMD/vector operations
    virtual bool isMatrixPE() const = 0;           // Matrix operations
    virtual bool isCustomISA() const = 0;          // Custom instruction set
    virtual bool supportsFloatingPoint() const = 0; // FP support

    // Performance hints
    virtual double getPeakGFLOPS() const = 0;      // Peak GFLOPS
    virtual double getPeakGOPS() const = 0;        // Peak integer ops/sec
    virtual uint32_t getVectorWidth() const = 0;   // SIMD width

    // Power and area
    virtual double getAreaMM2() const = 0;         // Area in mm²
    virtual double getStaticPowerMW() const = 0;   // Static power
    virtual double getDynamicPowerMW() const = 0;  // Dynamic power at peak
};

/**
 * @brief Base class for PE plugins
 */
class PEPluginBase : public PluginBase, public IPEPlugin {
public:
    PEPluginBase(const PluginMetadata& metadata)
        : PluginBase(metadata) {
        metadata_.type = PluginType::PE_TYPE;
    }

    virtual ~PEPluginBase() = default;

    PluginType getType() const override { return PluginType::PE_TYPE; }
};

/**
 * @brief Example custom PE plugin - Simple Scalar PE
 */
class ScalarPEPlugin : public PEPluginBase {
public:
    ScalarPEPlugin();

    // IPlugin interface
    bool initialize(const std::map<std::string, std::string>& config) override;
    void shutdown() override;

    bool validateConfig(const std::map<std::string, std::string>& config,
                        std::vector<std::string>& errors) const override;

    std::vector<std::string> getRequiredParameters() const override;
    std::vector<std::string> getOptionalParameters() const override;
    std::map<std::string, std::string> getParameterDescriptions() const override;

    // IPEPlugin interface
    std::unique_ptr<ProcessingElement> createPE(
        uint32_t pe_id,
        PEPlacementManager* placement_manager) override;

    PEArchitecture getArchitecture() const override { return arch_; }

    bool isVectorPE() const override { return false; }
    bool isMatrixPE() const override { return false; }
    bool isCustomISA() const override { return false; }
    bool supportsFloatingPoint() const override { return true; }

    double getPeakGFLOPS() const override {
        return arch_.frequency_mhz / 1000.0;  // 1 FLOP per cycle
    }

    double getPeakGOPS() const override {
        return arch_.frequency_mhz / 1000.0;  // 1 OP per cycle
    }

    uint32_t getVectorWidth() const override { return 1; }

    double getAreaMM2() const override { return arch_.area_mm2; }
    double getStaticPowerMW() const override { return arch_.power_mw * 0.3; }
    double getDynamicPowerMW() const override { return arch_.power_mw * 0.7; }

private:
    PEArchitecture arch_;
};

/**
 * @brief Example custom PE plugin - Vector PE (SIMD)
 */
class VectorPEPlugin : public PEPluginBase {
public:
    VectorPEPlugin();

    // IPlugin interface
    bool initialize(const std::map<std::string, std::string>& config) override;
    void shutdown() override;

    bool validateConfig(const std::map<std::string, std::string>& config,
                        std::vector<std::string>& errors) const override;

    std::vector<std::string> getRequiredParameters() const override;
    std::vector<std::string> getOptionalParameters() const override;
    std::map<std::string, std::string> getParameterDescriptions() const override;

    // IPEPlugin interface
    std::unique_ptr<ProcessingElement> createPE(
        uint32_t pe_id,
        PEPlacementManager* placement_manager) override;

    PEArchitecture getArchitecture() const override { return arch_; }

    bool isVectorPE() const override { return true; }
    bool isMatrixPE() const override { return false; }
    bool isCustomISA() const override { return false; }
    bool supportsFloatingPoint() const override { return true; }

    double getPeakGFLOPS() const override {
        return (arch_.frequency_mhz / 1000.0) * arch_.vector_width;
    }

    double getPeakGOPS() const override {
        return (arch_.frequency_mhz / 1000.0) * arch_.vector_width;
    }

    uint32_t getVectorWidth() const override { return arch_.vector_width; }

    double getAreaMM2() const override { return arch_.area_mm2; }
    double getStaticPowerMW() const override { return arch_.power_mw * 0.3; }
    double getDynamicPowerMW() const override { return arch_.power_mw * 0.7; }

private:
    PEArchitecture arch_;
};

/**
 * @brief Example custom PE plugin - Matrix PE (for tensor operations)
 */
class MatrixPEPlugin : public PEPluginBase {
public:
    MatrixPEPlugin();

    // IPlugin interface
    bool initialize(const std::map<std::string, std::string>& config) override;
    void shutdown() override;

    bool validateConfig(const std::map<std::string, std::string>& config,
                        std::vector<std::string>& errors) const override;

    std::vector<std::string> getRequiredParameters() const override;
    std::vector<std::string> getOptionalParameters() const override;
    std::map<std::string, std::string> getParameterDescriptions() const override;

    // IPEPlugin interface
    std::unique_ptr<ProcessingElement> createPE(
        uint32_t pe_id,
        PEPlacementManager* placement_manager) override;

    PEArchitecture getArchitecture() const override { return arch_; }

    bool isVectorPE() const override { return false; }
    bool isMatrixPE() const override { return true; }
    bool isCustomISA() const override { return false; }
    bool supportsFloatingPoint() const override { return true; }

    double getPeakGFLOPS() const override {
        // Matrix unit can do matrix_size^2 ops per cycle
        return (arch_.frequency_mhz / 1000.0) * (matrix_size_ * matrix_size_);
    }

    double getPeakGOPS() const override {
        return (arch_.frequency_mhz / 1000.0) * (matrix_size_ * matrix_size_);
    }

    uint32_t getVectorWidth() const override { return matrix_size_; }

    double getAreaMM2() const override { return arch_.area_mm2; }
    double getStaticPowerMW() const override { return arch_.power_mw * 0.2; }
    double getDynamicPowerMW() const override { return arch_.power_mw * 0.8; }

private:
    PEArchitecture arch_;
    uint32_t matrix_size_;  // e.g., 8x8, 16x16
};

} // namespace plugin
} // namespace pimid

#endif // PIMID_PE_PLUGIN_H
