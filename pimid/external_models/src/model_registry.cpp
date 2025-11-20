/**
 * @file model_registry.cpp
 * @brief Implementation of external model registration system
 */

#include "external_models/include/external_model_interface.h"
#include <map>
#include <string>
#include <memory>
#include <iostream>
#include <dlfcn.h>  // For dynamic loading

namespace {

// Global registry of models
std::map<std::string, PimidModelDescriptor> g_model_registry;

// Handles to dynamically loaded libraries
std::map<std::string, void*> g_library_handles;

} // anonymous namespace

//=============================================================================
// Model Registration
//=============================================================================

int pimid_register_model(const PimidModelDescriptor* descriptor) {
    if (!descriptor) {
        std::cerr << "[PIMID] Error: NULL descriptor" << std::endl;
        return -1;
    }

    if (!descriptor->name) {
        std::cerr << "[PIMID] Error: Model name is NULL" << std::endl;
        return -1;
    }

    std::string name(descriptor->name);

    // Check if already registered
    if (g_model_registry.find(name) != g_model_registry.end()) {
        std::cerr << "[PIMID] Warning: Model '" << name << "' already registered, replacing" << std::endl;
    }

    // Copy descriptor into registry
    g_model_registry[name] = *descriptor;

    std::cout << "[PIMID] Registered model: " << name
              << " (version: " << (descriptor->version ? descriptor->version : "unknown")
              << ", type: ";

    switch (descriptor->type) {
        case PIMID_MODEL_NETWORK:
            std::cout << "network";
            break;
        case PIMID_MODEL_MEMORY:
            std::cout << "memory";
            break;
        case PIMID_MODEL_POWER:
            std::cout << "power";
            break;
        case PIMID_MODEL_THERMAL:
            std::cout << "thermal";
            break;
        default:
            std::cout << "unknown";
    }

    std::cout << ")" << std::endl;

    return 0;
}

int pimid_unregister_model(const char* name) {
    if (!name) {
        return -1;
    }

    std::string model_name(name);

    auto it = g_model_registry.find(model_name);
    if (it == g_model_registry.end()) {
        std::cerr << "[PIMID] Error: Model '" << model_name << "' not found" << std::endl;
        return -1;
    }

    // Call destroy callback if available
    const PimidModelDescriptor& desc = it->second;
    if (desc.type == PIMID_MODEL_NETWORK && desc.callbacks.network.destroy) {
        desc.callbacks.network.destroy(desc.handle);
    } else if (desc.type == PIMID_MODEL_MEMORY && desc.callbacks.memory.destroy) {
        desc.callbacks.memory.destroy(desc.handle);
    }

    g_model_registry.erase(it);

    std::cout << "[PIMID] Unregistered model: " << model_name << std::endl;

    return 0;
}

const PimidModelDescriptor* pimid_get_model(const char* name) {
    if (!name) {
        return nullptr;
    }

    std::string model_name(name);

    auto it = g_model_registry.find(model_name);
    if (it == g_model_registry.end()) {
        return nullptr;
    }

    return &(it->second);
}

//=============================================================================
// Dynamic Loading
//=============================================================================

