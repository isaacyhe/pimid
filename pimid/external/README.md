# PIMID External Dependencies

This directory contains all external simulators and modeling tools integrated into PIMID.

## Included Simulators

### 1. Ramulator2 (DRAM Simulator)
- **Repository**: https://github.com/CMU-SAFARI/ramulator2
- **Purpose**: Cycle-accurate DRAM simulation
- **Technologies**: DDR4, DDR5, LPDDR4, LPDDR5, HBM2, HBM3, GDDR6
- **Location**: `ramulator/`
- **Documentation**: See `ramulator/README.md`

### 2. CACTI (SRAM/Cache Modeling)
- **Repository**: https://github.com/HewlettPackard/cacti
- **Purpose**: Cache and memory access time, area, and power modeling
- **Technologies**: SRAM, Cache hierarchies, DRAM
- **Location**: `cacti/`
- **Documentation**: See `cacti/README`

### 3. NVSim (Non-Volatile Memory Simulator)
- **Repository**: https://github.com/SEAL-UCSB/NVSim
- **Purpose**: Performance, energy, and area estimation for NVM
- **Technologies**: STT-RAM, PCM, ReRAM, FBDRAM, NAND Flash
- **Location**: `nvsim/`
- **Documentation**: See `nvsim/README`

### 4. McPAT (Power Modeling)
- **Repository**: https://github.com/HewlettPackard/mcpat
- **Purpose**: Integrated power, area, and timing modeling
- **Components**: Cores, caches, NoCs, memory controllers
- **Location**: `mcpat/`
- **Documentation**: See `mcpat/README`
- **Note**: Includes CACTI-P (enhanced CACTI for power)

### 5. ZSim (CPU Simulator)
- **Repository**: https://github.com/s5z/zsim
- **Purpose**: Fast and scalable x86-64 multicore simulator
- **Features**: Cycle-accurate, memory hierarchy modeling, fast-forward mode
- **Location**: `zsim/`
- **Documentation**: See `zsim/README.md`
- **Special**: **Upgraded to support Intel Pin 3.x** - See `zsim/PIN3_UPGRADE.md`

### 6. gem5 (System Simulator with GARNET)
- **Repository**: https://github.com/gem5/gem5
- **Purpose**: Full-system computer architecture simulator
- **PIMID Use**: GARNET 2.0 network-on-chip simulator
- **Location**: `gem5/` (GARNET at `gem5/src/mem/ruby/network/garnet/`)
- **Documentation**: See `gem5/README`

## Integration Status

| Simulator | Status | PIMID Integration | Notes |
|-----------|--------|-------------------|-------|
| Ramulator2 | ✅ Added | Ready | Latest version with modern DRAM standards |
| CACTI | ✅ Added | Ready | For SRAM modeling |
| NVSim | ✅ Added | Ready | For STT-MRAM and other NVM |
| McPAT | ✅ Added | Ready | System-wide power modeling |
| ZSim | ✅ Added + Upgraded | Ready | **Pin 3.x compatible** |
| gem5/GARNET | ✅ Added | Ready | Network-on-chip simulation |

## Build Instructions

### Prerequisites

Each simulator has its own build requirements. Common dependencies:
- GCC/G++ ≥ 7.0 or Clang ≥ 6.0
- Make or SCons
- Python 3.x (for some tools)
- Various libraries (see individual READMEs)

### ZSim Special Requirements

ZSim requires Intel Pin:
- **Pin 2.x**: Download Pin 2.14 from Intel (compatible with zsim out of box)
- **Pin 3.x**: Download Pin 3.28 or later (requires our compatibility patch - **already applied**)

Set the `PINPATH` environment variable:
```bash
export PINPATH=/path/to/intel-pin
```

See `zsim/PIN3_UPGRADE.md` for details on Pin 3.x support.

### Building Individual Simulators

#### Ramulator2
```bash
cd ramulator
mkdir build && cd build
cmake ..
make -j$(nproc)
```

#### CACTI
```bash
cd cacti
make -j$(nproc)
```

#### NVSim
```bash
cd nvsim
make -j$(nproc)
```

#### McPAT
```bash
cd mcpat
make -j$(nproc)
```

#### ZSim (with Pin 3.x)
```bash
cd zsim
export PINPATH=/path/to/pin-3.28  # Or pin-2.14
scons -j$(nproc)
```

#### gem5 (for GARNET)
```bash
cd gem5
# gem5 uses SCons
scons build/X86/gem5.opt -j$(nproc)
# Or for fast build:
scons build/X86/gem5.fast -j$(nproc)
```

## PIMID Integration Points

### Memory Models
- **DRAM**: `pimid/include/memory_models/dram_model.h` → `ramulator/`
- **SRAM**: `pimid/include/memory_models/sram_model.h` → `cacti/`
- **NVM**: `pimid/include/memory_models/nvm_model.h` → `nvsim/`

### CPU Simulation
- **Host Engine**: `pimid/include/host_engine/host_engine.h` → `zsim/`
- **Device Engine**: `pimid/include/device_engine/device_engine.h` → `zsim/`

### Network Modeling
- **Network**: `pimid/include/network/network_model.h` → `gem5/src/mem/ruby/network/garnet/`

### Power Modeling
- **Power**: `pimid/include/power/power_model.h` → `mcpat/`

## Version Information

All simulators are tracked as git submodules. To check versions:

```bash
cd /path/to/pimid-dev
git submodule status
```

To update to latest versions:

```bash
git submodule update --remote --recursive
```

## Licensing

Each external tool maintains its own license:
- **Ramulator2**: MIT License
- **CACTI**: HP Labs License
- **NVSim**: Custom License (see NVSim/README)
- **McPAT**: HP Labs License
- **ZSim**: GPL-2.0
- **gem5**: BSD License

PIMID itself is licensed under GPL-2.0.

## Citations

If you use PIMID with these simulators, please cite both PIMID and the respective tools:

### Ramulator2
```bibtex
@article{kim2015ramulator,
  title={Ramulator: A fast and extensible DRAM simulator},
  author={Kim, Yoongu and Yang, Weikun and Mutlu, Onur},
  journal={IEEE Computer Architecture Letters},
  year={2015}
}
```

### ZSim
```bibtex
@inproceedings{sanchez2013zsim,
  title={ZSim: Fast and accurate microarchitectural simulation of thousand-core systems},
  author={Sanchez, Daniel and Kozyrakis, Christos},
  booktitle={ISCA},
  year={2013}
}
```

### gem5/GARNET
```bibtex
@article{binkert2011gem5,
  title={The gem5 simulator},
  author={Binkert, Nathan and others},
  journal={ACM SIGARCH Computer Architecture News},
  year={2011}
}
```

## Support

For issues specific to:
- **PIMID integration**: See main PIMID repository
- **Individual simulators**: See their respective GitHub repositories

## Acknowledgments

PIMID gratefully acknowledges these excellent open-source projects that make comprehensive PIM simulation possible.
