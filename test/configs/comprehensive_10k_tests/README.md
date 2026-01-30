# 10,000 Comprehensive Test Suite

## Overview

This directory contains 10000 test configuration files for comprehensive
testing covering all dimensions:
- **Network Topologies**: BUS, CROSSBAR, MESH_2D, TORUS_2D, FAT_TREE, H_TREE
- **Memory Technologies**: DDR4, DDR5, HBM2, HBM3, GDDR6, LPDDR5
- **PIM Placement Levels**: subarray, bank, chip, rank
- **Core Models**: in-order, out-of-order, simple
- **Scheduling Policies**: nearest, round_robin, load_balanced, priority_based, data_aware

## Test Categories

1. **Memory technology sweep** (1400 tests): All memory techs with various PIM configs
2. **Single-level topology** (1500 tests): One topology level varied
3. **Multi-level topology** (1200 tests): 2-4 topology levels varied
4. **Core model variations** (1000 tests): All core types tested
5. **Scheduling policies** (1000 tests): All scheduling policies tested
6. **PIM placement levels** (1000 tests): All placement levels tested
7. **HBM-specific tests** (600 tests): HBM2/HBM3 focused
8. **DDR-specific tests** (600 tests): DDR4/DDR5 focused
9. **Comprehensive combinations** (1200 tests): Random comprehensive configs
10. **Stress tests** (500 tests): Extreme configurations

## Usage

```bash
# Run a single test
pimid --config test/configs/comprehensive_10k_tests/memtech_00000.yaml

# Run all tests with 8 parallel workers
python3 test/config_generator/run_10k_tests.py \
  --config-dir test/configs/comprehensive_10k_tests \
  --workers 8 \
  --report comprehensive_test_report.json
```

## Files

- `*.yaml`: Individual test configuration files
- `test_manifest.yaml`: Index of all tests with metadata
- `README.md`: This file
