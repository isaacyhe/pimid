# GARNET 2.0 Network-on-Chip Integration

## Overview

PIMID integrates with GARNET 2.0, a detailed and flexible on-chip network model from gem5, for accurate Network-on-Chip (NoC) simulation. GARNET provides cycle-accurate modeling of on-chip networks with support for various topologies, routing algorithms, and flow control mechanisms.

## GARNET Features

### Topologies Supported
- **Mesh**: 2D mesh network (default for many-core)
- **Torus**: 2D torus with wrap-around links
- **Crossbar**: Fully-connected crossbar
- **Custom**: User-defined topology via configuration

### Routing Algorithms
- **XY Routing**: Dimension-ordered routing (default)
- **Table-based Routing**: Configurable routing tables
- **Adaptive Routing**: Dynamic path selection based on congestion

### Flow Control
- **Virtual Channels**: Multiple VCs per physical channel
- **Credit-based**: Backpressure flow control
- **Wormhole**: Packet-based switching

## Integration Architecture

### Network Model Interface

PIMID's `NetworkModel` class provides the interface to GARNET:

```cpp
// pimid/network_models/include/network_model.h
class NetworkModel {
public:
    // Initialize network with topology configuration
    void initialize(const NetworkConfig& config);

    // Send packet through the network
    bool sendPacket(PacketID id, NodeID src, NodeID dest,
                   size_t size_bytes, PacketType type);

    // Tick the network (advance simulation)
    void tick();

    // Query network statistics
    double getAverageLatency() const;
    double getLinkUtilization() const;
    double getThroughput() const;
};
```

### Configuration

GARNET configuration is specified in `network_config.yaml`:

```yaml
network:
  # Topology
  topology: "mesh"  # mesh | torus | crossbar | custom

  # Mesh-specific
  num_rows: 4
  num_cols: 4

  # Link parameters
  link_width_bits: 128      # Flit size
  link_latency_cycles: 1    # Per-hop latency

  # Router parameters
  num_virtual_channels: 4   # VCs per physical channel
  vc_buffer_depth: 4        # Buffers per VC
  routing_algorithm: "XY"   # XY | table | adaptive

  # Clock
  network_clock_mhz: 2000   # 2 GHz
```

## Using GARNET in PIMID

### 1. Basic Setup

```cpp
#include "network_model.h"

// Create network configuration
NetworkConfig config;
config.topology = NetworkTopology::MESH;
config.num_rows = 4;
config.num_cols = 4;
config.link_width_bits = 128;
config.num_vcs = 4;

// Initialize network model
NetworkModel network(config);
network.initialize();
```

### 2. Sending Packets

```cpp
// Send a read request from PE 0 to Memory Controller at node 15
PacketID pid = network.sendPacket(
    0,              // source = PE 0
    15,             // dest = Memory controller
    64,             // 64 bytes
    PacketType::READ_REQUEST
);

// Tick simulation until packet arrives
while (!network.isPacketComplete(pid)) {
    network.tick();
}

// Get latency
Cycle latency = network.getPacketLatency(pid);
std::cout << "Packet latency: " << latency << " cycles" << std::endl;
```

### 3. Querying Statistics

```cpp
// After simulation
std::cout << "Network Statistics:" << std::endl;
std::cout << "  Average Latency: " << network.getAverageLatency() << " cycles" << std::endl;
std::cout << "  Link Utilization: " << network.getLinkUtilization() * 100 << "%" << std::endl;
std::cout << "  Throughput: " << network.getThroughput() << " GB/s" << std::endl;
std::cout << "  Total Packets: " << network.getTotalPackets() << std::endl;
```

## GARNET Source Location

GARNET source code is located in the gem5 submodule:

```
pimid/external/gem5/src/mem/ruby/network/garnet/
├── GarnetNetwork.cc          # Main network class
├── GarnetNetwork.hh
├── Router.cc                 # Router model
├── Router.hh
├── NetworkInterface.cc       # NI connecting cores to network
├── NetworkInterface.hh
├── NetworkLink.cc            # Inter-router links
├── NetworkLink.hh
├── RoutingUnit.cc            # Routing logic
├── RoutingUnit.hh
├── VirtualChannel.cc         # VC implementation
├── VirtualChannel.hh
└── flit.hh                   # Flit definition
```

## Advanced Features

### Custom Topologies

Define custom topologies by specifying node connections:

