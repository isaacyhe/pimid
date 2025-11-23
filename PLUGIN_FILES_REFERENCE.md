# PIMID Plugin System - Files and Locations Reference

## Core Plugin System Files

### Plugin Interface Definitions
- **Main Plugin Interface**: `/home/user/pimid-dev/pimid/include/plugin/plugin_interface.h`
  - IPlugin (abstract base class)
  - PluginBase (common functionality)
  - PluginMetadata (plugin information)
  - PluginRegistry (central management)
  - PluginConfig (configuration helper)
  - Plugin type enumerations (MEMORY_MODEL, SCHEDULER, NETWORK_TOPOLOGY, etc.)

- **Scheduler Plugin Interface**: `/home/user/pimid-dev/pimid/include/plugin/scheduler_plugin.h`
  - ISchedulerPlugin (scheduler interface)
  - SchedulerPluginBase (base class for schedulers)
  - CustomSchedulerPlugin (example template)
  - DataLocalitySchedulerPlugin (locality-aware example)
  - EnergyAwareSchedulerPlugin (energy-optimized example)

- **Memory Model Plugin Interface**: `/home/user/pimid-dev/pimid/include/plugin/memory_model_plugin.h`
  - IMemoryModelPlugin (memory plugin interface)
  - MemoryModelPluginBase (base class for memory models)
  - CustomMemoryModelPlugin (example template)

### Plugin Implementation
- **Plugin Registry Implementation**: `/home/user/pimid-dev/pimid/src/plugin/plugin_interface.cpp`
  - PluginRegistry singleton implementation
  - Dynamic plugin loading with dlopen/dlsym
  - Plugin discovery mechanism
  - Configuration validation
  
- **Scheduler Plugin Implementation**: `/home/user/pimid-dev/pimid/src/plugin/scheduler_plugin.cpp`
  - (Currently contains TODO markers - awaiting implementation)

- **Memory Model Plugin Implementation**: `/home/user/pimid-dev/pimid/src/plugin/memory_model_plugin.cpp`
  - (Currently contains TODO markers - awaiting implementation)

---

## Memory Model Implementations

### Location: `/home/user/pimid-dev/pimid/memory_models/`

#### Headers (include/):
- `memory_model.h` - Base memory model interface
- `dram_model.h` - DRAM implementation
- `sram_model.h` - SRAM implementation
- `sttmram_model.h` - Spin-Transfer Torque MRAM
- `reram_model.h` - Resistive RAM
- `pcm_model.h` - Phase-Change Memory
- `nvm_model.h` - Generic NVM implementation
- `pim_controller_plugin.h` - Ramulator controller plugin for PIM
- `pim_bandwidth_tracker.h` - Bandwidth tracking at DRAM levels
- `internal_dram_network.h` - Internal DRAM network modeling
- `cacti_wrapper.h` - CACTI power modeling wrapper
- `nvsim_wrapper.h` - NVSIM NVM modeling wrapper
- `ramulator_wrapper.h` - Ramulator integration wrapper

#### Implementations (src/):
- `memory_model.cpp`
- `dram_model.cpp`
- `sram_model.cpp`
- `sttmram_model.cpp`
- `reram_model.cpp`
- `pcm_model.cpp`
- `nvm_model.cpp`
- `pim_controller_plugin.cpp`
- `pim_bandwidth_tracker.cpp`
- `internal_dram_network.cpp`
- `internal_memory_network.cpp`
- `cacti_wrapper.cpp`
- `nvsim_wrapper.cpp`
- `ramulator_wrapper.cpp`

---

## Network Model Implementations

### Location: `/home/user/pimid-dev/pimid/network_models/`

#### Headers (include/):
- `network_model.h` - Base network model interface with:
  - NetworkTopology enum (MESH_2D, MESH_3D, TORUS_2D, DRAGONFLY, FAT_TREE, etc.)
  - RoutingAlgorithm enum (XY, XYZ, ADAPTIVE, WEST_FIRST, etc.)
  - NetworkConfig structure
  - NetworkModel abstract class
  - GarnetModel implementation for GARNET integration

