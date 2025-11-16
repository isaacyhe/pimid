# Network Models

This directory contains network-on-chip (NoC) model implementations for PIMID simulator.

## Structure

```
network_models/
├── include/           # Header files for network models
│   └── network_model.h     # Network model interface and GARNET implementation
│
└── src/              # Implementation files
    └── network_model.cpp   # GARNET network model
```

## GARNET Integration

PIMID uses GARNET 2.0 from gem5 for cycle-accurate NoC simulation.

### Supported Topologies
- **MESH_2D**: 2D mesh network
- **MESH_3D**: 3D mesh network
- **TORUS_2D**: 2D torus with wrap-around links
- **TORUS_3D**: 3D torus with wrap-around links
- **CROSSBAR**: Full crossbar interconnect
- **DRAGONFLY**: Dragonfly topology
- **FAT_TREE**: Fat-tree topology

### Routing Algorithms
- **XY**: Dimension-ordered routing (2D)
- **XYZ**: Dimension-ordered routing (3D)
- **ADAPTIVE**: Adaptive routing based on congestion
- **WEST_FIRST**: West-first turn model
- **NORTH_LAST**: North-last turn model
- **MINIMAL**: Minimal path routing
- **VALIANT**: Valiant's randomized routing

## Usage

### Host-Side Interconnect
```cpp
#include "host_engine/host_interconnect.h"

HostInterconnectConfig config;
config.type = HostInterconnectType::MESH_2D;
config.use_garnet = true;

auto interconnect = HostInterconnectFactory::createGarnet(config);
interconnect->initialize();
```

### Memory-Side NoC
```cpp
#include "network_models/network_model.h"

NetworkConfig config;
config.topology = NetworkTopology::MESH_2D;
config.num_rows = 4;
config.num_cols = 4;

auto network = std::make_unique<GarnetModel>(config);
network->initialize();
```

## Documentation

See [GARNET Integration Guide](../../docs/GARNET_INTEGRATION.md) for detailed information.
