# Switch Topology Customization

## Overview

The Internal DRAM Network model supports flexible topology customization, allowing users to override default switch configurations with custom settings. This enables modeling of different interconnect strategies, optimizing for port count, latency, or custom network designs.

## Supported Topology Types

The framework supports the following topology types:

| Topology | Description | Use Case |
|----------|-------------|----------|
| **BUS** | Shared bus, 1 switch, N ports | Simple, low-cost, higher latency |
| **CROSSBAR** | Full crossbar, N×N connections | High bandwidth, low latency, expensive |
| **MESH_2D** | 2D mesh grid, √N × √N | Scalable, moderate latency |
| **TORUS_2D** | 2D torus with wrap-around | Better than mesh, balanced |
| **FAT_TREE** | Hierarchical tree with increasing BW upward | Data center style, high bisection BW |
| **H_TREE** | Binary tree (H-shaped) | Common in DRAM (Global Sense Amp) |
| **CUSTOM** | User-defined topology | Complete flexibility |

## Basic Usage

### 1. Get Default Configuration

```cpp
#include "memory_models/include/internal_dram_network.h"

using namespace pimid;

auto network = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);

// Get default configuration
int num_channels = 1;
int ranks_per_channel = 1;
SwitchHierarchyConfig config = network->getSwitchHierarchyConfig(
    num_channels,
    ranks_per_channel
);

// Inspect default topology at each level
std::cout << "L0 topology: " << InternalDRAMNetwork::getTopologyName(config.l0_config.topology) << std::endl;
std::cout << "L0 switches: " << config.l0_config.num_switches << std::endl;
std::cout << "L0 ports per switch: " << config.l0_config.ports_per_switch << std::endl;
```

### 2. Customize Specific Levels

```cpp
// Start with default config
SwitchHierarchyConfig custom_config;
custom_config.num_channels = 1;
custom_config.ranks_per_channel = 1;

// Override L2 (chip level) to use a mesh instead of bus
custom_config.l2_config.level = 2;
custom_config.l2_config.use_default = false;  // Enable custom config
custom_config.l2_config.topology = TopologyType::MESH_2D;
custom_config.l2_config.num_endpoints = 8;  // 8 chips
custom_config.l2_config.num_switches = 8;   // 8 switches for mesh
custom_config.l2_config.ports_per_switch = 5;  // N,S,E,W,Local

// Apply the custom configuration
network->setCustomSwitchHierarchy(custom_config);
```

### 3. Reduce Port Count with Multiple Switches

If you have 32 banks at L0 but want to limit each switch to 8 ports:

```cpp
SwitchHierarchyConfig config;
config.num_channels = 1;
config.ranks_per_channel = 1;

// Configure L0: Use multiple switches to reduce port count
int num_banks = 32;
int max_ports = 8;

// Calculate optimal switch count
int optimal_switches = InternalDRAMNetwork::calculateOptimalSwitchCount(
    num_banks,
    max_ports,
    TopologyType::CROSSBAR
);

config.l0_config.level = 0;
config.l0_config.use_default = false;
config.l0_config.topology = TopologyType::CROSSBAR;
config.l0_config.num_endpoints = num_banks;
config.l0_config.num_switches = optimal_switches;  // Will be 4
config.l0_config.ports_per_switch = max_ports;

network->setCustomSwitchHierarchy(config);
```

## Advanced Examples

### Example 1: Mixed Topology Hierarchy

Use different topologies at different levels to optimize for specific characteristics:

```cpp
SwitchHierarchyConfig config;
config.num_channels = 4;
config.ranks_per_channel = 2;

// L0: H-Tree within bank (mimics DRAM Global Sense Amp)
config.l0_config.use_default = false;
config.l0_config.topology = TopologyType::H_TREE;
config.l0_config.num_endpoints = 16;  // 16 banks per BG
config.l0_config.num_switches = 15;   // N-1 for binary tree
config.l0_config.ports_per_switch = 4;

// L1: Bus for BG to chip (simple, cost-effective)
config.l1_config.use_default = false;
config.l1_config.topology = TopologyType::BUS;
config.l1_config.num_endpoints = 4;   // 4 BGs per chip
config.l1_config.num_switches = 1;
config.l1_config.ports_per_switch = 4;

// L2: Mesh for chip level (scalable)
config.l2_config.use_default = false;
config.l2_config.topology = TopologyType::MESH_2D;
config.l2_config.num_endpoints = 8;   // 8 chips per rank
config.l2_config.num_switches = 8;
config.l2_config.ports_per_switch = 5;

// L3-L5: Use defaults (crossbar at system level)
config.l3_config.use_default = true;
config.l4_config.use_default = true;
config.l5_config.use_default = true;

network->setCustomSwitchHierarchy(config);

// Verify total switch count
int total = network->getTotalNumberOfSwitches(config.num_channels, config.ranks_per_channel);
std::cout << "Total switches: " << total << std::endl;
```

### Example 2: Port-Constrained Design

Design a system where no switch exceeds 16 ports:

