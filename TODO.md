# PIMID Development Progress & TODO

Last updated: 2026-02-01

## Completed Work

### Phase 1: Core Architecture (Complete)
- [x] Project structure and CMake build system
- [x] Header files for all core components (17 files)
- [x] YAML-based configuration system
- [x] Memory model abstractions (DRAM, SRAM, NVM)
- [x] Network model abstractions
- [x] Power model abstractions
- [x] PE placement hierarchy (subarray → bank → chip → rank → logic die)
- [x] Task scheduler framework
- [x] Documentation (README, GETTING_STARTED, ARCHITECTURE)

### Phase 2: External Tool Integration (Complete)
- [x] ZSim integration (Pin-based binary instrumentation)
- [x] Ramulator2 integration (DRAM timing simulation)
- [x] CACTI 7.0 integration (SRAM/cache modeling)
- [x] NVSim integration (STT-MRAM, PCM, ReRAM modeling)
- [x] McPAT integration (power modeling wrapper)
- [x] Garnet extraction from gem5 (NoC simulation)

### Phase 3: ZSim Modernization (Complete)
- [x] Update ZSim for GCC 13 compatibility
- [x] Update ZSim for Pin 3.28 runtime compatibility
- [x] Add Pin 4.x native mode support with POSIX shared memory
- [x] Remove Pin 2.x/3.x support, integrate Pin 4.1 only
- [x] Fix SConstruct build issues (ROOT variable order)

### Phase 4: Unified Simulator (Complete)
- [x] Create standalone_main_unified.cpp with all simulation methods
- [x] Implement analytical simulation mode
- [x] Implement ZSim-based cycle-accurate mode
- [x] Implement host/device co-simulation mode
- [x] YAML config parsing for all parameters
- [x] CLI interface with --method, --scope, --config, --workload options

### Phase 5: Garnet Network Integration into ZSim (Complete)
- [x] Create GarnetNetwork class inheriting from Network base
- [x] Add mesh topology support with Manhattan distance routing
- [x] Implement analytical latency calculation (router + link latencies)
- [x] Add node registration and auto-registration from cache names
- [x] Update init.cpp to parse networkType="garnet" config
- [x] Store GarnetNetwork pointer in zinfo (GlobSimInfo)
- [x] Make ZSim config generation use YAML parameters (not hardcoded)
  - frequency_mhz, cache_line_size
  - l1d_size_kb, l1d_ways, l1i_size_kb, l1i_ways
  - l2_size_kb, l2_ways, enable_l2
  - noc_topology, noc_router_latency, noc_link_latency
  - phase_length, max_instructions, stats_interval

### Phase 6: Garnet Stats to McPAT Integration (Complete)
- [x] Create GarnetStats struct with all McPAT-required fields:
  - total_packets, total_flits, total_hops
  - buffer_reads, buffer_writes
  - crossbar_traversals, arbiter_events
  - link_traversals
  - total_cycles, total_latency
  - num_routers, num_rows, num_cols
  - flit_size_bits, clock_mhz
- [x] Update GarnetNetwork to track all router/link activity per access
- [x] Add writeStatsFile() to output garnet_stats.txt
- [x] Add garnet_network.h include to zsim.cpp
- [x] Add Garnet stats output in SimEnd() function
- [x] Create GarnetParsedStats struct in standalone_main_unified.cpp
- [x] Implement parseGarnetStats() to read garnet_stats.txt
- [x] Implement computeNoCPower() with McPATWrapper integration
- [x] Display NoC power breakdown (dynamic, leakage, total)
- [x] Calculate and display NoC energy

### Phase 7: Subarray Characterization (Complete)
- [x] Add subarray-level characteristic extraction to CACTI wrapper
- [x] Add subarray-level characteristic extraction to NVSim wrapper
- [x] Add subarray-level characteristic extraction to Ramulator wrapper

---

## Pending Work

