#include "plugin/plugin_interface.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <dlfcn.h>  // For dynamic library loading

namespace pimid {
namespace plugin {

//=============================================================================
// PluginBase Implementation
//=============================================================================

template<>
int64_t PluginBase::getConfigValueAs<int64_t>(const std::string& key,
                                                const int64_t& default_value) const {
    auto it = config_.find(key);
    if (it == config_.end()) {
        return default_value;
    }
    try {
        return std::stoll(it->second);
    } catch (...) {
        return default_value;
    }
}

template<>
double PluginBase::getConfigValueAs<double>(const std::string& key,
                                             const double& default_value) const {
    auto it = config_.find(key);
    if (it == config_.end()) {
        return default_value;
    }
    try {
        return std::stod(it->second);
    } catch (...) {
        return default_value;
    }
}

template<>
bool PluginBase::getConfigValueAs<bool>(const std::string& key,
                                         const bool& default_value) const {
    auto it = config_.find(key);
    if (it == config_.end()) {
        return default_value;
    }
    const std::string& val = it->second;
    if (val == "true" || val == "1" || val == "yes" || val == "on") {
        return true;
    } else if (val == "false" || val == "0" || val == "no" || val == "off") {
        return false;
    }
    return default_value;
}

//=============================================================================
// PluginRegistry Implementation
//=============================================================================

PluginRegistry& PluginRegistry::getInstance() {
    static PluginRegistry instance;
    return instance;
}

bool PluginRegistry::registerPlugin(const std::string& name,
                                     const PluginRegistration& registration) {
    if (plugins_.find(name) != plugins_.end()) {
        std::cerr << "Plugin already registered: " << name << std::endl;
        return false;
    }

    plugins_[name] = registration;
    std::cout << "Registered plugin: " << name
              << " (type: " << static_cast<int>(registration.metadata.type) << ")"
              << std::endl;
    return true;
}

bool PluginRegistry::unregisterPlugin(const std::string& name) {
    auto it = plugins_.find(name);
    if (it == plugins_.end()) {
        return false;
    }

    // Unload dynamic library if loaded
    auto lib_it = loaded_libraries_.find(name);
    if (lib_it != loaded_libraries_.end()) {
        dlclose(lib_it->second);
        loaded_libraries_.erase(lib_it);
    }

    plugins_.erase(it);
    return true;
}

std::shared_ptr<IPlugin> PluginRegistry::createPlugin(const std::string& name) {
    auto it = plugins_.find(name);
    if (it == plugins_.end()) {
        std::cerr << "Plugin not found: " << name << std::endl;
        return nullptr;
    }

    if (!it->second.create_func) {
        std::cerr << "Plugin has no creation function: " << name << std::endl;
        return nullptr;
    }

    return it->second.create_func();
}

std::shared_ptr<IPlugin> PluginRegistry::createPlugin(
    const std::string& name,
    const std::map<std::string, std::string>& config) {

    auto plugin = createPlugin(name);
    if (!plugin) {
        return nullptr;
    }

    std::vector<std::string> errors;
    if (!plugin->validateConfig(config, errors)) {
        std::cerr << "Plugin configuration validation failed: " << name << std::endl;
        for (const auto& error : errors) {
            std::cerr << "  - " << error << std::endl;
        }
        return nullptr;
    }

    if (!plugin->initialize(config)) {
        std::cerr << "Plugin initialization failed: " << name << std::endl;
        return nullptr;
    }

    return plugin;
}

bool PluginRegistry::hasPlugin(const std::string& name) const {
    return plugins_.find(name) != plugins_.end();
}

std::vector<std::string> PluginRegistry::getPluginNames() const {
    std::vector<std::string> names;
    names.reserve(plugins_.size());
    for (const auto& pair : plugins_) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<std::string> PluginRegistry::getPluginsByType(PluginType type) const {
    std::vector<std::string> names;
    for (const auto& pair : plugins_) {
        if (pair.second.metadata.type == type) {
            names.push_back(pair.first);
        }
    }
    return names;
}

PluginMetadata PluginRegistry::getPluginMetadata(const std::string& name) const {
    auto it = plugins_.find(name);
    if (it != plugins_.end()) {
        return it->second.metadata;
    }
    return PluginMetadata();
}

void PluginRegistry::discoverPlugins(const std::string& plugin_dir) {
    // TODO: Implement plugin discovery by scanning directory for .so files
    //
    // IMPLEMENTATION GUIDE (use std::filesystem, requires C++17):
    // 1. Include: #include <filesystem>
    // 2. Iterate: for (const auto& entry : std::filesystem::directory_iterator(plugin_dir))
    // 3. Filter: if (entry.path().extension() == ".so")
    // 4. Load: loadDynamicPlugin(entry.path().string())
    //
    // Example implementation:
    //   #ifdef __linux__
    //       const std::string ext = ".so";
    //   #elif _WIN32
    //       const std::string ext = ".dll";
    //   #elif __APPLE__
    //       const std::string ext = ".dylib";
    //   #endif
    //
    //   for (const auto& entry : std::filesystem::directory_iterator(plugin_dir)) {
    //       if (entry.is_regular_file() && entry.path().extension() == ext) {
    //           loadDynamicPlugin(entry.path().string());
    //       }
    //   }

    std::cout << "Discovering plugins in: " << plugin_dir << std::endl;
    std::cout << "NOTE: Plugin discovery not yet implemented. Use loadDynamicPlugin() directly." << std::endl;
    // This would scan for *.so files and try to load them
}

void PluginRegistry::loadDynamicPlugin(const std::string& library_path) {
    // Load dynamic library
    void* handle = dlopen(library_path.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "Failed to load plugin library: " << library_path << std::endl;
        std::cerr << "Error: " << dlerror() << std::endl;
        return;
    }

    // Look for registration function
    typedef void (*RegisterFunc)();
    RegisterFunc register_func = (RegisterFunc)dlsym(handle, "pimid_register_plugin");
    if (!register_func) {
        std::cerr << "Plugin library missing registration function: "
                  << library_path << std::endl;
        dlclose(handle);
        return;
    }

    // Call registration function
    register_func();

    // Store library handle
    loaded_libraries_[library_path] = handle;
    std::cout << "Loaded dynamic plugin: " << library_path << std::endl;
}

std::string PluginRegistry::generatePluginDocumentation(const std::string& name) const {
    auto it = plugins_.find(name);
    if (it == plugins_.end()) {
        return "Plugin not found: " + name;
    }

    const auto& meta = it->second.metadata;
    std::ostringstream doc;

    doc << "# Plugin: " << meta.name << "\n\n";
    doc << "**Version:** " << meta.version << "\n";
    doc << "**Author:** " << meta.author << "\n";
    doc << "**Type:** " << static_cast<int>(meta.type) << "\n\n";
    doc << "## Description\n\n" << meta.description << "\n\n";

    if (!meta.dependencies.empty()) {
        doc << "## Dependencies\n\n";
        for (const auto& dep : meta.dependencies) {
            doc << "- " << dep << "\n";
        }
        doc << "\n";
    }

    if (!meta.parameters.empty()) {
        doc << "## Parameters\n\n";
        for (const auto& param : meta.parameters) {
            doc << "- **" << param.first << "**: " << param.second << "\n";
        }
        doc << "\n";
    }

    return doc.str();
}

std::string PluginRegistry::generateAllPluginsDocs() const {
    std::ostringstream doc;
    doc << "# PIMID Plugins Documentation\n\n";
    doc << "Total plugins: " << plugins_.size() << "\n\n";

    for (const auto& pair : plugins_) {
        doc << generatePluginDocumentation(pair.first);
        doc << "---\n\n";
    }

    return doc.str();
}

//=============================================================================
// PluginConfig Implementation
//=============================================================================

PluginConfig::PluginConfig(const std::string& plugin_name)
    : plugin_name_(plugin_name) {}

PluginConfig& PluginConfig::setParameter(const std::string& key,
                                          const std::string& value) {
    parameters_[key] = value;
    return *this;
}

PluginConfig& PluginConfig::setParameters(
    const std::map<std::string, std::string>& params) {
    parameters_.insert(params.begin(), params.end());
    return *this;
}

bool PluginConfig::validate(std::vector<std::string>& errors) const {
    auto& registry = PluginRegistry::getInstance();
    if (!registry.hasPlugin(plugin_name_)) {
        errors.push_back("Plugin not found: " + plugin_name_);
        return false;
    }

    auto plugin = registry.createPlugin(plugin_name_);
    if (!plugin) {
        errors.push_back("Failed to create plugin instance: " + plugin_name_);
        return false;
    }

    return plugin->validateConfig(parameters_, errors);
}

std::shared_ptr<IPlugin> PluginConfig::create() const {
    auto& registry = PluginRegistry::getInstance();
    return registry.createPlugin(plugin_name_, parameters_);
}

} // namespace plugin
} // namespace pimid
