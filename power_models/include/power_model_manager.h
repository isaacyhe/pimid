#ifndef PIMID_POWER_MODEL_MANAGER_H
#define PIMID_POWER_MODEL_MANAGER_H

#include "power_model.h"
#include "mcpat_wrapper.h"
#include <memory>
#include <map>
#include <functional>

namespace pimid {

// Forward declarations
class RamulatorWrapper;
class GarnetModel;

/**
 * Power model source - where the power estimate came from
 */
enum class PowerModelSource {
    SPECIALIZED_SIMULATOR,  // From specialized simulator (Ramulator, GARNET, etc.)
    MCPAT,                  // From McPAT power model
    ANALYTICAL,             // From analytical model (fallback)
    UNKNOWN
};

/**
 * Power estimate result with source tracking
 */
struct PowerEstimate {
    PowerMetrics metrics;
    PowerModelSource source;
    std::string source_name;
    bool is_valid;

    PowerEstimate()
        : source(PowerModelSource::UNKNOWN)
        , source_name("unknown")
        , is_valid(false) {}

    PowerEstimate(const PowerMetrics& m, PowerModelSource s, const std::string& name)
        : metrics(m), source(s), source_name(name), is_valid(true) {}
};

/**
 * Hierarchical Power Model Manager
 *
 * Implements priority-based power modeling:
 * 1. Try specialized simulator (if available for component)
 * 2. Fall back to McPAT (guaranteed to work for all components)
 * 3. Final fallback to analytical models
 *
 * Usage:
 *   PowerModelManager mgr(tech_params);
 *   mgr.setRamulatorModel(ramulator);  // Register specialized models
 *   mgr.setGarnetModel(garnet);
 *   mgr.initialize();
 *
 *   // Will use Ramulator for memory, McPAT for core
 *   auto mem_power = mgr.getPower(PowerComponent::MEMORY, stats);
 *   auto core_power = mgr.getPower(PowerComponent::CORE, stats);
 */
class PowerModelManager {
public:
    explicit PowerModelManager(const TechnologyParams& params);
    ~PowerModelManager();

    // Initialization
    void initialize();
    void loadConfig(const std::string& config_path);

    //=========================================================================
    // Register Specialized Models
    //=========================================================================

    /**
     * Register Ramulator memory model for MEMORY component
     * When set, MEMORY power will come from Ramulator, not McPAT
     */
    void setRamulatorModel(std::shared_ptr<RamulatorWrapper> ramulator);

    /**
     * Register GARNET network model for NETWORK_ROUTER and NETWORK_LINK
     * When set, network power will come from GARNET, not McPAT
     */
    void setGarnetModel(std::shared_ptr<GarnetModel> garnet);

    /**
     * Register custom power model for any component
     * Useful for future extensions (e.g., custom PE models)
     */
    using CustomPowerFunc = std::function<PowerEstimate(const ActivityStats&)>;
    void registerCustomModel(PowerComponent component,
                             CustomPowerFunc func,
                             const std::string& name);

    //=========================================================================
    // Power Estimation (with automatic fallback)
    //=========================================================================

    /**
     * Get power estimate with automatic fallback hierarchy:
     * 1. Try specialized model (if registered)
     * 2. Try McPAT (if available)
     * 3. Fall back to analytical model
     *
     * Returns PowerEstimate with source tracking
     */
    PowerEstimate getPower(PowerComponent component,
                          const ActivityStats& stats);

    /**
     * Update activity and recompute power
     */
    void updateActivity(PowerComponent component,
                       const ActivityStats& stats);

    //=========================================================================
    // Query Functions
    //=========================================================================

    /**
     * Check if specialized model is available for component
     */
    bool hasSpecializedModel(PowerComponent component) const;

    /**
     * Check if McPAT is available
     */
    bool hasMcPAT() const { return mcpat_wrapper_ != nullptr; }

    /**
     * Get power source for a component (what would be used)
     */
    PowerModelSource getPowerSource(PowerComponent component) const;

    /**
     * Get total system power (from all components)
     */
    double getTotalPower() const;

    /**
     * Get total system energy
     */
    double getTotalEnergy() const;

    /**
     * Get power breakdown by source
     */
    struct PowerBreakdown {
        double specialized_power_w;
        double mcpat_power_w;
        double analytical_power_w;
        double total_power_w;

        std::map<std::string, double> source_breakdown;
    };
    PowerBreakdown getPowerBreakdown() const;

    //=========================================================================
    // Statistics and Debugging
    //=========================================================================

    /**
     * Print detailed power statistics with sources
     */
    void printStats() const;

    /**
     * Print power model coverage report
     */
    void printCoverageReport() const;

    /**
     * Reset all statistics
     */
    void resetStats();

    /**
     * Enable/disable McPAT fallback (default: enabled)
     */
    void setMcPATFallbackEnabled(bool enabled) { mcpat_fallback_enabled_ = enabled; }

    /**
     * Enable/disable analytical fallback (default: enabled)
     */
    void setAnalyticalFallbackEnabled(bool enabled) { analytical_fallback_enabled_ = enabled; }

    // Configuration access
    const TechnologyParams& getTechnologyParams() const { return tech_params_; }

private:
    //=========================================================================
    // Internal Helper Functions
    //=========================================================================

    /**
     * Try specialized model for component
     */
    PowerEstimate trySpecializedModel(PowerComponent component,
                                      const ActivityStats& stats);

    /**
     * Try McPAT for component
     */
    PowerEstimate tryMcPAT(PowerComponent component,
                          const ActivityStats& stats);

    /**
     * Analytical fallback (guaranteed to work)
     */
    PowerEstimate analyticalFallback(PowerComponent component,
                                     const ActivityStats& stats);

    /**
     * Extract power from Ramulator
     */
    PowerEstimate extractRamulatorPower(const ActivityStats& stats);

    /**
     * Extract power from GARNET (router or link)
     */
    PowerEstimate extractGarnetPower(PowerComponent component,
                                     const ActivityStats& stats);

    /**
     * Convert activity stats to McPAT format
     */
    void populateMcPATStats(const ActivityStats& stats);

    /**
     * Map PowerComponent to McPAT ComponentType
     */
    McPATWrapper::ComponentType toMcPATComponent(PowerComponent component) const;

    /**
     * Component name for logging
     */
    std::string componentName(PowerComponent component) const;

    //=========================================================================
    // Member Variables
    //=========================================================================

    TechnologyParams tech_params_;

    // McPAT wrapper (guaranteed fallback)
    std::shared_ptr<McPATWrapper> mcpat_wrapper_;
    McPATWrapper::SystemConfig mcpat_config_;

    // Specialized models (optional, take priority)
    std::shared_ptr<RamulatorWrapper> ramulator_model_;
    std::shared_ptr<GarnetModel> garnet_model_;

    // Custom power functions
    std::map<PowerComponent, CustomPowerFunc> custom_models_;
    std::map<PowerComponent, std::string> custom_model_names_;

    // Cached power estimates
    std::map<PowerComponent, PowerEstimate> cached_power_;
    std::map<PowerComponent, ActivityStats> activity_stats_;

    // Configuration
    bool mcpat_fallback_enabled_;
    bool analytical_fallback_enabled_;
    bool initialized_;

    // Statistics
    mutable std::map<PowerComponent, PowerModelSource> usage_stats_;
};

} // namespace pimid

#endif // PIMID_POWER_MODEL_MANAGER_H