```yaml
network:
  topology: "custom"
  nodes:
    - id: 0
      type: "pe"
      connections: [1, 4]    # Connected to nodes 1 and 4
    - id: 1
      type: "pe"
      connections: [0, 2, 5]
    # ... more nodes
```

### Routing Tables

For table-based routing, specify paths:

```yaml
network:
  routing_algorithm: "table"
  routing_tables:
    node_0:
      dest_15: [0, 1, 5, 9, 13, 14, 15]  # Path from node 0 to 15
```

### Traffic Injection

Synthetic traffic patterns for testing:

```cpp
// Uniform random traffic
network.injectTraffic(TrafficPattern::UNIFORM_RANDOM,
                     injection_rate_packets_per_cycle);

// Hotspot traffic (80% to node 15)
network.injectTraffic(TrafficPattern::HOTSPOT,
                     injection_rate, hotspot_node=15);

// Transpose traffic
network.injectTraffic(TrafficPattern::TRANSPOSE, injection_rate);
```

## Performance Considerations

### 1. Virtual Channels
- **More VCs** = Better performance under congestion
- **Fewer VCs** = Lower area and power
- **Recommended**: 4-8 VCs for general-purpose networks

### 2. Buffer Depth
- **Deeper buffers** = Handle bursts better
- **Shallow buffers** = Lower latency when lightly loaded
- **Recommended**: 4-8 flits per VC

### 3. Link Width
- **Wider links** = Higher bandwidth
- **Narrower links** = More energy-efficient for low traffic
- **Recommended**: 128-256 bits for PIM systems

## Power Modeling

GARNET power modeling uses DSENT (included in gem5):

```cpp
// Get power breakdown
double router_dynamic = network.getRouterDynamicPower();  // W
double router_leakage = network.getRouterLeakagePower();  // W
double link_dynamic = network.getLinkDynamicPower();      // W
double link_leakage = network.getLinkLeakagePower();      // W

double total_power = router_dynamic + router_leakage +
                     link_dynamic + link_leakage;
```

## Example: 4x4 Mesh NoC for PIM

```cpp
// Configure 4x4 mesh with 16 PE + 4 memory controllers
NetworkConfig config;
config.topology = NetworkTopology::MESH;
config.num_rows = 5;  // 4 rows of PEs + 1 row of MCs
config.num_cols = 4;
config.link_width_bits = 256;  // Wide links for memory traffic
config.num_vcs = 8;           // High VC count for diverse traffic
config.routing = RoutingAlgorithm::XY;

NetworkModel noc(config);
noc.initialize();

// Map PEs to nodes 0-15, MCs to nodes 16-19
for (int pe = 0; pe < 16; pe++) {
    int mc = 16 + (pe % 4);  // Distribute PEs across MCs

    // Send memory request
    noc.sendPacket(pe, mc, 64, PacketType::READ_REQUEST);
}

// Run simulation
for (int cycle = 0; cycle < 10000; cycle++) {
    noc.tick();
}

// Print statistics
noc.printStats();
```

## Troubleshooting

### High Latency
- **Cause**: Network congestion, insufficient VCs
- **Solution**: Increase VCs, widen links, use adaptive routing

### Low Throughput
- **Cause**: Narrow links, shallow buffers
- **Solution**: Increase link width, increase buffer depth

### High Power
- **Cause**: Many VCs, wide links, high activity
- **Solution**: Optimize VC count, use narrow links where possible

## References

1. **GARNET Paper**:
   Agarwal et al., "GARNET: A detailed on-chip network model inside a full-system simulator", ISPASS 2009

2. **gem5 Documentation**:
   http://www.gem5.org/documentation/

3. **GARNET Tutorial**:
   http://www.gem5.org/documentation/learning_gem5/part3/garnet/

## Integration Status

| Feature | Status | Notes |
|---------|--------|-------|
| Mesh Topology | ✅ Ready | Default topology |
| XY Routing | ✅ Ready | Default routing |
| Virtual Channels | ✅ Ready | Configurable |
| Power Modeling | ✅ Ready | Via DSENT |
| Custom Topologies | 🔨 Partial | Requires configuration |
| Adaptive Routing | 🔨 Partial | Under development |

## Next Steps

1. Implement custom topology loader
2. Add support for QoS policies
3. Integrate deadlock detection
4. Add congestion-aware routing

---

For questions or issues with GARNET integration, refer to the gem5 documentation or PIMID issue tracker.
