# PIMID Plugin System - Quick Start Guide

## Overview

PIMID provides a **modular plugin system** for extending the PIM simulator with custom:
- Memory models (DRAM variants, emerging memories)
- Schedulers (task scheduling algorithms)
- Network topologies (custom interconnects)
- Power models (technology-specific power estimation)
- And more...

## The 4 Main Plugin Types (As Requested)

### 1. Core Types (PE_TYPE - Processing Element Architectures)
**Purpose**: Define custom Processing Element architectures
- Custom accelerator designs
- PE with specialized instruction sets
- Heterogeneous PE types

**Status**: Framework ready, awaiting implementations

---

### 2. Schedulers (SCHEDULER - Task Scheduling Algorithms)
**Purpose**: Implement task scheduling policies for processing elements

**File**: `/home/user/pimid-dev/pimid/include/plugin/scheduler_plugin.h`

**Key Characteristics**:
- Schedule tasks to appropriate PEs
- Consider data locality
- Balance PE load
- Determine task priority handling
- Dynamic vs. static scheduling

**Existing Examples**:
- `DataLocalitySchedulerPlugin` - Optimizes for data locality
- `EnergyAwareSchedulerPlugin` - Minimizes energy consumption
- `CustomSchedulerPlugin` - Generic template

**Policies Supported**:
- NEAREST_PE - Schedule to PE closest to data
- ROUND_ROBIN - Distribute evenly
- LOAD_BALANCED - Consider PE utilization
- PRIORITY_BASED - Use task priorities
- DATA_AWARE - Data locality focused

---

### 3. Memory Models (MEMORY_MODEL - Memory Technology Plugins)
**Purpose**: Implement memory technologies and their characteristics

**File**: `/home/user/pimid-dev/pimid/include/plugin/memory_model_plugin.h`

**Key Characteristics**:
- Read/write latencies
- Bandwidth capacity
- Atomic operation support
- Power modeling capability
- Technology node specifications

**Existing Implementations**:
- DRAM (via Ramulator integration)
- SRAM
- STT-MRAM (Spin-Transfer Torque)
- ReRAM (Resistive RAM)
- PCM (Phase-Change Memory)
- Generic NVM

**Special Component**: `PIMControllerPlugin`
- Tracks bandwidth at DRAM levels
- Models internal DRAM networks
- Enforces port bitwidth constraints

---

### 4. Network Models (NETWORK_TOPOLOGY - NoC Interconnects)
**Purpose**: Define network topologies and routing algorithms

**File**: `/home/user/pimid-dev/pimid/network_models/include/network_model.h`

**Supported Topologies**:
- MESH_2D / MESH_3D - Grid topologies
- TORUS_2D / TORUS_3D - Grid with wraparound
- FAT_TREE - Hierarchical topology
- DRAGONFLY - Scalable interconnect
- H_TREE - DRAM internal interconnects
- CROSSBAR - Full connectivity

**Routing Algorithms**:
- XY/XYZ - Dimension-order routing
- ADAPTIVE - Path selection based on congestion
- WEST_FIRST / NORTH_LAST - Deadlock avoidance
- MINIMAL - Shortest path
- VALIANT - Alternative routing
- TREE_BASED - For hierarchical networks

**Current Implementation**: `GarnetModel`
- GARNET integration for cycle-accurate simulation
- Virtual networks and virtual channels
- Energy modeling for routers and links

---

## System Architecture

```
┌─────────────────────────────┐
│      PIMID Simulator        │
├─────────────────────────────┤
│  Loads plugins via:         │
│  PluginRegistry             │
│  (Singleton pattern)        │
└────────────┬────────────────┘
             │
      ┌──────┴──────────┬──────────┬──────────┐
      │                 │          │          │
      ↓                 ↓          ↓          ↓
  ┌────────────┐  ┌──────────┐┌────────┐┌─────────┐
  │ MEMORY     │  │SCHEDULER ││NETWORK ││ POWER   │
  │ MODELS     │  │PLUGINS   ││PLUGINS ││ PLUGINS │
  └────────────┘  └──────────┘└────────┘└─────────┘
```

