#ifndef PIMID_PLUGIN_INTERFACE_H
#define PIMID_PLUGIN_INTERFACE_H

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <functional>

namespace pimid {
namespace plugin {

/**
 * Plugin type enumeration
 */
enum class PluginType {
    MEMORY_MODEL,
    SCHEDULER,
    NETWORK_TOPOLOGY,
    POWER_MODEL,
    PE_TYPE,
    ADDRESS_MAPPER,
    CACHE_REPLACEMENT,
    PREFETCHER,
    CUSTOM
};

/**
 * Plugin metadata
 */
struct PluginMetadata {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    PluginType type;
    std::vector<std::string> dependencies;
    std::map<std::string, std::string> parameters;

    PluginMetadata()
        : name(""), version("1.0.0"), author(""), description(""),
          type(PluginType::CUSTOM) {}
};

/**
 * Base plugin interface
 * All plugins must inherit from this class
 */
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // Plugin lifecycle
    virtual bool initialize(const std::map<std::string, std::string>& config) = 0;
    virtual void shutdown() = 0;
    virtual bool isInitialized() const = 0;

    // Plugin information
    virtual PluginMetadata getMetadata() const = 0;
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual PluginType getType() const = 0;

    // Configuration validation
    virtual bool validateConfig(const std::map<std::string, std::string>& config,
                                std::vector<std::string>& errors) const = 0;

    // Get required parameters
    virtual std::vector<std::string> getRequiredParameters() const = 0;
    virtual std::vector<std::string> getOptionalParameters() const = 0;

    // Parameter descriptions for documentation
    virtual std::map<std::string, std::string> getParameterDescriptions() const = 0;
};

/**
 * Plugin base class with common functionality
 */
class PluginBase : public IPlugin {
public:
    PluginBase(const PluginMetadata& metadata)
        : metadata_(metadata), initialized_(false) {}

    virtual ~PluginBase() = default;

    // IPlugin interface
    std::string getName() const override { return metadata_.name; }
    std::string getVersion() const override { return metadata_.version; }
    PluginType getType() const override { return metadata_.type; }
    PluginMetadata getMetadata() const override { return metadata_; }
    bool isInitialized() const override { return initialized_; }

    // Helper for validation
    bool validateConfig(const std::map<std::string, std::string>& config,
                        std::vector<std::string>& errors) const override {
        // Check required parameters
        auto required = getRequiredParameters();
        for (const auto& param : required) {
            if (config.find(param) == config.end()) {
                errors.push_back("Missing required parameter: " + param);
            }
        }
        return errors.empty();
    }

protected:
    PluginMetadata metadata_;
    bool initialized_;
    std::map<std::string, std::string> config_;

    // Helper to get config value with default
    std::string getConfigValue(const std::string& key,
                               const std::string& default_value = "") const {
        auto it = config_.find(key);
        return (it != config_.end()) ? it->second : default_value;
    }

    template<typename T>
    T getConfigValueAs(const std::string& key, const T& default_value) const;
};

/**
 * Plugin factory function signature
 */
using PluginCreateFunc = std::function<std::shared_ptr<IPlugin>()>;

/**
 * Plugin registration structure
 */
struct PluginRegistration {
    PluginMetadata metadata;
    PluginCreateFunc create_func;
    std::string library_path;  // For dynamic plugins

    PluginRegistration()
        : metadata(), create_func(nullptr), library_path("") {}
};

/**
 * Plugin registry for managing all plugins
 */
class PluginRegistry {
public:
    static PluginRegistry& getInstance();

    // Plugin registration
    bool registerPlugin(const std::string& name,
                        const PluginRegistration& registration);
    bool unregisterPlugin(const std::string& name);

    // Plugin creation
    std::shared_ptr<IPlugin> createPlugin(const std::string& name);
    std::shared_ptr<IPlugin> createPlugin(const std::string& name,
                                          const std::map<std::string, std::string>& config);

    // Plugin queries
    bool hasPlugin(const std::string& name) const;
    std::vector<std::string> getPluginNames() const;
    std::vector<std::string> getPluginsByType(PluginType type) const;
    PluginMetadata getPluginMetadata(const std::string& name) const;

    // Plugin discovery
    void discoverPlugins(const std::string& plugin_dir);
    void loadDynamicPlugin(const std::string& library_path);

    // Documentation generation
    std::string generatePluginDocumentation(const std::string& name) const;
    std::string generateAllPluginsDocs() const;

private:
    PluginRegistry() = default;
    ~PluginRegistry() = default;
    PluginRegistry(const PluginRegistry&) = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;

    std::map<std::string, PluginRegistration> plugins_;
    std::map<std::string, void*> loaded_libraries_;  // For dynamic plugins
};

/**
 * Helper macro for plugin registration
 */
#define PIMID_REGISTER_PLUGIN(PluginClass, PluginName)                  \
    namespace {                                                         \
        struct PluginClass##Registrar {                                \
            PluginClass##Registrar() {                                 \
                PluginRegistration reg;                                \
                auto instance = std::make_shared<PluginClass>();       \
                reg.metadata = instance->getMetadata();                \
                reg.create_func = []() -> std::shared_ptr<IPlugin> {   \
                    return std::make_shared<PluginClass>();            \
                };                                                      \
                PluginRegistry::getInstance().registerPlugin(          \
                    PluginName, reg);                                  \
            }                                                           \
        };                                                              \
        static PluginClass##Registrar g_##PluginClass##_registrar;    \
    }

/**
 * Plugin configuration helper
 */
class PluginConfig {
public:
    PluginConfig(const std::string& plugin_name);

    // Set parameters
    PluginConfig& setParameter(const std::string& key, const std::string& value);
    PluginConfig& setParameters(const std::map<std::string, std::string>& params);

    // Validation
    bool validate(std::vector<std::string>& errors) const;

    // Get configuration
    const std::map<std::string, std::string>& getParameters() const {
        return parameters_;
    }

    // Create plugin with this configuration
    std::shared_ptr<IPlugin> create() const;

private:
    std::string plugin_name_;
    std::map<std::string, std::string> parameters_;
};

} // namespace plugin
} // namespace pimid

#endif // PIMID_PLUGIN_INTERFACE_H
