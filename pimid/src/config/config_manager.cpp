#include "config/config_manager.h"
#include "config/config_parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

namespace pimid {
namespace config {

// Singleton instance
ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager()
    : is_valid_(false), enable_env_expansion_(true) {
    loadDefaults();
}

void ConfigManager::loadDefaults() {
    // Set reasonable defaults for critical parameters
    config_["simulation.mode"] = "co-simulation";
    config_["simulation.max_cycles"] = "0";
    config_["simulation.warmup_cycles"] = "100000";
    config_["simulation.log_level"] = "INFO";

    // Memory defaults
    config_["memory.technology"] = "DRAM";
    config_["memory.dram_type"] = "DDR4";
    config_["memory.capacity"] = "16GB";
    config_["memory.channels"] = "4";

    // PIM defaults
    config_["pim.pe_placement_level"] = "BANK";
    config_["pim.num_pes_per_level"] = "1";
    config_["pim.pe_frequency_mhz"] = "1000";
    config_["pim.scheduler"] = "NEAREST_PE";

    // Cache defaults
    config_["caches.l1i.size_kb"] = "32";
    config_["caches.l1d.size_kb"] = "32";
    config_["caches.l2.size_kb"] = "256";
    config_["caches.l3.size_kb"] = "8192";

    // Memory hierarchy defaults
    config_["memory_hierarchy.subarray.size_mb"] = "2";
    config_["memory_hierarchy.bank.size_mb"] = "32";
    config_["memory_hierarchy.chip.size_gb"] = "2";
    config_["memory_hierarchy.rank.size_gb"] = "16";

    // Offload defaults
    config_["offload.completion_overhead_cycles"] = "500";
    config_["offload.offload_overhead_cycles"] = "1000";

    // Network defaults
    config_["network.enabled"] = "true";
    config_["network.topology"] = "MESH_2D";
    config_["network.num_rows"] = "4";
    config_["network.num_cols"] = "4";
}

bool ConfigManager::initialize(const std::string& config_file) {
    config_file_path_ = config_file;
    return loadConfiguration(config_file);
}

bool ConfigManager::loadConfiguration(const std::string& config_file) {
    ConfigParser parser;

    // Parse main config file
    if (!parser.parseFile(config_file, config_)) {
        std::cerr << "Failed to load configuration file: " << config_file << std::endl;
        return false;
    }

    config_file_path_ = config_file;

    // Load component configs if specified
    std::string host_config = get("components.host_config");
    if (!host_config.empty()) {
        std::map<std::string, std::string> host_cfg;
        if (parser.parseFile(host_config, host_cfg)) {
            // Merge host config
            for (const auto& kv : host_cfg) {
                config_["host." + kv.first] = kv.second;
            }
        }
    }

    std::string device_config = get("components.device_config");
    if (!device_config.empty()) {
        std::map<std::string, std::string> device_cfg;
        if (parser.parseFile(device_config, device_cfg)) {
            // Merge device config
            for (const auto& kv : device_cfg) {
                config_["device." + kv.first] = kv.second;
            }
        }
    }

    std::string memory_config = get("components.memory_config");
    if (!memory_config.empty()) {
        std::map<std::string, std::string> memory_cfg;
        if (parser.parseFile(memory_config, memory_cfg)) {
            // Merge memory config
            for (const auto& kv : memory_cfg) {
                config_["memory." + kv.first] = kv.second;
            }
        }
    }

    // Expand environment variables if enabled
    if (enable_env_expansion_) {
        for (auto& kv : config_) {
            kv.second = expandEnvironmentVariables(kv.second);
        }
    }

    is_valid_ = true;
    return true;
}

bool ConfigManager::reloadConfiguration() {
    if (config_file_path_.empty()) {
        return false;
    }
    return loadConfiguration(config_file_path_);
}

std::string ConfigManager::get(const std::string& key, const std::string& default_val) const {
    auto it = config_.find(key);
    if (it != config_.end()) {
        return it->second;
    }
    return default_val;
}

int64_t ConfigManager::getInt(const std::string& key, int64_t default_val) const {
    std::string val = get(key, "");
    if (val.empty()) {
        return default_val;
    }

    try {
        return std::stoll(val);
    } catch (...) {
        std::cerr << "Warning: Failed to parse int value for key '" << key
                  << "': '" << val << "'" << std::endl;
        return default_val;
    }
}

double ConfigManager::getFloat(const std::string& key, double default_val) const {
    std::string val = get(key, "");
    if (val.empty()) {
        return default_val;
    }

    try {
        return std::stod(val);
    } catch (...) {
        std::cerr << "Warning: Failed to parse float value for key '" << key
                  << "': '" << val << "'" << std::endl;
        return default_val;
    }
}

bool ConfigManager::getBool(const std::string& key, bool default_val) const {
    std::string val = get(key, "");
    if (val.empty()) {
        return default_val;
    }

    // Convert to lowercase for comparison
    std::string lower_val = val;
    for (char& c : lower_val) {
        c = std::tolower(c);
    }

    if (lower_val == "true" || lower_val == "1" || lower_val == "yes" || lower_val == "on") {
        return true;
    } else if (lower_val == "false" || lower_val == "0" || lower_val == "no" || lower_val == "off") {
        return false;
    }

    std::cerr << "Warning: Invalid boolean value for key '" << key
              << "': '" << val << "'" << std::endl;
    return default_val;
}

std::vector<std::string> ConfigManager::getList(const std::string& key) const {
    std::string val = get(key, "");
    if (val.empty()) {
        return {};
    }
    return ConfigParser::parseList(val);
}

void ConfigManager::set(const std::string& key, const std::string& value) {
    std::string old_value = get(key, "");
    config_[key] = value;
    notifyListeners(key, old_value, value);
}

void ConfigManager::set(const std::string& key, int64_t value) {
    set(key, std::to_string(value));
}

void ConfigManager::set(const std::string& key, double value) {
    set(key, std::to_string(value));
}

void ConfigManager::set(const std::string& key, bool value) {
    set(key, value ? "true" : "false");
}

std::map<std::string, std::string> ConfigManager::getSection(const std::string& section) const {
    std::map<std::string, std::string> result;
    std::string prefix = section + ".";

    for (const auto& kv : config_) {
        if (kv.first.find(prefix) == 0) {
            std::string sub_key = kv.first.substr(prefix.length());
            result[sub_key] = kv.second;
        }
    }

    return result;
}

bool ConfigManager::hasSection(const std::string& section) const {
    std::string prefix = section + ".";
    for (const auto& kv : config_) {
        if (kv.first.find(prefix) == 0) {
            return true;
        }
    }
    return false;
}

ValidationResult ConfigManager::validate() {
    ValidationResult result;
    result.valid = true;
    // TODO: Implement full validation using ConfigValidator
    return result;
}

bool ConfigManager::saveToFile(const std::string& filename) {
    std::string yaml_content = toYAML();
    std::ofstream out(filename);
    if (!out) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return false;
    }
    out << yaml_content;
    return true;
}

std::string ConfigManager::toYAML() const {
    return ConfigParser::toYAML(config_);
}

std::string ConfigManager::toJSON() const {
    return ConfigParser::toJSON(config_);
}

void ConfigManager::addChangeListener(std::shared_ptr<IConfigChangeListener> listener) {
    listeners_.push_back(listener);
}

void ConfigManager::removeChangeListener(std::shared_ptr<IConfigChangeListener> listener) {
    listeners_.erase(
        std::remove(listeners_.begin(), listeners_.end(), listener),
        listeners_.end()
    );
}

void ConfigManager::notifyListeners(const std::string& key,
                                     const std::string& old_value,
                                     const std::string& new_value) {
    for (auto& listener : listeners_) {
        listener->onConfigChanged(key, old_value, new_value);
    }
}

std::string ConfigManager::expandEnvironmentVariables(const std::string& value) const {
    std::string result = value;
    size_t pos = 0;

    while ((pos = result.find("${", pos)) != std::string::npos) {
        size_t end_pos = result.find("}", pos);
        if (end_pos == std::string::npos) {
            break;
        }

        std::string var_name = result.substr(pos + 2, end_pos - pos - 2);
        const char* env_val = std::getenv(var_name.c_str());

        if (env_val) {
            result.replace(pos, end_pos - pos + 1, env_val);
            pos += strlen(env_val);
        } else {
            pos = end_pos + 1;
        }
    }

    return result;
}

std::map<std::string, std::pair<std::string, std::string>>
ConfigManager::compareWith(const ConfigManager& other) const {
    std::map<std::string, std::pair<std::string, std::string>> diff;

    // Find keys that differ
    for (const auto& kv : config_) {
        std::string other_val = other.get(kv.first, "");
        if (other_val != kv.second) {
            diff[kv.first] = {kv.second, other_val};
        }
    }

    // Find keys only in other
    for (const auto& kv : other.config_) {
        if (config_.find(kv.first) == config_.end()) {
            diff[kv.first] = {"", kv.second};
        }
    }

    return diff;
}

std::string ConfigManager::generateDiffReport(const ConfigManager& other) const {
    auto diff = compareWith(other);
    std::ostringstream oss;

    oss << "Configuration Differences:\n";
    oss << "=========================\n\n";

    for (const auto& kv : diff) {
        oss << "Key: " << kv.first << "\n";
        oss << "  This:  " << kv.second.first << "\n";
        oss << "  Other: " << kv.second.second << "\n\n";
    }

    return oss.str();
}

// Plugin configuration
bool ConfigManager::loadPlugin(const std::string& plugin_name) {
    // TODO: Implement plugin loading
    std::cerr << "Plugin loading not yet implemented: " << plugin_name << std::endl;
    return false;
}

bool ConfigManager::configurePlugin(const std::string& plugin_name,
                                     const std::map<std::string, std::string>& config) {
    plugin_configs_[plugin_name] = config;
    return true;
}

std::vector<std::string> ConfigManager::getLoadedPlugins() const {
    std::vector<std::string> plugins;
    for (const auto& kv : plugin_configs_) {
        plugins.push_back(kv.first);
    }
    return plugins;
}

std::map<std::string, std::string> ConfigManager::getPluginConfig(
    const std::string& plugin_name) const {
    auto it = plugin_configs_.find(plugin_name);
    if (it != plugin_configs_.end()) {
        return it->second;
    }
    return {};
}

// Configuration presets (stubs for now)
void ConfigManager::loadPreset(const std::string& preset_name) {
    std::cerr << "Preset loading not yet implemented: " << preset_name << std::endl;
}

std::vector<std::string> ConfigManager::getAvailablePresets() const {
    return {"high_performance", "low_power", "balanced", "debug"};
}

void ConfigManager::saveAsPreset(const std::string& preset_name) {
    std::cerr << "Preset saving not yet implemented: " << preset_name << std::endl;
}

bool ConfigManager::importFromJSON(const std::string& json_str) {
    // TODO: Implement JSON import
    std::cerr << "JSON import not yet implemented" << std::endl;
    return false;
}

bool ConfigManager::importFromXML(const std::string& xml_str) {
    // TODO: Implement XML import
    std::cerr << "XML import not yet implemented" << std::endl;
    return false;
}

std::string ConfigManager::exportToXML() const {
    // TODO: Implement XML export
    std::cerr << "XML export not yet implemented" << std::endl;
    return "";
}

} // namespace config
} // namespace pimid