---

## How Plugins Work

### 1. **Plugin Creation**
- Inherit from appropriate base class (e.g., `SchedulerPluginBase`)
- Implement required virtual methods
- Create plugin metadata (name, version, description)

### 2. **Registration**
- Provide `extern "C" pimid_register_plugin()` function
- Creates PluginRegistration with metadata and factory function
- Registers with central PluginRegistry

### 3. **Dynamic Loading**
1. Simulator reads `pimid_config.yaml`
2. PluginRegistry discovers .so files in plugin_dir
3. dlopen() loads each library
4. dlsym() finds pimid_register_plugin() function
5. Plugin registers itself with the registry

### 4. **Plugin Instantiation**
- On demand, PluginRegistry creates plugin instances
- Validates configuration against requirements
- Calls initialize() with configuration parameters

### 5. **Usage During Simulation**
- Simulator calls appropriate plugin methods
- Memory model creates MemoryModel instances
- Scheduler plugin creates Scheduler instances
- Network plugin creates NetworkModel instances

---

## Quick Example: Custom Scheduler

### Step 1: Create Header (`my_scheduler.h`)
```cpp
#include "plugin/scheduler_plugin.h"

class MyScheduler : public pimid::plugin::SchedulerPluginBase {
public:
    MyScheduler();
    
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
    double getTypicalOverhead() const override { return 10.0; }
    bool isOptimalForWorkload(const std::string& workload_type) const override;
};
```

### Step 2: Implement & Register (`my_scheduler.cpp`)
```cpp
#include "my_scheduler.h"

// ... implementation of all methods ...

// Registration function
extern "C" {
    void pimid_register_plugin() {
        auto instance = std::make_shared<MyScheduler>();
        pimid::plugin::PluginRegistration reg;
        reg.metadata = instance->getMetadata();
        reg.create_func = []() -> std::shared_ptr<pimid::plugin::IPlugin> {
            return std::make_shared<MyScheduler>();
        };
        pimid::plugin::PluginRegistry::getInstance().registerPlugin(
            "MyScheduler", reg);
    }
}
```

### Step 3: Configure (`pimid_config.yaml`)
```yaml
plugins:
  plugin_dir: "plugins/"
  auto_discover: true
  loaded_plugins:
    - name: "MyScheduler"
      library: "plugins/libmy_scheduler.so"
      config:
        weight_locality: 0.6
        weight_load: 0.4

pim:
  scheduler: "MyScheduler"
```

### Step 4: Build
```bash
# Create CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(MyScheduler)

find_package(PIMID REQUIRED)

add_library(my_scheduler SHARED
    src/my_scheduler.cpp
)

target_include_directories(my_scheduler PRIVATE
    ${PIMID_INCLUDE_DIRS}
    include/
)

target_link_libraries(my_scheduler ${PIMID_LIBRARIES})

install(TARGETS my_scheduler LIBRARY DESTINATION plugins/)
```

```bash
mkdir build && cd build
cmake ..
make
make install  # Installs to plugins/ directory
```

---

## Plugin Type Comparison

| Type | Purpose | Header | Interface | Example |
|------|---------|--------|-----------|---------|
| **PE_TYPE** | Processor architectures | plugin_interface.h | IPlugin | Custom accelerator |
| **SCHEDULER** | Task scheduling | scheduler_plugin.h | ISchedulerPlugin | DataLocalityScheduler |
| **MEMORY_MODEL** | Memory technologies | memory_model_plugin.h | IMemoryModelPlugin | DRAMModel |
| **NETWORK_TOPOLOGY** | NoC topologies | network_model.h | (via enum) | GarnetModel |
| **POWER_MODEL** | Power estimation | plugin_interface.h | IPlugin | (framework ready) |
| **ADDRESS_MAPPER** | Address mapping | plugin_interface.h | IPlugin | (framework ready) |
| **CACHE_REPLACEMENT** | Cache policies | plugin_interface.h | IPlugin | (framework ready) |
| **PREFETCHER** | Prefetching | plugin_interface.h | IPlugin | (framework ready) |

