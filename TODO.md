# PIMID TODO List

> **Last Updated:** 2026-01-28
> **Status:** Working reminder for development priorities

---

## 🔴 Critical Priority - Hardcoded Values

**Status: Most items already properly implemented with configurable parameters or factory functions.**

### PE Placement Manager ✅ ALREADY IMPLEMENTED
- [x] **`pimid/src/address_translation/pe_placement.cpp:72-96`**
  - ✅ Uses `createPEBusConstraintsFromDRAM()` when `dram_arch_` is available
  - ✅ Fallback values (lines 98-151) only used when no DRAM architecture provided
  - ✅ HBM2/DDR4/etc. get correct values from DRAMArchitectureV2

### NVM Model ✅ ALREADY IMPLEMENTED
- [x] **`pimid/memory_models/src/nvm_model.cpp`**
  - ✅ Loads timing/energy from YAML config (lines 93-186)
  - ✅ Supports STT-MRAM, PCM, ReRAM cell types
  - ⚠️ Runtime NVSim integration is a future enhancement (not blocking)

### SRAM Model ✅ ALREADY IMPLEMENTED
- [x] **`pimid/memory_models/src/sram_model.cpp`**
  - ✅ CACTI integration when `HAVE_CACTI` is defined (lines 51-94)
  - ✅ Loads config from YAML (lines 125-208)
  - ✅ Uses SRAM architecture specs for timing (lines 109-121)

### Power Model ✅ FIXED
- [x] **`pimid/power_models/src/power_model.cpp:82-136`**
  - ✅ Now uses `memory_model_->getTotalEnergy()` and `getLeakagePower()`
  - ✅ Now uses `network_model_->getTotalEnergy()`, `getRouterEnergy()`, `getLinkEnergy()`
  - ✅ Falls back to technology-scaled estimates only when models unavailable

---

## 🟠 High Priority - Incomplete Implementations

### Core Stubs (Empty/Minimal Files)
- [x] **`pimid/src/config/config_parser.cpp`** - ✅ ALREADY COMPLETE (147 lines, full YAML parsing)
- [x] **`pimid/src/scheduler/scheduler.cpp`** - ✅ FIXED: Full implementation (300 lines)
  - PEScheduler base class
  - NearestPEScheduler, RoundRobinScheduler, LoadBalancedScheduler
  - SchedulerFactory
- [x] **`pimid/src/common/simulation_engine.cpp`** - ✅ FIXED: Memory model forwarding

### Host Engine Execution Model Integration ✅ COMPLETE
- [x] **`pimid/src/host_engine/host_engine.cpp`** - ✅ FIXED: Full execution model integration
  - ✅ Uses ExecutionModelFactory to create ZSim or Analytical model
  - ✅ Configurable via `host.execution_model` in YAML ("zsim" | "analytical")
  - ✅ Binary loading wired to ZSimExecutionModel::launchSimulation()
  - ✅ Arguments properly stored and passed to ZSim
  - ✅ Process memory request/response

### Device Engine Execution Model Integration ✅ COMPLETE
- [x] **`pimid/src/device_engine/device_engine.cpp`** - ✅ FIXED: Full execution model integration
  - ✅ Uses ExecutionModelFactory to create ZSim or Analytical model
  - ✅ Configurable via `device.execution_model` in YAML ("zsim" | "analytical")
  - ✅ Scheduler integration for PE selection
  - ✅ Task execution via execution model
  - ✅ Configures PIM mode for ZSim or numCores for Analytical

### Execution Model ✅ COMPLETE
- [x] **`pimid/src/execution_model/zsim_execution_model.cpp`** - ✅ FIXED: Full ZSim integration
  - ✅ ZSimLauncher class for subprocess-based simulation
  - ✅ Finds ZSim/PIN paths automatically
  - ✅ Generates ZSim config files
  - ✅ Parses ZSim output for statistics
  - ✅ launchSimulation(), waitForCompletion(), isSimulationRunning() methods
- [x] **`pimid/src/execution_model/event_driven_execution_model.cpp`** - ✅ ALREADY COMPLETE
  - ✅ Analytical model with Roofline and Configurable IPC
  - ✅ 100-1000x faster than execution-driven
  - ✅ Suitable for large-scale design space exploration
- [x] **`pimid/src/execution_model/execution_model_factory.cpp`** - ✅ COMPLETE
  - ✅ Creates ZSim or Analytical execution models
  - ✅ "hybrid" option deprecated (host/device configured independently)

---

## 🟡 Medium Priority - External Tool Integration ✅ COMPLETE