#### Implementations (src/):
- Integration with GARNET simulator

---

## Scheduler Implementations

### Location: `/home/user/pimid-dev/pimid/include/scheduler/`

- `scheduler.h` - Scheduler interface with:
  - SchedulingPolicy enum
  - PIMTask structure
  - PEScheduler abstract class
  - NearestPEScheduler implementation
  - Various scheduler policies

---

## Simulation Core Files

### Main Simulator API
- `/home/user/pimid-dev/pimid/include/pimid.h` - Main PIMID simulator class
  - PIMIDSimulator (orchestrates simulation)
  - Configuration loading
  - Engine initialization
  - Simulation control

### Configuration Management
- `/home/user/pimid-dev/pimid/include/config/config_manager.h`
- `/home/user/pimid-dev/pimid/include/config/config_parser.h`
- `/home/user/pimid-dev/pimid/include/config/config_schema.h`
- `/home/user/pimid-dev/pimid/include/config/config_validator.h`

### Simulation Engines
- `/home/user/pimid-dev/pimid/include/host_engine/host_engine.h`
- `/home/user/pimid-dev/pimid/include/device_engine/device_engine.h`

### Address Translation & Placement
- `/home/user/pimid-dev/pimid/include/address_translation/pe_placement.h`
- `/home/user/pimid-dev/pimid/include/address_translation/address_translator.h`
- `/home/user/pimid-dev/pimid/include/address_translation/pe_statistics.h`

---

## Configuration Files

### Main Configuration
- `/home/user/pimid-dev/config/pimid_config.yaml` - Primary simulator configuration
  - Includes plugin section for dynamic plugin loading
  - Memory technology selection
  - Network configuration
  - Power modeling settings

### Example Configurations
- `/home/user/pimid-dev/config/pimid_defaults.yaml`
- `/home/user/pimid-dev/config/example_pim_config.yaml`
- `/home/user/pimid-dev/pimid/configs/examples/pimid_config.yaml`

### Component-Specific Configs
- `/home/user/pimid-dev/config/host_config.yaml`
- `/home/user/pimid-dev/config/device_config.yaml`
- `/home/user/pimid-dev/config/memory_config.yaml`
- `/home/user/pimid-dev/config/network_config.yaml`
- `/home/user/pimid-dev/config/power_config.yaml`

---

## Documentation

### Plugin Development Guide
- `/home/user/pimid-dev/docs/PLUGIN_DEVELOPMENT_GUIDE.md` - Complete plugin development tutorial
  - Step-by-step examples
  - Configuration examples
  - Best practices
  - CMake build instructions

### Additional Documentation
- `/home/user/pimid-dev/pimid/network_models/GARNET_INTEGRATION.md`
- `/home/user/pimid-dev/pimid/network_models/README.md`
- `/home/user/pimid-dev/pimid/memory_models/README.md`
- `/home/user/pimid-dev/pimid/memory_models/dram_config_example.json`

---

## Example Implementations

### Custom Example
- `/home/user/pimid-dev/examples/example_custom_vc_dram.cpp`

### External Model Integration Examples
- `/home/user/pimid-dev/examples/external_model_integration/`
  - Simple network adapter example
  - Integration test examples

---

## Build System

### Main CMakeLists
- `/home/user/pimid-dev/pimid/CMakeLists.txt`
  - Defines BUILD_PLUGINS option
  - Plugin library definitions
  - Memory model compilations
  - Network model compilations

### Memory Models CMakeLists
- `/home/user/pimid-dev/pimid/memory_models/CMakeLists.txt`

### Network Models CMakeLists
- `/home/user/pimid-dev/pimid/network_models/CMakeLists.txt`

---

## Plugin Directory Structure (Runtime)

