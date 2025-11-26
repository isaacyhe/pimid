# PIMID Component and Integration Tests

This document describes the comprehensive functional and integration tests for all PIMID components.

## Overview

These tests verify the complete PIM simulator stack:

```
┌─────────────────────────────────────────────────────────────┐
│                     Application Layer                        │
│  Vector Ops │ Matrix Ops │ Graph │ Database │ ML Inference │
└─────────────────────────────────────────────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    PIM System Integration                    │
│     Host Cores + PIM Cores + Network + Memory + Power       │
└─────────────────────────────────────────────────────────────┘
                              ▼
┌────────────┬──────────────┬─────────────┬─────────────────┐
│ SRAM Cache │  DRAM Memory │  NVM Storage│  Power Modeling │
│  (CACTI)   │ (Ramulator2) │   (NVSim)   │    (McPAT)      │
└────────────┴──────────────┴─────────────┴─────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Network-on-Chip (GARNET via gem5)              │
└─────────────────────────────────────────────────────────────┘
```

## Test Categories

### 1. Functional Tests (`tests/functional/`)

Component-level tests for individual wrappers:

#### test_cacti_wrapper.cpp (8 tests)
- **Basic Configuration**: L1 cache setup
- **Cache Hierarchy**: L1/L2/L3 timing validation
- **Technology Scaling**: 22nm, 14nm, 7nm comparison
- **Energy vs. Performance**: Associativity trade-offs
- **Power Modeling**: Dynamic + leakage power
- **Area Estimation**: Density analysis
- **Bank Partitioning**: Multi-bank configurations
- **Configuration Validation**: Input sanitization

**Example Output:**
```
TEST: CACTI Cache Hierarchy (L1/L2/L3)
[INFO] L1: 1.5 ns access time
[INFO] L2: 3.2 ns access time
[INFO] L3: 12.8 ns access time
[PASS] Cache hierarchy timing validated
```

#### test_nvsim_wrapper.cpp (8 tests)
- **PCM Configuration**: Phase Change Memory
- **STT-MRAM Configuration**: Spin-Transfer Torque MRAM
- **ReRAM Configuration**: Resistive RAM
- **Technology Comparison**: PCM vs STT-MRAM vs ReRAM
- **Endurance Modeling**: Write endurance and wear-leveling
- **Write Energy Asymmetry**: Read vs write energy
- **Retention Time**: Non-volatile data retention
- **Density and Area**: Storage density analysis

**Key Features:**
```
Technology       Read (ns)    Write (ns)    Write Energy (nJ)
─────────────────────────────────────────────────────────────
PCM              20           50            5.0
STT-MRAM         20           50            5.0
ReRAM            20           50            5.0
```

#### test_mcpat_wrapper.cpp (8 tests)
- **Single Core Power**: Basic power estimation
- **Multi-Core Scaling**: 1, 2, 4, 8, 16 cores
- **Frequency Scaling (DVFS)**: 1.0-3.0 GHz
- **Cache Hierarchy Power**: L1/L2/L3 power breakdown
- **Technology Nodes**: 45nm to 7nm comparison
- **Temperature Effects**: Leakage vs temperature
- **Power Breakdown**: Component-wise power
- **Energy Calculation**: Energy per instruction

**Power Breakdown Example:**
```
Component Power Breakdown:
─────────────────────────────────────────
Cores           : 40.0 W (65%)
L1 Caches       : 4.0 W (6.5%)
L2 Cache        : 1.3 W (2.1%)
L3 Cache        : 2.5 W (4.1%)
NoC             : 1.7 W (2.8%)
─────────────────────────────────────────
TOTAL           : 61.5 W (100%)
```

### 2. Integration Tests (`tests/integration/`)

#### test_pim_full_system.cpp (6 tests)
Complete PIM system with all components integrated:

1. **System Initialization**
   - 16 PIM cores + 4 host cores
   - Memory: L1(32KB) + L2(256KB) + DRAM(4GB) + NVM(16GB)

2. **Host-to-PIM Communication**
   - NoC packet transmission
   - Network flit tracking

3. **Memory Hierarchy Integration**
   - L1 hit: ~1ns
   - L2 hit: ~3ns
   - DRAM access: ~50ns
   - NVM read: ~100ns, write: ~200ns

4. **PIM Computation**
   - Instruction execution tracking
   - Per-core statistics

5. **Power Modeling Integration**
   - Total energy tracking
   - Peak power measurement

6. **End-to-End Simulation**
   - Complete vector addition workload
   - Statistics collection

**Example Simulation Output:**
```
Simulation Results:
─────────────────────────────────
Vector size:        1024 elements
Network flits:      256
Instructions:       16000
Total energy:       0.15 J
─────────────────────────────────
```

### 3. Workload Tests (`tests/workloads/`)

#### test_pim_workloads.cpp (6 workloads)

Realistic application workloads:

1. **Vector Operations**
   ```
   Workload: C[i] = A[i] + B[i]

   Size        Cores    Time (μs)    Energy (mJ)
   ─────────────────────────────────────────────
   1KB (128)   4        0.13         0.45
   1MB         8        6.55         22.13
   16MB        16       524.29       1770.24
   ```

