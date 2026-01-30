# External Tools Integration

## Status Overview

| Tool | Purpose | Status |
|------|---------|--------|
| Ramulator2 | DRAM timing simulation | Integrated |
| CACTI | SRAM/cache modeling | Integrated (via McPAT) |
| NVSim | NVM modeling (STT-MRAM, PCM, ReRAM) | Integrated |
| McPAT | Power modeling | Integrated |
| GARNET | Network-on-chip simulation | Integrated |

## Build System

All external tools are built automatically via CMake:

```cmake
# Conditional compilation flags
-DHAVE_RAMULATOR  # DRAM timing
-DHAVE_CACTI      # SRAM/cache (via McPAT)
-DHAVE_NVSIM      # NVM modeling
-DHAVE_MCPAT      # Power modeling
```

## Tool Details

### Ramulator2 (DRAM)
- Cycle-accurate DDR4/DDR5, LPDDR4/5, HBM2/3, GDDR6 simulation
- Energy tracking for read/write operations
- Location: `external/ramulator/`

### CACTI (SRAM/Cache)
- Area, timing, and power for SRAM structures
- Bundled with McPAT for seamless integration
- Location: `external/mcpat/cacti/`

### NVSim (NVM)
- STT-MRAM, PCM, ReRAM characterization
- Area, latency, and energy modeling
- Location: `external/nvsim/`
- Note: Fixed C++17 compatibility (renamed `data` to `data_array`)

### McPAT (Power)
- System-wide power estimation
- CPU cores, caches, memory controllers, NoC
- Location: `external/mcpat/`

### GARNET (Network)
- Network-on-chip simulation
- Supports H-Tree, Mesh, Crossbar topologies
- Virtual channels and credit-based flow control
- Location: `network_models/`

## Verification

```bash
# Check integrated models
./pimid --version

# Expected output:
# Integrated External Models:
#   ✓ Ramulator2 (DRAM timing simulation)
#   ✓ CACTI (SRAM/cache modeling)
#   ✓ NVSim (NVM modeling)
#   ✓ McPAT (power modeling)
```

## Architecture

```
pimid/
├── memory_models/
│   ├── ramulator_wrapper.*   # DRAM interface
│   ├── cacti_wrapper.*       # SRAM interface
│   └── nvsim_wrapper.*       # NVM interface
├── power_models/
│   └── mcpat_wrapper.*       # Power interface
├── network_models/
│   └── garnet_detailed.*     # Network interface
└── external/
    ├── ramulator/            # Ramulator2 (submodule)
    ├── nvsim/                # NVSim
    └── mcpat/                # McPAT + CACTI
```
