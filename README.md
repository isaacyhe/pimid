# PIMID: Processing-In-Memory Infrastructure for Design-space Exploration

[![License](https://img.shields.io/badge/License-GPL--2.0-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)

<p align="center">
  <img src="assets/qr-code.png" alt="PIMID Repository QR Code" width="150" />
  <br/>
  <em>Scan to access the PIMID repository</em>
</p>

PIMID is a comprehensive simulator for Processing-in-Memory (PIM) architectures, enabling design-space exploration across memory technologies, PE placements, and network topologies.

## Features

- **Multi-Technology Memory**: DRAM (Ramulator2), SRAM (CACTI), STT-MRAM/PCM/ReRAM (NVSim)
- **Flexible PE Placement**: Subarray, Bank, Bank-Group, Chip, Rank, Logic Die
- **Network Modeling**: GARNET-based NoC with H-Tree, Mesh, Crossbar topologies
- **Power Analysis**: McPAT integration for comprehensive energy estimation
- **Unified Binary**: Single `pimid` executable for all simulation modes

## Quick Start

```bash
# Build
cd pimid
mkdir build && cd build
cmake .. && make -j$(nproc)

# Run simulation
./pimid --config ../configs/test_complex_sttmram_16banks_l1cache.yaml

# Show version and integrated models
./pimid --version
```

## Simulation Modes

```bash
# Config-driven PIM simulation (default)
./pimid --mode sim --config <config.yaml>

# Host/Device co-simulation
./pimid --mode cosim --size 1000000

# Workload execution
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

## External Models

| Model | Purpose | Status |
|-------|---------|--------|
| Ramulator2 | DRAM timing (DDR4/5, HBM2/3) | Integrated |
| CACTI | SRAM/cache modeling | Integrated |
| NVSim | NVM modeling (STT-MRAM, PCM, ReRAM) | Integrated |
| McPAT | Power modeling | Integrated |
| GARNET | Network-on-chip | Integrated |

## Build Requirements

- C++17 compiler (GCC 7+ or Clang 6+)
- CMake 3.15+
- yaml-cpp
- Boost (system, filesystem)
- **Intel PIN 3.x or 4.x** (required for ZSim-based workload execution)

### Intel PIN Setup

```bash
# Download PIN 4.1 (recommended) or PIN 3.28
# PIN 4.x
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-external-4.1-99687-gd9b8f822c-gcc-linux.tar.gz
tar xzf pin-external-4.1-99687-gd9b8f822c-gcc-linux.tar.gz
export PINPATH=$PWD/pin-external-4.1-99687-gd9b8f822c-gcc-linux

# Or PIN 3.28 (legacy)
# wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.28-98749-g6643ecee5-gcc-linux.tar.gz
```

Note: PIN is only required for instruction-level workload tracing (`--mode standalone`). Config-driven simulation (`--mode sim`) and co-simulation (`--mode cosim`) work without PIN.

## Project Structure

```
pimid/
├── src/                    # Core simulator source
├── memory_models/          # Memory technology models
├── network_models/         # Network topology models
├── power_models/           # Power estimation
├── configs/                # Example configurations
└── external/               # External simulators
    ├── ramulator/          # Ramulator2 (DRAM)
    ├── nvsim/              # NVSim (NVM)
    └── mcpat/              # McPAT + CACTI
```

## License

GPL-2.0. See [LICENSE](LICENSE).
