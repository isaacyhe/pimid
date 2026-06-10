# PIMID External Dependencies

This directory contains all external simulators and modeling tools integrated into PIMID.

## Included Components

### 1. Ramulator2 (DRAM Simulator)
- **Repository**: https://github.com/CMU-SAFARI/ramulator2
- **Purpose**: Cycle-accurate DRAM timing simulation
- **Technologies**: DDR4, DDR5, LPDDR4, LPDDR5, HBM2, HBM3, GDDR6
- **Location**: `ramulator/`
- **Build**: Integrated via CMake (auto-built with PIMID)

### 2. CACTI 7.0 (SRAM/Cache Characterization)
- **Repository**: https://github.com/HewlettPackard/cacti
- **Purpose**: Cache and memory access time, area, and power modeling
- **Technologies**: SRAM, Cache hierarchies, subarray characterization
- **Location**: `cacti/`
- **Build**: Integrated via CMake (auto-built with PIMID)

### 3. NVSim (Non-Volatile Memory Simulator)
- **Repository**: https://github.com/SEAL-UCSB/NVSim
- **Purpose**: Performance, energy, and area estimation for NVM
- **Technologies**: STT-MRAM, PCM, ReRAM, NAND Flash
- **Location**: `nvsim/`
- **Note**: **Namespaced** (`nvsim::`) to avoid symbol collision with CACTI
- **Build**: Integrated via CMake (auto-built with PIMID)

### 4. McPAT (Power Modeling)
- **Repository**: https://github.com/HewlettPackard/mcpat
- **Purpose**: Integrated power, area, and timing modeling
- **Components**: Cores, caches, NoCs, memory controllers
- **Location**: `mcpat/`
- **Note**: Uses **bundled CACTI 6.5-P** (API compatibility with McPAT internals)
- **Build**: Integrated via CMake (auto-built with PIMID)

### 5. Garnet (Network-on-Chip Simulator)
- **Origin**: Extracted from gem5
- **Purpose**: Detailed NoC simulation with virtual channels, routing, flow control
- **Topologies**: Mesh, H-Tree, Crossbar, Ring
- **Location**: `garnet/`
- **Note**: **Standalone** - no gem5 dependency, includes gem5 compatibility headers
- **Build**: Integrated via CMake (auto-built with PIMID)

### 6. ZSim (Workload Execution)
- **Repository**: https://github.com/s5z/zsim (modified)
- **Purpose**: Fast x86-64 multicore simulation for workload execution
- **Features**: Cycle-accurate, memory hierarchy, fast-forward mode
- **Location**: `zsim/`
- **Requires**: **Intel PIN 4.1** (see below)
- **Build**: `cd zsim && scons -j$(nproc)`

### 7. Intel PIN (Optional)
- **Purpose**: Dynamic binary instrumentation for ZSim
- **Version**: **PIN 4.1** (pin-external-4.1-99687-gd9b8f822c-gcc-linux)
- **Location**: `pin/` (symlink to actual installation)
- **Required for**: `pimid --mode standalone` only
- **Not required for**: `pimid --mode sim` (config-driven simulation)

## Integration Status

| Component | Version | Build | Notes |
|-----------|---------|-------|-------|
| Ramulator2 | Latest | CMake | Auto-built |
| CACTI | 7.0 | CMake | Auto-built |
| NVSim | Namespaced | CMake | Auto-built, avoids CACTI collision |
| McPAT | 1.3 | CMake | Auto-built with bundled CACTI 6.5-P |
| Garnet | Extracted | CMake | Auto-built, standalone |
| ZSim | Modified | SCons | Manual build required |
| PIN | 4.1 | N/A | User-provided (optional) |

## PIMID Integration Points

### Memory Models (`src/memory/`, `include/memory/`)
- **DRAM**: `ramulator_wrapper.cpp` → `ramulator/`
- **SRAM**: `cacti_wrapper.cpp` → `cacti/`
- **NVM**: `nvsim_wrapper.cpp` → `nvsim/`

### Network Models (`src/network/`, `include/network/`)
- **Garnet NoC**: `garnet_wrapper.cpp` → `garnet/`

### Power Models (`src/power/`, `include/power/`)
- **McPAT**: `mcpat_wrapper.cpp` → `mcpat/`

## Building

All external models (except ZSim/PIN) are built automatically with PIMID:

```bash
cd pimid
mkdir build && cd build
cmake .. && make -j$(nproc)
```

### Building ZSim (Optional)

Only needed for `--mode standalone`:

```bash
# Set up PIN first
export PINPATH=/path/to/pin-4.1

# Build ZSim
cd pimid/external/zsim
scons -j$(nproc)
```

### Setting up PIN (Optional)

```bash
cd pimid/external
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-external-4.1-99687-gd9b8f822c-gcc-linux.tar.gz
tar xzf pin-external-4.1-99687-gd9b8f822c-gcc-linux.tar.gz
ln -s pin-external-4.1-99687-gd9b8f822c-gcc-linux pin
```

## CACTI Version Notes

PIMID uses **two versions of CACTI**:

1. **CACTI 7.0** (`external/cacti/`): Used by PIMID's `cacti_wrapper` for standalone SRAM/subarray characterization. Modern API with improved accuracy.

2. **CACTI 6.5-P** (`external/mcpat/cacti/`): Bundled with McPAT. Required for McPAT's internal power calculations due to API dependencies (`TechnologyParameter::DeviceType`, etc.).

This separation avoids API conflicts while leveraging the strengths of both versions.

## Licensing

| Component | License |
|-----------|---------|
| Ramulator2 | MIT |
| CACTI | HP Labs |
| NVSim | Custom (see nvsim/README) |
| McPAT | HP Labs |
| Garnet | BSD (gem5) |
| ZSim | GPL-2.0 |

PIMID itself is GPL-2.0.
