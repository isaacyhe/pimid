# Dynamic Switch Count Calculation

## Overview

The Internal DRAM Network model now supports dynamic calculation of the number of switches and switch levels based on the actual DRAM hierarchy configuration. This replaces previously hardcoded values and enables accurate modeling of different memory configurations.

## Network Topology Hierarchy

The network hierarchy is based on the DRAM organization, with 6 levels of switches (L0 through L5):

```
L0: Banks in a BG share one L0 switch         → 1 per bank group
L1: Each BG has 1 L1 switch connecting banks  → 1 per bank group
L2: Each chip has 1 L2 switch connecting BGs  → 1 per chip
L3: Each rank has 1 L3 switch connecting chips → 1 per rank
L4: Each MC has 1 L4 switch connecting ranks  → 1 per channel
L5: One root switch connecting all channels    → 1 per system
```

## Switch Count Calculation

### Formula

For a given DRAM configuration with:
- `num_channels` memory channels
- `ranks_per_channel` ranks per channel
- `chips_per_rank` chips (devices) per rank
- `bgs_per_chip` bank groups per chip
- `banks_per_bg` banks per bank group

The number of switches at each level is:

```
num_bgs = num_channels × ranks_per_channel × chips_per_rank × bgs_per_chip
num_chips = num_channels × ranks_per_channel × chips_per_rank
num_ranks = num_channels × ranks_per_channel

L0 switches = num_bgs
L1 switches = num_bgs
L2 switches = num_chips
L3 switches = num_ranks
L4 switches = num_channels
L5 switches = 1

Total switches = 2×num_bgs + num_chips + num_ranks + num_channels + 1
```

### Example Calculations

#### DDR4 Configuration (1 channel, 1 rank)
- Channels: 1
- Ranks per channel: 1
- Chips per rank: 8
- Bank groups per chip: 4
- Banks per BG: 4

**Calculation:**
```
num_bgs = 1 × 1 × 8 × 4 = 32
num_chips = 1 × 1 × 8 = 8
num_ranks = 1 × 1 = 1

L0: 32 switches
L1: 32 switches
L2: 8 switches
L3: 1 switch
L4: 1 switch
L5: 1 switch

Total: 75 switches across 6 levels
```

#### HBM2 Configuration (8 channels, 2 ranks)
- Channels: 8
- Ranks per channel: 2
- Chips per rank: 2
- Bank groups per chip: 4
- Banks per BG: 4

**Calculation:**
```
num_bgs = 8 × 2 × 2 × 4 = 128
num_chips = 8 × 2 × 2 = 32
num_ranks = 8 × 2 = 16

L0: 128 switches
L1: 128 switches
L2: 32 switches
L3: 16 switches
L4: 8 switches
L5: 1 switch

Total: 313 switches across 6 levels
```

## API Usage

### C++ API

```cpp
#include "memory_models/include/internal_dram_network.h"

using namespace pimid;

// Create an internal DRAM network
auto network = createInternalDRAMNetwork(
    "DDR4",    // DRAM type
    16,        // subarrays per bank
    4,         // banks per bank group
    4,         // bank groups per chip
    8          // chips per rank
);

// Get the number of switch levels (returns 6)
int num_levels = network->getNumberOfSwitchLevels();

// Calculate total number of switches
int num_channels = 1;
int ranks_per_channel = 1;
int total_switches = network->getTotalNumberOfSwitches(
    num_channels,
    ranks_per_channel
);

// Get switches at a specific level
for (int level = 0; level <= 5; level++) {
    int count = network->getNumberOfSwitchesAtLevel(
        level,
        num_channels,
        ranks_per_channel
    );
    std::cout << "L" << level << ": " << count << " switches" << std::endl;
}
```

### Output Example

When calling `getTotalNumberOfSwitches()`, you'll get detailed output like:

```
[InternalDRAMNetwork] Switch hierarchy calculation:
  Configuration:
    Channels: 1
    Ranks per channel: 1
    Chips per rank: 8
    Bank groups per chip: 4
    Banks per BG: 4

  Hierarchy totals:
    Total ranks: 1
    Total chips: 8
    Total bank groups: 32
    Total banks: 128

  Switch counts by level:
    L0 (BG level): 32 switches
    L1 (BG level): 32 switches
    L2 (Chip level): 8 switches
    L3 (Rank level): 1 switches
    L4 (Channel level): 1 switches
    L5 (System level): 1 switch
    TOTAL: 75 switches across 6 levels
```

## Integration with Power/Area Models

These dynamic switch counts can be used to calculate:
- Network power consumption based on the actual number of switches
- Area overhead for the interconnect fabric
- Energy per data transfer across different hierarchy levels

Example integration with power models:

```cpp
auto network = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);

int num_channels = 1;
int ranks_per_channel = 1;

// Get switch counts for power calculation
int total_switches = network->getTotalNumberOfSwitches(
    num_channels,
    ranks_per_channel
);

// Calculate power based on switch count
double power_per_switch = 0.5;  // mW per switch
double total_network_power = total_switches * power_per_switch;

std::cout << "Network power: " << total_network_power << " mW" << std::endl;
```

## Supported DRAM Types

The switch calculation is supported for all DRAM types:
- DDR3, DDR4, DDR5
- LPDDR5
- GDDR6
- HBM, HBM2, HBM3
- NVM types: SRAM, STT-MRAM, PCM, ReRAM

Each type may have different default configurations for banks per BG, BGs per chip, etc.

## References

- Network topology design based on DRAM architecture specifications
- Switch hierarchy matches physical organization from bank level to system level
- Designed for accurate PIM (Processing-In-Memory) data movement modeling
