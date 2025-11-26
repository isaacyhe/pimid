# PIMID Plugin Development Guide

This guide will help you create custom plugins for the PIMID simulator to extend its functionality with your own models, schedulers, and algorithms.

## Table of Contents

1. [Introduction](#introduction)
2. [Plugin Types](#plugin-types)
3. [Creating Your First Plugin](#creating-your-first-plugin)
4. [Memory Model Plugins](#memory-model-plugins)
5. [Scheduler Plugins](#scheduler-plugins)
6. [Network Topology Plugins](#network-topology-plugins)
7. [Power Model Plugins](#power-model-plugins)
8. [Building and Installing Plugins](#building-and-installing-plugins)
9. [Advanced Topics](#advanced-topics)
10. [Best Practices](#best-practices)

---

## Introduction

PIMID's plugin system allows you to extend the simulator with custom components without modifying the core codebase. Plugins are dynamically loaded at runtime and integrated seamlessly into the simulation framework.

### Why Use Plugins?

- **Modularity**: Keep your custom models separate from the core simulator
- **Flexibility**: Easy to enable/disable different models
- **Sharing**: Share your models with the community
- **Experimentation**: Quickly test new ideas without rebuilding the entire simulator

---

## Plugin Types

PIMID supports the following plugin types:

| Plugin Type | Purpose | Examples |
|-------------|---------|----------|
| `MEMORY_MODEL` | Custom memory technologies | New DRAM variants, emerging memories |
| `SCHEDULER` | PE task scheduling algorithms | Energy-aware, ML-based schedulers |
| `NETWORK_TOPOLOGY` | NoC topologies | Custom interconnects, hierarchical networks |
| `POWER_MODEL` | Power estimation models | Technology-specific models |
| `PE_TYPE` | Processing element architectures | Custom accelerators |
| `ADDRESS_MAPPER` | Address mapping schemes | Optimized data layouts |
| `CACHE_REPLACEMENT` | Cache replacement policies | ML-based, workload-aware policies |
| `PREFETCHER` | Data prefetching algorithms | Stride, pattern-based prefetchers |

---

## Creating Your First Plugin

Let's create a simple custom scheduler plugin step by step.

### Step 1: Create Plugin Header

Create `my_scheduler_plugin.h`:

```cpp
#ifndef MY_SCHEDULER_PLUGIN_H
#define MY_SCHEDULER_PLUGIN_H

#include "plugin/scheduler_plugin.h"

namespace myplugins {

class MySchedulerPlugin : public pimid::plugin::SchedulerPluginBase {
public:
    MySchedulerPlugin();
    ~MySchedulerPlugin() override = default;

    // IPlugin interface
    bool initialize(const std::map<std::string, std::string>& config) override;
    void shutdown() override;

    bool validateConfig(const std::map<std::string, std::string>& config,
                        std::vector<std::string>& errors) const override;

    std::vector<std::string> getRequiredParameters() const override;
    std::vector<std::string> getOptionalParameters() const override;
    std::map<std::string, std::string> getParameterDescriptions() const override;

    // ISchedulerPlugin interface
    std::unique_ptr<pimid::PEScheduler> createScheduler(
        pimid::PEPlacementManager* pe_manager) override;

    bool isDataAware() const override { return true; }
    bool isLoadAware() const override { return true; }
    bool isPriorityBased() const override { return false; }
    bool isDynamic() const override { return true; }

    double getTypicalOverhead() const override { return 15.0; }
    bool isOptimalForWorkload(const std::string& workload_type) const override;

private:
    double locality_weight_;
    double load_weight_;
};

} // namespace myplugins

#endif
```

### Step 2: Implement Plugin

Create `my_scheduler_plugin.cpp`:

```cpp
#include "my_scheduler_plugin.h"
#include "scheduler/scheduler.h"

namespace myplugins {

MySchedulerPlugin::MySchedulerPlugin()
    : SchedulerPluginBase(createMetadata()),
      locality_weight_(0.6),
      load_weight_(0.4) {}

pimid::plugin::PluginMetadata MySchedulerPlugin::createMetadata() {
    pimid::plugin::PluginMetadata meta;
    meta.name = "MyScheduler";
    meta.version = "1.0.0";
    meta.author = "Your Name";
    meta.description = "Custom locality and load-aware scheduler";
    meta.type = pimid::plugin::PluginType::SCHEDULER;
    return meta;
}

bool MySchedulerPlugin::initialize(
    const std::map<std::string, std::string>& config) {

    config_ = config;

    // Read configuration
    locality_weight_ = getConfigValueAs<double>("locality_weight", 0.6);
    load_weight_ = getConfigValueAs<double>("load_weight", 0.4);

    // Validate weights
    if (locality_weight_ + load_weight_ != 1.0) {
        std::cerr << "Weights must sum to 1.0" << std::endl;
        return false;
    }

    initialized_ = true;
    return true;
}

void MySchedulerPlugin::shutdown() {
    initialized_ = false;
}

bool MySchedulerPlugin::validateConfig(
    const std::map<std::string, std::string>& config,
    std::vector<std::string>& errors) const {

    // Check required parameters
    if (config.find("locality_weight") == config.end()) {
        errors.push_back("Missing required parameter: locality_weight");
    }
    if (config.find("load_weight") == config.end()) {
        errors.push_back("Missing required parameter: load_weight");
    }

    // Validate weight values
    if (!errors.empty()) return false;

    try {
        double loc = std::stod(config.at("locality_weight"));
        double load = std::stod(config.at("load_weight"));

        if (loc < 0.0 || loc > 1.0) {
            errors.push_back("locality_weight must be in range [0.0, 1.0]");
        }
        if (load < 0.0 || load > 1.0) {
            errors.push_back("load_weight must be in range [0.0, 1.0]");
        }
        if (std::abs(loc + load - 1.0) > 0.001) {
            errors.push_back("locality_weight + load_weight must equal 1.0");
        }
    } catch (...) {
        errors.push_back("Invalid weight values");
    }

    return errors.empty();
}

std::vector<std::string> MySchedulerPlugin::getRequiredParameters() const {
    return {"locality_weight", "load_weight"};
}

std::vector<std::string> MySchedulerPlugin::getOptionalParameters() const {
    return {};
}

std::map<std::string, std::string>
MySchedulerPlugin::getParameterDescriptions() const {
    return {
        {"locality_weight", "Weight for data locality (0.0-1.0)"},
        {"load_weight", "Weight for load balancing (0.0-1.0)"}
    };
}

std::unique_ptr<pimid::PEScheduler> MySchedulerPlugin::createScheduler(
    pimid::PEPlacementManager* pe_manager) {

    // Create your custom scheduler implementation
    return std::make_unique<MyCustomScheduler>(
        pe_manager, locality_weight_, load_weight_);
}

bool MySchedulerPlugin::isOptimalForWorkload(
    const std::string& workload_type) const {

    // Define which workloads this scheduler works best with
    return workload_type == "streaming" || workload_type == "graph";
}

} // namespace myplugins

// Register the plugin
extern "C" {
    void pimid_register_plugin() {
        pimid::plugin::PluginRegistration reg;
        auto instance = std::make_shared<myplugins::MySchedulerPlugin>();
        reg.metadata = instance->getMetadata();
        reg.create_func = []() -> std::shared_ptr<pimid::plugin::IPlugin> {
            return std::make_shared<myplugins::MySchedulerPlugin>();
        };
        pimid::plugin::PluginRegistry::getInstance().registerPlugin(
            "MyScheduler", reg);
    }
}
```

### Step 3: Configure Your Plugin

Add to your `pimid_config.yaml`:

```yaml
plugins:
  loaded_plugins:
    - name: "MyScheduler"
      library: "plugins/libmy_scheduler.so"
      config:
        locality_weight: 0.7
        load_weight: 0.3

# Use your scheduler
pim:
  scheduler: "MyScheduler"
```

---

## Memory Model Plugins

### Example: Custom Memory Model

Create a plugin for a novel memory technology:

```cpp
#include "plugin/memory_model_plugin.h"

class NovelMemoryPlugin : public pimid::plugin::MemoryModelPluginBase {
public:
    NovelMemoryPlugin() : MemoryModelPluginBase(createMetadata()) {}

    std::shared_ptr<pimid::MemoryModel> createMemoryModel(
        const std::string& config_path) override {
        return std::make_shared<NovelMemoryModel>(config_path, config_);
    }

    pimid::Cycle getTypicalReadLatency() const override {
        return getConfigValueAs<int64_t>("read_latency", 50);
    }

    pimid::Cycle getTypicalWriteLatency() const override {
        return getConfigValueAs<int64_t>("write_latency", 100);
    }

    uint64_t getMaxBandwidth() const override {
        return getConfigValueAs<int64_t>("bandwidth", 25600);  // MB/s
    }

    std::string getTechnologyName() const override {
        return "NovelMemory";
    }

    uint32_t getTechnologyNode() const override {
        return getConfigValueAs<int64_t>("tech_node_nm", 22);
    }

private:
    static pimid::plugin::PluginMetadata createMetadata() {
        pimid::plugin::PluginMetadata meta;
        meta.name = "NovelMemory";
        meta.version = "1.0.0";
        meta.author = "Your Institution";
        meta.description = "Novel memory technology model";
        meta.type = pimid::plugin::PluginType::MEMORY_MODEL;
        return meta;
    }
};
```

### Configuration

```yaml
memory:
  technology: "NovelMemory"

plugins:
  loaded_plugins:
    - name: "NovelMemory"
      library: "plugins/libnovel_memory.so"
      config:
        read_latency: 50
        write_latency: 100
        bandwidth: 51200
        tech_node_nm: 14
        endurance: 1000000
```

---

## Scheduler Plugins

### Energy-Aware Scheduler Example

```cpp
class EnergyAwareSchedulerPlugin : public pimid::plugin::SchedulerPluginBase {
public:
    EnergyAwareSchedulerPlugin();

    std::unique_ptr<pimid::PEScheduler> createScheduler(
        pimid::PEPlacementManager* pe_manager) override {

        double energy_weight = getConfigValueAs<double>("energy_weight", 0.6);
        double perf_weight = getConfigValueAs<double>("perf_weight", 0.4);

        return std::make_unique<EnergyAwareScheduler>(
            pe_manager, energy_weight, perf_weight);
    }

    std::vector<std::string> getRequiredParameters() const override {
        return {"energy_weight", "perf_weight"};
    }

    std::map<std::string, std::string>
    getParameterDescriptions() const override {
        return {
            {"energy_weight", "Weight for energy minimization (0.0-1.0)"},
            {"perf_weight", "Weight for performance (0.0-1.0)"},
            {"power_threshold", "Max power per PE in watts (optional)"}
        };
    }
};
```

---

## Network Topology Plugins

### Custom Topology Example

```cpp
class CustomTopologyPlugin : public pimid::plugin::IPlugin {
public:
    // Create custom network topology
    std::shared_ptr<pimid::NetworkModel> createTopology(
        const pimid::NetworkConfig& config) {

        // Implement your custom topology
        return std::make_shared<MyCustomTopology>(config, params_);
    }

    std::vector<std::string> getRequiredParameters() const override {
        return {"num_nodes", "connectivity_pattern"};
    }

private:
    std::string connectivity_pattern_;
    uint32_t num_nodes_;
};
```

---

## Building and Installing Plugins

### Using CMake

Create `CMakeLists.txt` for your plugin:

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyPIMIDPlugins)

# Find PIMID
find_package(PIMID REQUIRED)

# Build plugin as shared library
add_library(my_scheduler SHARED
    src/my_scheduler_plugin.cpp
    src/my_scheduler_impl.cpp
)

target_include_directories(my_scheduler PRIVATE
    ${PIMID_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(my_scheduler
    ${PIMID_LIBRARIES}
)

# Install plugin
install(TARGETS my_scheduler
    LIBRARY DESTINATION ${PIMID_PLUGIN_DIR}
)
```

### Build Commands

```bash
mkdir build && cd build
cmake ..
make
sudo make install

# Or install to custom directory
make install DESTDIR=/path/to/pimid/plugins
```

---

## Advanced Topics

### Plugin Dependencies

Specify dependencies on other plugins:

```cpp
pimid::plugin::PluginMetadata meta;
meta.dependencies = {"BaseScheduler", "PowerModel"};
```

### Dynamic Configuration

Update plugin configuration at runtime:

```cpp
class AdaptivePlugin : public PluginBase {
public:
    void updateConfig(const std::string& key, const std::string& value) {
        config_[key] = value;
        reconfigure();
    }

private:
    void reconfigure() {
        // Adapt behavior based on new configuration
    }
};
```

### Plugin Hooks

Implement callbacks for simulation events:

```cpp
class MonitoringPlugin : public PluginBase {
public:
    void onCycleComplete(pimid::Cycle cycle) {
        // Called every cycle
        collectStatistics(cycle);
    }

    void onTaskComplete(const pimid::PIMTask& task) {
        // Called when a task completes
        updateMetrics(task);
    }
};
```

---

## Best Practices

### 1. Documentation

Always document your plugin thoroughly:

```cpp
/**
 * @brief Energy-aware scheduler that optimizes for minimal energy consumption
 * @details This scheduler uses a weighted scoring function to balance energy
 *          and performance. It considers:
 *          - Distance to data (affects network energy)
 *          - PE utilization (affects leakage energy)
 *          - Memory access patterns (affects DRAM energy)
 *
 * @config energy_weight Weight for energy in scoring (0.0-1.0)
 * @config perf_weight Weight for performance in scoring (0.0-1.0)
 * @config power_threshold Optional max power per PE in watts
 */
class EnergyAwareSchedulerPlugin { ... };
```

### 2. Error Handling

Validate all inputs and provide helpful error messages:

```cpp
bool validateConfig(const std::map<std::string, std::string>& config,
                    std::vector<std::string>& errors) const override {
    // Check required parameters
    if (config.find("threshold") == config.end()) {
        errors.push_back(
            "Missing 'threshold' parameter. "
            "Expected: integer value between 1 and 100. "
            "Example: threshold: 50");
        return false;
    }

    // Validate ranges
    int threshold = std::stoi(config.at("threshold"));
    if (threshold < 1 || threshold > 100) {
        errors.push_back(
            "Invalid 'threshold' value: " + std::to_string(threshold) + ". "
            "Must be in range [1, 100]");
        return false;
    }

    return true;
}
```

### 3. Testing

Create unit tests for your plugin:

```cpp
TEST(MySchedulerTest, BasicFunctionality) {
    MySchedulerPlugin plugin;
    std::map<std::string, std::string> config = {
        {"locality_weight", "0.6"},
        {"load_weight", "0.4"}
    };

    std::vector<std::string> errors;
    EXPECT_TRUE(plugin.validateConfig(config, errors));
    EXPECT_TRUE(plugin.initialize(config));

    auto scheduler = plugin.createScheduler(&pe_manager);
    EXPECT_NE(scheduler, nullptr);
}
```

### 4. Performance

Optimize hot paths in your plugin:

```cpp
class FastScheduler {
private:
    // Cache frequently accessed data
    std::vector<uint32_t> available_pes_;
    std::unordered_map<pimid::Address, uint32_t> addr_to_pe_cache_;

public:
    uint32_t scheduleTask(const pimid::PIMTask& task) override {
        // Check cache first
        auto it = addr_to_pe_cache_.find(task.data_addr);
        if (it != addr_to_pe_cache_.end()) {
            return it->second;
        }

        // Compute and cache
        uint32_t pe = computeBestPE(task);
        addr_to_pe_cache_[task.data_addr] = pe;
        return pe;
    }
};
```

### 5. Versioning

Use semantic versioning for your plugins:

```cpp
pimid::plugin::PluginMetadata meta;
meta.version = "2.1.0";  // MAJOR.MINOR.PATCH
```

---

## Example Plugins

Complete example plugins are available in `pimid/examples/plugins/`:

- `stream_aware_scheduler/` - Scheduler optimized for streaming workloads
- `hybrid_memory_model/` - Memory model with DRAM+NVM tiers
- `ring_topology/` - Ring network topology
- `ml_power_model/` - Machine learning-based power predictor

---

## Plugin Development Checklist

- [ ] Header file with plugin class
- [ ] Implementation file with all required methods
- [ ] Registration function `extern "C" void pimid_register_plugin()`
- [ ] CMakeLists.txt for building
- [ ] README.md documenting the plugin
- [ ] Unit tests
- [ ] Example configuration file
- [ ] Documentation of all parameters
- [ ] Version number following semver

---

## Getting Help

- **Documentation**: See `docs/` for detailed API documentation
- **Examples**: Check `examples/plugins/` for working examples
- **Issues**: Report bugs at https://github.com/yourusername/pimid-dev/issues
- **Community**: Join our discussion forum

---

## Contributing Your Plugin

We welcome plugin contributions! To share your plugin:

1. Ensure it follows this guide's best practices
2. Include comprehensive documentation
3. Add unit tests
4. Submit a pull request to `pimid-dev-plugins` repository

Happy plugin development!
