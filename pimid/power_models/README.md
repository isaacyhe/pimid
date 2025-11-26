# Power Models

This directory contains power model implementations for PIMID simulator.

## Structure

```
power_models/
├── include/           # Header files for power models
│   └── power_model.h       # Power model interface
│
└── src/              # Implementation files
    ├── power_model.cpp     # Base power model
    └── mcpat_model.cpp     # McPAT integration
```

## McPAT Integration

PIMID integrates McPAT for processor power modeling.

### Supported Components
- **Cores**: In-order and out-of-order processors
- **Caches**: L1, L2, L3 caches
- **Memory Controllers**: DRAM controllers
- **Network**: NoC routers and links
- **PEs**: Processing elements

### Technology Nodes
- 45nm, 32nm, 22nm, 14nm, 7nm

### Device Types
- **HP**: High-performance
- **LSTP**: Low-standby-power
- **LOP**: Low-operating-power

## Usage

```cpp
#include "power/power_model.h"

TechnologyParams params;
params.tech_node_nm = 22;
params.device_type = "HP";
params.temperature_k = 350;
params.frequency_ghz = 2.0;

auto power_model = std::make_shared<McPATModel>(params);
power_model->initialize();

// Estimate power
ActivityStats stats;
// ... fill in activity statistics
PowerMetrics metrics = power_model->estimatePower(
    PowerComponent::CORE, stats);

std::cout << "Dynamic power: " << metrics.dynamic_power_w << " W" << std::endl;
std::cout << "Leakage power: " << metrics.leakage_power_w << " W" << std::endl;
std::cout << "Total energy: " << metrics.total_energy_j << " J" << std::endl;
```

## Adding Custom Power Models

See [Plugin Development Guide](../../docs/PLUGIN_DEVELOPMENT_GUIDE.md) for creating custom power models.