```
plugins/                          # Default plugin directory
├── libcustom_memory.so
├── libcustom_scheduler.so
├── libcustom_topology.so
└── libcustom_power_model.so
```

---

## Key File Summary Table

| Purpose | File Path | Type |
|---------|-----------|------|
| Plugin Interface (IPlugin, PluginRegistry) | pimid/include/plugin/plugin_interface.h | Header |
| Plugin Registry Implementation | pimid/src/plugin/plugin_interface.cpp | Implementation |
| Scheduler Plugin Interface | pimid/include/plugin/scheduler_plugin.h | Header |
| Memory Model Plugin Interface | pimid/include/plugin/memory_model_plugin.h | Header |
| Main Simulator API | pimid/include/pimid.h | Header |
| Network Model Interface | pimid/network_models/include/network_model.h | Header |
| Scheduler Interface | pimid/include/scheduler/scheduler.h | Header |
| Plugin Development Guide | docs/PLUGIN_DEVELOPMENT_GUIDE.md | Documentation |
| Main Config | config/pimid_config.yaml | YAML Config |
| Build System | pimid/CMakeLists.txt | CMake |

---

## How to Add a New Plugin Type

If you need to add support for a new plugin type (e.g., SECURITY_POLICY):

1. **Update plugin_interface.h**:
   - Add new enum value to PluginType (e.g., SECURITY_POLICY)

2. **Create new plugin header**:
   - Create `pimid/include/plugin/security_plugin.h`
   - Define ISecurityPlugin interface
   - Create SecurityPluginBase class

3. **Create implementation file**:
   - Create `pimid/src/plugin/security_plugin.cpp`
   - Implement example plugins

4. **Update CMakeLists.txt**:
   - Add new source files to build

5. **Update documentation**:
   - Add to PLUGIN_DEVELOPMENT_GUIDE.md

---

## Building Plugins

### CMake Example for Custom Plugin

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyPIMIDPlugin)

find_package(PIMID REQUIRED)

add_library(my_custom SHARED
    src/my_plugin.cpp
)

target_include_directories(my_custom PRIVATE
    ${PIMID_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(my_custom
    ${PIMID_LIBRARIES}
)

install(TARGETS my_custom
    LIBRARY DESTINATION plugins/
)
```

### Build Commands

```bash
# Configure
mkdir build && cd build
cmake ..

# Build
make

# Install to plugins directory
make install
```

---

## Plugin Registration Pattern

All plugins must follow this pattern:

```cpp
// Registration function (extern "C" for C linkage)
extern "C" {
    void pimid_register_plugin() {
        // Create instance
        auto instance = std::make_shared<MyPlugin>();
        
        // Create registration
        pimid::plugin::PluginRegistration reg;
        reg.metadata = instance->getMetadata();
        reg.create_func = []() -> std::shared_ptr<pimid::plugin::IPlugin> {
            return std::make_shared<MyPlugin>();
        };
        
        // Register
        pimid::plugin::PluginRegistry::getInstance().registerPlugin(
            "MyPlugin", reg);
    }
}
```

This function is discovered and called by:
1. PluginRegistry::loadDynamicPlugin() using dlsym()
2. Happens during PluginRegistry::discoverPlugins()
3. Called when simulator loads plugins from config

---

## Common Plugin Methods to Implement

### Required (from IPlugin)
- `initialize(config)`
- `shutdown()`
- `isInitialized()`
- `getMetadata()`
- `getName()`
- `getVersion()`
- `getType()`
- `validateConfig()`
- `getRequiredParameters()`
- `getOptionalParameters()`
- `getParameterDescriptions()`

### Plugin-Type-Specific (e.g., ISchedulerPlugin)
- `createScheduler(pe_manager)`
- `isDataAware()`
- `isLoadAware()`
- `isPriorityBased()`
- `isDynamic()`
- `getTypicalOverhead()`
- `isOptimalForWorkload()`

