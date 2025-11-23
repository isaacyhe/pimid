# PIMID Plugin System - Status & Usage Guide

## Executive Summary

This document provides a **comprehensive review** of the 4 main plugin types requested:
1. **Core Models (PE_TYPE)** - Processing Element architectures
2. **Schedulers (SCHEDULER)** - Task scheduling algorithms
3. **Memory Models (MEMORY_MODEL)** - Memory technology plugins
4. **Network Models (NETWORK_TOPOLOGY)** - Network-on-chip topologies

**Current Status**: All 4 plugin types are now **fully implemented and usable**! ✅

---

## Status Before vs After Implementation

### Before (What I Found)

| Plugin Type | Header | Implementation | Status |
|------------|--------|----------------|---------|
| **Schedulers** | ✅ Defined | ❌ Empty TODO | **Not Usable** |
| **Memory Models** | ✅ Defined | ❌ Empty TODO | **Not Usable** |
| **Network Models** | ✅ Defined | ✅ Implemented | **Usable** |
| **Core Models (PE)** | ⚠️ Enum only | ❌ No interface | **Not Usable** |

### After (What I Implemented)

| Plugin Type | Header | Implementation | Examples | Status |
|------------|--------|----------------|----------|---------|
| **Schedulers** | ✅ Defined | ✅ **Implemented** | 3 plugins | **✅ Fully Usable** |
| **Memory Models** | ✅ Defined | ✅ **Implemented** | 1 template | **✅ Fully Usable** |
| **Network Models** | ✅ Defined | ✅ Implemented | GarnetModel | **✅ Fully Usable** |
| **Core Models (PE)** | ✅ **Created** | ✅ **Implemented** | 3 plugins | **✅ Fully Usable** |

---

## 1. Core Models (PE_TYPE) - Processing Element Plugins

### Status: ✅ **FULLY IMPLEMENTED** (Created from scratch)

### What Was Done
- Created complete PE plugin interface: `pimid/include/plugin/pe_plugin.h`
- Implemented 3 example PE plugins: `pimid/src/plugin/pe_plugin.cpp`
- Defined `ProcessingElement` base class for PE instances
- Created `PEArchitecture` specification structure

### Example Plugins Provided

#### 1. ScalarPEPlugin - Simple Scalar PE
```cpp
// Basic scalar processing element
// - 32 registers, 64-bit width
// - Integer + Floating-point ops
// - 1 GHz, 0.5 mm², 50 mW
```

#### 2. VectorPEPlugin - SIMD/Vector PE
```cpp
// SIMD/vector processing element
// - 32 vector registers, 512-bit width
// - Vector width: 8 elements in parallel
// - 1 GHz, 2.0 mm², 150 mW
```

#### 3. MatrixPEPlugin - Matrix/Tensor PE
```cpp
// Matrix multiplication accelerator
// - 64 registers, 256-bit width
// - 16x16 matrix operations
// - 1 GHz, 5.0 mm², 300 mW
```

### How Easy Is It to Add a New PE Plugin?

**Very Easy!** Just 3 steps:

**Step 1**: Create your PE plugin class
```cpp
#include "plugin/pe_plugin.h"

class MyCustomPEPlugin : public pimid::plugin::PEPluginBase {
public:
    MyCustomPEPlugin();

    // Implement required methods
    bool initialize(const std::map<std::string, std::string>& config) override;
    void shutdown() override;

    std::unique_ptr<pimid::ProcessingElement> createPE(
        uint32_t pe_id,
        pimid::PEPlacementManager* placement_manager) override;

    pimid::PEArchitecture getArchitecture() const override;

    // PE characteristics
    bool isVectorPE() const override { return true; }
    double getPeakGFLOPS() const override { return 10.0; }
    // ... other methods
};
```

**Step 2**: Implement the constructor and methods
```cpp
MyCustomPEPlugin::MyCustomPEPlugin()
    : PEPluginBase(PluginMetadata{
        "MyCustomPE",
        "1.0.0",
        "Your Name",
        "My custom PE architecture",
        PluginType::PE_TYPE
    }) {

    // Set up architecture
    arch_.name = "MyCustomPE";
    arch_.vector_width = 16;
    arch_.frequency_mhz = 2000.0;
    // ... configure architecture
}
```