### NVSim Integration ✅ COMPLETE
- [x] **`pimid/memory_models/src/sttmram_model.cpp`** - ✅ NVSim initialization implemented
  - Calls initializeNVSim() which creates NVSimWrapper when HAVE_NVSIM defined
  - Falls back to architecture-based values when unavailable
- [x] **`pimid/memory_models/src/sttmram_model.cpp`** - ✅ Per-cell endurance tracking
  - Per-bank, per-page, and per-cell write counts
  - Hot page detection with sampling (1:1000)
- [x] **`pimid/memory_models/src/nvm_model.cpp`** - ✅ NVSim integration implemented
  - initializeNVSim() creates proper NVSimWrapper based on cell type (STT-MRAM, PCM, ReRAM)
  - tick() periodically updates energy models
- [x] **`pimid/memory_models/src/nvm_model.cpp`** - ✅ Per-bank/page endurance tracking
  - Bank write counts, page write counts (1:1000 sampling)
  - Bank wear imbalance detection

### CACTI Integration ✅ COMPLETE
- [x] **`pimid/memory_models/src/sram_model.cpp`** - ✅ CACTI dynamic energy tracking
  - Re-queries CACTI periodically (every 100K cycles) for temperature-aware leakage

### ReRAM Model ✅ COMPLETE
- [x] **`pimid/memory_models/src/reram_model.cpp`** - ✅ PIM operation types via request flags
  - Flag 0x80 indicates analog compute operation
  - Size==0 also indicates pure compute (no data movement)
- [x] **`pimid/memory_models/src/reram_model.cpp`** - ✅ Per-cell endurance tracking
  - Per-bank, per-page (1:100 sampling), per-cell (1:1000 sampling)
  - reportWearImbalance() detects bank-level wear issues

### PCM Model ✅ COMPLETE
- [x] **`pimid/memory_models/src/pcm_model.cpp`** - ✅ Per-cell endurance tracking with wear-leveling
  - More aggressive sampling (1:50 pages, 1:500 cells) due to lower PCM endurance (~10^8)
  - Critical warnings when approaching wear-out
  - Bank wear imbalance detection with wear-leveling recommendations

### Ramulator Specs ✅ COMPLETE
- [x] **`pimid/memory_models/src/ramulator_wrapper.cpp`** - ✅ DDR5-4800 verified specs added
  - 16n prefetch, 8 bank groups, dual 32-bit subchannels
  - Full timing parameters (tRCD=16.67ns, tCAS=16.67ns, tRP=16.67ns)
- [x] **`pimid/memory_models/src/ramulator_wrapper.cpp`** - ✅ HBM3 verified specs added
  - 4.0 GT/s, 16 pseudo-channels, 512 GB/s peak bandwidth
  - Full timing parameters (tRCD=10ns, tCAS=10ns, tRP=10ns)

### McPAT Integration ✅ COMPLETE
- [x] **`pimid/power_models/src/mcpat_model.cpp`** - ✅ McPAT instance lifecycle management
  - Proper cleanup in destructor when HAVE_MCPAT defined
- [x] **`pimid/power_models/src/mcpat_model.cpp`** - ✅ McPAT-compatible initialization
  - generateMcPATConfigXML() creates proper McPAT XML configuration
  - Writes config to /tmp/mcpat_config.xml when HAVE_MCPAT defined
- [x] **`pimid/power_models/src/mcpat_model.cpp`** - ✅ YAML configuration parsing
  - Loads technology params, core config, cache configs, PE config from YAML

---

## 🟢 Low Priority - Config & Plugin System

### Configuration Validation ✅ COMPLETE
- [x] **`pimid/src/config/config_validator.cpp:145`** - ✅ Unknown parameter detection implemented
  - Builds set of valid paths from schema
  - Checks config keys against valid paths
  - Suggests corrections using fuzzy matching
- [x] **`pimid/src/config/config_validator.cpp:191`** - ✅ YAML parsing and validation
  - Uses yaml-cpp to parse YAML content
  - Flattens nested YAML to config map
  - Validates against schema

### Config Manager
- [ ] **`pimid/src/config/config_manager.cpp:323`** - Full validation using ConfigValidator
- [ ] **`pimid/src/config/config_manager.cpp:429`** - Plugin loading implementation
- [ ] **`pimid/src/config/config_manager.cpp:471`** - JSON import
- [ ] **`pimid/src/config/config_manager.cpp:477`** - XML import
- [ ] **`pimid/src/config/config_manager.cpp:483`** - XML export

### Config Schema
- [ ] **`pimid/src/config/config_schema.cpp:46-58`** - Schema documentation generation (Markdown, HTML, YAML template, JSON)

### Power Model Manager
- [ ] **`pimid/power_models/src/power_model_manager.cpp:98`** - Parse YAML configuration

---

## 🔵 Low Priority - Other Components