```cpp
auto network = createInternalDRAMNetwork("HBM2", 16, 4, 4, 2);

SwitchHierarchyConfig config;
config.num_channels = 8;
config.ranks_per_channel = 2;

int max_ports = 16;

// For each level, calculate optimal switch count
for (int level = 0; level <= 5; level++) {
    NetworkLevelConfig* level_config = nullptr;
    int num_endpoints = 0;

    switch (level) {
        case 0:
            level_config = &config.l0_config;
            num_endpoints = 4;  // 4 banks per BG
            break;
        case 1:
            level_config = &config.l1_config;
            num_endpoints = 4;  // 4 BGs per chip
            break;
        case 2:
            level_config = &config.l2_config;
            num_endpoints = 2;  // 2 chips per rank (HBM2)
            break;
        case 3:
            level_config = &config.l3_config;
            num_endpoints = 2;  // 2 ranks per channel (HBM2)
            break;
        case 4:
            level_config = &config.l4_config;
            num_endpoints = 8;  // 8 channels
            break;
        case 5:
            level_config = &config.l5_config;
            num_endpoints = 8;  // Connect 8 channels
            break;
    }

    if (level_config && num_endpoints > 0) {
        level_config->use_default = false;
        level_config->level = level;
        level_config->topology = TopologyType::CROSSBAR;
        level_config->num_endpoints = num_endpoints;

        // Calculate switches needed for port constraint
        level_config->num_switches = InternalDRAMNetwork::calculateOptimalSwitchCount(
            num_endpoints,
            max_ports,
            level_config->topology
        );

        level_config->ports_per_switch = InternalDRAMNetwork::calculatePortsPerSwitch(
            level_config->topology,
            num_endpoints,
            level_config->num_switches
        );

        std::cout << "L" << level << ": " << level_config->num_switches
                  << " switches, " << level_config->ports_per_switch
                  << " ports each" << std::endl;
    }
}

network->setCustomSwitchHierarchy(config);
```

### Example 3: Fat-Tree for High Bandwidth

Use fat-tree topology for better bisection bandwidth at upper levels:

```cpp
SwitchHierarchyConfig config;
config.num_channels = 4;
config.ranks_per_channel = 2;

// Use fat-tree at L3 and above for high bisection bandwidth
config.l3_config.use_default = false;
config.l3_config.topology = TopologyType::FAT_TREE;
config.l3_config.num_endpoints = 8;  // 8 ranks total
config.l3_config.num_switches = InternalDRAMNetwork::calculateOptimalSwitchCount(
    8, 16, TopologyType::FAT_TREE
);
config.l3_config.ports_per_switch = 16;

config.l4_config.use_default = false;
config.l4_config.topology = TopologyType::FAT_TREE;
config.l4_config.num_endpoints = 4;  // 4 channels
config.l4_config.num_switches = 2;
config.l4_config.ports_per_switch = 8;

network->setCustomSwitchHierarchy(config);
```

## Helper Functions

### calculateOptimalSwitchCount()

Calculate how many switches are needed to satisfy a port constraint:

```cpp
// Example: 64 endpoints, max 8 ports per switch, crossbar topology
int num_switches = InternalDRAMNetwork::calculateOptimalSwitchCount(
    64,        // num_endpoints
    8,         // max_ports_per_switch
    TopologyType::CROSSBAR
);
// Result: 8 switches (64/8 = 8)
```

### calculatePortsPerSwitch()

Calculate ports per switch for a topology:

```cpp
// Example: 16 endpoints, 4 switches, mesh topology
int ports = InternalDRAMNetwork::calculatePortsPerSwitch(
    TopologyType::MESH_2D,
    16,        // num_endpoints
    4          // num_switches
);
// Result: 5 (N,S,E,W,Local)
```

## Benefits of Custom Topologies

1. **Port Count Optimization**: Reduce switch complexity by using multiple smaller switches
2. **Latency Tuning**: Choose topologies with better latency characteristics (mesh vs bus)
3. **Bandwidth Management**: Use fat-tree or H-tree for better bandwidth distribution
4. **Cost-Performance Trade-offs**: Balance between crossbar (expensive, fast) and bus (cheap, slower)
5. **Power Optimization**: Fewer ports = lower power per switch
6. **Scalability**: Mesh/torus topologies scale better to large systems

## Power/Area Impact

Custom topologies directly affect power and area calculations:

```cpp
// Get current configuration
SwitchHierarchyConfig config = network->getSwitchHierarchyConfig(num_channels, ranks_per_channel);

// Calculate power based on actual configuration
double total_power = 0.0;
double power_per_port = 0.1;  // mW per port

for (int level = 0; level <= 5; level++) {
    NetworkLevelConfig* cfg = /* get config for level */;
    double level_power = cfg->num_switches *
                        cfg->ports_per_switch *
                        power_per_port;
    total_power += level_power;
}

std::cout << "Network power: " << total_power << " mW" << std::endl;
```

## Best Practices

1. **Start with defaults**: Get default config and modify only what's needed
2. **Validate constraints**: Ensure ports_per_switch matches topology and endpoints
3. **Document custom configs**: Keep track of why specific topologies were chosen
4. **Test at scale**: Verify custom configs work for target system size
5. **Consider locality**: Match topology to expected data movement patterns

## See Also

- `docs/switch_calculation.md` - Basic switch count calculations
- `test/unit/test_switch_calculation.cpp` - Example usage
- `pimid/memory_models/include/internal_dram_network.h` - API reference
