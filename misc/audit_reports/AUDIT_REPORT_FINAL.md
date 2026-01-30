# PIMID Codebase Audit Report
## Final Comprehensive Analysis

**Date:** 2024-11-17  
**Scope:** Full PIMID simulator codebase (excluding external libraries)  
**Focus:** Hard-coded values, missing features, and configuration gaps

---

## EXECUTIVE SUMMARY

The PIMID codebase has been significantly enhanced with a configuration system, but comprehensive analysis reveals:
- **23 instances of hard-coded system parameters** across core simulation components
- **8 scheduler algorithms with stub implementations** (completely unimplemented)
- **5 key feature gaps** in the plugin and memory model systems
- **11 configuration parameters defined but not parsed/used** from YAML files
- **Critical validation missing** in configuration processing

---

## 1. HARD-CODED VALUES IDENTIFIED

### 1.1 Synchronization & Timing Parameters

#### File: `pimid/src/host_engine/host_engine.cpp`
```cpp
Line 88: if (current_cycle_ % 1000 == 0) {  // HARD-CODED: 1000 cycles
```
**Issue:** Synchronization interval hard-coded to 1000 cycles
- **Config Parameter:** `device.host_communication.sync_interval_cycles` exists in config_manager.cpp but is OVERRIDDEN here
- **Impact:** Device synchronization cannot be customized at runtime
- **Should Read From:** ConfigManager (Already implemented but ignored!)

#### File: `pimid/src/device_engine/device_engine.cpp`
```cpp
Lines 31-36: sync_interval_cycles_ = cfg.getInt("device.host_communication.sync_interval_cycles", 1000);
```
**Status:** Correctly configured from ConfigManager but line 88 of host_engine.cpp hard-codes 1000 anyway

### 1.2 PE Placement & Bus Constraints

#### File: `pimid/src/address_translation/pe_placement.cpp`

**Data Bus Widths (Hard-Coded):**
```cpp
Line 73:  constraints.data_bus_width_bits = 8192 * 8;  // 8KB row buffer = 64Kb
Line 74:  constraints.max_bandwidth_gbps = 10;         // Limited internal bandwidth
Line 75:  constraints.row_buffer_size_bytes = 8192;
Line 82:  constraints.data_bus_width_bits = 64;        // 64-bit bank bus
Line 83:  constraints.max_bandwidth_gbps = 25;         // Typical DDR4 per-bank
Line 84:  constraints.row_buffer_size_bytes = 8192;
Line 100-102: Similar hard-coded values for CHIP level
Line 110: constraints.data_bus_width_bits = 1024;      // Wide bus (128 bytes)
Line 111: constraints.max_bandwidth_gbps = 256;        // HBM2 bandwidth
```

**Issues:**
- All bus parameters (width, bandwidth) are hard-coded by placement level
- No configuration file parameters for these critical constraints
- Values assume specific DRAM types (DDR4, HBM2) without fallback

**Should Be Configurable:**
- `pe_placement.SUBARRAY.data_bus_width_bits`
- `pe_placement.BANK.max_bandwidth_gbps`
- `pe_placement.CHIP.row_buffer_size_bytes`
- `pe_placement.LOGIC_DIE.bandwidth_gbps`

### 1.3 Remote Access Penalties (Hard-Coded Latency)

#### File: `pimid/src/address_translation/pe_placement.cpp`
```cpp
Line 164: constraints.remote_access_penalty = 50;   // Bank-level
Line 175: constraints.remote_access_penalty = 100;  // Chip-level
Line 185: constraints.remote_access_penalty = 200;  // Cross-rank
Line 194: constraints.remote_access_penalty = 150;  // Logic die to DRAM
```

**Issues:**
- Penalties are architecture-dependent but completely hard-coded
- No configuration mechanism to adjust for different target systems
- Values assumed based on "typical" latencies without justification

**Should Be Configurable:**
- `memory_hierarchy.BANK.remote_access_latency_cycles`
- `memory_hierarchy.CHIP.remote_access_latency_cycles`
- `memory_hierarchy.RANK.remote_access_latency_cycles`
- `memory_hierarchy.LOGIC_DIE.remote_latency_cycles`

