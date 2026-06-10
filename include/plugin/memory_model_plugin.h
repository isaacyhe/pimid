#ifndef PIMID_MEMORY_MODEL_PLUGIN_H
#define PIMID_MEMORY_MODEL_PLUGIN_H

#include "plugin/plugin_interface.h"
#include "common/types.h"
#include <memory>

namespace pimid {

// Forward declaration
class MemoryModel;

namespace plugin {

/**
 * Memory model plugin interface
 * Allows users to create custom memory models
 */
class IMemoryModelPlugin : public IPlugin {
public:
    virtual ~IMemoryModelPlugin() = default;

    // Create memory model instance
    virtual std::shared_ptr<MemoryModel> createMemoryModel(
        const std::string& config_path) = 0;

    // Memory model capabilities
    virtual bool supportsReadWrite() const = 0;
    virtual bool supportsAtomic() const = 0;
    virtual bool supportsPowerModeling() const = 0;
    virtual bool supportsTimingVariation() const = 0;

    // Performance characteristics
    virtual Cycle getTypicalReadLatency() const = 0;
    virtual Cycle getTypicalWriteLatency() const = 0;
    virtual uint64_t getMaxBandwidth() const = 0;  // bytes/second

    // Technology information
    virtual std::string getTechnologyName() const = 0;
    virtual uint32_t getTechnologyNode() const = 0;  // nm
};

/**
 * Base class for memory model plugins
 */
class MemoryModelPluginBase : public PluginBase, public IMemoryModelPlugin {
public:
    MemoryModelPluginBase(const PluginMetadata& metadata)
        : PluginBase(metadata) {
        metadata_.type = PluginType::MEMORY_MODEL;
    }

    virtual ~MemoryModelPluginBase() = default;

    PluginType getType() const override { return PluginType::MEMORY_MODEL; }
};

/**
 * Example custom memory model plugin template
 * Users can copy this to create their own memory models
 */
class CustomMemoryModelPlugin : public MemoryModelPluginBase {
public:
    CustomMemoryModelPlugin();

    // IPlugin interface
    bool initialize(const std::map<std::string, std::string>& config) override;
    void shutdown() override;

    bool validateConfig(const std::map<std::string, std::string>& config,
                        std::vector<std::string>& errors) const override;

    std::vector<std::string> getRequiredParameters() const override;
    std::vector<std::string> getOptionalParameters() const override;
    std::map<std::string, std::string> getParameterDescriptions() const override;

    // IMemoryModelPlugin interface
    std::shared_ptr<MemoryModel> createMemoryModel(
        const std::string& config_path) override;

    bool supportsReadWrite() const override { return true; }
    bool supportsAtomic() const override { return false; }
    bool supportsPowerModeling() const override { return true; }
    bool supportsTimingVariation() const override { return false; }

    Cycle getTypicalReadLatency() const override;
    Cycle getTypicalWriteLatency() const override;
    uint64_t getMaxBandwidth() const override;

    std::string getTechnologyName() const override;
    uint32_t getTechnologyNode() const override;

private:
    Cycle read_latency_;
    Cycle write_latency_;
    uint64_t bandwidth_;
    std::string tech_name_;
    uint32_t tech_node_;
};

} // namespace plugin
} // namespace pimid

#endif // PIMID_MEMORY_MODEL_PLUGIN_H