**Step 3**: Register and configure
```yaml
# pimid_config.yaml
plugins:
  loaded_plugins:
    - name: "MyCustomPE"
      library: "plugins/libmy_custom_pe.so"
      config:
        vector_width: 16
        frequency_mhz: 2000
        area_mm2: 3.0
```

### Key Features

✅ **Flexible Architecture**: Define custom instruction sets, register files, pipeline stages
✅ **Performance Modeling**: Specify GFLOPS, area, power characteristics
✅ **Operation Latencies**: Customize latency for different operations
✅ **Energy Modeling**: Define static and dynamic power consumption

---

## 2. Schedulers (SCHEDULER) - Task Scheduling Plugins

### Status: ✅ **FULLY IMPLEMENTED** (Previously empty, now complete)

### What Was Done
- Implemented all 3 example scheduler plugins in `pimid/src/plugin/scheduler_plugin.cpp`
- Full validation and configuration support
- Comprehensive documentation

### Example Plugins Implemented

#### 1. CustomSchedulerPlugin - Generic Template
```cpp
// Generic scheduler template for users
// - Data locality aware
// - Load balancing support
// - 10 cycles overhead
```

#### 2. DataLocalitySchedulerPlugin - Data-Centric
```cpp
// Optimizes for data locality
// - Minimizes data movement
// - Considers cache line alignment
// - Optional bank conflict avoidance
// - 5 cycles overhead
```

#### 3. EnergyAwareSchedulerPlugin - Energy-Optimized
```cpp
// Balances energy and performance
// - Configurable energy/performance weights
// - Prefers nearby PEs
// - 15 cycles overhead
```

### How Easy Is It to Add a New Scheduler?

**Very Easy!** Just implement a few methods:

**Step 1**: Create your scheduler plugin
```cpp
#include "plugin/scheduler_plugin.h"

class MySchedulerPlugin : public pimid::plugin::SchedulerPluginBase {
public:
    MySchedulerPlugin();

    // Create scheduler instance
    std::unique_ptr<pimid::PEScheduler> createScheduler(
        pimid::PEPlacementManager* pe_manager) override;

    // Scheduler characteristics
    bool isDataAware() const override { return true; }
    bool isLoadAware() const override { return true; }
    double getTypicalOverhead() const override { return 8.0; }

    // Configuration methods
    bool initialize(const std::map<std::string, std::string>& config) override;
    // ...
};
```

**Step 2**: Configure in YAML
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

### Configuration Example

```yaml
# Use DataLocalityScheduler
pim:
  scheduler: "DataLocalityScheduler"

plugins:
  loaded_plugins:
    - name: "DataLocalityScheduler"
      config:
        cache_line_size: 64
        consider_bank_conflicts: true
```

---

## 3. Memory Models (MEMORY_MODEL) - Memory Technology Plugins

### Status: ✅ **FULLY IMPLEMENTED** (Previously empty, now complete)

### What Was Done
- Implemented `CustomMemoryModelPlugin` in `pimid/src/plugin/memory_model_plugin.cpp`
- Complete parameter validation
- Support for latency, bandwidth, technology node configuration
- Comprehensive documentation

### Template Plugin Implemented

#### CustomMemoryModelPlugin - Configurable Memory Template
```cpp
// Customizable memory model
// - Configurable read/write latencies
// - Bandwidth specification
// - Technology node selection
// - Power modeling support
```

**Default Parameters**:
- Read latency: 50 cycles
- Write latency: 100 cycles
- Bandwidth: 25.6 GB/s
- Technology node: 22nm

### How Easy Is It to Add a New Memory Model?

**Very Easy!** The template handles most of the work:

**Step 1**: Subclass the template (or use it directly)
```cpp
class MyMemoryModelPlugin : public pimid::plugin::MemoryModelPluginBase {
public:
    std::shared_ptr<pimid::MemoryModel> createMemoryModel(
        const std::string& config_path) override {

        // Create your custom memory model
        return std::make_shared<MyCustomMemoryModel>(
            config_path,
            read_latency_,
            write_latency_,
            bandwidth_
        );
    }

    // Specify characteristics
    Cycle getTypicalReadLatency() const override { return 30; }
    Cycle getTypicalWriteLatency() const override { return 60; }
    std::string getTechnologyName() const override { return "MyMemory"; }
};
```

