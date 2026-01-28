#ifndef PIMID_POWER_MODEL_H
#define PIMID_POWER_MODEL_H

#include "common/types.h"
#include <string>
#include <map>

namespace pimid {

/**
 * Component types for power modeling
 */
enum class PowerComponent {
    CORE,
    L1_CACHE,
    L2_CACHE,
    L3_CACHE,
    MEMORY_CONTROLLER,
    MEMORY,
    NETWORK_ROUTER,
    NETWORK_LINK,
    PE
};

/**
 * Power metrics for a component
 */
struct PowerMetrics {
    double dynamic_power_w;
    double leakage_power_w;
    double total_power_w;
    double total_energy_j;

    PowerMetrics() : dynamic_power_w(0.0), leakage_power_w(0.0),
                     total_power_w(0.0), total_energy_j(0.0) {}
};

/**
 * Activity statistics for power estimation
 */
struct ActivityStats {
    uint64_t total_instructions;
    uint64_t integer_instructions;
    uint64_t fp_instructions;
    uint64_t load_instructions;
    uint64_t store_instructions;
    uint64_t branch_instructions;

    uint64_t l1_reads;
    uint64_t l1_writes;
    uint64_t l1_misses;

    uint64_t l2_reads;
    uint64_t l2_writes;
    uint64_t l2_misses;

    uint64_t memory_reads;
    uint64_t memory_writes;

    Cycle total_cycles;

    ActivityStats() : total_instructions(0), integer_instructions(0),
                      fp_instructions(0), load_instructions(0),
                      store_instructions(0), branch_instructions(0),
                      l1_reads(0), l1_writes(0), l1_misses(0),
                      l2_reads(0), l2_writes(0), l2_misses(0),
                      memory_reads(0), memory_writes(0),
                      total_cycles(0) {}
};

/**
 * Technology parameters
 * NOTE: The default constructor provides placeholder values.
 * For accurate power modeling, these should be populated from your
 * system configuration (e.g., from DRAM architecture or processor specs).
 */
struct TechnologyParams {
    uint32_t tech_node_nm;      // Technology node (e.g., 45, 22, 14)
    std::string device_type;    // HP, LSTP, LOP
    double temperature_k;       // Operating temperature
    uint32_t core_count;
    double frequency_ghz;

    // Default constructor - override these with actual configuration values
    TechnologyParams() : tech_node_nm(22), device_type("HP"),
                         temperature_k(350.0), core_count(1),
                         frequency_ghz(2.0) {}
};

/**
 * Abstract power model interface
 */
class PowerModel {
public:
    PowerModel(const TechnologyParams& params);
    virtual ~PowerModel() = default;

    // Initialization
    virtual void initialize() = 0;
    virtual void loadConfig(const std::string& config_path) = 0;

    // Power estimation
    virtual PowerMetrics estimatePower(PowerComponent component,
                                       const ActivityStats& stats) = 0;
    virtual double getDynamicPower(PowerComponent component) const = 0;
    virtual double getLeakagePower(PowerComponent component) const = 0;
    virtual double getTotalPower() const = 0;

    // Energy calculation
    virtual double getEnergy(PowerComponent component) const = 0;
    virtual double getTotalEnergy() const = 0;

    // Update activity statistics
    virtual void updateActivity(PowerComponent component,
                                const ActivityStats& stats) = 0;

    // Statistics
    virtual void printStats() const = 0;
    virtual void resetStats() = 0;

    // Configuration
    const TechnologyParams& getTechnologyParams() const { return tech_params_; }

protected:
    TechnologyParams tech_params_;
    std::map<PowerComponent, PowerMetrics> component_metrics_;
    std::map<PowerComponent, ActivityStats> activity_stats_;
};

/**
 * McPAT-based power model implementation
 */
class McPATModel : public PowerModel {
public:
    explicit McPATModel(const TechnologyParams& params);
    ~McPATModel() override;

    // PowerModel interface implementation
    void initialize() override;
    void loadConfig(const std::string& config_path) override;

    PowerMetrics estimatePower(PowerComponent component,
                               const ActivityStats& stats) override;
    double getDynamicPower(PowerComponent component) const override;
    double getLeakagePower(PowerComponent component) const override;
    double getTotalPower() const override;

    double getEnergy(PowerComponent component) const override;
    double getTotalEnergy() const override;

    void updateActivity(PowerComponent component,
                        const ActivityStats& stats) override;

    void printStats() const override;
    void resetStats() override;

private:
    // McPAT interface (placeholder - will integrate actual McPAT)
    void* mcpat_instance_;

    // Component-specific configurations
    struct CoreConfig {
        uint32_t pipeline_depth;
        uint32_t issue_width;
        uint32_t alu_count;
        uint32_t mul_count;
        uint32_t fpu_count;
    };

    struct CacheConfig {
        uint64_t size_bytes;
        uint32_t line_size;
        uint32_t associativity;
        uint32_t banks;
        std::string replacement_policy;
    };

    std::map<PowerComponent, CoreConfig> core_configs_;
    std::map<PowerComponent, CacheConfig> cache_configs_;

    // Helper functions
    void initializeDefaultConfigs();
    void generateMcPATInput(PowerComponent component,
                            const ActivityStats& stats);
    PowerMetrics parseMcPATOutput();
    double calculateComponentEnergy(PowerComponent component, Cycle cycles);
    std::string generateMcPATConfigXML() const;

    // XML generation helpers
    void generateCoreXML(std::ofstream& xml, const ActivityStats& stats);
    void generateCacheXML(std::ofstream& xml, const std::string& level, const ActivityStats& stats);
    void generateMemoryControllerXML(std::ofstream& xml, const ActivityStats& stats);
    uint32_t getNumCores(PowerComponent component) const;

    PowerMetrics estimateCorePower(const ActivityStats& stats);
    PowerMetrics estimateCachePower(PowerComponent component,
                                     const ActivityStats& stats);
    PowerMetrics estimateMemoryControllerPower(const ActivityStats& stats);
    PowerMetrics estimatePEPower(const ActivityStats& stats);

    std::string componentToString(PowerComponent component) const;
};

/**
 * Composite power model that combines multiple power models
 * Integrates McPAT for processors with memory-specific models
 */
class CompositePowerModel : public PowerModel {
public:
    explicit CompositePowerModel(const TechnologyParams& params);
    ~CompositePowerModel() override = default;

    // Add specialized power models
    void addMemoryModel(std::shared_ptr<class MemoryModel> mem_model);
    void addNetworkModel(std::shared_ptr<class NetworkModel> net_model);

    // PowerModel interface implementation
    void initialize() override;
    void loadConfig(const std::string& config_path) override;

    PowerMetrics estimatePower(PowerComponent component,
                               const ActivityStats& stats) override;
    double getDynamicPower(PowerComponent component) const override;
    double getLeakagePower(PowerComponent component) const override;
    double getTotalPower() const override;

    double getEnergy(PowerComponent component) const override;
    double getTotalEnergy() const override;

    void updateActivity(PowerComponent component,
                        const ActivityStats& stats) override;

    void printStats() const override;
    void resetStats() override;

private:
    std::shared_ptr<McPATModel> mcpat_model_;
    std::shared_ptr<class MemoryModel> memory_model_;
    std::shared_ptr<class NetworkModel> network_model_;
};

} // namespace pimid

#endif // PIMID_POWER_MODEL_H
