# GARNET Network Integration Guide

This document explains how PIMID uses GARNET for both host-side interconnect and memory-side network-on-chip (NoC).

## Overview

PIMID integrates **GARNET 2.0** (from gem5) for cycle-accurate network-on-chip simulation in two contexts:

1. **Host-Side Interconnect**: Connecting cores, caches, and memory controllers on the host processor
2. **Memory-Side NoC**: Connecting processing elements (PEs) within the PIM device

Both use the same underlying GARNET infrastructure but with different configurations and topologies.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    PIMID Simulator                          │
├──────────────────────────┬──────────────────────────────────┤
│   Host Side              │   Memory Side                    │
│                          │                                  │
│  ┌────────────────┐      │  ┌────────────────┐             │
│  │ Cores (0-N)    │      │  │ PEs (0-M)      │             │
│  ├────────────────┤      │  ├────────────────┤             │
│  │ LLC            │      │  │ Memory Banks   │             │
│  ├────────────────┤      │  ├────────────────┤             │
│  │ Memory Ctrl    │      │  │ Memory Ctrl    │             │
│  └────────────────┘      │  └────────────────┘             │
│         │                │         │                        │
│         ▼                │         ▼                        │
│  ┌────────────────┐      │  ┌────────────────┐             │
│  │ GARNET NoC     │      │  │ GARNET NoC     │             │
│  │ (Host Interconnect)   │  │ (Memory-Side)  │             │
│  │                │      │  │                │             │
│  │ - Mesh/Crossbar│      │  │ - Mesh/Torus   │             │
│  │ - Cache coherent│     │  │ - Data movement│             │
│  │ - XY routing   │      │  │ - Adaptive routing│           │
│  └────────────────┘      │  └────────────────┘             │
└──────────────────────────┴──────────────────────────────────┘
```

---

## Host-Side Interconnect Using GARNET

### Purpose

The host-side interconnect connects:
- **Cores** (CPU cores)
- **LLC** (Last Level Cache)
- **Memory Controllers**

This models the cache-coherent interconnect found in modern processors (e.g., Intel's ring/mesh, AMD's Infinity Fabric).

### Configuration

#### YAML Configuration

```yaml
# host_config.yaml
interconnect:
  # Use GARNET-based interconnect
  type: "mesh_2d"  # Options: crossbar, mesh_2d, ring, custom_garnet

  # Bandwidth (for simple models)
  bandwidth_gbs: 64

  # Latency (for simple models)
  latency_cycles: 10

  # GARNET-specific configuration
  garnet:
    enabled: true
    topology: "MESH_2D"
    num_rows: 2
    num_cols: 3
    virtual_channels: 4
    link_width_bytes: 8
    router_latency: 2
    link_latency: 1
```

#### C++ Configuration

```cpp
#include "host_engine/host_interconnect.h"

// Configure GARNET for host interconnect
HostInterconnectConfig config;
config.type = HostInterconnectType::MESH_2D;
config.num_cores = 4;
config.num_memory_controllers = 2;
config.use_garnet = true;

// GARNET-specific settings
config.garnet_config.topology = NetworkTopology::MESH_2D;
config.garnet_config.num_rows = 2;
config.garnet_config.num_cols = 3;
config.garnet_config.virtual_channels = 4;
config.garnet_config.routing = RoutingAlgorithm::XY;

// Create interconnect
auto interconnect = HostInterconnectFactory::createGarnet(config);
interconnect->initialize();
```

### Host-Side Topology Examples

#### 4-Core with LLC and Memory Controller (2x3 Mesh)

```
┌──────┬──────┬──────┐
│ Core │ Core │ LLC  │
│  0   │  1   │      │
├──────┼──────┼──────┤
│ Core │ Core │ MC   │
│  2   │  3   │      │
└──────┴──────┴──────┘
```

Configuration:
```yaml
garnet:
  topology: "MESH_2D"
  num_rows: 2
  num_cols: 3
  routing: "XY"
```

#### 8-Core with Crossbar (High Performance)

```
         ┌─────────────────┐
         │    Crossbar     │
         └─────────────────┘
    ┌──────┬────┬────┬─────┬────┐
    │      │    │    │     │    │
  Core   Core  LLC  MC0  MC1  ...
   0-7
```

Configuration:
```yaml
garnet:
  topology: "CROSSBAR"
```

### Usage in Host Engine

```cpp
// In host_engine.cpp
#include "host_engine/host_interconnect.h"

