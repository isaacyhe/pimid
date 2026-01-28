# PIMID Design Philosophy

> **Last Updated:** 2025-01-28
> **Purpose:** Document core design principles and requirements for the PIMID simulator

---

## Core Principles

### 1. CLI Interface with YAML Configuration

**Requirement:** The simulator MUST always be invoked via a command-line interface with a YAML configuration file.

```bash
# Correct usage pattern
pimid --config workload.yaml

# NOT hardcoded values in code
```

**Rationale:**
- Reproducibility: Configuration files can be version-controlled and shared
- Flexibility: Easy to sweep parameters without recompilation
- Documentation: Config files serve as self-documenting experiment records

### 2. Retrieve Values from Actual Simulators (No Hardcoding)

**Requirement:** Memory timing, power, and energy values MUST be retrieved from the actual external simulators, not hardcoded.

| Component | Source | NOT Acceptable |
|-----------|--------|----------------|
| SRAM timing/energy | CACTI | Hardcoded latency values |
| NVM timing/energy | NVSim | Hardcoded read/write cycles |
| DRAM timing | Ramulator + DRAMArchitectureV2 | Hardcoded tRCD, tCAS, etc. |
| Power estimates | McPAT + Memory/Network models | Placeholder power values |

**Implementation Pattern:**
```cpp
// CORRECT: Query from actual model
if (memory_model_) {
    double energy = memory_model_->getTotalEnergy();
    double leakage = memory_model_->getLeakagePower();
}

// FALLBACK ONLY when model unavailable (with technology scaling)
else {
    double scale = std::pow(22.0 / tech_node_nm, 2.0);
    energy = default_value * scale;
}
```

### 3. Subarray as Universal Building Block

**Requirement:** Within each memory tool (CACTI, NVSim), the subarray is the fundamental building block. Once process node and transistor mode are selected, there should be ONE subarray configuration that is reused.

**Architecture:**
```
CACTI (SRAM):     g_tp → DynamicParameter → Subarray (one per Mat)
NVSim (NVM):      Technology → SubArray (shared config)
Ramulator (DRAM): Timing embedded in tRCD/tCAS (subarray activation time)
```

### 4. Factory Functions over Hardcoded Values

**Requirement:** Use factory functions to create configuration objects from architecture specifications.

```cpp
// CORRECT: Use factory function
PEBusConstraints constraints = createPEBusConstraintsFromDRAM(*dram_arch_, level);

// NOT: Hardcoded switch statement with magic numbers
constraints.data_bus_width_bits = 64;  // DON'T DO THIS
```

### 5. Hierarchical Memory Organization

**Requirement:** Memory hierarchy must be explicitly modeled with clear PE placement levels.

```
PEPlacementLevel:
  SUBARRAY   → Finest granularity, dedicated row buffer access
  BANK       → Share bank I/O bus
  CHIP       → Share chip-level interconnect
  RANK       → Share rank-level bus
  LOGIC_DIE  → HBM/HMC logic die (wide bandwidth)
```

---

## Configuration Structure

### Required YAML Sections

```yaml
simulation:
  mode: "standalone|co-simulation|host-only|device-only"

memory:
  technology: "DRAM|SRAM|STT_MRAM|PCM|RERAM"
  # Technology-specific parameters loaded from sub-configs

processing_elements:
  placement_level: "SUBARRAY|BANK|RANK|LOGIC_DIE"
  num_pes_per_level: <integer>

network:
  topology: "MESH_2D|TORUS_2D|H_TREE|..."
  routing: "XY|ADAPTIVE|TREE_BASED|..."

power:
  enabled: true
  tech_node_nm: 22
```

---

## External Tool Integration

### Integration Hierarchy

```
PIMID Core
├── Memory Models (wrappers)
│   ├── CACTIWrapper    → SRAM timing/energy
│   ├── NVSimWrapper    → NVM timing/energy
│   ├── RamulatorWrapper → DRAM timing
│   └── DRAMArchitectureV2 → Bus constraints, organization
├── Network Models
│   └── GarnetModel     → NoC simulation
└── Power Models
    └── McPATModel      → Processor power
```

### Wrapper Requirements

1. Each external tool has a dedicated wrapper class
2. Wrappers expose standardized interfaces (`getLatency()`, `getEnergy()`, etc.)
3. Wrappers handle tool initialization and configuration parsing
4. Wrappers provide fallback values only when tool unavailable

---

## Code Organization Principles

### 1. No Magic Numbers

```cpp
// BAD
constraints.max_bandwidth_gbps = 25;

// GOOD
constraints.max_bandwidth_gbps = dram_arch.bandwidth_limits.bank_effective_bw_GBs;
```

### 2. Configuration Validation

- Unknown YAML parameters should generate warnings
- Required parameters should fail with clear error messages
- Range validation for numeric parameters

### 3. Statistics and Observability

- Every model must implement `printStats()` and `resetStats()`
- Energy tracking must be cumulative and queryable
- Cycle-level timing must be traceable

---

## Testing Philosophy

### Test Coverage Requirements

1. **Memory Technologies:** All 5 types (DRAM, SRAM, STT-MRAM, PCM, ReRAM)
2. **PE Placement Levels:** All levels (SUBARRAY through LOGIC_DIE)
3. **Network Topologies:** All supported topologies
4. **Workloads:** Representative set (BFS, GEMM, SpMV, etc.)

### Test Organization

```
test/
├── unit/           → Individual component tests
├── integration/    → Component interaction tests
├── functional/     → End-to-end simulation tests
└── scripts/        → Test automation
```

---

## Documentation Requirements

1. **README.md** - Quick start and overview
2. **QUICKSTART.md** - 5-minute setup guide
3. **USER_GUIDE.md** - Comprehensive usage documentation
4. **docs/CONFIGURATION_GUIDE.md** - Full configuration reference
5. **docs/implementation_reports/** - Technical implementation details

---

## Version Control

### Branch Naming
- Feature branches: `feature/<description>`
- Bug fixes: `fix/<description>`
- Claude sessions: `claude/<description>-<session_id>`

### Commit Messages
- Use conventional commits: `feat:`, `fix:`, `chore:`, `docs:`
- Include context in commit body when needed

---

## Anti-Patterns to Avoid

1. **Hardcoded timing values** - Always query from model/config
2. **Magic numbers** - Use named constants or config values
3. **Global state** - Prefer dependency injection
4. **Placeholder implementations** - Use fallbacks with scaling, not fixed values
5. **Undocumented configuration** - Every config parameter needs documentation
6. **Silent failures** - Errors should be logged and propagated

---

## Future Considerations

- Python API for scripting experiments
- GUI configuration tool
- CI/CD pipeline integration
- Distributed simulation support
