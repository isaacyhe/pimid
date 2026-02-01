# PIMID: Processing-In-Memory Infrastructure for Design-space Exploration

[![License](https://img.shields.io/badge/License-GPL--2.0-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)

PIMID is a comprehensive simulator for Processing-in-Memory (PIM) architectures, enabling design-space exploration across memory technologies, PE placements, and network topologies.

## Features

- **Multi-Technology Memory**: DRAM (Ramulator2), SRAM (CACTI 7.0), STT-MRAM/PCM/ReRAM (NVSim)
- **Flexible PE Placement**: Subarray, Bank, Bank-Group, Chip, Rank, Logic Die
- **Network Modeling**: Garnet-based NoC with H-Tree, Mesh, Crossbar topologies
- **Power Analysis**: McPAT integration for comprehensive energy estimation
- **Unified Binary**: Single `pimid` executable for all simulation modes

## Quick Start

```bash
# Build
mkdir build && cd build
cmake .. && make -j$(nproc)

# Run simulation with config file
./pimid --config ../configs/test_complex_sttmram_16banks_l1cache.yaml

# Show help
./pimid --help
```

## Simulation Modes

```bash
# Config-driven PIM simulation (default)
./pimid --mode sim --config <config.yaml>

# Host/Device co-simulation
./pimid --mode cosim --size 1000000

# Workload execution (requires PIN)
./pimid --mode standalone --workload ./benchmark
```

## Example Configuration

```yaml
# 16-Bank STT-MRAM with In-Order Cores and L1 Cache
memory:
  technology: "STT_MRAM"
  organization:
    num_banks: 16
    num_subarrays_per_bank: 8

processing_element:
  type: "in_order_core"
  num_pes: 16
  l1_cache:
    enable: true
    l1d:
      size_KB: 32
      associativity: 8

network:
  topology: "H_TREE"
  model: "GARNET"
```

## Sample Output

```
ARCHITECTURE SUMMARY
  Memory:      STT_MRAM (16 banks)
  Processing:  in_order_core (16 PEs)
  Network:     H_TREE
  Cache:       L1D: 32KB, L1I: 16KB per PE

PERFORMANCE SUMMARY
  Throughput:       32.00 GOPS
  Eff. Bandwidth:   0.13 GB/s
  Speedup vs CPU:   1.56x
  Bottleneck:       Compute
```

## Project Structure (Self-Contained)

```
pimid/
├── src/                    # Core simulator source
│   ├── memory/             # Memory model implementations
│   ├── network/            # Network model implementations
│   ├── power/              # Power model implementations
│   └── ...                 # Other core components
├── include/                # Headers
│   ├── memory/             # Memory model headers
│   ├── network/            # Network model headers
│   └── power/              # Power model headers
├── configs/                # Example configurations (1000+ YAML files)
├── external/               # External simulators (all included)
│   ├── ramulator/          # Ramulator2 (DRAM timing)
│   ├── cacti/              # CACTI 7.0 (SRAM/cache characterization)
│   ├── nvsim/              # NVSim (NVM modeling, namespaced)
│   ├── mcpat/              # McPAT + CACTI 6.5-P (power modeling)
│   ├── garnet/             # Garnet NoC (extracted from gem5)
│   ├── zsim/               # ZSim (workload execution)
│   └── pin/                # Intel PIN symlink (see below)
├── ext/                    # Header-only libraries
│   ├── yaml-cpp/           # YAML parsing
│   ├── spdlog/             # Logging
│   └── argparse/           # Argument parsing
└── memory/                 # DRAM architecture specifications
```

## External Models

| Model | Purpose | Version | Notes |
|-------|---------|---------|-------|
| Ramulator2 | DRAM timing (DDR4/5, HBM2/3) | Latest | Included |
| CACTI | SRAM/subarray characterization | 7.0 | Included |
| NVSim | NVM modeling (STT-MRAM, PCM, ReRAM) | Namespaced | Included |
| McPAT | Power modeling | 1.3 | Bundled with CACTI 6.5-P |
| Garnet | Network-on-chip simulation | Extracted | Standalone (no gem5 dependency) |
| ZSim | Workload execution | Modified | Included |

## Build Requirements

- C++17 compiler (GCC 9+ recommended)
- CMake 3.15+
- pthread

### Optional Dependencies

- **Intel PIN 4.1**: Required only for `--mode standalone` (workload execution)
- **Boost**: Enhanced filesystem operations (auto-detected)

### Intel PIN Setup (Optional)

PIN is only needed for instruction-level workload tracing. Config-driven simulation works without it.

```bash
# Option 1: Set environment variable
export PINPATH=/path/to/pin

# Option 2: Create symlink in external/
cd pimid/external
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-external-4.1-99687-gd9b8f822c-gcc-linux.tar.gz
tar xzf pin-external-4.1-99687-gd9b8f822c-gcc-linux.tar.gz
ln -s pin-external-4.1-99687-gd9b8f822c-gcc-linux pin
```

## Environment Variables

| Variable | Purpose | Default |
|----------|---------|---------|
| `PIMID_ROOT` | PIMID installation directory | Auto-detected from executable |
| `PINPATH` | Intel PIN root directory | `external/pin` |
| `ZSIM_PATH` | ZSim installation directory | `external/zsim` |

## License

GPL-2.0. See [LICENSE](LICENSE).

## Acknowledgments

Built upon: [Ramulator2](https://github.com/CMU-SAFARI/ramulator2), [CACTI](https://github.com/HewlettPackard/cacti), [NVSim](https://github.com/SEAL-UCSB/NVSim), [McPAT](https://github.com/HewlettPackard/mcpat), [gem5/Garnet](https://www.gem5.org/), ZSim.
