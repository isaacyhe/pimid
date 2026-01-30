# PIMID: Processing-In-Memory Infrastructure for Design-space Exploration

[![License](https://img.shields.io/badge/License-GPL--2.0-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)

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
mkdir build && cd build
cmake .. && make -j$(nproc)

# Run simulation with config file
./pimid --config configs/test_complex_sttmram_16banks_l1cache.yaml

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

## Project Structure

```
pimid/
├── src/                    # Core simulator source
├── memory_models/          # Memory technology models
├── network_models/         # Network topology models
├── power_models/           # Power estimation
├── configs/                # Example configurations
└── external/               # External simulators (submodules)
```

## Citation

```bibtex
@article{pimid2025,
  title={PIMID: Processing-In-Memory Infrastructure for Design-space Exploration},
  author={He, Yuan and Kondo, Masaaki and Shipman, Galen M. and others},
  year={2025}
}
```

## License

GPL-2.0. See [LICENSE](LICENSE).

## Acknowledgments

Built upon: [Ramulator2](https://github.com/CMU-SAFARI/ramulator2), [CACTI](https://github.com/HewlettPackard/cacti), [NVSim](https://github.com/SEAL-UCSB/NVSim), [McPAT](https://github.com/HewlettPackard/mcpat), [GARNET/gem5](https://www.gem5.org/).
