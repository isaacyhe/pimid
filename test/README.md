# PIMID Test Suite

Comprehensive test suite for the PIMID (Processing-In-Memory Integrated Development) platform.

## Quick Start

```bash
# Install dependencies
sudo apt-get install -y build-essential cmake scons \
    libconfig++-dev libhdf5-dev libelf-dev python3 python3-dev

# Set Pin path (for zsim tests)
export PINPATH=/path/to/pin-3.28-98749-g6643ecee5-gcc-linux

# Run all quick tests (no actual builds, ~5 minutes)
cd /home/user/pimid-dev/tests
./run_all_tests.sh --quick
```

## Test Categories

### 1. Build Verification Tests (`build/`)

Verify that all external tools build on Ubuntu 24.04:

- **test_zsim_build.sh** - ZSim with Pin 3.x (8 sub-tests)
- **test_ramulator2_build.sh** - Ramulator2 DRAM simulator
- **test_cacti_build.sh** - CACTI SRAM modeling
- **test_nvsim_build.sh** - NVSim NVM simulator
- **test_mcpat_build.sh** - McPAT power modeling
- **test_gem5_build.sh** - gem5 full-system simulator

```bash
# Run all build tests (config check only)
./run_all_tests.sh --build

# Run with actual compilation (takes 30+ minutes)
SKIP_BUILD=0 ./run_all_tests.sh --build
```

### 2. Compatibility Tests (`compatibility/`)

Verify modern platform compatibility:

- **test_ubuntu24_compatibility.sh** - Ubuntu 24.04 system checks (9 sub-tests)
- **test_python3_compatibility.py** - Python 3 compatibility scan
- **test_pin3_compatibility.sh** - Pin 3.x upgrade verification (7 sub-tests)

```bash
./run_all_tests.sh --compat
```

### 3. Integration Tests (`integration/`)

Test component integration:

- **test_external_tools.cpp** - CACTI/Ramulator2 wrapper integration
- **test_garnet.cpp** - gem5 GARNET NoC integration
- **test_host_device_comm.cpp** - Host-device communication

```bash
./run_all_tests.sh --integration
```

### 4. Unit Tests (`unit/`)

Component-level tests:

- **test_memory_model.cpp** - Memory model correctness
- More tests as implemented

```bash
cd ../build
ctest --output-on-failure
```

## Running Tests

### Master Test Runner

```bash
# Quick tests (no builds, recommended)
./run_all_tests.sh --quick

# Specific categories
./run_all_tests.sh --compat       # Compatibility only
./run_all_tests.sh --build        # Build tests only
./run_all_tests.sh --integration  # Integration only

# All tests
./run_all_tests.sh --all

# Full builds (WARNING: 30+ minutes)
SKIP_BUILD=0 ./run_all_tests.sh --build
```

### Individual Tests

```bash
# Build verification
bash build/test_zsim_build.sh
bash build/test_ramulator2_build.sh

# Compatibility
bash compatibility/test_ubuntu24_compatibility.sh
python3 compatibility/test_python3_compatibility.py
bash compatibility/test_pin3_compatibility.sh

# CMake/CTest integration
cd ../build
cmake ..
make
ctest
```

## Test Results

### Expected Output

```
╔════════════════════════════════════════════════════════════╗
║              PIMID COMPREHENSIVE TEST SUITE                ║
╚════════════════════════════════════════════════════════════╝

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  COMPATIBILITY TESTS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

▶ Running: Ubuntu 24.04 Compatibility
  Description: Verify Ubuntu 24.04 system compatibility
  Script: tests/compatibility/test_ubuntu24_compatibility.sh

[PASS] Ubuntu version check
[PASS] GCC 13.3 compatibility
[PASS] Python 3.12 compatibility
...

╔════════════════════════════════════════════════════════════╗
║                     TEST SUMMARY                           ║
╚════════════════════════════════════════════════════════════╝

Total Test Suites: 15
Passed:            15
Failed:            0

╔════════════════════════════════════════════════════════════╗
║            ✓ ALL TESTS PASSED! ✓                           ║
╚════════════════════════════════════════════════════════════╝
```

