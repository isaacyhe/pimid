# PIMID Quick Start Guide

This guide will get you up and running with PIMID in the shortest time possible.

## Table of Contents
- [5-Minute Quick Start](#5-minute-quick-start)
- [System Requirements](#system-requirements)
- [Installation](#installation)
- [First Simulation](#first-simulation)
- [Running Tests](#running-tests)
- [Common Workflows](#common-workflows)
- [Next Steps](#next-steps)

---

## 5-Minute Quick Start

```bash
# 1. Clone and setup (Ubuntu 24.04)
git clone https://github.com/yourusername/pimid-dev.git
cd pimid-dev

# 2. Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake libboost-all-dev \
    libyaml-cpp-dev python3 python3-pip scons

# 3. Download PIN 3.28 (for ZSim)
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.28-98749-g6643ecee5-gcc-linux.tar.gz
tar xzf pin-3.28-98749-g6643ecee5-gcc-linux.tar.gz
export PINPATH=$PWD/pin-3.28-98749-g6643ecee5-gcc-linux

# 4. Build PIMID
mkdir -p build && cd build
cmake ../pimid
make -j$(nproc)

# 5. Run your first simulation
./pimid/pimid --config ../DAC26/configs/bank1_32KB_libcom.yaml \
              --workload ../DAC26/workloads_pimid/bfs_message_pimid
```

**Done!** You've just run your first PIMID simulation with LIBCom interconnect.

---

## System Requirements

### Recommended Setup
- **OS**: Ubuntu 24.04 LTS (or 22.04, 20.04)
- **CPU**: 4+ cores (8+ recommended for faster builds)
- **RAM**: 8GB minimum, 16GB+ recommended
- **Storage**: 10GB for source + build artifacts

### Minimum Setup
- **OS**: Ubuntu 18.04+ or any Linux with GCC 7+
- **CPU**: 2+ cores
- **RAM**: 4GB
- **Storage**: 5GB

### Supported Platforms
| Platform | PIN Version | Status |
|----------|-------------|--------|
| Ubuntu 24.04 LTS | PIN 3.28+ | ✅ Fully Supported |
| Ubuntu 22.04 LTS | PIN 3.21+ | ✅ Supported |
| Ubuntu 20.04 LTS | PIN 3.7+ | ✅ Supported |
| Ubuntu 18.04 LTS | PIN 2.14 | ✅ Legacy Support |

---

## Installation

### Step 1: Install System Dependencies

#### Ubuntu 24.04 / 22.04 / 20.04:
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    gcc g++ \
    libboost-all-dev \
    libyaml-cpp-dev \
    python3 python3-pip \
    scons \
    git \
    wget
```

#### Ubuntu 18.04 (Legacy):
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    gcc-7 g++-7 \
    libboost-all-dev \
    libyaml-cpp-dev \
    python3 python3-pip \
    scons \
    git \
    wget

# Set GCC 7 as default
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-7 60
```

### Step 2: Download Intel PIN

Choose the appropriate PIN version for your Ubuntu:

#### For Ubuntu 24.04 (PIN 3.28):
```bash
cd /home/user/pimid-dev
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.28-98749-g6643ecee5-gcc-linux.tar.gz
tar xzf pin-3.28-98749-g6643ecee5-gcc-linux.tar.gz
export PINPATH=$PWD/pin-3.28-98749-g6643ecee5-gcc-linux

# Make permanent
echo "export PINPATH=$PWD/pin-3.28-98749-g6643ecee5-gcc-linux" >> ~/.bashrc
```

#### For Ubuntu 18.04 (PIN 2.14):
```bash
cd /home/user/pimid-dev
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-2.14-71313-gcc.4.4.7-linux.tar.gz
tar xzf pin-2.14-71313-gcc.4.4.7-linux.tar.gz
export PINPATH=$PWD/pin-2.14-71313-gcc.4.4.7-linux

# Make permanent
echo "export PINPATH=$PWD/pin-2.14-71313-gcc.4.4.7-linux" >> ~/.bashrc
```

**Note**: PIN will auto-detect during ZSim build. See [PIN Version Options](pimid/external/zsim/PIN_VERSION_BUILD_OPTIONS.md) for manual control.

### Step 3: Clone Repository

```bash
git clone https://github.com/yourusername/pimid-dev.git
cd pimid-dev
git submodule update --init --recursive
```

### Step 4: Build PIMID

```bash
# Create build directory
mkdir -p build && cd build

# Configure
cmake ../pimid

# Build (use all cores)
make -j$(nproc)

# Verify build
ls pimid/pimid  # Should show the binary
```

**Build Time**: 5-15 minutes depending on your system.

---

## First Simulation

### Example 1: BFS with LIBCom Interconnect

```bash
cd /home/user/pimid-dev/build

# Run BFS on 32KB bank with LIBCom
./pimid/pimid \
    --config ../DAC26/configs/bank1_32KB_libcom.yaml \
    --workload ../DAC26/workloads_pimid/bfs_message_pimid
```

**Expected Output:**
```
[PIMID] Initializing simulation...
[PIMID] Memory Technology: DRAM
[PIMID] PIM Granularity: BANK
[PIMID] Network: LIBCom
[PIMID] Running workload: bfs_message_pimid
[PIMID] Simulation complete!
[PIMID] Total cycles: XXXXX
[PIMID] Energy: XXX.XX mJ
```

### Example 2: GEMM with Baseline H-tree

```bash
./pimid/pimid \
    --config ../DAC26/configs/bank1_32KB_baseline.yaml \
    --workload ../DAC26/workloads_pimid/gemm_message_pimid
```

### Example 3: Using Different Memory Technologies

```bash
# SRAM (fastest)
./pimid/pimid \
    --config ../test/configs/sram_bank_test.yaml \
    --workload ../DAC26/workloads_pimid/dotproduct_message_pimid

# STT-MRAM (non-volatile)
./pimid/pimid \
    --config ../test/configs/sttmram_bank_test.yaml \
    --workload ../DAC26/workloads_pimid/reduction_message_pimid

# ReRAM (high density)
./pimid/pimid \
    --config ../test/configs/reram_bank_test.yaml \
    --workload ../DAC26/workloads_pimid/histogram_message_pimid
```

---

## Running Tests

PIMID includes comprehensive test suites with 100% pass rate.

### Quick Test (5 tests, ~30 seconds)

```bash
cd /home/user/pimid-dev
python3 test/scripts/test_quick_execution_models.py
```

### Comprehensive Test (1000 tests, ~5 minutes)

```bash
cd /home/user/pimid-dev
python3 test/scripts/comprehensive_test_suite_zsim_1000.py
```

**Results Location**: `test/results/test_results_zsim_1000/`

### Production Audit (10,000 tests, ~5 minutes)

```bash
cd /home/user/pimid-dev
python3 test/scripts/comprehensive_audit_suite_zsim_10000.py
```

**Status**: ✅ 100% pass rate (10,000/10,000 tests)
**Results**: `test/results/test_results_zsim_10000/`
**Report**: `test/reports/COMPREHENSIVE_AUDIT_REPORT_10000.md`

### Test Coverage

| Test Suite | Tests | Coverage | Pass Rate | Time |
|------------|-------|----------|-----------|------|
| Quick smoke tests | 5 | Basic | 100% | ~30s |
| Comprehensive | 1,000 | All workloads | 100% | ~5min |
| **Production audit** | **10,000** | **All configs** | **100%** | **~5min** |

See [Test Organization](test/TEST_ORGANIZATION.md) for full details.

---

## Common Workflows

### Workflow 1: Compare Memory Technologies

```bash
cd /home/user/pimid-dev/build

# Run same workload with different memory techs
for tech in SRAM DRAM STT_MRAM PCM RERAM; do
    ./pimid/pimid --config ../configs/${tech}_test.yaml \
                  --workload ../DAC26/workloads_pimid/gemm_message_pimid \
                  > results_${tech}.txt
done

# Compare results
grep "Total cycles" results_*.txt
grep "Energy" results_*.txt
```

### Workflow 2: Scale PE Count

```bash
# Test with different PE counts (1, 2, 4, 8, 16, 32, 64)
for pes in 1 2 4 8 16 32 64; do
    ./pimid/pimid --config ../configs/bank_${pes}pe.yaml \
                  --workload ../DAC26/workloads_pimid/spmv_message_pimid \
                  > results_${pes}pe.txt
done
```

### Workflow 3: Baseline vs LIBCom Comparison

```bash
# Baseline H-tree
./pimid/pimid --config ../DAC26/configs/bank1_32KB_baseline.yaml \
              --workload ../DAC26/workloads_pimid/bfs_message_pimid \
              > results_baseline.txt

# LIBCom interconnect
./pimid/pimid --config ../DAC26/configs/bank1_32KB_libcom.yaml \
              --workload ../DAC26/workloads_pimid/bfs_message_pimid \
              > results_libcom.txt

# Compare
echo "Baseline:" && grep "Total cycles" results_baseline.txt
echo "LIBCom:" && grep "Total cycles" results_libcom.txt
```

### Workflow 4: Run All Workloads

```bash
#!/bin/bash
WORKLOADS=(
    bfs_message_pimid
    gemm_message_pimid
    spmv_message_pimid
    dotproduct_message_pimid
    reduction_message_pimid
    histogram_message_pimid
    prefixsum_message_pimid
    stencil1d_message_pimid
)

for wl in "${WORKLOADS[@]}"; do
    echo "Running $wl..."
    ./pimid/pimid --config ../DAC26/configs/bank1_32KB_libcom.yaml \
                  --workload ../DAC26/workloads_pimid/$wl \
                  > results_${wl}.txt
done
```

---

## Next Steps

### 📚 Documentation

| Document | Purpose |
|----------|---------|
| [pimid/README.md](pimid/README.md) | Main PIMID documentation |
| [RECENT_FEATURES.md](RECENT_FEATURES.md) | Latest features and improvements |
| [Test Organization](test/TEST_ORGANIZATION.md) | Complete test suite documentation |
| [PIN Version Options](pimid/external/zsim/PIN_VERSION_BUILD_OPTIONS.md) | PIN 2.x/3.x build control |
| [PIN 3.x Upgrade Guide](pimid/external/zsim/PIN3_UPGRADE.md) | Ubuntu 24.04 compatibility |

### 🔧 Advanced Usage

1. **Custom Configurations**
   - See `DAC26/configs/` for examples
   - Modify YAML files for your needs
   - Documentation: [Config Schema](pimid/include/config/config_schema.h)

2. **Custom Workloads**
   - See `DAC26/workloads_pimid/` for examples
   - Compile with provided Makefile
   - Documentation: [Workload Summary](test/benchmarks/WORKLOADS_SUMMARY.md)

3. **Build ZSim Separately** (if needed)
   ```bash
   cd pimid/external/zsim
   scons -j$(nproc)                    # Auto-detect PIN version
   scons --pin-version=3 -j$(nproc)   # Force PIN 3.x
   scons --pin-version=2 -j$(nproc)   # Force PIN 2.x
   ```

4. **Run Comprehensive Tests**
   ```bash
   # See test/TEST_ORGANIZATION.md for all options
   python3 test/scripts/comprehensive_audit_suite_zsim_10000.py
   ```

### 🐛 Troubleshooting

#### Issue: PIN not found
```
ERROR: PIN not found!
```
**Solution**: Set PINPATH environment variable
```bash
export PINPATH=/path/to/pin-3.28-98749-g6643ecee5-gcc-linux
```

#### Issue: Build fails with "pin.H not found"
**Solution**: Verify PIN installation
```bash
ls $PINPATH/source/include/pin/pin.H    # PIN 3.x
ls $PINPATH/source/include/pin.H        # PIN 2.x
```

#### Issue: Wrong PIN version detected
**Solution**: Use manual override
```bash
cd pimid/external/zsim
scons --pin-version=3 -j$(nproc)
```

#### Issue: Tests failing
**Solution**: Check workload binaries are compiled
```bash
cd DAC26/workloads_pimid
make all
```

#### Issue: Simulation crashes
**Solution**: Check config file format and paths
```bash
# Verify config is valid YAML
python3 -c "import yaml; yaml.safe_load(open('config.yaml'))"
```

For more issues, see:
- [PIN Build Options](pimid/external/zsim/PIN_VERSION_BUILD_OPTIONS.md) - All 7 error scenarios
- [Test Reports](test/reports/) - Known issues and fixes

### 📊 Performance Tips

1. **Use parallel builds**: `make -j$(nproc)`
2. **Use SRAM for speed**: Fastest memory technology
3. **Start with small PE counts**: 1-16 PEs for quick iteration
4. **Use quick tests first**: Validate before long runs

### 🎯 Example Research Workflows

**Energy Efficiency Study:**
```bash
# Compare energy across memory technologies
for tech in SRAM DRAM STT_MRAM RERAM; do
    ./pimid/pimid --config configs/${tech}_bank.yaml \
                  --workload workloads/gemm_pimid \
                  | grep "Energy"
done
```

**Performance Scaling Study:**
```bash
# Study PE count scaling
for pe_count in 1 2 4 8 16 32 64; do
    ./pimid/pimid --config configs/bank_${pe_count}pe.yaml \
                  --workload workloads/bfs_pimid \
                  | grep "Total cycles"
done
```

**Interconnect Comparison:**
```bash
# Compare baseline H-tree vs LIBCom
./pimid/pimid --config configs/baseline_htree.yaml ...
./pimid/pimid --config configs/libcom.yaml ...
```

---

## Summary

✅ **You've learned:**
- How to install PIMID (5 minutes)
- How to run your first simulation
- How to run test suites (100% pass rate)
- Common research workflows
- Where to find detailed documentation

✅ **Key Achievements:**
- **PIN 3.x Support**: Works on Ubuntu 24.04 (unlike competitors)
- **10,000 Test Suite**: 100% pass rate, production-ready
- **5 Memory Technologies**: SRAM, DRAM, STT-MRAM, PCM, ReRAM
- **16 Workloads**: Message passing + Shared memory models
- **2 Interconnects**: Baseline H-tree + LIBCom

**Ready to dive deeper?** See [pimid/README.md](pimid/README.md) for complete documentation.

---

**Questions or Issues?**
- Check [Troubleshooting](#-troubleshooting) above
- Review [test/TEST_ORGANIZATION.md](test/TEST_ORGANIZATION.md)
- See [PIN_VERSION_BUILD_OPTIONS.md](pimid/external/zsim/PIN_VERSION_BUILD_OPTIONS.md)

**Last Updated**: 2025-11-23
**Version**: 1.0
**Status**: ✅ Production Ready