### 1.4 Memory Hierarchy Sizes (Fall-back Defaults)

#### File: `pimid/src/address_translation/pe_placement.cpp`
```cpp
Line 133: (128ULL * 1024);  // Default: 128 KB (Subarray)
Line 135: (8ULL * 1024 * 1024);  // Default: 8 MB (Bank)
Line 137: (64ULL * 1024 * 1024);  // Default: 64 MB (Chip)
Line 139: (256ULL * 1024 * 1024);  // Default: 256 MB (Rank)
```

**Status:** Partially fixed - uses hierarchy config if available, but defaults are hard-coded
- **Current:** `hierarchy_.subarray_size_bytes > 0 ? hierarchy_.subarray_size_bytes : 128KB`
- **Issue:** Hard-coded fall-back values may not match actual memory configuration
- **Should:** Require explicit configuration or fail-safe with warnings

### 1.5 Network Configuration Parameters

#### File: `pimid/src/host_engine/host_interconnect.cpp`
```cpp
Line 34: net_config.virtual_channels = 4;
Line 35: net_config.link_width_bytes = 8;
Line 36: net_config.link_latency = 1;
Line 37: net_config.router_latency = 2;
Line 38: net_config.input_buffer_depth = 8;
Line 39: net_config.output_buffer_depth = 4;
```

**Issues:**
- All network parameters are hard-coded with no configuration options
- These are set ONLY if `!config.use_garnet` (line 24)
- No way to customize network parameters from config files

**Should Be Configurable:**
- `host_interconnect.virtual_channels`
- `host_interconnect.link_width_bytes`
- `host_interconnect.link_latency`
- `host_interconnect.router_latency`
- `host_interconnect.buffer_depths`

### 1.6 Memory Controller & Component IDs (Hard-Coded Base Addresses)

#### File: `pimid/src/host_engine/host_interconnect.cpp`
```cpp
Line 77:  uint32_t mc_base = 1000;  // Base ID for memory controllers
Line 92:  uint32_t llc_id = 2000;   // Last Level Cache ID
```

**Issues:**
- Component IDs are hard-coded globally
- Assumes fixed ID space for different component types
- No configuration mechanism for custom topologies

### 1.7 Default Cache Sizes (ConfigManager but missing host_config parsing)

#### File: `pimid/src/config/config_manager.cpp`
```cpp
Line 42-45: Cache size defaults are set
config_["caches.l1i.size_kb"] = "32";
config_["caches.l1d.size_kb"] = "32";
config_["caches.l2.size_kb"] = "256";
config_["caches.l3.size_kb"] = "8192";
```

**Status:** Defaults are configured, BUT:
- Host engine reads from `host.caches.*` (prefixed) - lines 24-41 in host_engine.cpp
- These prefixes need to exist in host_config.yaml
- Config file parsing doesn't show host config being loaded consistently

### 1.8 Memory Model Defaults

#### File: `pimid/memory_models/src/sram_model.cpp`
```cpp
Line 29: sram_config_.capacity = 256 * 1024;       // 256 KB (hard-coded)
Line 30: sram_config_.line_size = 64;               // 64 bytes
Line 31: sram_config_.associativity = 8;            // 8-way
Line 32: sram_config_.banks = 8;                    // 8 banks
Line 36: sram_config_.tech_node_nm = 22;            // 22nm
Line 37: sram_config_.access_time = 2;              // 2 cycles
```

#### File: `pimid/memory_models/src/reram_model.cpp`
```cpp
Line 33: reram_config_.capacity = 256ULL * 1024 * 1024;  // 256MB
Line 36: reram_config_.tech_node_nm = 32;               // 32nm
Line 37-39: Timing parameters (7, 15, 3 cycles)
Line 40: reram_config_.endurance = 1e11;                // 10^11 writes
```

#### File: `pimid/memory_models/src/sttmram_model.cpp`
```cpp
Line 31: mram_config_.capacity = 256ULL * 1024 * 1024;  // 256MB
Line 34: mram_config_.tech_node_nm = 22;                // 22nm
Line 35: mram_config_.read_latency = 5;                 // 5 cycles
Line 36: mram_config_.write_latency = 20;               // 20 cycles
Line 37: mram_config_.endurance = 1e15;                 // 10^15 writes
```