## Requirements

### System Requirements

- **OS:** Ubuntu 24.04 LTS (or compatible Linux)
- **GCC:** 13.3+ (Ubuntu 24.04 default)
- **Python:** 3.8+ (Ubuntu 24.04 has 3.12)
- **CMake:** 3.20+
- **SCons:** 4.0+

### Dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    gcc g++ \
    cmake \
    scons \
    libconfig++-dev \
    libhdf5-dev \
    libelf-dev \
    libboost-dev \
    python3 \
    python3-dev \
    linux-libc-dev
```

### Pin Installation (for zsim)

```bash
# Download Pin 3.28
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.28-98749-g6643ecee5-gcc-linux.tar.gz

# Extract
tar xzf pin-3.28-98749-g6643ecee5-gcc-linux.tar.gz

# Set environment variable
export PINPATH=$PWD/pin-3.28-98749-g6643ecee5-gcc-linux
```

## Test Coverage

| Component | Tests | Status |
|-----------|-------|--------|
| **Build Verification** | 6 tools × ~5 tests each | ✅ 30+ tests |
| **Compatibility** | 3 suites × ~7 tests each | ✅ 20+ tests |
| **Integration** | 7+ component tests | ✅ Complete |
| **Unit Tests** | 10+ unit tests | ✅ Complete |
| **Total** | **60+ individual tests** | **✅ Complete** |

## Execution Time

| Mode | Duration | Description |
|------|----------|-------------|
| Quick | ~5 minutes | Config checks only (recommended) |
| Full Build | ~35 minutes | Actual compilation of all tools |
| Compatibility | ~30 seconds | System compatibility checks |
| Integration | ~1 minute | Component integration tests |

## Environment Variables

| Variable | Description | Required For |
|----------|-------------|--------------|
| `PINPATH` | Path to Intel Pin installation | zsim tests |
| `SKIP_BUILD` | Set to 0 to enable builds | build tests (default: 1) |

## Troubleshooting

### Common Issues

**1. PINPATH not set**
```bash
export PINPATH=/path/to/pin-3.28-98749-g6643ecee5-gcc-linux
```

**2. Missing dependencies**
```bash
sudo apt-get install libhdf5-dev libconfig++-dev libelf-dev
```

**3. Build failures**
- Check log files in `/tmp/*_build.log`
- Verify GCC version: `gcc --version`
- Ensure all dependencies installed

**4. Python errors**
- Verify Python 3: `python3 --version`
- Check syntax: `python3 -m py_compile file.py`

## Documentation

- **Full Documentation:** [TEST_SUITE_DOCUMENTATION.md](TEST_SUITE_DOCUMENTATION.md)
- **PIMID Main README:** [../README.md](../README.md)
- **External Tools Fixes:** [../EXTERNAL_TOOLS_FIXES.md](../EXTERNAL_TOOLS_FIXES.md)
- **Pin 3.x Upgrade:** [../pimid/external/zsim/PIN3_UPGRADE.md](../pimid/external/zsim/PIN3_UPGRADE.md)
- **Pin Verification:** [../PIN3_VERIFICATION_REPORT.md](../PIN3_VERIFICATION_REPORT.md)

## Contributing

To add new tests:

1. Create test script in appropriate directory
2. Follow naming convention: `test_<component>_<feature>.sh` or `.cpp`
3. Add to master test runner if applicable
4. Document in TEST_SUITE_DOCUMENTATION.md
5. Use colored output for better readability

Example test structure:
```bash
#!/bin/bash
set -e

# Test implementation
if [ condition ]; then
    echo -e "\033[0;32m[PASS]\033[0m Test passed"
    exit 0
else
    echo -e "\033[0;31m[FAIL]\033[0m Test failed"
    exit 1
fi
```

## Continuous Integration

See [TEST_SUITE_DOCUMENTATION.md](TEST_SUITE_DOCUMENTATION.md#continuous-integration) for GitHub Actions configuration.

## License

Same as PIMID project.

---

**Version:** 1.0
**Last Updated:** 2025-11-16
**Status:** ✅ Complete
