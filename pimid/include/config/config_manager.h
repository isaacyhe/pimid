#ifndef PIMID_CONFIG_MANAGER_H
#define PIMID_CONFIG_MANAGER_H

#include "config/config_schema.h"
#include "config/config_validator.h"
#include "plugin/plugin_interface.h"
#include <string>
#include <map>
#include <memory>
#include <functional>

namespace pimid {
namespace config {

/**
 * Configuration change listener
 */
class IConfigChangeListener {
public:
    virtual ~IConfigChangeListener() = default;
    virtual void onConfigChanged(const std::string& key,
                                 const std::string& old_value,
                                 const std::string& new_value) = 0;
};

/**
 * Central configuration manager
 * Manages all simulator configuration with validation, defaults, and plugins
 */
class ConfigManager {
public:
    static ConfigManager& getInstance();

    // Initialization
    bool initialize(const std::string& config_file);
    bool loadConfiguration(const std::string& config_file);
    bool reloadConfiguration();

    // Configuration access
    std::string get(const std::string& key, const std::string& default_val = "") const;
    int64_t getInt(const std::string& key, int64_t default_val = 0) const;
    double getFloat(const std::string& key, double default_val = 0.0) const;
    bool getBool(const std::string& key, bool default_val = false) const;
    std::vector<std::string> getList(const std::string& key) const;

    // Configuration modification
    void set(const std::string& key, const std::string& value);
    void set(const std::string& key, int64_t value);
    void set(const std::string& key, double value);
    void set(const std::string& key, bool value);

    // Section access
    std::map<std::string, std::string> getSection(const std::string& section) const;
    bool hasSection(const std::string& section) const;

    // Validation
    ValidationResult validate();
    bool isValid() const { return is_valid_; }

    // Export
    bool saveToFile(const std::string& filename);
    std::string toYAML() const;
    std::string toJSON() const;

    // Schema access
    const ConfigSchema& getSchema() const { return schema_; }

    // Plugin configuration
    bool loadPlugin(const std::string& plugin_name);
    bool configurePlugin(const std::string& plugin_name,
                         const std::map<std::string, std::string>& config);
    std::vector<std::string> getLoadedPlugins() const;
    std::map<std::string, std::string> getPluginConfig(
        const std::string& plugin_name) const;

    // Change notification
    void addChangeListener(std::shared_ptr<IConfigChangeListener> listener);
    void removeChangeListener(std::shared_ptr<IConfigChangeListener> listener);

    // Configuration presets
    void loadPreset(const std::string& preset_name);
    std::vector<std::string> getAvailablePresets() const;
    void saveAsPreset(const std::string& preset_name);

    // Configuration comparison
    std::map<std::string, std::pair<std::string, std::string>>
    compareWith(const ConfigManager& other) const;
    std::string generateDiffReport(const ConfigManager& other) const;

    // Environment variable expansion
    void setEnableEnvExpansion(bool enable) { enable_env_expansion_ = enable; }
    bool getEnableEnvExpansion() const { return enable_env_expansion_; }

    // Configuration import/export
    bool importFromJSON(const std::string& json_str);
    bool importFromXML(const std::string& xml_str);
    std::string exportToXML() const;

private:
    ConfigManager();
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    ConfigSchema schema_;
    std::map<std::string, std::string> config_;
    std::string config_file_path_;
    bool is_valid_;
    bool enable_env_expansion_;

    std::vector<std::shared_ptr<IConfigChangeListener>> listeners_;
    std::map<std::string, std::map<std::string, std::string>> plugin_configs_;

    void notifyListeners(const std::string& key,
                         const std::string& old_value,
                         const std::string& new_value);
    std::string expandEnvironmentVariables(const std::string& value) const;
    void loadDefaults();
};

/**
 * Configuration wizard for interactive setup
 */
class ConfigWizard {
public:
    ConfigWizard(const ConfigSchema& schema);

    // Interactive configuration
    std::map<std::string, std::string> runInteractive();
    std::map<std::string, std::string> runInteractiveSection(
        const std::string& section_name);

    // Guided setup for common scenarios
    std::map<std::string, std::string> setupForHighPerformance();
    std::map<std::string, std::string> setupForLowPower();
    std::map<std::string, std::string> setupForAccuracy();
    std::map<std::string, std::string> setupForCustomWorkload(
        const std::string& workload_type);

    // Quick setup
    std::map<std::string, std::string> quickSetup();

    // Save wizard result
    bool saveConfiguration(const std::string& filename);

private:
    ConfigSchema schema_;
    std::map<std::string, std::string> config_;

    std::string promptUser(const std::string& question,
                           const std::string& default_val);
    std::string promptChoice(const std::string& question,
                            const std::vector<std::string>& choices,
                            const std::string& default_choice);
    bool promptYesNo(const std::string& question, bool default_val);
    int64_t promptInteger(const std::string& question,
                          int64_t default_val,
                          std::optional<int64_t> min,
                          std::optional<int64_t> max);
};

/**
 * Configuration presets
 */
class ConfigPresets {
public:
    // Predefined configurations for common use cases
    static std::map<std::string, std::string> getHighPerformancePreset();
    static std::map<std::string, std::string> getLowPowerPreset();
    static std::map<std::string, std::string> getBalancedPreset();
    static std::map<std::string, std::string> getDebugPreset();

    // Memory technology presets
    static std::map<std::string, std::string> getDRAMPreset();
    static std::map<std::string, std::string> getSRAMPreset();
    static std::map<std::string, std::string> getSTTMRAMPreset();
    static std::map<std::string, std::string> getHBMPreset();

    // PE placement presets
    static std::map<std::string, std::string> getSubarrayPEPreset();
    static std::map<std::string, std::string> getBankPEPreset();
    static std::map<std::string, std::string> getChipPEPreset();
    static std::map<std::string, std::string> getRankPEPreset();
    static std::map<std::string, std::string> getLogicDiePEPreset();

    // Network topology presets
    static std::map<std::string, std::string> getMesh2DPreset();
    static std::map<std::string, std::string> getTorus2DPreset();
    static std::map<std::string, std::string> getCrossbarPreset();
    static std::map<std::string, std::string> getDragonflyPreset();

    // Application-specific presets
    static std::map<std::string, std::string> getMLWorkloadPreset();
    static std::map<std::string, std::string> getGraphWorkloadPreset();
    static std::map<std::string, std::string> getStreamingWorkloadPreset();
    static std::map<std::string, std::string> getRandomAccessWorkloadPreset();

    // Get all available presets
    static std::map<std::string, std::map<std::string, std::string>>
    getAllPresets();
};

/**
 * Configuration documentation generator
 */
class ConfigDocGenerator {
public:
    ConfigDocGenerator(const ConfigSchema& schema);

    // Generate different documentation formats
    std::string generateQuickReference();
    std::string generateFullDocumentation();
    std::string generateParameterReference();
    std::string generateExampleConfigurations();
    std::string generateTroubleshootingGuide();

    // Generate for specific sections
    std::string generateSectionDocs(const std::string& section_name);

    // Generate comparison tables
    std::string generatePresetComparison();
    std::string generateTechnologyComparison();

private:
    ConfigSchema schema_;

    std::string formatParameter(const ParameterSchema& param);
    std::string formatSection(const SectionSchema& section);
};

} // namespace config
} // namespace pimid

#endif // PIMID_CONFIG_MANAGER_H