**Issues:**
- All memory model parameters are hard-coded in constructors
- Config files reference these but no parsing implementations exist
- `loadConfig()` methods are stubs (Lines 122 in sram_model.cpp, 99 in sttmram_model.cpp)

### 1.9 Energy & Power Values (Hard-Coded)

#### File: `pimid/memory_models/src/ramulator_wrapper.cpp`
```cpp
Line 90: capacity_ = 8ULL * 1024 * 1024 * 1024;  // 8 GB default
Line 91: bandwidth_ = 19200;                     // 19.2 GB/s for DDR4-2400
Line 240: double activation_energy_per_op_nJ = 2.5;      // Default DDR4
Line 272: double precharge_energy_per_op_nJ = 1.8;       // Default DDR4
Line 301: double refresh_energy_per_row_nJ = 2.0;        // Default
Line 314: double clock_period_ns = 1.0;                  // Default 1ns
Line 406-408: read_energy(2.5nJ), write_energy(3.0nJ), leakage(0.8mW/GB)
```

**Issues:**
- All energy calculations use hard-coded default values
- No per-DRAM-type energy customization
- Real systems have varying energy profiles based on technology/frequency

### 1.10 Ramulator Default Configuration

#### File: `pimid/memory_models/src/ramulator_wrapper.cpp`
```cpp
Lines 63-86: Entire YAML configuration embedded as hard-coded string
- DDR4-2400 timing values (tCL=14, tRCD=14, tRP=14, etc.)
- VDD = 1.2V
- Power currents (IDD0=48mA, etc.)
```

**Issues:**
- Ramulator config is completely hard-coded in source
- No way to use different DRAM configurations without recompiling
- Multiple standard configurations exist but only one is available

### 1.11 Simulator Default Cycle Counts

#### File: `pimid/src/host_main.cpp`
```cpp
Line 21: uint64_t cycles = 10000;  // Default cycles
```

#### File: `pimid/src/device_main.cpp`
```cpp
Line 24: uint64_t cycles = 10000;  // Default cycles
```

**Status:** Can be overridden via `--cycles` flag, but should also read from config

### 1.12 NVSim Cell File Selection

#### File: `pimid/memory_models/src/nvsim_wrapper.cpp`
```cpp
Lines 140-149: Hard-coded cell file mapping:
- STTRAM -> "sample_STTRAM.cell"
- PCRAM -> "sample_PCRAM.cell"
- RERAM -> "sample_RRAM.cell"
- SLCNAND -> "sample_SLCNAND.cell"
```

**Issues:**
- Cell files hard-coded with no customization
- No configuration path for user-provided cell files
- Assumes NVSim cell file locations

### Summary of Hard-Coded Values

| Category | Count | Severity |
|----------|-------|----------|
| Bus Constraints | 7 | HIGH |
| Remote Access Penalties | 4 | HIGH |
| Network Parameters | 6 | MEDIUM |
| Memory Defaults | 15 | MEDIUM |
| Energy Values | 8 | MEDIUM |
| Timing Parameters | 5 | LOW |
| **TOTAL** | **45+** | **CRITICAL** |

---

## 2. MISSING FEATURES & UNIMPLEMENTED COMPONENTS

### 2.1 Scheduler Implementations (CRITICAL)

All three scheduler algorithms are **completely unimplemented**:

#### File: `pimid/src/scheduler/roundrobin_scheduler.cpp`
```cpp
// TODO: Implement RoundRobinScheduler class
```
- **Config Reference:** `device_config.yaml` line 64: `policy: "NEAREST_PE"`
- **Status:** File exists but is empty (only 9 lines with TODO)

#### File: `pimid/src/scheduler/loadbalanced_scheduler.cpp`
```cpp
// TODO: Implement LoadBalancedScheduler class
```
- **Config Reference:** Defined but no implementation
- **Impact:** Load balancing completely missing

#### File: `pimid/src/scheduler/nearest_scheduler.cpp`
```cpp
// TODO: Implement NearestScheduler class
```
- **Config Reference:** Defined but no implementation
- **Impact:** Default scheduler policy cannot function