---

## Key Files

**Core Plugin System**:
- `/home/user/pimid-dev/pimid/include/plugin/plugin_interface.h` - Main interface
- `/home/user/pimid-dev/pimid/src/plugin/plugin_interface.cpp` - Registry implementation
- `/home/user/pimid-dev/pimid/include/plugin/scheduler_plugin.h` - Scheduler interface
- `/home/user/pimid-dev/pimid/include/plugin/memory_model_plugin.h` - Memory interface

**Implementations**:
- `/home/user/pimid-dev/pimid/memory_models/` - Memory models
- `/home/user/pimid-dev/pimid/network_models/` - Network models
- `/home/user/pimid-dev/examples/` - Example integrations

**Configuration**:
- `/home/user/pimid-dev/config/pimid_config.yaml` - Main config

**Documentation**:
- `/home/user/pimid-dev/docs/PLUGIN_DEVELOPMENT_GUIDE.md` - Detailed tutorial
- `/home/user/pimid-dev/PLUGIN_ARCHITECTURE_OVERVIEW.md` - Architecture details
- `/home/user/pimid-dev/PLUGIN_FILES_REFERENCE.md` - File locations

---

## Configuration Example

```yaml
# pimid_config.yaml - Plugin Configuration Section

plugins:
  # Directory for plugin discovery
  plugin_dir: "plugins/"
  
  # Auto-discover plugins
  auto_discover: true
  
  # Explicitly loaded plugins
  loaded_plugins:
    # Example: Custom Memory Model
    - name: "CustomMemory"
      library: "plugins/libcustom_memory.so"
      config:
        read_latency: 50
        write_latency: 100
        bandwidth: 25600
    
    # Example: Custom Scheduler
    - name: "MyScheduler"
      library: "plugins/libmy_scheduler.so"
      config:
        locality_weight: 0.6
        load_weight: 0.4
  
  # Additional search paths
  search_paths:
    - "plugins/"
    - "/usr/local/lib/pimid/plugins/"

# Use configured plugins
memory:
  technology: "CustomMemory"

pim:
  scheduler: "MyScheduler"

network:
  topology: "MESH_2D"
```

---

## Common Mistakes to Avoid

1. **Forgetting registration function** - Must have `extern "C" void pimid_register_plugin()`
2. **Incorrect configuration names** - Parameter names must match exactly
3. **Missing validation** - Always validate configuration before use
4. **Not handling defaults** - Use `getConfigValueAs<T>(key, default_value)`
5. **Incorrect enum values** - Check PluginType enum in plugin_interface.h

---

## Debugging Plugins

### Check if plugin is loaded:
```cpp
auto& registry = pimid::plugin::PluginRegistry::getInstance();
std::vector<std::string> names = registry.getPluginNames();
// Check if "MyScheduler" is in the list
```

### Generate plugin documentation:
```cpp
std::string doc = registry.generateAllPluginsDocs();
```

### Validate configuration:
```cpp
std::vector<std::string> errors;
if (!registry.hasPlugin("MyScheduler")) {
    std::cerr << "Plugin not found!" << std::endl;
}
```

---

## Next Steps

1. Read `/home/user/pimid-dev/PLUGIN_ARCHITECTURE_OVERVIEW.md` for detailed information
2. Check `/home/user/pimid-dev/PLUGIN_FILES_REFERENCE.md` for file locations
3. Read `/home/user/pimid-dev/docs/PLUGIN_DEVELOPMENT_GUIDE.md` for step-by-step tutorial
4. Look at existing implementations in `/home/user/pimid-dev/pimid/memory_models/`
5. Create your first plugin following the example above

---

## Support Resources

- **Full Architecture Guide**: PLUGIN_ARCHITECTURE_OVERVIEW.md
- **File Reference**: PLUGIN_FILES_REFERENCE.md
- **Development Tutorial**: docs/PLUGIN_DEVELOPMENT_GUIDE.md
- **Example Code**: examples/external_model_integration/
- **Memory Models**: pimid/memory_models/
- **Network Models**: pimid/network_models/