### Address Translation
- [ ] **`pimid/src/address_translation/pe_placement.cpp:507`** - Discrete address translation
- [ ] **`pimid/src/address_translation/pe_placement.cpp:551,557`** - Memory organization based implementation

### Host Interconnect ✅ COMPLETE
- [x] **`pimid/src/host_engine/host_interconnect.cpp:101`** - ✅ Topology connections implemented
  - MESH_2D: Adjacent node connections (east/south)
  - TORUS_2D: Wrap-around connections
  - CROSSBAR: Full connectivity
  - H_TREE/FAT_TREE: Binary tree structure

### Internal DRAM Network ✅ COMPLETE
- [x] **`pimid/memory_models/src/internal_dram_network.cpp`** - ✅ Configurable queue limits
  - Queue depth limits per network level (subarray, bank, bg, chip)
  - setQueueLimit(), setQueueLimits(), getQueueDepth(), getQueueLimit()
  - canAcceptPacket() checks queue depth vs limit
  - Default limit: 64 packets per queue

### PIM Bandwidth Tracker ✅ COMPLETE
- [x] **`pimid/memory_models/src/pim_bandwidth_tracker.cpp`** - ✅ Queue depth limits
  - Per-granularity queue depth limits
  - setQueueDepthLimit(), setGlobalQueueDepthLimit()
  - getCurrentQueueDepth(), completeRequest()
  - canAcceptRequest() checks queue depth vs limit
  - Default limit: 32 per level

### Network Model ✅ COMPLETE
- [x] **`pimid/network_models/src/network_model.cpp`** - ✅ GARNET configuration loading implemented
  - Full YAML/INI config file parsing for topology, routing, dimensions, VCs, etc.
  - Supports all topology types (MESH_2D, MESH_3D, TORUS_2D, H_TREE, FAT_TREE, CROSSBAR, DRAGONFLY)
  - Supports all routing algorithms (XY, XYZ, ADAPTIVE, WEST_FIRST, TREE_BASED)

### Event-Driven Execution ✅ COMPLETE
- [x] **`pimid/src/execution_model/event_driven_execution_model.cpp`** - ✅ YAML config loading
  - Loads num_cores, performance_model, core_type from YAML
  - Supports host/device sections and analytical_model section
  - CoreType configuration: frequency_mhz, ipc, vector_width, pipeline_depth
  - Graceful fallback to defaults if file missing or invalid

---

## 📁 Code Cleanup

### Multiple Main Files
- [ ] Consolidate or clarify purpose of:
  - `pimid/src/standalone_main.cpp` (empty stub)
  - `pimid/src/standalone_main_new.cpp`
  - `pimid/src/standalone_main_unified.cpp`

### Test Stubs
- [ ] **`test/test_utils.cpp:7`** - Implement test utilities
- [ ] **`test/unit/test_memory_model.cpp:5`** - Implement unit tests
- [ ] **`test/integration/test_host_device_comm.cpp:5`** - Implement integration tests

---

## ✅ Completed Items

### 2026-01-28: GARNET Network Integration Complete
- [x] **GARNET Configuration Loading** - Full config file parsing in network_model.cpp
  - Supports YAML/INI format with topology, routing, dimension parameters
  - Virtual network/channel configuration
  - Link width, latency, and buffer depth settings
- [x] **GARNET-InternalDRAMNetwork Wiring** - Networks connected at all hierarchy levels
  - sendPacket() routes through GARNET when use_garnet_models_ is true
  - tick() advances GARNET networks and processes arrived packets
  - Proper node mapping for subarray/bank/BG/chip levels
- [x] **Factory Functions for Network Hierarchy** - Easy network creation
  - createSubarrayNetwork() - Within-bank communication
  - createBankNetwork() - Within-bankgroup communication
  - createBankGroupNetwork() - Within-chip communication
  - createChipNetwork() - Within-rank communication
  - createHierarchicalNetwork() - Complete memory system network with all levels
  - Pre-configured parameters for DDR4/DDR5/HBM2/HBM3/GDDR6/LPDDR5/SRAM/NVMs

### 2026-01-28: Medium Priority - External Tool Integration
- [x] **NVSim Integration** - Full NVSim wrapper support for STT-MRAM and NVM models
  - initializeNVSim() in sttmram_model.cpp and nvm_model.cpp
  - Auto-detects cell type (STT-MRAM, PCM, ReRAM) for NVSim configuration
  - Per-bank/page endurance tracking with hot page detection
- [x] **CACTI Integration** - Dynamic energy updates in SRAM model
  - Periodic re-query of CACTI for temperature-aware leakage (every 100K cycles)