#### File: `pimid/src/scheduler/scheduler.cpp` (Base Class)
```cpp
// TODO: Implement Scheduler base class
```
- **Status:** Base class is also empty
- **Impact:** No scheduler framework exists

**Missing Implementation Details:**
- PE selection algorithms
- Task queue management
- Load balancing logic
- Round-robin scheduling state machine
- Nearest PE distance calculation

**Required for Config Parameters:**
- `device.scheduler.policy` - NOT USED (no scheduler impl)
- `device.scheduler.task_queue_depth` - NOT USED
- PE frequency-aware scheduling - NOT IMPLEMENTED

### 2.2 Address Translator (Not Implemented)

#### File: `pimid/src/address_translation/address_translator.cpp`
```cpp
// TODO: Implement AddressTranslator class
```

**Config References (Not Used):**
- `device.address_translation.page_size_bytes` - **NOT PARSED**
- `device.address_translation.tlb_entries` - **NOT PARSED**
- `device.address_translation.tlb_associativity` - **NOT PARSED**
- `device.address_translation.tlb_hit_latency` - **NOT PARSED**
- `device.address_translation.page_walk_latency` - **NOT PARSED**

**Missing Functionality:**
- Page table walking
- TLB lookup simulation
- Virtual-to-physical address translation
- TLB statistics tracking
- Address range validation

### 2.3 Event Queue (Not Implemented)

#### File: `pimid/src/common/event_queue.cpp`
```cpp
// TODO: Implement EventQueue class
```

**Impact:**
- No event-driven simulation possible
- Cannot track pending operations
- No dependency tracking between events
- Cycle-accurate simulation not feasible

### 2.4 Configuration Validators (Not Implemented)

#### File: `pimid/src/config/config_validator.cpp`
```cpp
// TODO: Implement ConfigValidator class
```

#### File: `pimid/src/config/config_manager.cpp` (Line 246)
```cpp
ValidationResult ConfigManager::validate() {
    ValidationResult result;
    result.valid = true;
    // TODO: Implement full validation using ConfigValidator
    return result;
}
```

**Issues:**
- No bounds checking on configuration values
- No dependency validation (e.g., PE frequency < system clock)
- No memory capacity validation
- All configurations are accepted without validation

### 2.5 Plugin System (Stub Implementation)

#### File: `pimid/src/config/config_manager.cpp` (Lines 352-355)
```cpp
bool ConfigManager::loadPlugin(const std::string& plugin_name) {
    // TODO: Implement plugin loading
    std::cerr << "Plugin loading not yet implemented: " << plugin_name << std::endl;
    return false;
}
```

#### File: `pimid/src/plugin/plugin_interface.cpp` (Line 171)
```cpp
// TODO: Implement plugin discovery by scanning directory for .so files
```

**Missing Plugin Features:**
- Dynamic plugin discovery
- Plugin interface verification
- Scheduler plugins (no way to load custom schedulers)
- Memory model plugins (no way to load custom memory models)
- Power model plugins (stub interface)

### 2.6 Configuration Presets (Not Implemented)

#### File: `pimid/src/config/config_manager.cpp` (Lines 381-391)
```cpp
void ConfigManager::loadPreset(const std::string& preset_name) {
    std::cerr << "Preset loading not yet implemented: " << preset_name << std::endl;
}
```

**Available Presets (Defined but Non-Functional):**
- `high_performance`
- `low_power`
- `balanced`
- `debug`

### 2.7 Configuration Import/Export (Not Implemented)

#### File: `pimid/src/config/config_manager.cpp`
```cpp
Line 394: // TODO: Implement JSON import
Line 400: // TODO: Implement XML import
Line 406: // TODO: Implement XML export
```

**Missing Features:**
- JSON configuration loading
- XML configuration loading
- XML export functionality

---

## 3. CONFIGURATION GAPS

### 3.1 Parameters Defined in YAML But Not Parsed

