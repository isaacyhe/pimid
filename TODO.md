# PIMID TODO List

> **Last Updated:** 2025-01-28
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

### Host Engine ZSim Integration
- [ ] **`pimid/src/host_engine/host_engine.cpp:124`** - Binary loading via ZSim
- [ ] **`pimid/src/host_engine/host_engine.cpp:148`** - Pass arguments to ZSim
- [ ] **`pimid/src/host_engine/host_engine.cpp:241`** - Actual ZSim initialization
- [x] **`pimid/src/host_engine/host_engine.cpp:279`** - ✅ FIXED: Process memory request/response

### Device Engine
- [x] **`pimid/src/device_engine/device_engine.cpp:220`** - ✅ FIXED: Use scheduler to select PE
- [x] **`pimid/src/device_engine/device_engine.cpp:277`** - ✅ FIXED: Process memory response
- [ ] **`pimid/src/device_engine/device_engine.cpp:323`** - Actual execution on PE via ZSim

### Execution Model
- [ ] **`pimid/src/execution_model/zsim_execution_model.cpp:276`** - ZSim initialization

---

## 🟡 Medium Priority - External Tool Integration

### NVSim Integration
- [ ] **`pimid/memory_models/src/sttmram_model.cpp:72`** - Initialize NVSim for runtime calculations
- [ ] **`pimid/memory_models/src/sttmram_model.cpp:275`** - Per-cell endurance tracking
- [ ] **`pimid/memory_models/src/nvm_model.cpp:50`** - Initialize NVSim instance
- [ ] **`pimid/memory_models/src/nvm_model.cpp:255`** - Update energy models with NVSim
- [ ] **`pimid/memory_models/src/nvm_model.cpp:357`** - Per-bank/page endurance tracking

### CACTI Integration
- [ ] **`pimid/memory_models/src/sram_model.cpp:245`** - Update dynamic energy when CACTI integrated

### ReRAM Model
- [ ] **`pimid/memory_models/src/reram_model.cpp:123`** - Extend MemoryRequest for PIM operation types
- [ ] **`pimid/memory_models/src/reram_model.cpp:253`** - Per-cell endurance tracking

### PCM Model
- [ ] **`pimid/memory_models/src/pcm_model.cpp:223`** - Per-cell endurance tracking with wear-leveling

### Ramulator Specs
- [ ] **`pimid/memory_models/src/ramulator_wrapper.cpp:508`** - Add DDR5 verified specs
- [ ] **`pimid/memory_models/src/ramulator_wrapper.cpp:514`** - Add HBM3 verified specs

### McPAT Integration
- [ ] **`pimid/power_models/src/mcpat_model.cpp:21`** - Clean up McPAT instance
- [ ] **`pimid/power_models/src/mcpat_model.cpp:33`** - Initialize McPAT instance
- [ ] **`pimid/power_models/src/mcpat_model.cpp:46`** - Parse YAML configuration file

---

## 🟢 Low Priority - Config & Plugin System

### Configuration Validation
- [ ] **`pimid/src/config/config_validator.cpp:145`** - Implement unknown parameter detection
- [ ] **`pimid/src/config/config_validator.cpp:191`** - Parse YAML and validate

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

### Host Interconnect
- [ ] **`pimid/src/host_engine/host_interconnect.cpp:101`** - Set up connections based on topology

### Internal DRAM Network
- [ ] **`pimid/memory_models/src/internal_dram_network.cpp:651`** - Configurable queue limits

### PIM Bandwidth Tracker
- [ ] **`pimid/memory_models/src/pim_bandwidth_tracker.cpp:118`** - Queue depth limits

### Network Model
- [ ] **`pimid/network_models/src/network_model.cpp:115`** - Load GARNET-specific configuration

### Event-Driven Execution
- [ ] **`pimid/src/execution_model/event_driven_execution_model.cpp:274`** - Load from actual YAML config file

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
