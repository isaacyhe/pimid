#ifndef PIMID_SCHEDULER_PLUGIN_H
#define PIMID_SCHEDULER_PLUGIN_H

#include "plugin/plugin_interface.h"
#include "scheduler/scheduler.h"
#include <memory>

namespace pimid {
namespace plugin {

/**
 * Scheduler plugin interface
 * Allows users to create custom PE scheduling algorithms
 */
class ISchedulerPlugin : public IPlugin {
public:
    virtual ~ISchedulerPlugin() = default;

    // Create scheduler instance
    virtual std::unique_ptr<PEScheduler> createScheduler(
        PEPlacementManager* pe_manager) = 0;

    // Scheduler characteristics
    virtual bool isDataAware() const = 0;        // Considers data locality
    virtual bool isLoadAware() const = 0;        // Considers PE load
    virtual bool isPriorityBased() const = 0;    // Supports task priorities
    virtual bool isDynamic() const = 0;          // Can adapt at runtime

    // Performance hints
    virtual double getTypicalOverhead() const = 0;  // cycles per decision
    virtual bool isOptimalForWorkload(const std::string& workload_type) const = 0;
};

/**
 * Base class for scheduler plugins
 */
class SchedulerPluginBase : public PluginBase, public ISchedulerPlugin {
public:
    SchedulerPluginBase(const PluginMetadata& metadata)
        : PluginBase(metadata) {
        metadata_.type = PluginType::SCHEDULER;
    }

    virtual ~SchedulerPluginBase() = default;

    PluginType getType() const override { return PluginType::SCHEDULER; }
};

/**
 * Example custom scheduler plugin
 * Template for users to create their own schedulers
 */
class CustomSchedulerPlugin : public SchedulerPluginBase {
public:
    CustomSchedulerPlugin();

    // IPlugin interface
    bool initialize(const std::map<std::string, std::string>& config) override;
    void shutdown() override;

    bool validateConfig(const std::map<std::string, std::string>& config,
                        std::vector<std::string>& errors) const override;

    std::vector<std::string> getRequiredParameters() const override;
    std::vector<std::string> getOptionalParameters() const override;
    std::map<std::string, std::string> getParameterDescriptions() const override;

    // ISchedulerPlugin interface
    std::unique_ptr<PEScheduler> createScheduler(
        PEPlacementManager* pe_manager) override;

    bool isDataAware() const override { return true; }
    bool isLoadAware() const override { return true; }
    bool isPriorityBased() const override { return false; }
    bool isDynamic() const override { return true; }

    double getTypicalOverhead() const override { return 10.0; }  // 10 cycles
    bool isOptimalForWorkload(const std::string& workload_type) const override;
};

/**
 * Data-locality-aware scheduler plugin
 */
class DataLocalitySchedulerPlugin : public SchedulerPluginBase {
public:
    DataLocalitySchedulerPlugin();

    bool initialize(const std::map<std::string, std::string>& config) override;
    void shutdown() override;

    bool validateConfig(const std::map<std::string, std::string>& config,
                        std::vector<std::string>& errors) const override;

    std::vector<std::string> getRequiredParameters() const override;
    std::vector<std::string> getOptionalParameters() const override;
    std::map<std::string, std::string> getParameterDescriptions() const override;

    std::unique_ptr<PEScheduler> createScheduler(
        PEPlacementManager* pe_manager) override;

    bool isDataAware() const override { return true; }
    bool isLoadAware() const override { return false; }
    bool isPriorityBased() const override { return false; }
    bool isDynamic() const override { return false; }

    double getTypicalOverhead() const override { return 5.0; }
    bool isOptimalForWorkload(const std::string& workload_type) const override;

private:
    uint32_t cache_line_size_;
    bool consider_bank_conflicts_;
};

/**
 * Energy-aware scheduler plugin
 */
class EnergyAwareSchedulerPlugin : public SchedulerPluginBase {
public:
    EnergyAwareSchedulerPlugin();

    bool initialize(const std::map<std::string, std::string>& config) override;
    void shutdown() override;

    bool validateConfig(const std::map<std::string, std::string>& config,
                        std::vector<std::string>& errors) const override;

    std::vector<std::string> getRequiredParameters() const override;
    std::vector<std::string> getOptionalParameters() const override;
    std::map<std::string, std::string> getParameterDescriptions() const override;

    std::unique_ptr<PEScheduler> createScheduler(
        PEPlacementManager* pe_manager) override;

    bool isDataAware() const override { return true; }
    bool isLoadAware() const override { return true; }
    bool isPriorityBased() const override { return false; }
    bool isDynamic() const override { return true; }

    double getTypicalOverhead() const override { return 15.0; }
    bool isOptimalForWorkload(const std::string& workload_type) const override;

private:
    double energy_weight_;
    double performance_weight_;
    bool prefer_nearby_pes_;
};

} // namespace plugin
} // namespace pimid

#endif // PIMID_SCHEDULER_PLUGIN_H
