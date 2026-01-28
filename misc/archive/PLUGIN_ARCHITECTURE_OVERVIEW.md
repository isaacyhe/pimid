# PIMID Plugin System Architecture - Comprehensive Overview

## Table of Contents
1. [Overview](#overview)
2. [Plugin Types](#plugin-types)
3. [Core Components](#core-components)
4. [Plugin Registration and Management](#plugin-registration-and-management)
5. [Existing Implementations](#existing-implementations)
6. [Usage Examples](#usage-examples)
7. [Architecture Diagrams](#architecture-diagrams)

---

## Overview

The PIMID simulator uses a **modular, extensible plugin system** that allows developers to add custom components without modifying the core codebase. The system is built on a **base plugin interface** with specialized interfaces for different plugin types.

### Key Characteristics:
- **Dynamic Loading**: Plugins are loaded at runtime as shared libraries (.so/.dll/.dylib)
- **Type-Based**: Different plugin types serve different simulation components
- **Registry Pattern**: Central registry manages all plugin lifecycle
- **Configuration-Driven**: Plugins are configured via YAML configuration files
- **Validation Support**: Built-in configuration validation for plugins

---

## Plugin Types

PIMID supports **8 plugin types** defined in `plugin_interface.h`:

### 1. **MEMORY_MODEL** - Memory Technology Plugins
**Purpose**: Implement custom memory technologies beyond standard DRAM
**File**: `/home/user/pimid-dev/pimid/include/plugin/memory_model_plugin.h`

**Interface**: `IMemoryModelPlugin`

**Key Methods**:
```cpp
// Create memory model instance
virtual std::shared_ptr<MemoryModel> createMemoryModel(
    const std::string& config_path) = 0;

// Memory capabilities
virtual bool supportsReadWrite() const = 0;
virtual bool supportsAtomic() const = 0;
virtual bool supportsPowerModeling() const = 0;
virtual bool supportsTimingVariation() const = 0;

// Performance characteristics
virtual Cycle getTypicalReadLatency() const = 0;
virtual Cycle getTypicalWriteLatency() const = 0;
virtual uint64_t getMaxBandwidth() const = 0;

// Technology info
virtual std::string getTechnologyName() const = 0;
virtual uint32_t getTechnologyNode() const = 0;  // nm
```

**Base Class**: `MemoryModelPluginBase` extends `PluginBase`

**Example Plugin**: `CustomMemoryModelPlugin` in memory_model_plugin.h
- Provides template for creating custom memory models
- Supports configurable latency, bandwidth, power modeling

**Existing Memory Models** (in `/home/user/pimid-dev/pimid/memory_models/include/`):
- `dram_model.h` - DRAM implementation
- `sram_model.h` - SRAM implementation
- `sttmram_model.h` - STT-MRAM implementation
- `reram_model.h` - ReRAM implementation
- `pcm_model.h` - Phase-Change Memory implementation
- `nvm_model.h` - Generic NVM implementation

---

### 2. **SCHEDULER** - PE Task Scheduling Plugins
**Purpose**: Implement custom task scheduling algorithms for Processing Elements
**File**: `/home/user/pimid-dev/pimid/include/plugin/scheduler_plugin.h`

**Interface**: `ISchedulerPlugin`

**Key Methods**:
```cpp
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
```

**Base Class**: `SchedulerPluginBase` extends `PluginBase`

**Example Plugins**:
1. `CustomSchedulerPlugin` - Template for custom schedulers
2. `DataLocalitySchedulerPlugin` - Optimizes for data locality
3. `EnergyAwareSchedulerPlugin` - Minimizes energy consumption

**Scheduling Policies Supported**:
```cpp
enum class SchedulingPolicy {
    NEAREST_PE,         // Schedule to PE closest to data
    ROUND_ROBIN,        // Round-robin across PEs
    LOAD_BALANCED,      // Balance load across PEs
    PRIORITY_BASED,     // Based on task priority
    DATA_AWARE          // Consider data locality
};
```

---

### 3. **NETWORK_TOPOLOGY** - Network Interconnect Plugins
**Purpose**: Implement custom network-on-chip (NoC) topologies
**Enumeration**: Defined in `plugin_interface.h` (type value: NETWORK_TOPOLOGY)

**Key Topology Types** (from `network_model.h`):
```cpp
enum class NetworkTopology {
    MESH_2D,        // Standard 2D mesh
    MESH_3D,        // 3D mesh topology
    TORUS_2D,       // 2D torus with wraparound
    TORUS_3D,       // 3D torus topology
    DRAGONFLY,      // Dragonfly topology
    FAT_TREE,       // Fat-tree topology
    H_TREE,         // H-tree for DRAM internal interconnects
    CROSSBAR        // Full crossbar
};
```

**Routing Algorithms**:
```cpp
enum class RoutingAlgorithm {
    XY,             // XY routing
    XYZ,            // XYZ routing for 3D
    ADAPTIVE,       // Adaptive routing
    WEST_FIRST,     // West-first deadlock avoidance
    NORTH_LAST,     // North-last deadlock avoidance
    MINIMAL,        // Minimal path routing
    VALIANT,        // Valiant routing
    TREE_BASED      // For H-tree and Fat-tree
};
```

**Current Implementation**: `GarnetModel` class in `network_model.h`
- Integrates GARNET for cycle-accurate NoC simulation
- Supports virtual networks and virtual channels
- Includes energy modeling for routers and links

---

### 4. **POWER_MODEL** - Power Estimation Plugins
**Purpose**: Implement technology-specific power consumption models
**Enumeration**: Defined in `plugin_interface.h` (type value: POWER_MODEL)

**Note**: Power model plugin interface is defined but detailed headers not yet created. The system can support custom power models for:
- Technology-specific leakage/dynamic power
- Thermal models
- Workload-dependent power estimation

---

### 5. **PE_TYPE** - Processing Element Architecture Plugins
**Purpose**: Define custom Processing Element (PE) architectures
**Enumeration**: Defined in `plugin_interface.h` (type value: PE_TYPE)

**Purpose Examples**:
- Custom accelerator architectures
- PE with specialized instruction sets
- Heterogeneous PE designs

---

### 6-8. **Other Plugin Types**

#### **ADDRESS_MAPPER** - Address Mapping Plugins
```cpp
enum value: ADDRESS_MAPPER
```
- Custom address mapping schemes
- Optimized data layouts
- Memory interleaving strategies

#### **CACHE_REPLACEMENT** - Cache Replacement Policy Plugins
```cpp
enum value: CACHE_REPLACEMENT
```
- ML-based replacement policies
- Workload-aware policies
- Custom eviction strategies

#### **PREFETCHER** - Data Prefetching Plugins
```cpp
enum value: PREFETCHER
```
- Stride-based prefetchers
- Pattern-based prefetchers
- ML-based prefetchers

#### **CUSTOM** - Custom Plugin Type
```cpp
enum value: CUSTOM
```
- User-defined plugin types
- Experimental plugins

---

## Core Components

### 1. **Plugin Interface Hierarchy**

```
IPlugin (Abstract Base)
├── PluginBase (Common Functionality)
│   ├── MemoryModelPluginBase
│   │   └── CustomMemoryModelPlugin
│   └── SchedulerPluginBase
│       ├── CustomSchedulerPlugin
│       ├── DataLocalitySchedulerPlugin
│       └── EnergyAwareSchedulerPlugin
```

**File**: `/home/user/pimid-dev/pimid/include/plugin/plugin_interface.h`

### 2. **PluginMetadata Structure**
```cpp
struct PluginMetadata {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    PluginType type;
    std::vector<std::string> dependencies;
    std::map<std::string, std::string> parameters;
};
```

### 3. **PluginRegistry** - Central Management
**Location**: `/home/user/pimid-dev/pimid/src/plugin/plugin_interface.cpp`

**Key Methods**:
```cpp
class PluginRegistry {
public:
    static PluginRegistry& getInstance();  // Singleton
    
    // Registration
    bool registerPlugin(const std::string& name,
                       const PluginRegistration& registration);
    bool unregisterPlugin(const std::string& name);
    
    // Creation
    std::shared_ptr<IPlugin> createPlugin(const std::string& name);
    std::shared_ptr<IPlugin> createPlugin(
        const std::string& name,
        const std::map<std::string, std::string>& config);
    
    // Queries
    bool hasPlugin(const std::string& name) const;
    std::vector<std::string> getPluginNames() const;
    std::vector<std::string> getPluginsByType(PluginType type) const;
    
    // Dynamic Loading
    void discoverPlugins(const std::string& plugin_dir);
    void loadDynamicPlugin(const std::string& library_path);
    
    // Documentation
    std::string generatePluginDocumentation(const std::string& name) const;
    std::string generateAllPluginsDocs() const;
};
```

### 4. **PluginConfig Helper**
```cpp
class PluginConfig {
public:
    PluginConfig(const std::string& plugin_name);
    
    PluginConfig& setParameter(const std::string& key, 
                               const std::string& value);
    PluginConfig& setParameters(
        const std::map<std::string, std::string>& params);
    
    bool validate(std::vector<std::string>& errors) const;
    std::shared_ptr<IPlugin> create() const;
};
```

---

## Plugin Registration and Management

### Registration Process

**Step 1: Static Registration (Built-in plugins)**
```cpp
// In plugin source file
extern "C" {
    void pimid_register_plugin() {
        PluginRegistration reg;
        auto instance = std::make_shared<MyPlugin>();
        reg.metadata = instance->getMetadata();
        reg.create_func = []() -> std::shared_ptr<IPlugin> {
            return std::make_shared<MyPlugin>();
        };
        PluginRegistry::getInstance().registerPlugin(
            "MyPlugin", reg);
    }
}
```

**Step 2: Macro-based Registration (Convenience)**
```cpp
// Convenient helper macro
#define PIMID_REGISTER_PLUGIN(PluginClass, PluginName)
```

### Dynamic Loading Process

1. **Plugin Discovery** (during initialization)
   ```
   PluginRegistry::discoverPlugins("plugins/")
     ↓
   Scans directory for .so/.dll/.dylib files
     ↓
   Loads each plugin library with dlopen()
   ```

2. **Plugin Registration** (per library)
   ```
   dlopen() loads library
     ↓
   dlsym() finds pimid_register_plugin() function
     ↓
   Calls pimid_register_plugin()
     ↓
   Plugin self-registers with registry
   ```

3. **Plugin Creation** (on demand)
   ```
   PluginRegistry::createPlugin(name, config)
     ↓
   Lookup plugin in registry
     ↓
   Call create_func() to instantiate
     ↓
   Validate configuration
     ↓
   Call initialize(config)
   ```

### Configuration-Based Plugin Loading

**YAML Configuration** (`pimid_config.yaml`):
```yaml
plugins:
  # Plugin directory for dynamic loading
  plugin_dir: "plugins/"
  
  # Auto-discover plugins
  auto_discover: true
  
  # Explicitly loaded plugins
  loaded_plugins:
    - name: "custom_memory_model"
      library: "plugins/libcustom_memory.so"
      config:
        latency: 100
        bandwidth: 25600
```

---

## Existing Implementations

### 1. **Memory Model Plugins**

**Location**: `/home/user/pimid-dev/pimid/memory_models/`

**Implementations**:
- `dram_model.cpp` - DRAM with Ramulator backend
- `sram_model.cpp` - Static RAM
- `sttmram_model.cpp` - Spin-Transfer Torque MRAM
- `reram_model.cpp` - Resistive RAM
- `pcm_model.cpp` - Phase-Change Memory
- `nvm_model.cpp` - Generic NVM

**Special Component**: `PIMControllerPlugin`
```cpp
// Location: pimid/memory_models/include/pim_controller_plugin.h
// A Ramulator IControllerPlugin for PIM-specific features
class PIMControllerPlugin : public Ramulator::IControllerPlugin {
    - Tracks bandwidth at each DRAM level
    - Models internal DRAM networks
    - Enforces port bitwidth constraints
    - Manages PE placement and contention
};
```

### 2. **Scheduler Implementations**

**Location**: `/home/user/pimid-dev/pimid/include/plugin/scheduler_plugin.h`

**Template Implementations**:
1. **CustomSchedulerPlugin** - Generic template
   - Data locality aware
   - Load-aware
   - Dynamic adaptation
   - Configurable overhead (10 cycles default)

2. **DataLocalitySchedulerPlugin** - Data-centric scheduling
   - Optimizes for data locality
   - Considers cache line size
   - Can evaluate bank conflicts
   - Lower overhead (5 cycles)

3. **EnergyAwareSchedulerPlugin** - Energy optimization
   - Balances energy and performance
   - Configurable weight factors
   - Nearby PE preference
   - Typical overhead (15 cycles)

### 3. **Network Models**

**Location**: `/home/user/pimid-dev/pimid/network_models/`

**Current Implementation**: `GarnetModel` class
- Abstract base class: `NetworkModel`
- GARNET integration for cycle-accurate simulation
- Supports multiple topologies and routing algorithms
- Energy tracking for routers and links

**Features**:
- Virtual networks for message classes
- Virtual channels for deadlock avoidance
- Router pipeline complexity control
- Packet-based routing simulation

### 4. **Configuration System**

**Location**: `/home/user/pimid-dev/config/`

**Main Config File**: `pimid_config.yaml`
- Simulation settings
- Component configurations
- Memory technology selection
- PIM configuration
- Network-on-chip settings
- Power modeling parameters
- **Plugin configuration section**

---

## Usage Examples

### Example 1: Creating a Custom Scheduler Plugin

**Header File**: `my_scheduler_plugin.h`
```cpp
#include "plugin/scheduler_plugin.h"

namespace myplugins {

class MySchedulerPlugin : public pimid::plugin::SchedulerPluginBase {
public:
    MySchedulerPlugin();
    
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
};

} // namespace myplugins
```

**Implementation**: `my_scheduler_plugin.cpp`
```cpp
#include "my_scheduler_plugin.h"
#include "scheduler/scheduler.h"

namespace myplugins {

MySchedulerPlugin::MySchedulerPlugin()
    : SchedulerPluginBase(createMetadata()) {}

pimid::plugin::PluginMetadata MySchedulerPlugin::createMetadata() {
    pimid::plugin::PluginMetadata meta;
    meta.name = "MyScheduler";
    meta.version = "1.0.0";
    meta.author = "Your Name";
    meta.description = "Custom scheduler";
    meta.type = pimid::plugin::PluginType::SCHEDULER;
    return meta;
}

bool MySchedulerPlugin::initialize(
    const std::map<std::string, std::string>& config) {
    config_ = config;
    
    // Read configuration
    double locality_weight = 
        getConfigValueAs<double>("locality_weight", 0.6);
    
    // Validate and initialize
    initialized_ = true;
    return true;
}

std::unique_ptr<pimid::PEScheduler> MySchedulerPlugin::createScheduler(
    pimid::PEPlacementManager* pe_manager) {
    return std::make_unique<MyCustomScheduler>(pe_manager);
}

// ... other methods ...

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

**Configuration**: `pimid_config.yaml`
```yaml
plugins:
  loaded_plugins:
    - name: "MyScheduler"
      library: "plugins/libmy_scheduler.so"
      config:
        locality_weight: 0.7
        load_weight: 0.3

pim:
  scheduler: "MyScheduler"
```

### Example 2: Creating a Custom Memory Model Plugin

**Structure**:
```cpp
class NovelMemoryPlugin : public pimid::plugin::MemoryModelPluginBase {
public:
    std::shared_ptr<pimid::MemoryModel> createMemoryModel(
        const std::string& config_path) override {
        return std::make_shared<NovelMemoryModel>(config_path, config_);
    }
    
    Cycle getTypicalReadLatency() const override {
        return getConfigValueAs<int64_t>("read_latency", 50);
    }
    
    Cycle getTypicalWriteLatency() const override {
        return getConfigValueAs<int64_t>("write_latency", 100);
    }
    
    uint64_t getMaxBandwidth() const override {
        return getConfigValueAs<int64_t>("bandwidth", 25600);
    }
    
    std::string getTechnologyName() const override {
        return "NovelMemory";
    }
    
    uint32_t getTechnologyNode() const override {
        return getConfigValueAs<int64_t>("tech_node_nm", 22);
    }
};
```

**Configuration**:
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

## Architecture Diagrams

### Plugin System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    PIMID Simulator                          │
├─────────────────────────────────────────────────────────────┤
│  PIMIDSimulator                                             │
│  ├─ Host Engine                                             │
│  ├─ Device Engine                                           │
│  ├─ Memory Model (via plugin)                              │
│  ├─ Network Model (via plugin)                             │
│  └─ Power Model (via plugin)                               │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ↓
        ┌──────────────────────┐
        │  PluginRegistry      │
        │  (Singleton)         │
        ├──────────────────────┤
        │ - plugins_map        │
        │ - loaded_libraries   │
        └──────────────────────┘
                   ↕
        (registerPlugin, createPlugin, 
         discoverPlugins, loadDynamicPlugin)
                   │
        ┌──────────┴──────────┬──────────┬──────────┐
        ↓                     ↓          ↓          ↓
    ┌────────────┐  ┌─────────────┐  ┌────────┐  ┌─────────┐
    │   Memory   │  │ Scheduler   │  │Network │  │  Power  │
    │   Plugins  │  │  Plugins    │  │Plugins │  │ Plugins │
    └────────────┘  └─────────────┘  └────────┘  └─────────┘
        │                │              │          │
        ├─DRAM           ├─Locality     ├─MESH    ├─Leakage
        ├─SRAM           ├─EnergyAware  ├─TORUS   ├─Dynamic
        ├─RERAM          ├─Custom       ├─FAT_TREE└─Thermal
        └─PCM            └─Priority     └─DRAGONFLY
```

### Plugin Lifecycle

```
┌─────────────────┐
│   Simulator     │
│   Startup       │
└────────┬────────┘
         │
         ↓
┌──────────────────────────┐
│ Load Configuration File  │
│ (pimid_config.yaml)      │
└────────┬─────────────────┘
         │
         ↓
┌──────────────────────────────┐
│ PluginRegistry::             │
│ discoverPlugins()            │
│ - Scan plugin_dir            │
│ - Find *.so files            │
└────────┬─────────────────────┘
         │
         ↓
┌──────────────────────────────┐
│ For each plugin file:        │
│ dlopen(library_path)         │
│ dlsym(pimid_register_plugin) │
│ call pimid_register_plugin() │
└────────┬─────────────────────┘
         │
         ↓
┌──────────────────────────────┐
│ Plugin::pimid_register_      │
│ plugin()                     │
│ Create PluginRegistration    │
│ Register with registry       │
└────────┬─────────────────────┘
         │
         ↓
┌──────────────────────────────┐
│ For each loaded_plugin:      │
│ PluginRegistry::             │
│ createPlugin(name, config)   │
│ - Lookup                     │
│ - Instantiate                │
│ - Validate config            │
│ - Call initialize()          │
└────────┬─────────────────────┘
         │
         ↓
┌──────────────────────────────┐
│ Simulator uses initialized   │
│ plugin instances during      │
│ simulation run               │
└──────────────────────────────┘
```

### Component Interaction

```
                    Simulator
                        │
         ┌──────────────┼──────────────┐
         │              │              │
         ↓              ↓              ↓
    ┌─────────┐   ┌──────────┐   ┌─────────┐
    │ Memory  │   │Scheduler │   │ Network │
    │ Plugin  │   │ Plugin   │   │ Plugin  │
    └────┬────┘   └─────┬────┘   └────┬────┘
         │              │             │
         │ createMemory │ createScheduler
         │   Model()    │    &          │ createTopology
         │              │  createScheduler
         ↓              ↓             ↓
    ┌─────────┐   ┌──────────┐   ┌─────────┐
    │ Memory  │   │Scheduler │   │ Network │
    │ Model   │   │ Instance │   │ Model   │
    │Instance │   │          │   │Instance │
    └─────────┘   └──────────┘   └─────────┘
```

---

## Configuration Reference

### Plugin Configuration Section (pimid_config.yaml)

```yaml
plugins:
  # Directory where plugins are stored
  plugin_dir: "plugins/"
  
  # Whether to automatically discover plugins in plugin_dir
  auto_discover: true
  
  # Explicitly loaded plugins with configuration
  loaded_plugins:
    - name: "plugin_name"
      # Path to shared library file
      library: "plugins/libplugin_name.so"
      # Plugin-specific configuration parameters
      config:
        param1: value1
        param2: value2
  
  # Additional search paths for plugin discovery
  search_paths:
    - "plugins/"
    - "/usr/local/lib/pimid/plugins/"
    - "~/.pimid/plugins/"
```

### Memory Model Plugin Configuration Example

```yaml
memory:
  technology: "CustomMemory"

plugins:
  loaded_plugins:
    - name: "CustomMemory"
      library: "plugins/libcustom_memory.so"
      config:
        read_latency: 50      # cycles
        write_latency: 100    # cycles
        bandwidth: 25600      # MB/s
        tech_node_nm: 22      # nanometers
```

### Scheduler Plugin Configuration Example

```yaml
pim:
  scheduler: "MyScheduler"

plugins:
  loaded_plugins:
    - name: "MyScheduler"
      library: "plugins/libmy_scheduler.so"
      config:
        locality_weight: 0.6
        load_weight: 0.4
        power_threshold: 10.0  # watts (optional)
```

---

## Best Practices for Plugin Development

1. **Documentation**: Thoroughly document plugin purpose, parameters, and behavior
2. **Error Handling**: Provide clear error messages in validateConfig()
3. **Configuration Validation**: Validate all required and optional parameters
4. **Versioning**: Use semantic versioning (MAJOR.MINOR.PATCH)
5. **Performance**: Cache frequently accessed data in plugins
6. **Testing**: Create unit tests for plugin functionality
7. **Dependencies**: Declare plugin dependencies in metadata

---

## Summary Table

| Plugin Type | Purpose | Header File | Example Implementations |
|-------------|---------|-------------|----------------------|
| MEMORY_MODEL | Memory technologies | memory_model_plugin.h | DRAM, SRAM, RERAM, PCM, STT-MRAM |
| SCHEDULER | PE task scheduling | scheduler_plugin.h | Locality-aware, Energy-aware, Custom |
| NETWORK_TOPOLOGY | NoC topologies | plugin_interface.h | MESH, TORUS, FAT_TREE, DRAGONFLY (via GarnetModel) |
| POWER_MODEL | Power estimation | plugin_interface.h | (Framework ready, implementations TBD) |
| PE_TYPE | PE architectures | plugin_interface.h | (Framework ready, implementations TBD) |
| ADDRESS_MAPPER | Address mapping | plugin_interface.h | (Framework ready, implementations TBD) |
| CACHE_REPLACEMENT | Cache policies | plugin_interface.h | (Framework ready, implementations TBD) |
| PREFETCHER | Prefetching | plugin_interface.h | (Framework ready, implementations TBD) |