### HIGH PRIORITY: Pin Compatibility Fix
- [ ] **Build ZSim with PinCRT to fix glibc 2.39 compatibility**
  - Issue: Pin 4.1's internal loader doesn't support `R_X86_64_IRELATIVE` (relocation type 37)
  - Ubuntu 24.04 uses glibc 2.39 which uses this relocation type
  - Solution: Rebuild ZSim using Pin's PinCRT (Pin C Runtime) instead of system glibc
  - Files to modify:
    - `external/zsim/SConstruct` - add PinCRT compilation flags
    - May need to modify source files to use PinCRT-compatible APIs
  - Reference: Pin User Manual section on PinCRT
  - Alternative: Use Docker with Ubuntu 22.04 for testing

### Network Improvements
- [ ] Add cycle-accurate Garnet simulation mode (currently analytical only)
- [ ] Integrate actual Garnet router models for contention simulation
- [ ] Add support for different routing algorithms (currently XY only)
- [ ] Add virtual channel support

### Memory Model Improvements
- [ ] Connect Ramulator2 for cycle-accurate DRAM timing
- [ ] Add DDR5/HBM3 memory configurations
- [ ] Implement memory controller queuing models

### Power Model Improvements
- [ ] Full McPAT system power integration (not just NoC)
- [ ] Add thermal modeling
- [ ] Per-component power breakdown in stats output

### Testing & Validation
- [ ] Create comprehensive test suite for Garnet-McPAT integration
- [ ] Add regression tests for ZSim+Garnet
- [ ] Validate power numbers against published data
- [ ] Performance benchmarking suite

### Documentation
- [ ] Update README with Garnet integration details
- [ ] Add power modeling usage guide
- [ ] Document YAML configuration options

---

## Known Issues

1. **Pin 4.1 + glibc 2.39 incompatibility** (Ubuntu 24.04)
   - Error: `unsupported relocation type 37`
   - Workaround: Use Docker with Ubuntu 22.04
   - Fix: Build with PinCRT (TODO above)

2. **ZSim stats location**
   - garnet_stats.txt is written to zinfo->outputDir
   - PIMID looks in current directory and ./zsim_out/
   - May need to sync output directory handling

---

## File Change Summary (Recent Session)

### Modified in ZSim (`external/zsim/`)
- `src/garnet_network.h` - Added GarnetStats struct, McPAT-compatible stats tracking
- `src/zsim.h` - Added garnetNetwork pointer to GlobSimInfo
- `src/zsim.cpp` - Added include and SimEnd() stats output
- `src/init.cpp` - Store GarnetNetwork in zinfo
- `src/network.h` - Made Network base class with virtual getRTT()
- `SConstruct` - Fixed ROOT variable definition order

### Modified in PIMID (`src/`)
- `standalone_main_unified.cpp`:
  - Added mcpat_wrapper.h include
  - Added GarnetParsedStats struct
  - Added parseGarnetStats() function
  - Added computeNoCPower() function with full McPAT integration
  - Extended UnifiedConfig with all ZSim parameters
  - Updated generateZSimConfig() to use config parameters

---

## Architecture Notes

### Data Flow: ZSim Garnet → McPAT
```
ZSim Simulation (GarnetNetwork)
    │ tracks per-access stats
    ▼
SimEnd() → garnet_stats.txt
    │
    ▼
PIMID parseGarnetStats()
    │ parses key=value format
    ▼
GarnetParsedStats struct
    │
    ▼
computeNoCPower()
    │ creates McPATWrapper::SystemConfig
    │ creates McPATWrapper::NoCActivityStats
    ▼
McPATWrapper.setNoCActivity()
McPATWrapper.computePower()
    │
    ▼
Power Results (dynamic, leakage, energy)
```

### Key Structs Alignment
- `GarnetStats` (garnet_network.h) ↔ `GarnetParsedStats` (standalone_main_unified.cpp) ↔ `NoCActivityStats` (mcpat_wrapper.h)
- All three have matching fields for seamless data transfer

---

## Commands Reference

```bash
# Build ZSim
cd external/zsim && scons -j4

# Build PIMID
cd build && make pimid -j4

# Run analytical mode
./pimid --method analytical --config ../configs/garnet_sttmram_4x4_16banks.yaml

# Run ZSim mode (requires Pin compatibility fix or Docker)
./pimid --method zsim --config ../configs/garnet_sttmram_4x4_16banks.yaml --workload ../workloads/simple_test

# Test with manual garnet_stats.txt
# Create garnet_stats.txt in build/ directory, then run zsim mode
```