void HostEngine::initialize() {
    // Create GARNET-based host interconnect
    HostInterconnectConfig ic_config;
    ic_config.type = HostInterconnectType::MESH_2D;
    ic_config.use_garnet = true;

    host_interconnect_ = HostInterconnectFactory::create(ic_config);
    host_interconnect_->initialize();
}

void HostEngine::sendMemoryRequest(const MemoryRequest& req) {
    // Convert to interconnect request
    HostInterconnectRequest ic_req;
    ic_req.src_id = req.src_id;  // Core ID
    ic_req.dst_id = 1000;         // Memory controller ID
    ic_req.addr = req.addr;
    ic_req.size = req.size;
    ic_req.type = req.type;
    ic_req.issue_cycle = getCurrentCycle();

    // Send through GARNET interconnect
    if (host_interconnect_->canSend(ic_req.src_id)) {
        host_interconnect_->sendRequest(ic_req);
    }
}

void HostEngine::tick() {
    // Advance interconnect
    host_interconnect_->tick();

    // Check for responses
    for (uint32_t core_id = 0; core_id < num_cores_; core_id++) {
        if (host_interconnect_->hasResponse(core_id)) {
            auto response = host_interconnect_->getResponse(core_id);
            processResponse(response);
        }
    }
}
```

---

## Memory-Side NoC Using GARNET

### Purpose

The memory-side NoC connects:
- **Processing Elements (PEs)** at various memory hierarchy levels
- **Memory Banks**
- **Data movement infrastructure**

This models the communication network within the PIM device for inter-PE and PE-to-memory traffic.

### Configuration

#### YAML Configuration

```yaml
# network_config.yaml
topology:
  type: "MESH_2D"
  dimensions:
    rows: 4
    cols: 4

routing:
  algorithm: "XY"  # or ADAPTIVE for better performance

garnet:
  enabled: true
  version: "garnet2.0"
  detailed_model: true

  # Router configuration
  pipeline:
    num_stages: 4
    cycles_per_stage: [1, 1, 1, 1]

  # Virtual channels
  num_vcs: 4

  # Link parameters
  link_width_bytes: 8
  link_latency_cycles: 1

  # Buffer sizes
  input_buffer_depth: 8
  output_buffer_depth: 4
```

#### C++ Configuration

```cpp
#include "network/network_model.h"

// Configure GARNET for memory-side NoC
NetworkConfig config;
config.topology = NetworkTopology::MESH_2D;
config.num_rows = 4;
config.num_cols = 4;
config.routing = RoutingAlgorithm::XY;
config.virtual_channels = 4;
config.link_width_bytes = 8;
config.link_latency = 1;
config.router_latency = 4;

// Create GARNET network
auto network = std::make_unique<GarnetModel>(config);
network->initialize();
```

### Memory-Side Topology Examples

#### 4x4 Mesh for 16 PEs

```
PE0 ─── PE1 ─── PE2 ─── PE3
 │       │       │       │
PE4 ─── PE5 ─── PE6 ─── PE7
 │       │       │       │
PE8 ─── PE9 ─── PE10─── PE11
 │       │       │       │
PE12─── PE13─── PE14─── PE15
```

Each PE connects to local memory banks. Data movement uses the mesh network.

Configuration:
```yaml
topology:
  type: "MESH_2D"
  dimensions:
    rows: 4
    cols: 4

pim:
  pe_placement_level: "BANK"
  num_pes_per_level: 1
```

#### 3D Mesh for HBM-Style Architecture

```
Layer 0 (Top Die - PEs):
PE0 ─── PE1
 │       │
PE2 ─── PE3

  ┃     ┃     (Vertical TSV connections)

Layer 1 (Memory):
Bank0 ─ Bank1
 │       │
Bank2 ─ Bank3
```

Configuration:
```yaml
topology:
  type: "MESH_3D"
  dimensions:
    rows: 2
    cols: 2
    layers: 2

memory_hierarchy:
  logic_die:
    has_logic_die: true
```

### Usage in Device Engine

```cpp
// In device_engine.cpp
#include "network/network_model.h"

void DeviceEngine::initialize() {
    // Create GARNET network for memory-side NoC
    NetworkConfig net_config;
    net_config.topology = NetworkTopology::MESH_2D;
    net_config.num_rows = 4;
    net_config.num_cols = 4;

    network_model_ = std::make_unique<GarnetModel>(net_config);
    network_model_->initialize();

    // Add PEs as network nodes
    for (uint32_t i = 0; i < num_pes_; i++) {
        NetworkNode node(i, PEPlacementLevel::BANK, i);
        network_model_->addNode(node);
    }
}

