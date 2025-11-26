# PIMID Architecture Design

## Overview

PIMID is a **general-purpose Processing-In-Memory (PIM) simulator** that can simulate any workload binary with configurable memory technologies, PE types, and placement strategies.

## Core Principles

1. **Workload-Agnostic**: PIMID should run ANY workload binary (BFS, SpMV, DNN, custom applications)
2. **Single Entry Point**: `pimid` binary is the only simulator executable
3. **Configuration-Driven**: All simulation parameters specified via YAML configs
4. **External Workloads**: Workloads are separate binaries that PIMID instruments
5. **Standard APIs**: Support OpenMP, MPI, and standard parallel programming models

## Architecture Layers

```
┌─────────────────────────────────────────────────────────────┐
│                     User Interface                           │
│  pimid --config sim.yaml --workload ./bfs [args]            │
└─────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼─────────────────────────────────┐
│                   PIMID Simulator Core                        │
├───────────────────────────────────────────────────────────────┤
│  • Configuration Parser (YAML)                                │
│  • Workload Loader & Instrumentation                          │
│  • Memory Operation Interceptor                               │
│  • PIM Execution Engine                                       │
│  • Statistics Collector                                       │
│  • Results Reporter                                           │
└───────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
┌───────▼────────┐  ┌─────────▼────────┐  ┌────────▼──────────┐
│ Memory Models  │  │  PE Simulators   │  │ Interconnect      │
├────────────────┤  ├──────────────────┤  ├───────────────────┤
│ • DRAM         │  │ • Simple ALU     │  │ • Inner-Bank      │
│ • SRAM         │  │ • In-Order Core  │  │ • Inter-Bank      │
│ • STT-MRAM     │  │ • Out-of-Order   │  │ • Global I/O      │
│ • PCM          │  │ • Custom PE      │  │                   │
│ • ReRAM        │  │                  │  │                   │
└────────────────┘  └──────────────────┘  └───────────────────┘
                              │
┌─────────────────────────────▼─────────────────────────────────┐
│                    External Workloads                         │
│  (Separate binaries, not part of PIMID)                      │
├───────────────────────────────────────────────────────────────┤
│  • bfs_workload (OpenMP)                                      │
│  • spmv_workload (OpenMP/MPI)                                 │
│  • dnn_inference (custom)                                     │
│  • user_application (any)                                     │
└───────────────────────────────────────────────────────────────┘
```

## Usage Model

### Basic Simulation
```bash
pimid --config config.yaml --workload ./bfs_binary --graph 256k --degree 16
```

### Configuration File (config.yaml)
```yaml
simulation:
  name: "BFS_Bank_InOrder_SRAM"
  mode: "standalone"  # or "host-device"

memory:
  technology: "SRAM"
  capacity_mb: 256
  banks: 4
  subarrays_per_bank: 4

processing_elements:
  type: "in_order_core"
  placement_level: "BANK"  # SUBARRAY, BANK, RANK
  num_pes: 4

  in_order_core:
    pipeline_stages: 5
    fetch_decode_ns: 2.0
    execute_compare_ns: 1.0
    branch_penalty_ns: 3.0
    branch_prediction_accuracy: 0.5

output:
  stats_file: "results/stats.txt"
  trace_file: "results/trace.out"
  detailed: true
```

### Workload Interface

Workloads implement a simple API:

```c
// workload_api.h
typedef struct {
    void* memory_base;
    size_t memory_size;
    void (*pim_offload)(void* data, size_t size, const char* kernel);
} pimid_context_t;

// Workload entry point
int workload_main(int argc, char** argv, pimid_context_t* pimid);
```

## Component Details

### 1. PIMID Simulator Core

**Responsibilities**:
- Load and parse YAML configuration
- Initialize memory models based on config
- Load workload binary
- Instrument memory operations
- Simulate PIM execution
- Collect and report statistics

**Key Files**:
- `src/standalone_main.cpp` - Main entry point
- `src/common/config_parser.cpp` - YAML parser
- `src/common/simulation_engine.cpp` - Core simulation logic
- `src/common/workload_loader.cpp` - Dynamic workload loading

### 2. Memory Models

**Existing Implementation** (already complete):
- DRAM, SRAM, STT-MRAM, PCM, ReRAM models
- Cycle-accurate timing
- Energy modeling
- Inner-bank datapath modeling

**Location**: `memory_models/src/`

### 3. PE Simulators

**Supported PE Types**:
- Simple ALU: Fast, no branch support
- In-Order Core: 5-stage pipeline, branch prediction
- Out-of-Order Core: (future)
- Custom PE: User-defined

**Configuration**: Specified in YAML, instantiated at runtime

### 4. Workload Infrastructure

**Design**:
- Workloads are **separate executables**
- PIMID loads them dynamically or instruments them
- Workloads use standard APIs (OpenMP, MPI, PIMID API)
- No workload-specific code in PIMID core

**Example Workloads**:
```
workloads/
├── bfs/
│   ├── bfs.cpp          # BFS implementation with OpenMP
│   ├── Makefile
│   └── README.md
├── spmv/
│   ├── spmv.cpp         # SpMV with OpenMP/MPI
│   ├── Makefile
│   └── README.md
└── template/
    ├── workload_template.cpp
    └── Makefile
```

### 5. Instrumentation Modes

**Option A: LD_PRELOAD Interception**
```bash
LD_PRELOAD=libpimid_intercept.so ./workload
```
- Intercept malloc, memory accesses
- No workload modifications needed
- Works with any binary