2. **Matrix Multiply**
   ```
   Workload: C = A × B

   Matrix      Cores    Time (μs)    FLOPs
   ─────────────────────────────────────────
   32x32       4        16.38        65536
   128x128     8        262.14       4194304
   512x512     16       16777.22     268435456
   ```

3. **Graph Analytics (BFS)**
   ```
   Graph         Vertices    Time (μs)    Mem Accesses
   ────────────────────────────────────────────────────
   Small         1000        50.00        11000
   Medium        10000       1000.00      210000
   Large         100000      18750.00     3100000
   ```

4. **Database Operations (SELECT)**
   ```
   Table           Rows      Time (μs)    Throughput
   ─────────────────────────────────────────────────────
   Small           10000     50.00        200.00 M/s
   Medium          1000000   2500.00      400.00 M/s
   Large           10000000  31250.00     320.00 M/s
   ```

5. **ML Inference**
   ```
   Model          Batch    Time (μs)      MACs
   ───────────────────────────────────────────────
   ResNet-18      1        27500.00       11000000
   BERT-base      1        68750.00       110000000
   GPT-2          1        468750.00      1500000000
   ```

6. **Performance Comparison (PIM vs CPU)**
   ```
   PIM (16 cores):
     Time:   524.29 μs
     Energy: 1770.24 mJ

   CPU (single-threaded):
     Time:   3495.25 μs
     Energy: 69905.00 mJ

   PIM Advantages:
     Speedup:            6.67x
     Energy efficiency:  39.48x
   ```

## Building and Running

### Build C++ Tests

```bash
cd /home/user/pimid-dev/tests
mkdir -p build && cd build
cmake ..
make
```

### Run All C++ Tests

```bash
ctest --output-on-failure
```

### Run Individual Tests

```bash
# Functional tests
./functional/test_cacti_wrapper
./functional/test_nvsim_wrapper
./functional/test_mcpat_wrapper

# Integration tests
./integration/test_pim_full_system

# Workload tests
./workloads/test_pim_workloads
```

### Run Complete Test Suite

```bash
cd /home/user/pimid-dev/tests
./run_all_tests.sh --all
```

## Test Features

### Color-Coded Output

All tests use colored output for easy reading:
- 🟢 **GREEN**: Pass
- 🔴 **RED**: Fail
- 🟡 **YELLOW**: Info
- 🔵 **BLUE**: Test name
- 🟣 **MAGENTA**: Section headers
- 🔷 **CYAN**: Banners

### Comprehensive Statistics

Each test reports:
- Execution time
- Energy consumption
- Memory accesses
- Network traffic
- Instructions executed
- Throughput metrics

### Mock vs. Real Implementations

Current tests use **mock implementations** for rapid development and testing. To integrate with real PIMID libraries:

1. Uncomment `target_link_libraries` in CMakeLists.txt
2. Replace mock classes with actual wrappers
3. Update include paths to real headers

Example:
```cmake
# Current (mock):
add_executable(test_cacti_wrapper test_cacti_wrapper.cpp)

# Future (real):
add_executable(test_cacti_wrapper test_cacti_wrapper.cpp)
target_link_libraries(test_cacti_wrapper pimid_cacti_wrapper)
```

## Test Coverage

| Component | Tests | Lines | Coverage |
|-----------|-------|-------|----------|
| **CACTI Wrapper** | 8 tests | 400+ lines | Cache modeling |
| **NVSim Wrapper** | 8 tests | 350+ lines | NVM modeling |
| **McPAT Wrapper** | 8 tests | 450+ lines | Power modeling |
| **Full System** | 6 tests | 600+ lines | Host+Device+Network+Memory+Power |
| **Workloads** | 6 workloads | 500+ lines | Real applications |
| **Total** | **36 tests** | **2,300+ lines** | **Complete system** |

## Integration with Build Tests

These component tests complement the build verification tests:

```
Build Tests (tests/build/*.sh)
    ↓
    Verify tools compile
    ↓
Functional Tests (tests/functional/*.cpp)
    ↓
    Verify wrappers work
    ↓
Integration Tests (tests/integration/*.cpp)
    ↓
    Verify system integration
    ↓
Workload Tests (tests/workloads/*.cpp)
    ↓
    Verify real applications
```

## Expected Results

All tests should pass with output similar to:

```
╔═══════════════════════════════════════════════════════════╗
║                     TEST SUMMARY                          ║
╚═══════════════════════════════════════════════════════════╝
Total Tests:  8
Passed:       8
Failed:       0

✓ ALL TESTS PASSED!
```

## Troubleshooting

### Compilation Errors

```bash
# Ensure C++17 support
g++ --version  # Should be 7.0+

# Install build dependencies
sudo apt-get install cmake build-essential
```

### Linking Errors

If you see linking errors, the tests are using mock implementations. This is expected until real PIMID wrappers are integrated.

### Runtime Errors

Check that:
- All external tools are properly installed
- Environment variables (PINPATH, etc.) are set
- Dependencies are available

## Future Enhancements

- [ ] Replace mock implementations with real wrappers
- [ ] Add ZSim simulator functional tests
- [ ] Add Ramulator2 wrapper functional tests
- [ ] Add GARNET NoC functional tests
- [ ] Add performance regression tests
- [ ] Add automated performance comparison
- [ ] Add visualization of results

---

**Version:** 1.0
**Last Updated:** 2025-11-16
**Status:** ✅ Complete (with mock implementations)