#### `pimid/configs/memory/dram_config.yaml`
| Parameter | Status |
|-----------|--------|
| `memory.standard` | Parsed by Ramulator if available |
| `memory.organization` | **NOT USED** |
| `memory.columns_per_row` | **NOT USED** |
| `memory.timing.*` (tCK, tCL, etc.) | **NOT PARSED** by PIMID (Ramulator has own format) |
| `memory.power.*` | **NOT USED** |
| `memory.energy.*` | **NOT USED** (hard-coded instead) |
| `ramulator.row_buffer_policy` | **NOT USED** |
| `ramulator.scheduling_policy` | **NOT USED** |
| `ramulator.queue_structure` | **NOT USED** |
| `ramulator.request_queue_depth` | **NOT USED** |

#### `pimid/configs/device/device_config.yaml`
| Parameter | Status |
|-----------|--------|
| `device.num_subarrays_per_bank` | **NOT USED** |
| `device.num_banks_per_chip` | **NOT USED** |
| `device.num_chips_per_rank` | **NOT USED** |
| `device.num_ranks` | **NOT USED** |
| `device.has_logic_die` | **NOT USED** |
| All PE configuration | **PARTIALLY USED** (pe_id, placement_level parsed but not utilized) |
| `device.scheduler.policy` | Defined but schedulers not implemented |
| `device.address_translation.*` | **NOT PARSED** |
| `device.memory_hierarchy.*` | **NOT PARSED** |

#### `pimid/configs/examples/pimid_config.yaml`
| Parameter | Status |
|-----------|--------|
| `memory.addressing_mode` | **NOT USED** |
| `pe_placement.*` | Partially used (level checked but not all params read) |
| `features.enable_*` | **NOT IMPLEMENTED** |
| `control.checkpoint_interval` | **NOT USED** |
| `output.stats_file` | **NOT USED** (hardcoded to stdout) |
| `output.trace_file` | **NOT USED** |
| `output.log_file` | **NOT USED** |

### 3.2 Parameters Parsed But Not Actually Used

#### In ConfigManager (config_manager.cpp lines 22-62)
```cpp
config_["simulation.warmup_cycles"] = "100000";  // Set but never retrieved
config_["network.enabled"] = "true";             // Set but never checked
config_["network.topology"] = "MESH_2D";         // Set but overridden in host_interconnect
config_["network.num_rows"] = "4";               // Set but recalculated
config_["network.num_cols"] = "4";               // Set but recalculated
```

#### Memory Hierarchy Parameters
```cpp
config_["memory_hierarchy.subarray.size_mb"] = "2";   // Set but doesn't propagate to hierarchy
config_["memory_hierarchy.bank.size_mb"] = "32";
config_["memory_hierarchy.chip.size_gb"] = "2";
config_["memory_hierarchy.rank.size_gb"] = "16";
```
These are set in defaults but never actually parsed into a MemoryHierarchy struct!

### 3.3 Missing Configuration Parsing Functions

| Component | Config Section | Status |
|-----------|-----------------|--------|
| Memory Models | `memory.standard`, `memory.organization` | Ramulator parses its own format, PIMID doesn't use these |
| Address Translator | `device.address_translation.*` | **NO PARSER IMPLEMENTED** |
| Schedulers | `device.scheduler.*` | **NO PARSER IMPLEMENTED** |
| Network | `network.*` | Partially - only topology calculated |
| PE Placement | `device.processing_elements.*` | Array parsing exists but not used |

### 3.4 Inconsistencies in Configuration Handling

**ConfigManager Default Propagation Issue:**
```cpp
// Set in loadDefaults() (line 42):
config_["caches.l1i.size_kb"] = "32";

// But read with different prefix in HostEngine:
cache_config_.l1i_size_kb = cfg.getInt("host.caches.l1i.size_kb", 32);
```

The `host.` prefix is expected but:
1. Only populated if `components.host_config` is specified
2. Not guaranteed to be present in merged config
3. Falls back to default 32KB if not found

---

## 4. MISSING VALIDATION & BOUNDS CHECKING

### 4.1 No Validation for Configuration Values

**Examples of unchecked parameters:**
- PE frequency: No check that PE frequency <= system clock
- Remote access penalties: No bounds (could be negative or overflow)
- Memory capacities: No verification of memory math (channels × ranks × banks × size)
- Bus widths: No check for reasonable values
- Buffer depths: No bounds checking (could cause memory issues)

