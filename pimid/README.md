# PIMID: A Full-System Simulator with Intricacy and Diversity for Processing-in-Memory

[![License](https://img.shields.io/badge/License-GPL--2.0-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)

PIMID is a comprehensive full-system simulator for Processing-in-Memory (PIM) architectures that addresses critical gaps in current PIM simulation capabilities. Built on an extensible design with standardized interfaces, PIMID supports diverse memory technologies, flexible processing element placement, advanced networking, host-device co-simulation, and comprehensive power modeling.

## Key Features

### 🔧 Full-System Co-Simulation
- **Independent Simulation Engines**: Separate host and device simulation domains
- **Socket-Based Communication**: True host-device co-simulation with timing causality
- **ZSim Integration**: Leverages proven cycle-accurate CPU simulation

### 💾 Multi-Technology Memory Support
- **DRAM**: via Ramulator integration
- **SRAM**: via CACTI integration
- **STT-MRAM**: via NVSim integration
- Enables direct comparison across memory technologies under identical workloads

### 🎯 Fine-Grained PE Placement
- **Hierarchical Placement**: From subarrays to logic dies
  - Subarray level
  - Bank level
  - Chip level
  - Rank level
  - Logic die (for HBM/HMC)
- **Flexible Addressing**: Both unified and discrete addressing modes
- **Virtual Memory Support**: Page table walker (PTW) for realistic address translation

### 🌐 Advanced Network Modeling
- **GARNET Integration**: Cycle-accurate NoC simulation
- **Multiple Topologies**: Mesh, Torus, Dragonfly, Fat-tree, Crossbar
- **Flexible Routing**: XY, adaptive, minimal, Valiant routing
- **Flow Control**: Credit-based, virtual channels

### ⚡ Comprehensive Power Modeling
- **McPAT Integration**: Complete system power estimation
- **Component-Level**: Host processor, PEs, caches, memory, network
- **Energy Tracking**: Dynamic and leakage power for all components

### 🔌 Extensible Plugin Architecture
- **Standardized Interfaces**: Easy integration of new components
- **YAML-Based Configuration**: Flexible system specification
- **Modular Design**: Replace or extend any component

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        PIMID Architecture                        │
├───────────────────────────────┬─────────────────────────────────┤
│       Host Simulation         │      Device Simulation          │
│          Engine               │           Engine                │
│  ┌────────┐  ┌──────────┐   │   ┌──────┐  ┌──────────┐        │
│  │  Core  │  │  Core    │    │   │  PE  │  │   PE     │        │
│  │        │  │          │    │   │      │  │          │        │
│  └────────┘  └──────────┘   │   └──────┘  └──────────┘        │
│  ┌────────────────────────┐  │   ┌────────────────────────┐    │
│  │   Shared Cache         │  │   │  Address Translation   │    │
│  └────────────────────────┘  │   └────────────────────────┘    │
│  ┌────────────────────────┐  │   ┌────────────────────────┐    │
│  │  Memory Controller     │  │   │      Network           │    │
│  └────────────────────────┘  │   └────────────────────────┘    │
└───────────────────────────────┴─────────────────────────────────┘
                │ Socket Communication │
┌─────────────────────────────────────────────────────────────────┐
│                   Modeling Infrastructure                        │
├──────────┬──────────┬──────────┬──────────┬─────────────────────┤
│ DRAM     │  SRAM    │ STT-MRAM │  GARNET  │      McPAT          │
│(Ramulator)│ (CACTI)  │ (NVSim)  │ (Network)│      (Power)        │
└──────────┴──────────┴──────────┴──────────┴─────────────────────┘
```

## Installation

### Prerequisites

- **C++ Compiler**: GCC ≥ 7.0 or Clang ≥ 6.0 with C++17 support
- **CMake**: Version ≥ 3.15
- **Boost**: System and filesystem libraries
- **yaml-cpp**: For configuration parsing (optional but recommended)
- **Intel Pin**: For ZSim (Pin 3.28+ recommended for Ubuntu 24.04, Pin 2.14 for Ubuntu 18.04 - see external/zsim/PIN3_UPGRADE.md)

#### External Simulators (✅ Included as Git Submodules)
- **ZSim**: For CPU simulation (**Pin 3.x compatible - works on Ubuntu 24.04!**)
- **Ramulator2**: For DRAM modeling (DDR4/5, LPDDR4/5, HBM2/3, GDDR6)
- **CACTI**: For SRAM modeling
- **NVSim**: For NVM modeling (STT-RAM, PCM, ReRAM)
- **GARNET**: For network modeling (from gem5)
- **McPAT**: For power modeling

See [external/README.md](external/README.md) for detailed information on each simulator.

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/pimid.git
cd pimid

# Initialize and update submodules (required for external simulators)
git submodule update --init --recursive

# Build external dependencies (optional, can be done separately)
# See external/README.md for details

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
make -j$(nproc)

# Install (optional)
sudo make install

# Run tests
make test
```

**Note**: The external simulators are included as git submodules. Make sure to run `git submodule update --init --recursive` after cloning.

### Build Options

```bash
# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release build with optimizations
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build with specific compiler
cmake -DCMAKE_CXX_COMPILER=g++-9 ..
```

## Quick Start

### 1. Configure Your Simulation

Edit the main configuration file:

```yaml
# configs/examples/pimid_config.yaml
simulation:
  mode: "co-simulation"
  host_config: "configs/host/host_config.yaml"
  device_config: "configs/device/device_config.yaml"
  memory_config: "configs/memory/dram_config.yaml"

memory:
  technology: "DRAM"
  addressing_mode: "unified"

pe_placement:
  level: "RANK"
  num_pes_per_level: 4

features:
  enable_power_modeling: true
  enable_network_modeling: true
```

### 2. Run a Simulation

#### Co-Simulation Mode (Host + Device)

```bash
# Terminal 1: Start device simulator
./build/pimid_device configs/examples/pimid_config.yaml

# Terminal 2: Start host simulator and run workload
./build/pimid_host configs/examples/pimid_config.yaml ./workload/benchmark
```

#### Standalone Mode (Debug)

```bash
# Run in single-process mode
./build/pimid_standalone configs/examples/pimid_config.yaml ./workload/benchmark
```

### 3. Analyze Results

```bash
# View statistics
cat results/stats.txt

# View power breakdown
cat results/power_stats.txt

# View network statistics
cat results/network_stats.txt
```

## Usage Examples

### Example 1: Compare Memory Technologies

```bash
# Run with DRAM
./pimid_standalone -c configs/examples/dram_config.yaml ./benchmark

# Run with SRAM
./pimid_standalone -c configs/examples/sram_config.yaml ./benchmark

# Run with STT-MRAM
./pimid_standalone -c configs/examples/mram_config.yaml ./benchmark
```

### Example 2: Evaluate PE Placement Strategies

```bash
# PEs at subarray level
./pimid_standalone --pe-level=SUBARRAY ./benchmark

# PEs at bank level
./pimid_standalone --pe-level=BANK ./benchmark

# PEs at rank level
./pimid_standalone --pe-level=RANK ./benchmark

# PEs on logic die
./pimid_standalone --pe-level=LOGIC_DIE ./benchmark
```

### Example 3: Network Topology Exploration

```bash
# 2D Mesh topology
./pimid_standalone --network-topology=MESH_2D ./benchmark

# Crossbar topology
./pimid_standalone --network-topology=CROSSBAR ./benchmark

# Dragonfly topology
./pimid_standalone --network-topology=DRAGONFLY ./benchmark
```

## Programming Model

PIMID supports PIM offloading through code annotations:

```cpp
#include "pimid/pimid_hooks.h"

int main() {
    // Mark region of interest
    pimid_roi_begin();

    // Offload parallel region to PIM
    pimid_offload_begin();
    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        // Computation on PIM
        data[i] = compute(data[i]);
    }
    pimid_offload_end();

    pimid_roi_end();
    return 0;
}
```

## Configuration

PIMID uses YAML-based configuration for maximum flexibility:

### Host Configuration
- Core count and type (in-order/out-of-order)
- Cache hierarchy (L1/L2/L3)
- Memory controller
- Address translation (TLB, page walk)

### Device Configuration
- PE count and placement
- Memory hierarchy structure
- Scheduling policy
- Local caches for PEs

### Memory Configuration
- Technology selection (DRAM/SRAM/STT-MRAM)
- Timing parameters
- Power parameters
- Capacity and bandwidth

### Network Configuration
- Topology (mesh, torus, etc.)
- Routing algorithm
- Flow control
- Link parameters

### Power Configuration
- Technology node
- Frequency and voltage
- Component configurations
- Activity factors

See `configs/examples/` for complete examples.

## API Reference

### C++ API

```cpp
#include "pimid/pimid.h"

// Create simulator instance
pimid::PIMIDSimulator sim;

// Load configuration
sim.loadConfiguration("config.yaml");

// Initialize
sim.initialize();

// Run simulation
sim.run(1000000);  // Run for 1M cycles

// Get statistics
auto stats = sim.getStats();
sim.printStats();
```

### Python API (Coming Soon)

```python
import pimid

# Create and configure simulator
sim = pimid.Simulator("config.yaml")
sim.initialize()

# Run simulation
stats = sim.run(cycles=1000000)
print(stats)
```

## Extending PIMID

### Adding a New Memory Model

1. Create header in `include/memory_models/`
2. Inherit from `MemoryModel` base class
3. Implement required interfaces
4. Register in `MemoryModelFactory`
5. Add configuration parser

Example:

```cpp
class MyMemoryModel : public MemoryModel {
public:
    MyMemoryModel(const std::string& config);
    Cycle access(const MemoryRequest& req) override;
    // ... implement other interfaces
};
```

### Adding a New Network Topology

1. Extend `NetworkModel` or `GarnetModel`
2. Implement topology-specific routing
3. Update configuration parser
4. Add to network factory

### Adding a New Scheduler

1. Inherit from `PEScheduler`
2. Implement scheduling policy
3. Register in `SchedulerFactory`

## Performance Tips

- Use Release build for production simulations
- Enable only necessary features (power/network modeling)
- Use appropriate granularity for PE placement
- Tune configuration for your workload
- Use standalone mode for debugging, co-simulation for accuracy

## Troubleshooting

### Common Issues

**Issue**: Socket connection timeout in co-simulation
```bash
# Solution: Increase timeout or check firewall
communication:
  timeout_ms: 30000  # Increase timeout
```

**Issue**: Memory model initialization fails
```bash
# Solution: Check configuration file paths
# Ensure all external tools are properly installed
```

**Issue**: Compilation errors
```bash
# Solution: Verify C++17 support
g++ --version  # Should be ≥ 7.0
cmake --version  # Should be ≥ 3.15
```

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Setup

```bash
# Install development dependencies
pip install pre-commit clang-format

# Setup pre-commit hooks
pre-commit install

# Run tests before committing
make test
```

## Citation

If you use PIMID in your research, please cite:

```bibtex
@article{pimid2025,
  title={PIMID: A Full-System Simulator with Intricacy and Diversity for Processing-in-Memory},
  author={He, Yuan and Kondo, Masaaki and Shipman, Galen M. and Dominguez-Trujillo, Jered B. and Asifuzzaman, Kazi},
  journal={IEEE Computer Architecture Letters},
  year={2025}
}
```

## License

PIMID is released under the GPL-2.0 License. See [LICENSE](LICENSE) for details.

## Acknowledgments

PIMID builds upon excellent prior work:
- [ZSim](https://github.com/s5z/zsim) for CPU simulation
- [Ramulator](https://github.com/CMU-SAFARI/ramulator) for DRAM modeling
- [CACTI](https://www.hpl.hp.com/research/cacti/) for SRAM modeling
- [NVSim](https://github.com/SEAL-UCSB/NVSim) for NVM modeling
- [GARNET](https://www.gem5.org/) for network simulation
- [McPAT](https://www.hpl.hp.com/research/mcpat/) for power modeling
- [MultiPIM](https://github.com/Systems-ShiftLab/MultiPIM) for foundational PIM simulation concepts

## Contact

- Yuan He - isaacyhe@acm.org
- Project Link: https://github.com/yourusername/pimid

## Roadmap

- [x] Core simulation engine
- [x] Multi-technology memory support
- [x] Network modeling
- [x] Power modeling
- [ ] Python API
- [ ] FPGA validation platform
- [ ] GUI for configuration and visualization
- [ ] Extended PE models (vector units, accelerators)
- [ ] Support for additional memory technologies
- [ ] Performance optimizations
- [ ] Integration with gem5