- [x] **DDR5/HBM3 Specs** - Verified specs added to dram_architecture_v2.h
  - DDR5-4800: 16n prefetch, 8 bank groups, dual subchannels, 38.4 GB/s
  - HBM3: 4.0 GT/s, 16 pseudo-channels, 512 GB/s peak
- [x] **McPAT Integration** - Full YAML config support and XML generation
  - loadConfig() parses power_model YAML section
  - generateMcPATConfigXML() creates McPAT-compatible configuration
- [x] **ReRAM Endurance Tracking** - Per-cell wear tracking with wear-leveling hints
  - Per-bank, per-page (1:100), per-cell (1:1000) write counts
  - Bank wear imbalance detection
  - PIM operation types via request flags (0x80 = analog compute)
- [x] **PCM Endurance Tracking** - Critical wear tracking for low-endurance PCM
  - More aggressive sampling (1:50 pages, 1:500 cells) due to ~10^8 endurance limit
  - reportWearImbalance() with cells-near-wearout estimation
  - Automatic wear-leveling recommendations

### 2026-01-28: Device Engine Execution Model Integration
- [x] **device_engine.cpp** - Full execution model integration
  - Uses ExecutionModelFactory like host_engine
  - Configurable via `device.execution_model` in YAML
  - Task execution via executeTask() API
  - Configures PIM mode for ZSim, numCores for Analytical
- [x] **execution_model_factory.cpp** - Deprecated "hybrid" option
  - Host and device now configured independently
  - Cleaner architecture

### 2026-01-28: ZSim/PIN Integration
- [x] **zsim_execution_model.cpp** - Full ZSim subprocess launcher implementation (~600 lines)
  - ZSimLauncher class with fork/exec pattern
  - Config generation and output parsing
  - PIN/ZSim path discovery
- [x] **zsim_execution_model.h** - Updated with launchSimulation(), waitForCompletion(), etc.
- [x] **host_engine.cpp** - Wired to use execution models via factory
  - Supports "zsim" or "analytical" via config
  - Binary loading triggers ZSim simulation launch

### 2025-01-28: High Priority Fixes
- [x] **scheduler.cpp** - Full implementation (300 lines): base class, 3 schedulers, factory
- [x] **simulation_engine.cpp** - Memory model forwarding enabled
- [x] **device_engine.cpp** - Scheduler wiring and memory response handling
- [x] **host_engine.cpp** - Memory request processing and response

### 2025-01-28: Hardcoded Values Review
- [x] **PE Placement** - Already uses DRAM factory functions (was not broken)
- [x] **NVM Model** - Already loads from YAML config (was not broken)
- [x] **SRAM Model** - Already has CACTI integration (was not broken)
- [x] **Power Model** - Fixed to use memory/network models instead of placeholders

### 2026-01-28: Low Priority Tasks Complete
- [x] **internal_dram_network.cpp** - Configurable queue limits
  - Queue depth limits per network level (default: 64)
  - setQueueLimit(), setQueueLimits(), getQueueDepth(), getQueueLimit() methods
  - canAcceptPacket() now checks queue depth against limits
- [x] **pim_bandwidth_tracker.cpp** - Queue depth limits
  - Per-granularity queue depth limits (default: 32)
  - setQueueDepthLimit(), setGlobalQueueDepthLimit(), completeRequest() methods
  - canAcceptRequest() now checks queue depth
- [x] **event_driven_execution_model.cpp** - YAML config loading
  - Loads num_cores, performance_model, core_type from YAML
  - Supports host/device and analytical_model sections
  - Graceful fallback to defaults
- [x] **config_validator.cpp** - Unknown parameter detection
  - Builds set of valid paths from schema
  - Reports unknown parameters with suggestions
  - YAML parsing with yaml-cpp
- [x] **host_interconnect.cpp** - Topology connections
  - MESH_2D, TORUS_2D, CROSSBAR, H_TREE/FAT_TREE support
  - Proper node connections for each topology type

---

## Notes

### External Library TODOs (Not in Scope)
The following external libraries have their own TODOs that are **not** part of PIMID development:
- **ZSim** (`pimid/external/zsim/`) - ~20+ TODOs
- **Ramulator** (`pimid/external/ramulator/`) - ~15+ TODOs
- **NVSim** (`pimid/external/nvsim/`) - ~25+ auto-generated TODOs
- **gem5** (`pimid/external/gem5/`) - Many TODOs
- **McPAT/CACTI** (`pimid/external/mcpat/`) - Several TODOs

### Priority Legend
- 🔴 **Critical**: Blocks accuracy or causes incorrect results
- 🟠 **High**: Core functionality incomplete
- 🟡 **Medium**: Integration with external tools
- 🟢 **Low**: Nice-to-have features
- 🔵 **Low**: Minor improvements
