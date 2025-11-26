# 10,000 Custom Topology Test Suite

## Overview

This directory contains 10000 test configuration files for comprehensive
testing of custom network topology and external model integration features.

## Test Categories

1. **Single-level topology** (2000 tests): One level customized
2. **Two-level topology** (1500 tests): Two levels customized
3. **All-levels topology** (1000 tests): All six levels customized
4. **Port count optimization** (1500 tests): Testing port reduction
5. **Specific patterns** (1000 tests): Common topology combinations
6. **Mesh/Torus focused** (500 tests): Mesh and torus topologies
7. **H-TREE focused** (500 tests): H-tree topologies
8. **FAT_TREE focused** (500 tests): Fat-tree topologies
9. **Mixed DRAM types** (1000 tests): Various DRAM configurations
10. **External models** (500 tests): External model integration

## Usage

```bash
# Run a single test
pimid --config test/configs/10k_topology_tests/single_level_00000.yaml

# Run all tests
python3 test/config_generator/run_10k_tests.py
```

## Files

- `*.yaml`: Individual test configuration files
- `test_manifest.yaml`: Index of all tests with metadata
- `README.md`: This file