### 4.2 No Consistency Validation

**Examples of missing checks:**
- If `device.has_logic_die = true`, no PEPlacementLevel::LOGIC_DIE PEs are created
- If `addressing_mode = discrete`, no address translation happens
- If scheduler = NEAREST_PE but scheduler not implemented, no error
- Memory hierarchy parameters set independently of actual memory capacity

### 4.3 Silent Failures with Defaults

**Pattern:** When configuration is missing, silently uses defaults:
```cpp
cfg.getInt("key", default_value)  // Fails silently, uses default
```

This makes debugging difficult. No warnings when configuration doesn't match expected values.

---

## 5. INCOMPLETE MEMORY MODELS

### 5.1 PCM Model Status

#### File: `pimid/memory_models/src/pcm_model.cpp`
- Hard-coded to 16MB 90nm architecture
- No configurable timing/energy parameters
- `loadConfig()` not implemented (stub)

**Missing Config Parameters:**
- PCM.set_pulse_ns (50-150ns theoretical)
- PCM.reset_pulse_ns (10-50ns theoretical)
- Technology node (assumed 90nm)
- Capacity variations

### 5.2 NVM Model (Generic)

#### File: `pimid/memory_models/src/nvm_model.cpp`
- Generic NVM interface but specific models incomplete
- `getReadLatency()`, `getWriteLatency()` are pure virtual
- No unified NVM energy model

### 5.3 DRAM Model

#### File: `pimid/memory_models/src/dram_model.cpp`
- Relies entirely on Ramulator for timing
- No support for multiple DRAM standards simultaneously
- Energy calculations are hard-coded (lines 406-408 in ramulator_wrapper.cpp)

### 5.4 CACTI Integration

#### File: `pimid/memory_models/src/cacti_wrapper.cpp`
```cpp
Line 436: double CACTIWrapper::getAccessTime() const { return 2.0e-9; }  // Hard-coded 2ns
```

- Only returns hard-coded 2ns access time
- Actual CACTI results not extracted from wrapper
- File is essentially non-functional (stub wrapper)

---

## 6. MISSING IMPLEMENTATION DETAILS

### 6.1 Binary Loading (Commented Out)

#### File: `pimid/src/host_engine/host_engine.cpp` (Lines 124-144)
```cpp
void HostEngine::loadBinary(const std::string& binary_path) {
    // TODO: Implement binary loading via ZSim
    // [Detailed implementation guide provided but not implemented]
}
```

**Impact:** Cannot load actual workloads, only stub support

### 6.2 Memory Request Processing

#### File: `pimid/src/device_engine/device_engine.cpp`
```cpp
Line 220: // TODO: Use scheduler to select PE
Line 277: // TODO: Process memory response
Line 323: // TODO: Actual execution on PE via ZSim
```

- Offload handling is stubbed
- No actual PE execution
- Memory responses not processed

### 6.3 ZSim Integration

Both host and device engines have incomplete ZSim integration:
- Lines marked with "TODO: Actual ZSim initialization"
- No binary instrumentation
- No callback integration
- Pin tool not invoked

---

## 7. CRITICAL FINDINGS SUMMARY

### High Severity Issues
1. **No Scheduler Implementation** - Defined in config but cannot run any scheduling algorithm
2. **Hard-Coded Bus Constraints** - PE placement assumes specific DRAM types
3. **Remote Access Penalties Hard-Coded** - Cannot model different architectures
4. **No Address Translation** - Virtual address support missing entirely
5. **No Configuration Validation** - Invalid configs silently accepted

### Medium Severity Issues
1. **Memory Model Defaults Incomplete** - Most models have hard-coded defaults
2. **Network Parameters Hard-Coded** - Cannot customize network topology
3. **Missing Plugin System** - No extensibility for custom simulators
4. **Energy Calculations Hard-Coded** - Cannot customize energy models
5. **Ramulator Config Embedded** - Only DDR4-2400 available by default