void DeviceEngine::sendDataToPE(uint32_t src_pe, uint32_t dst_pe,
                                 Address addr, uint64_t size) {
    // Create network packet
    NetworkPacket packet(src_pe, dst_pe, PacketType::DATA,
                        size, addr, getCurrentCycle());

    // Inject into GARNET network
    if (network_model_->canInject(src_pe)) {
        network_model_->injectPacket(packet);
    }
}

void DeviceEngine::tick() {
    // Advance network
    network_model_->tick();

    // Check for arrived packets at each PE
    for (uint32_t pe_id = 0; pe_id < num_pes_; pe_id++) {
        if (network_model_->hasArrived(pe_id)) {
            auto packet = network_model_->extractPacket(pe_id);
            processPEData(pe_id, packet);
        }
    }
}
```

---

## GARNET Features Utilized

### 1. Virtual Channels

Both host and memory-side networks use virtual channels for:
- **Deadlock avoidance**
- **Quality of Service (QoS)**
- **Traffic class separation**

```yaml
virtual_channels:
  num_vcs: 4

  vc_classes:
    - name: "request"
      num_vcs: 2
      priority: 1

    - name: "response"
      num_vcs: 2
      priority: 2
```

### 2. Routing Algorithms

**Host-Side** (typically simpler):
- XY routing (deterministic)
- Minimal routing

**Memory-Side** (more sophisticated):
- Adaptive routing (congestion-aware)
- WEST_FIRST, NORTH_LAST (deadlock-free)
- Custom routing algorithms

```cpp
// Example: Adaptive routing for memory-side
config.routing = RoutingAlgorithm::ADAPTIVE;
config.adaptive_threshold = 8;  // Buffer occupancy threshold
```

### 3. Power Modeling

GARNET includes detailed power modeling via DSENT:

```yaml
garnet:
  power_model_enabled: true

  dsent:
    enabled: true
    tech_node_nm: 22
    config_file: "configs/router_22nm.cfg"
```

Power breakdown:
- Router dynamic power (per flit)
- Router leakage power
- Link dynamic power (per bit)
- Link leakage power

### 4. Flow Control

Credit-based flow control prevents buffer overflow:

```yaml
buffers:
  flow_control: "credit"
  credit_latency_cycles: 2
  input_buffer_depth: 8
```

---

## Performance Comparison

### Host-Side Interconnect

| Topology | Latency | Throughput | Power | Use Case |
|----------|---------|------------|-------|----------|
| Crossbar | Low (1-2 cycles) | Highest | Highest | Small core count (<8) |
| Ring | Medium | Medium | Low | Medium core count (8-16) |
| 2D Mesh | Medium-High | High | Medium | Large core count (16+) |
| GARNET Mesh | Accurate | Accurate | Accurate | Detailed modeling |

### Memory-Side NoC

| Topology | Data Locality | Scalability | Power | Use Case |
|----------|---------------|-------------|-------|----------|
| Mesh 2D | Good | Excellent | Medium | General purpose |
| Mesh 3D | Excellent | Excellent | Medium | HBM/HMC systems |
| Torus | Very Good | Excellent | Higher | Large-scale PIM |
| Crossbar | N/A | Poor | Highest | Small PE count |

---

## Configuration Examples

### Example 1: Host with 8-Core Mesh + Memory with 4x4 Mesh

```yaml
# Host interconnect (8 cores)
host:
  interconnect:
    type: "mesh_2d"
    garnet:
      enabled: true
      topology: "MESH_2D"
      num_rows: 3
      num_cols: 4  # 8 cores + LLC + 2 MCs + 1 extra = 12 nodes
      routing: "XY"

# Memory-side NoC (16 PEs)
network:
  topology:
    type: "MESH_2D"
    dimensions:
      rows: 4
      cols: 4

  garnet:
    enabled: true
    routing:
      algorithm: "ADAPTIVE"
```

### Example 2: Crossbar Host + 3D Mesh Memory

```yaml
# Host interconnect (simple crossbar)
host:
  interconnect:
    type: "crossbar"
    bandwidth_gbs: 100

# Memory-side NoC (3D mesh for HBM)
network:
  topology:
    type: "MESH_3D"
    dimensions:
      rows: 2
      cols: 2
      layers: 4  # 4 memory stacks

  garnet:
    enabled: true
    routing:
      algorithm: "XYZ"