**Step 2**: Configure in YAML
```yaml
memory:
  technology: "MyMemory"

plugins:
  loaded_plugins:
    - name: "MyMemory"
      library: "plugins/libmy_memory.so"
      config:
        read_latency: 30
        write_latency: 60
        bandwidth: 51200  # MB/s
        tech_node_nm: 14
```

### Existing Memory Models

The codebase already has 6 implemented memory models you can use:
1. **DRAM** (via Ramulator integration)
2. **SRAM**
3. **STT-MRAM** (Spin-Transfer Torque)
4. **ReRAM** (Resistive RAM)
5. **PCM** (Phase-Change Memory)
6. **Generic NVM**

---

## 4. Network Models (NETWORK_TOPOLOGY) - NoC Plugins

### Status: ✅ **FULLY IMPLEMENTED** (Already working)

### What Exists
- Complete `NetworkModel` base class
- `GarnetModel` implementation for cycle-accurate NoC simulation
- Support for 8 different topologies

### Supported Topologies

| Topology | Description | Use Case |
|----------|-------------|----------|
| **MESH_2D** | 2D grid network | Standard PIM arrays |
| **MESH_3D** | 3D grid network | 3D-stacked memory |
| **TORUS_2D** | 2D grid with wraparound | Reduce diameter |
| **TORUS_3D** | 3D grid with wraparound | 3D networks |
| **H_TREE** | Hierarchical tree | DRAM internal |
| **FAT_TREE** | Hierarchical with fat links | Data centers |
| **DRAGONFLY** | Group-based topology | Large-scale systems |
| **CROSSBAR** | Full connectivity | Small systems |

### Routing Algorithms Supported

- **XY / XYZ** - Dimension-order routing
- **ADAPTIVE** - Congestion-aware routing
- **WEST_FIRST / NORTH_LAST** - Deadlock avoidance
- **MINIMAL** - Shortest path
- **VALIANT** - Load balancing
- **TREE_BASED** - For hierarchical networks

### How Easy Is It to Configure?

**Very Easy!** Just configure in YAML:

```yaml
network:
  topology: MESH_2D
  routing: XY

  # Topology parameters
  num_rows: 8
  num_cols: 8

  # Virtual channels
  virtual_networks: 2
  virtual_channels_per_vn: 2

  # Link parameters
  link_width_bytes: 8
  link_latency: 1

  # Router parameters
  router_pipeline: FULL  # FULL, REDUCED, SIMPLE, MINIMAL
  input_buffer_depth: 4
```

**Example for 3D Mesh**:
```yaml
network:
  topology: MESH_3D
  routing: XYZ
  num_rows: 4
  num_cols: 4
  num_layers: 4
```

---

## Complete Usage Example - All 4 Plugin Types Together

Here's how you can use all 4 plugin types in a single configuration:

```yaml
# pimid_config.yaml - Complete Plugin Configuration

plugins:
  plugin_dir: "plugins/"
  auto_discover: true

  loaded_plugins:
    # 1. Custom PE Plugin
    - name: "VectorPE"
      library: "plugins/libvector_pe.so"
      config:
        vector_width: 16
        frequency_mhz: 2000
        area_mm2: 3.0
        power_mw: 200

    # 2. Custom Scheduler Plugin
    - name: "DataLocalityScheduler"
      config:
        cache_line_size: 64
        consider_bank_conflicts: true

    # 3. Custom Memory Model Plugin
    - name: "CustomMemory"
      library: "plugins/libcustom_memory.so"
      config:
        read_latency: 40
        write_latency: 80
        bandwidth: 51200
        tech_name: "HBM2"
        tech_node_nm: 14

# Use the plugins
pim:
  pe_type: "VectorPE"          # Use custom PE
  scheduler: "DataLocalityScheduler"  # Use custom scheduler

memory:
  technology: "CustomMemory"    # Use custom memory

network:
  topology: MESH_2D              # Use network topology
  routing: XY
  num_rows: 8
  num_cols: 8
```

---

## Ease of Use Assessment

### ✅ Very Easy to Add

All 4 plugin types follow the same pattern:

1. **Inherit from Base Class** (e.g., `PEPluginBase`, `SchedulerPluginBase`)
2. **Implement Required Methods** (well-defined interfaces)
3. **Register Plugin** (simple `extern "C"` function)
4. **Configure in YAML** (straightforward key-value pairs)

### Example: Adding a New Plugin (5 steps)