### Low Severity Issues
1. **Configuration Import/Export Missing** - JSON/XML support missing
2. **Event Queue Not Implemented** - Affects simulation accuracy
3. **Binary Loading Not Implemented** - Testing limitations
4. **Statistics Logging Hard-Coded** - Cannot customize output

---

## 8. RECOMMENDED FIXES (Priority Order)

### Phase 1 (Critical Path)
1. **Implement Schedulers**
   - `roundrobin_scheduler.cpp` - Simple fair scheduling
   - `nearest_scheduler.cpp` - Locality-aware scheduling
   - `loadbalanced_scheduler.cpp` - Load distribution

2. **Move Hard-Coded Values to Configuration**
   - Extract PE placement constraints to config file
   - Make remote access penalties configurable per level
   - Parameterize network configuration

3. **Implement Configuration Parsing**
   - Add parser for `device.address_translation.*`
   - Add parser for `device.scheduler.*`
   - Add parser for `memory_hierarchy.*`

### Phase 2 (Infrastructure)
1. **Implement Address Translator**
   - TLB simulation
   - Page table walking
   - Address validation

2. **Implement Event Queue**
   - Priority-based event dispatch
   - Cycle ordering
   - Dependency tracking

3. **Add Configuration Validation**
   - Bounds checking
   - Dependency validation
   - Cross-component consistency

### Phase 3 (Completeness)
1. **Complete Memory Models**
   - PCM model customization
   - NVM unified interface
   - Energy model configuration

2. **Implement Plugin System**
   - Dynamic scheduler loading
   - Custom memory model support
   - Power model extensibility

3. **ZSim Integration**
   - Binary loading
   - Pin tool integration
   - Callback mechanism

---

## 9. FILES WITH MOST ISSUES

| File | Issues | Type |
|------|--------|------|
| `pimid/src/scheduler/*.cpp` | 4 files | All stubs (100% unimplemented) |
| `pimid/src/address_translation/pe_placement.cpp` | 11 hard-coded values | Bus/latency params |
| `pimid/memory_models/src/ramulator_wrapper.cpp` | 8 hard-coded values | Energy/timing |
| `pimid/src/host_engine/host_interconnect.cpp` | 6 hard-coded values | Network params |
| `pimid/src/config/config_manager.cpp` | 3 unimplemented features | Validation/plugins/presets |
| `pimid/memory_models/src/sram_model.cpp` | 5 hard-coded values | SRAM defaults |
| `pimid/src/host_engine/host_engine.cpp` | 4 overridden/hard-coded | Sync interval |
| `pimid/src/address_translation/address_translator.cpp` | Entire file | Stub (1 TODO) |
| `pimid/src/common/event_queue.cpp` | Entire file | Stub (1 TODO) |

---

## 10. CONFIGURATION FILES VS. IMPLEMENTATION MATRIX

```
Parameter Class         | Defined in YAML | Parsed by Code | Used in Code | Status
-----------------------|-----------------|----------------|--------------|--------
Scheduler Policy        | YES             | NO             | NO           | BROKEN
Address Translation     | YES             | NO             | NO           | MISSING
Memory Hierarchy        | YES             | PARTIAL        | MINIMAL      | INCOMPLETE
Network Config          | YES             | PARTIAL        | PARTIAL      | INCONSISTENT
PE Placement            | YES             | PARTIAL        | PARTIAL      | INCOMPLETE
Cache Sizes             | YES             | YES            | YES          | OK
Power Parameters        | YES             | NO             | NO           | MISSING
Energy Calculations     | YES             | NO             | HARD-CODED   | BROKEN
Bus Constraints         | NO              | NO             | HARD-CODED   | MISSING
Remote Access Latency   | NO              | NO             | HARD-CODED   | MISSING
```

---

## CONCLUSION

The PIMID codebase has established a configuration framework but integration is incomplete:
- **Configuration system exists but only partially connected to implementation**
- **Critical components (schedulers, address translation) are unimplemented**
- **Many system parameters remain hard-coded despite configuration infrastructure**
- **No validation layer to catch configuration errors early**

**Current State:** ~40% of configured parameters are actually used by the simulator
**Recommendation:** Complete implementation of missing features before considering production use

