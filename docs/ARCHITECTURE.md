# PIMID Architecture Documentation

## Overview

PIMID (Processing-In-Memory Interface Development) is a full-system simulator designed for comprehensive evaluation of Processing-in-Memory architectures. This document describes the internal architecture and design decisions.

## Design Principles

1. **Modularity**: Each component is self-contained with well-defined interfaces
2. **Extensibility**: Easy to add new memory models, schedulers, or network topologies
3. **Accuracy**: Cycle-accurate simulation of both host and PIM devices
4. **Flexibility**: Support for diverse configurations via YAML
5. **Performance**: Optimized for simulation speed while maintaining accuracy

## System Architecture

### High-Level Components

```
┌──────────────────────────────────────────────────────────────┐
│                      PIMID Simulator                         │
├──────────────────────────┬───────────────────────────────────┤
│   Host Engine (ZSim)     │   Device Engine (ZSim)            │
│                          │                                   │
│   - CPU cores            │   - Processing Elements (PEs)     │
│   - Cache hierarchy      │   - Local memory                  │
│   - Memory controller    │   - Address translation           │
│   - Address translation  │   - Task scheduler                │
└──────────────────────────┴───────────────────────────────────┘
           │                            │
           └────────── Socket ──────────┘
                    Communication
                         │
┌────────────────────────┴───────────────────────────────────┐
│              Shared Modeling Infrastructure                │
├──────────────┬──────────────┬──────────────┬───────────────┤
│   Memory     │   Network    │    Power     │   Common      │
│              │              │              │               │
│ - Ramulator  │ - GARNET     │ - McPAT      │ - Event queue │
│ - CACTI      │ - Topologies │ - Energy     │ - Config      │
│ - NVSim      │ - Routing    │ - Activity   │ - Stats       │
└──────────────┴──────────────┴──────────────┴───────────────┘
```

## Core Components

### 1. Simulation Engines

#### Host Engine (`host_engine/`)

**Purpose**: Simulates conventional processor and initiates PIM offloads

**Key Classes**:
- `HostEngine`: Main host simulation coordinator
- Inherits from `SimulationEngine` base class

**Responsibilities**:
- Execute host code
- Manage cache hierarchy
- Initiate PIM offloads
- Synchronize with device

**Integration**:
- Uses ZSim for cycle-accurate CPU simulation
- Communicates with device via `SocketComm`

#### Device Engine (`device_engine/`)

**Purpose**: Simulates memory-side processing elements

**Key Classes**:
- `DeviceEngine`: Main device simulation coordinator
- Inherits from `SimulationEngine` base class

**Responsibilities**:
- Execute PIM code on PEs
- Manage PE scheduling
- Handle local memory accesses
- Synchronize with host

**Integration**:
- Uses ZSim configured as PIM PEs
- Manages PEs via `PEPlacementManager`
- Uses `PEScheduler` for task assignment

### 2. Communication Layer (`communication/`)

**Purpose**: Enable host-device co-simulation

**Key Classes**:
- `SocketComm`: Socket-based communication
- `CommMessage`: Message structure

**Features**:
- Asynchronous message passing
- Timing synchronization
- Request/response protocol
- Serialization/deserialization

**Protocol**:
```
Host                          Device
  │                              │
  ├──── OFFLOAD_REQUEST ────────>│
  │                              │
  │                           Execute
  │                              │
  │<──── OFFLOAD_COMPLETE ───────┤
  │                              │
  ├──── SYNC_REQUEST ───────────>│
  │<──── SYNC_ACK ───────────────┤
```

### 3. Memory Models (`memory_models/`)

**Purpose**: Provide accurate memory timing and power

**Architecture**:
```
MemoryModel (base class)
    │
    ├── DRAMModel (Ramulator)
    ├── SRAMModel (CACTI)
    └── NVMModel (NVSim)
```

**Interface**:
- `access()`: Process memory request
- `tick()`: Advance one cycle
- `getEnergy()`: Query energy consumption

**Inner-Bank Timing** (NEW!):
- Detailed breakdown of DRAM internal datapath components
- H-tree network latency modeling (horizontal + vertical segments)
- Global I/O vs Local I/O distinction
- Column decoder, mux, and output driver timing
- See `INNER_BANK_TIMING_RESEARCH.md` and `pimid/memory/dram_architecture_v2.h`
- **Impact**: 6.65ns inner-bank delay (DDR4), 3.05ns (HBM2)
- **Key for PIM**: Enables accurate subarray-to-subarray communication modeling

**Extensibility**:
- Implement `MemoryModel` interface
- Register in `MemoryModelFactory`
- Add configuration parsing

### 4. Network Model (`network/`)

**Purpose**: Model intra-memory communication

**Architecture**:
```
NetworkModel (base class)
    │
    └── GarnetModel (GARNET 2.0)
```