**Option B: Explicit API**
```c
#include <pimid/api.h>

void kernel() {
    // Mark PIM region
    PIMID_BEGIN_PIM_REGION();

    // Memory operations tracked
    for (int i = 0; i < n; i++) {
        array[i] = compute(array[i]);
    }

    PIMID_END_PIM_REGION();
}
```

**Option C: Source Instrumentation**
- Compile-time instrumentation using LLVM
- Automatic tracking
- Most accurate

## Simulation Flow

```
1. User runs: pimid --config sim.yaml --workload ./bfs --graph 256k

2. PIMID loads:
   - Parse sim.yaml
   - Initialize memory model (SRAM, 4 banks, etc.)
   - Initialize PE simulators (4 In-Order Cores at bank level)
   - Load ./bfs binary

3. PIMID instruments workload:
   - Intercept memory allocations
   - Hook memory reads/writes
   - Track PIM offload regions

4. Execute workload:
   - Run BFS code
   - Capture memory operations
   - Simulate PIM execution for offload regions
   - Collect timing and energy stats

5. Output results:
   - Total execution time
   - Energy consumption
   - Memory access patterns
   - PIM vs host execution breakdown
   - Detailed statistics
```

## Configuration Schema

```yaml
simulation:
  name: string
  description: string
  mode: "standalone" | "host-device" | "trace-driven"

memory:
  technology: "DRAM" | "SRAM" | "STT_MRAM" | "PCM" | "ReRAM"
  config_file: path  # Optional: technology-specific config

  hierarchy:
    capacity_mb: int
    num_banks: int
    num_subarrays_per_bank: int
    subarray_size_kb: int

  timing:  # Optional overrides
    subarray_read_ns: float
    subarray_write_ns: float
    inner_bank_htree_ns: float

processing_elements:
  type: "simple_alu" | "in_order_core" | "out_of_order_core"
  placement_level: "SUBARRAY" | "BANK" | "RANK" | "CHIP"
  num_pes: int

  simple_alu:
    compare_ns: float
    alu_ns: float
    energy_pj: float

  in_order_core:
    pipeline_stages: int
    fetch_decode_ns: float
    execute_compare_ns: float
    execute_alu_ns: float
    memory_access_ns: float
    writeback_ns: float
    branch_penalty_ns: float
    branch_prediction_accuracy: float
    energy:
      fetch_decode_pj: float
      alu_pj: float
      branch_pj: float

workload:
  binary: path
  arguments: [string]
  environment: {key: value}

  # Workload-specific parameters
  graph:
    num_vertices: int
    avg_degree: int
    topology: string

  matrix:
    rows: int
    cols: int
    sparsity: float

simulation_control:
  max_cycles: int
  enable_power_modeling: bool
  enable_detailed_stats: bool
  enable_tracing: bool
  checkpoint_interval: int

output:
  stats_file: path
  trace_file: path
  detailed_log: path
  log_level: "DEBUG" | "INFO" | "WARNING" | "ERROR"
  format: "text" | "json" | "csv"
```

## Implementation Phases

### Phase 1: Core Infrastructure ✓ (Mostly Done)
- [x] Memory models (DRAM, SRAM, NVMs)
- [x] Basic PE timing models
- [x] Inner-bank datapath modeling
- [ ] Complete YAML config parser
- [ ] PIMID standalone binary

### Phase 2: Workload Support (Current Focus)
- [ ] Workload loader infrastructure
- [ ] Memory operation instrumentation
- [ ] BFS as external workload
- [ ] SpMV template workload
- [ ] Workload API definition

### Phase 3: Simulation Engine
- [ ] PIM execution simulator
- [ ] Statistics collection
- [ ] Energy tracking
- [ ] Results output

### Phase 4: Testing & Validation
- [ ] Update comprehensive test suite
- [ ] Regression testing
- [ ] Performance validation
- [ ] Documentation

### Phase 5: Advanced Features (Future)
- [ ] Host-device co-simulation
- [ ] MPI support
- [ ] Trace-driven simulation
- [ ] Cache coherence modeling
- [ ] Network-on-chip simulation

## Benefits of This Architecture

1. **Flexibility**: Any workload can run in PIMID
2. **Maintainability**: Workloads separate from simulator
3. **Extensibility**: Easy to add new memory techs, PE types
4. **Realism**: Use real application binaries
5. **Standard APIs**: OpenMP, MPI support
6. **Research-Friendly**: Easy to experiment with PIM designs

## File Organization

```
pimid/
├── src/
│   ├── standalone_main.cpp          # Main PIMID entry point
│   ├── common/
│   │   ├── config_parser.cpp        # YAML config parser
│   │   ├── simulation_engine.cpp    # Core simulator
│   │   ├── workload_loader.cpp      # Dynamic workload loading
│   │   └── statistics_collector.cpp # Stats tracking
│   ├── instrumentation/
│   │   ├── memory_interceptor.cpp   # Memory op interception
│   │   └── pim_api.cpp              # PIMID API implementation
│   └── ...
├── include/
│   └── pimid/
│       ├── api.h                    # Public PIMID API
│       └── workload_interface.h     # Workload interface
├── memory_models/                   # Existing memory models
├── configs/                         # YAML configurations
├── workloads/                       # External workload binaries
│   ├── bfs/
│   ├── spmv/
│   └── template/
├── scripts/                         # Test/analysis scripts
└── docs/                            # Documentation
```

## Next Steps

1. Implement complete YAML config parser
2. Refactor standalone_main.cpp as full simulator
3. Create workload interface and API
4. Refactor BFS as external workload with OpenMP
5. Update comprehensive test suite
6. Document architecture and usage