```bash
# 1. Create header file
cat > my_plugin.h << 'EOF'
#include "plugin/scheduler_plugin.h"
class MyPlugin : public pimid::plugin::SchedulerPluginBase {
    // ... implement interface
};
EOF

# 2. Create implementation
cat > my_plugin.cpp << 'EOF'
#include "my_plugin.h"
MyPlugin::MyPlugin() : SchedulerPluginBase(...) {}
// ... implement methods

extern "C" {
    void pimid_register_plugin() {
        // Register plugin
    }
}
EOF

# 3. Create CMakeLists.txt
cat > CMakeLists.txt << 'EOF'
add_library(my_plugin SHARED my_plugin.cpp)
target_link_libraries(my_plugin ${PIMID_LIBRARIES})
EOF

# 4. Build
mkdir build && cd build && cmake .. && make

# 5. Configure
# Edit pimid_config.yaml to load the plugin
```

---

## Summary: Are They Really Usable?

### ✅ YES! All 4 Plugin Types Are Fully Usable

| Plugin Type | Ease of Adding | Existing Examples | Documentation |
|-------------|----------------|-------------------|---------------|
| **Core Models (PE)** | ⭐⭐⭐⭐⭐ Very Easy | 3 plugins | ✅ Complete |
| **Schedulers** | ⭐⭐⭐⭐⭐ Very Easy | 3 plugins | ✅ Complete |
| **Memory Models** | ⭐⭐⭐⭐⭐ Very Easy | 1 template + 6 existing | ✅ Complete |
| **Network Models** | ⭐⭐⭐⭐ Easy | 1 (GarnetModel) | ✅ Complete |

### What Makes Them Easy to Use?

1. **Clear Interfaces** - Well-defined base classes with documented methods
2. **Working Examples** - 3 examples for PE, 3 for schedulers, template for memory
3. **Validation Support** - Built-in configuration validation
4. **Good Defaults** - Optional parameters with sensible defaults
5. **YAML Configuration** - Simple, readable configuration format
6. **Documentation** - Comprehensive comments and usage examples

---

## Files Created/Modified

### Created (New Files)
1. `pimid/include/plugin/pe_plugin.h` - PE plugin interface
2. `pimid/src/plugin/pe_plugin.cpp` - PE plugin implementation

### Modified (Implemented Empty Files)
1. `pimid/src/plugin/scheduler_plugin.cpp` - Scheduler implementations
2. `pimid/src/plugin/memory_model_plugin.cpp` - Memory model implementation

### Documentation
1. `PLUGIN_QUICK_START.md` - Quick start guide
2. `PLUGIN_ARCHITECTURE_OVERVIEW.md` - Detailed architecture
3. `PLUGIN_SYSTEM_STATUS_AND_USAGE_GUIDE.md` - This file!

---

## Recommendations

### For Core Models (PE_TYPE)
- ✅ **Ready to use** - 3 example plugins provided
- Use `ScalarPEPlugin` for simple processing elements
- Use `VectorPEPlugin` for SIMD workloads
- Use `MatrixPEPlugin` for AI/ML workloads
- Easy to extend for custom accelerators

### For Schedulers
- ✅ **Ready to use** - 3 example plugins provided
- Use `DataLocalityScheduler` for memory-intensive workloads
- Use `EnergyAwareScheduler` for power-constrained systems
- Use `CustomScheduler` as a starting template

### For Memory Models
- ✅ **Ready to use** - Template + 6 existing models
- Use `CustomMemoryModelPlugin` as a template
- Existing DRAM/SRAM/NVM models available
- Easy to model emerging memory technologies

### For Network Models
- ✅ **Already working** - GarnetModel fully implemented
- 8 topologies supported out of the box
- Multiple routing algorithms available
- Comprehensive configuration options

---

## Next Steps

1. **Try the Examples** - Run simulations with the provided plugins
2. **Create Custom Plugins** - Follow the templates to create your own
3. **Mix and Match** - Combine different plugin types for your use case
4. **Extend Further** - The plugin system is extensible for future needs

---

## Conclusion

**All 4 plugin types are now fully implemented and ready to use!**

- ✅ Core Models (PE_TYPE) - **Created from scratch, 3 examples**
- ✅ Schedulers - **Fully implemented, 3 examples**
- ✅ Memory Models - **Fully implemented, 1 template + 6 existing**
- ✅ Network Models - **Already working, 8 topologies**

Adding new plugins is straightforward following the provided examples and templates.