int pimid_load_model(const char* library_path) {
    if (!library_path) {
        std::cerr << "[PIMID] Error: NULL library path" << std::endl;
        return -1;
    }

    std::cout << "[PIMID] Loading model from: " << library_path << std::endl;

    // Load shared library
    void* handle = dlopen(library_path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "[PIMID] Error loading library: " << dlerror() << std::endl;
        return -1;
    }

    // Look for entry point function
    PimidModelCreateFunc create_func =
        (PimidModelCreateFunc)dlsym(handle, "pimid_model_create");

    if (!create_func) {
        std::cerr << "[PIMID] Error: Entry point 'pimid_model_create' not found" << std::endl;
        std::cerr << "[PIMID] " << dlerror() << std::endl;
        dlclose(handle);
        return -1;
    }

    // Call entry point to get model descriptor
    PimidModelDescriptor* descriptor = create_func();
    if (!descriptor) {
        std::cerr << "[PIMID] Error: pimid_model_create returned NULL" << std::endl;
        dlclose(handle);
        return -1;
    }

    // Register the model
    int result = pimid_register_model(descriptor);
    if (result != 0) {
        dlclose(handle);
        return -1;
    }

    // Store library handle for later cleanup
    if (descriptor->name) {
        g_library_handles[std::string(descriptor->name)] = handle;
    }

    std::cout << "[PIMID] Successfully loaded model: " << descriptor->name << std::endl;

    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

void pimid_list_models() {
    std::cout << "[PIMID] Registered models:" << std::endl;

    if (g_model_registry.empty()) {
        std::cout << "  (none)" << std::endl;
        return;
    }

    for (const auto& pair : g_model_registry) {
        const PimidModelDescriptor& desc = pair.second;
        std::cout << "  - " << pair.first
                  << " (version: " << (desc.version ? desc.version : "unknown")
                  << ", type: ";

        switch (desc.type) {
            case PIMID_MODEL_NETWORK: std::cout << "network"; break;
            case PIMID_MODEL_MEMORY: std::cout << "memory"; break;
            case PIMID_MODEL_POWER: std::cout << "power"; break;
            case PIMID_MODEL_THERMAL: std::cout << "thermal"; break;
            default: std::cout << "unknown";
        }

        std::cout << ")" << std::endl;
    }
}

void pimid_cleanup_models() {
    std::cout << "[PIMID] Cleaning up registered models..." << std::endl;

    // Unregister all models (calls destroy callbacks)
    std::vector<std::string> names;
    for (const auto& pair : g_model_registry) {
        names.push_back(pair.first);
    }

    for (const auto& name : names) {
        pimid_unregister_model(name.c_str());
    }

    // Close all library handles
    for (auto& pair : g_library_handles) {
        dlclose(pair.second);
    }
    g_library_handles.clear();

    std::cout << "[PIMID] Cleanup complete" << std::endl;
}

//=============================================================================
// C++ Convenience Wrappers
//=============================================================================

#ifdef __cplusplus
namespace pimid {

class ExternalModel {
public:
    explicit ExternalModel(const char* name) {
        descriptor_ = pimid_get_model(name);
        if (!descriptor_) {
            throw std::runtime_error(std::string("Model not found: ") + name);
        }
    }

    const char* getName() const { return descriptor_->name; }
    const char* getVersion() const { return descriptor_->version; }
    PimidModelType getType() const { return descriptor_->type; }

    const PimidModelDescriptor* getDescriptor() const { return descriptor_; }

protected:
    const PimidModelDescriptor* descriptor_;
};

class ExternalNetworkModel : public ExternalModel {
public:
    using ExternalModel::ExternalModel;

    int init(const char* config_file) {
        if (!descriptor_->callbacks.network.init) return -1;
        return descriptor_->callbacks.network.init(descriptor_->handle, config_file);
    }

    int sendPacket(const PimidNetworkPacket& packet) {
        if (!descriptor_->callbacks.network.send_packet) return -1;
        return descriptor_->callbacks.network.send_packet(descriptor_->handle, &packet);
    }

    void tick() {
        if (descriptor_->callbacks.network.tick) {
            descriptor_->callbacks.network.tick(descriptor_->handle);
        }
    }

    uint64_t getLatency(uint32_t src, uint32_t dst, uint64_t bytes) {
        if (!descriptor_->callbacks.network.get_latency) return 0;
        return descriptor_->callbacks.network.get_latency(descriptor_->handle, src, dst, bytes);
    }

    PimidNetworkStats getStats() {
        PimidNetworkStats stats = {};
        if (descriptor_->callbacks.network.get_stats) {
            descriptor_->callbacks.network.get_stats(descriptor_->handle, &stats);
        }
        return stats;
    }
};

class ExternalMemoryModel : public ExternalModel {
public:
    using ExternalModel::ExternalModel;

    int init(const char* config_file) {
        if (!descriptor_->callbacks.memory.init) return -1;
        return descriptor_->callbacks.memory.init(descriptor_->handle, config_file);
    }

    int sendRequest(const PimidMemoryRequest& req) {
        if (!descriptor_->callbacks.memory.send_request) return -1;
        return descriptor_->callbacks.memory.send_request(descriptor_->handle, &req);
    }

    bool isReady(const PimidMemoryRequest& req) {
        if (!descriptor_->callbacks.memory.is_ready) return true;
        return descriptor_->callbacks.memory.is_ready(descriptor_->handle, &req);
    }

    void tick() {
        if (descriptor_->callbacks.memory.tick) {
            descriptor_->callbacks.memory.tick(descriptor_->handle);
        }
    }

    uint64_t getLatency(uint64_t addr, uint64_t bytes, bool is_write) {
        if (!descriptor_->callbacks.memory.get_latency) return 0;
        return descriptor_->callbacks.memory.get_latency(descriptor_->handle, addr, bytes, is_write);
    }

    PimidMemoryStats getStats() {
        PimidMemoryStats stats = {};
        if (descriptor_->callbacks.memory.get_stats) {
            descriptor_->callbacks.memory.get_stats(descriptor_->handle, &stats);
        }
        return stats;
    }
};

} // namespace pimid
#endif // __cplusplus