**Features**:
- Multiple topologies (mesh, torus, crossbar, etc.)
- Flexible routing algorithms
- Virtual channels and flow control
- Power modeling

**Node Mapping**:
- Maps memory components to network nodes
- PEs, banks, chips, ranks as network endpoints

### 5. Power Model (`power/`)

**Purpose**: Comprehensive system power estimation

**Architecture**:
```
PowerModel (base class)
    │
    ├── McPATModel (for host/PEs)
    └── CompositePowerModel (aggregates all sources)
```

**Components**:
- Host processor power
- PE power
- Memory power (from memory models)
- Network power (from network model)

**Metrics**:
- Dynamic power
- Leakage power
- Total energy

### 6. Address Translation (`address_translation/`)

**Purpose**: Virtual memory support and PE placement

**Key Classes**:
- `AddressTranslator`: TLB and page table walker
- `PEPlacementManager`: PE hierarchy management

**Features**:
- Per-PE TLBs
- Shared page table
- Configurable page sizes
- Address mapping for discrete mode

**Placement Hierarchy**:
```
Logic Die
    │
    └── Rank
         │
         └── Chip
              │
              └── Bank
                   │
                   └── Subarray
```

### 7. Scheduler (`scheduler/`)

**Purpose**: Task assignment to PEs

**Implementations**:
- `NearestPEScheduler`: Data-aware placement
- `RoundRobinScheduler`: Simple round-robin
- `LoadBalancedScheduler`: Minimize load variance

**Interface**:
- `submitTask()`: Add task to queue
- `scheduleTask()`: Assign task to PE
- `markPEBusy/Idle()`: Update PE status

## Data Flow

### Offload Execution Flow

```
1. Host executes code
     │
     ├─> Encounters pimid_offload_begin()
     │
2. Host sends OFFLOAD_REQUEST to device
     │
3. Device receives request
     │
     ├─> Scheduler selects PE
     │
     ├─> Execute on selected PE
     │
     ├─> Memory accesses via memory model
     │
     ├─> Network communication via network model
     │
4. Execution completes
     │
5. Device sends OFFLOAD_COMPLETE to host
     │
6. Host resumes execution
```

### Memory Access Flow

```
PE/Core issues memory request
     │
     ├─> Address translation (TLB lookup)
     │
     ├─> If TLB miss: Page table walk
     │
     ├─> Physical address to memory model
     │
     ├─> Memory model simulates timing
     │
     ├─> Power model updates energy
     │
     └─> Response to PE/Core
```

## Configuration System

### YAML-Based Configuration

**Hierarchy**:
```
pimid_config.yaml (main)
    ├── host_config.yaml
    ├── device_config.yaml
    ├── memory_config.yaml
    ├── network_config.yaml
    └── power_config.yaml
```

**Parser**: `ConfigParser` class
- Loads YAML files
- Validates configurations
- Creates component instances

## Extension Points

### Adding a New Memory Model

1. Create `include/memory_models/my_model.h`
2. Inherit from `MemoryModel`
3. Implement required methods
4. Add to `MemoryModelFactory::createMemoryModel()`
5. Update YAML parser

### Adding a New Scheduler

1. Create `include/scheduler/my_scheduler.h`
2. Inherit from `PEScheduler`
3. Implement `scheduleTask()`
4. Add to `SchedulerFactory::createScheduler()`

### Adding a New Network Topology

1. Extend `GarnetModel`
2. Implement topology-specific routing
3. Update `NetworkConfig` enum
4. Add configuration parsing

## Performance Optimization

### Simulation Speed Optimization

1. **Event-driven simulation**: Only process events at scheduled times
2. **Parallel processing**: Independent components can run in parallel
3. **Fast-forward mode**: Skip non-critical regions
4. **Sampling**: Detailed simulation of representative intervals

### Memory Optimization

1. **Lazy initialization**: Create objects only when needed
2. **Shared models**: Single memory/network model instance
3. **Efficient data structures**: Use appropriate containers

## Debugging and Profiling

### Debug Features

- Configurable log levels (DEBUG, INFO, WARNING, ERROR)
- Component-level tracing
- State dumping
- Assertion checks (in debug builds)

### Profiling Tools

- Cycle-accurate statistics
- Component-level performance counters
- Memory access patterns
- Network traffic analysis

## Future Enhancements

1. **Extended PE Models**: Support for vector units, accelerators
2. **Checkpoint/Restore**: Save and resume simulation state
3. **Distributed Simulation**: Multi-node simulation for large systems
4. **GUI**: Visualization and interactive configuration
5. **Integration**: gem5, SST, other simulators

## References

- ZSim: https://github.com/s5z/zsim
- Ramulator: https://github.com/CMU-SAFARI/ramulator
- GARNET: https://www.gem5.org/
- McPAT: https://www.hpl.hp.com/research/mcpat/
- MultiPIM: https://github.com/Systems-ShiftLab/MultiPIM
