# Memory Models

This directory contains memory model implementations for PIMID simulator.

## Structure

```
memory_models/
├── include/           # Header files for memory models
│   ├── memory_model.h      # Base memory model interface
│   ├── dram_model.h        # DRAM model (Ramulator)
│   ├── sram_model.h        # SRAM model (CACTI)
│   └── nvm_model.h         # NVM model (NVSim)
│
└── src/              # Implementation files
    ├── memory_model.cpp    # Factory and base implementation
    ├── dram_model.cpp      # DRAM implementation
    ├── sram_model.cpp      # SRAM implementation
    └── nvm_model.cpp       # NVM implementation
```

## Supported Technologies

### DRAM (via Ramulator)
- DDR3, DDR4, DDR5
- LPDDR4, LPDDR5
- HBM, HBM2, HBM3
- GDDR6

### SRAM (via CACTI)
- On-chip SRAM arrays
- Cache modeling
- Technology-scalable

### Emerging NVM (via NVSim)
- STT-MRAM
- PCM (Phase Change Memory)
- ReRAM (Resistive RAM)

## Usage

```cpp
#include "memory_models/memory_model.h"

// Create DRAM model
auto dram = MemoryModelFactory::createMemoryModel(
    MemoryTechnology::DRAM,
    "config/dram_config.yaml"
);

// Initialize
dram->initialize();

// Perform memory access
MemoryRequest req(addr, MemoryRequestType::READ, 64, cycle, domain, id);
Cycle latency = dram->access(req);

// Get energy consumption
double energy = dram->getTotalEnergy();
```

## Adding New Memory Models

See [Plugin Development Guide](../../docs/PLUGIN_DEVELOPMENT_GUIDE.md) for creating custom memory models.