```

### Example 3: Both Using GARNET (Maximum Accuracy)

```yaml
# Host interconnect (GARNET mesh)
host:
  interconnect:
    type: "custom_garnet"
    garnet:
      enabled: true
      topology: "MESH_2D"
      num_rows: 2
      num_cols: 3
      virtual_channels: 4
      detailed_model: true
      power_model_enabled: true

# Memory-side NoC (GARNET mesh)
network:
  topology:
    type: "MESH_2D"
    dimensions:
      rows: 4
      cols: 4

  garnet:
    enabled: true
    detailed_model: true
    power_model_enabled: true
    version: "garnet2.0"

  routing:
    algorithm: "ADAPTIVE"

  virtual_channels:
    num_vcs: 4
```

---

## Statistics and Debugging

### Collecting Statistics

Both host and memory-side networks provide detailed statistics:

```cpp
// Host interconnect stats
host_interconnect->printStats();
// Output:
// Total requests: 10000
// Total responses: 9998
// Average latency: 15.3 cycles
// Underlying GARNET network stats:
//   Total packets: 10000
//   Total flits: 40000
//   Avg packet latency: 12.5 cycles

// Memory-side NoC stats
network_model->printStats();
// Output:
// Total packets: 50000
// Total flits: 200000
// Average latency: 8.7 cycles
// Max latency: 45 cycles
// Link utilization: 23.4%
// Total energy: 1.234 mJ
```

### Debug Output

Enable detailed logging:

```yaml
network:
  garnet:
    debug: true
    trace_output: "garnet_trace.txt"

simulation:
  log_level: "DEBUG"
```

Output includes:
- Router pipeline stages
- VC allocation decisions
- Switch arbitration
- Credit updates
- Flit movements

---

## Integration with External Tools

### GARNET from gem5

PIMID uses GARNET 2.0 from gem5:

```bash
# Location
pimid/external/gem5/src/mem/ruby/network/garnet/

# Key files
GarnetNetwork.py
Router.py
NetworkInterface.py
```

### DSENT for Power

GARNET uses DSENT for power modeling:

```bash
# Location
pimid/external/gem5/ext/dsent/

# Configuration
configs/router_22nm.cfg
```

---

## Best Practices

### 1. Choose Appropriate Topology

**Host-side**:
- ≤ 8 cores: Crossbar or Ring
- 8-16 cores: Ring or small Mesh
- 16+ cores: 2D Mesh with GARNET

**Memory-side**:
- ≤ 16 PEs: 2D Mesh
- 16-64 PEs: 2D Mesh or Torus
- 64+ PEs: 3D Mesh or Dragonfly

### 2. Virtual Channel Allocation

- Use 4 VCs for general workloads
- Use 8 VCs for high-traffic scenarios
- Separate VCs for request/response (avoid deadlock)

### 3. Router Pipeline

- 4-stage pipeline is typical (route, VC alloc, switch alloc, switch traversal)
- Reduce to 2-3 stages for low latency
- Increase to 5-6 stages for high frequency

### 4. Power-Performance Trade-offs

```yaml
# High performance
garnet:
  link_width_bytes: 16
  virtual_channels: 8
  input_buffer_depth: 16

# Low power
garnet:
  link_width_bytes: 4
  virtual_channels: 2
  input_buffer_depth: 4
```

---

## Troubleshooting

### Issue: High Network Latency

**Diagnosis**:
```bash
# Check statistics
Average packet latency: 150 cycles  # Too high!
```

**Solutions**:
1. Use adaptive routing
2. Increase virtual channels
3. Increase buffer depths
4. Check for hotspots in traffic pattern

### Issue: Deadlock

**Diagnosis**:
```bash
Simulation hangs, no packets moving
```

**Solutions**:
1. Ensure sufficient VCs for deadlock avoidance
2. Use escape VCs
3. Verify routing algorithm is deadlock-free

### Issue: Power Too High

**Diagnosis**:
```bash
Network power: 15W  # Too high for target
```

**Solutions**:
1. Reduce link width
2. Enable power gating
3. Use smaller buffers
4. Clock gate idle routers

---

## References

- GARNET 2.0: gem5 documentation
- DSENT: dsent.mit.edu
- PIMID Architecture: docs/ARCHITECTURE.md
- Configuration Guide: docs/CONFIGURATION_GUIDE.md

---

**Last Updated**: 2025-11-16
**Version**: 2.0.0
