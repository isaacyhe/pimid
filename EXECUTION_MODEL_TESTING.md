# Execution Model Testing Guide

This document describes the comprehensive test suite for verifying different execution models (ZSim vs Event-Driven) across various scales and workloads.

## Overview

The test suite validates both execution models with:
- **Scales**: 10K, 20K, 50K, 100K processing elements
- **Execution Models**: ZSim (execution-driven) and Event-Driven (analytical)
- **Workloads**: BFS, GEMM, SpMV, DotProduct, Reduction, Histogram
- **Memory Technologies**: DRAM, SRAM, ReRAM, STT-MRAM, PCM
- **Small Data Inputs**: For fast testing and quick feedback

## Test Scripts

### 1. Quick Verification Test (`test_quick_execution_models.py`)

**Purpose**: Fast smoke test to verify both execution models work correctly.

**Usage**:
```bash
cd /home/user/pimid-dev
chmod +x test_quick_execution_models.py
./test_quick_execution_models.py
```

**Features**:
- Runs 7 quick tests (~5 minutes)
- Tests both ZSim and Event-Driven models
- Validates basic functionality
- Uses minimal workload sizes

**Output**:
- Console summary with pass/fail status
- Exit code 0 if all tests pass

---

### 2. Scale Comparison Test (`test_scale_comparison.py`)

**Purpose**: Compare ZSim vs Event-Driven execution models across different scales.

**Usage**:
```bash
cd /home/user/pimid-dev
chmod +x test_scale_comparison.py
./test_scale_comparison.py
```

**Features**:
- Tests at 10K, 20K, 50K, 100K PE scales
- Compares execution time between models
- Tests multiple workloads and memory technologies
- Generates detailed comparison reports

**Outputs**:
- `scale_comparison_results.json` - Detailed results
- `scale_comparison.csv` - CSV format for analysis
- Console report with speedup comparisons

**Example Output**:
```
Scale: 10,000 PEs
  ZSim avg time: 145.2ms (16 tests)
  Event-Driven avg time: 12.3ms (16 tests)
  Speedup (Event/ZSim): 11.8x

Scale: 50,000 PEs
  ZSim avg time: 687.4ms (16 tests)
  Event-Driven avg time: 18.7ms (16 tests)
  Speedup (Event/ZSim): 36.7x
```

---

### 3. Comprehensive Test Suite (`test_execution_models_comprehensive.py`)

**Purpose**: Exhaustive testing of all combinations.

**Usage**:
```bash
cd /home/user/pimid-dev
chmod +x test_execution_models_comprehensive.py

# Run all tests (may take several hours)
./test_execution_models_comprehensive.py

# Run limited number of tests
./test_execution_models_comprehensive.py --max-tests 50

# Specify output directory
./test_execution_models_comprehensive.py --output-dir my_results
```

**Features**:
- Tests all combinations of:
  - 4 scales (10K, 20K, 50K, 100K PEs)
  - 3 execution model configurations
  - 6 workloads
  - 3 memory technologies
  - 2 PE placements
- Generates hundreds of test configurations
- Detailed comparison and analysis

**Outputs**:
- `test_results_execution_models/test_results.json` - All results
- `test_results_execution_models/execution_model_comparison.csv` - CSV report
- Individual config files for each test

---

## Test Configurations

### Execution Models Tested

#### 1. ZSim with Simple Cores
- **Type**: `zsim` with `Simple` core type
- **Characteristics**: In-order, single-issue cores
- **Use Case**: Accurate modeling of simple PIM cores

#### 2. ZSim with ALU Cores
- **Type**: `zsim` with `ALU` core type
- **Characteristics**: Pure computation, no cache hierarchy
- **Use Case**: Bank/subarray-level processing elements

#### 3. Event-Driven Analytical Model
- **Type**: `event_driven` with analytical performance model
- **Characteristics**: Fast roofline-based estimation
- **Use Case**: Design space exploration, large-scale studies

### Workload Sizes (Small for Speed)

| Workload | Parameter | Small Input Size |
|----------|-----------|------------------|
| BFS | num_vertices | 64-128 |
| GEMM | matrix_size | 32-64 |
| SpMV | matrix_size | 128 |
| Dot Product | vector_length | 256 |
| Reduction | elements_per_subarray | 256 |
| Histogram | num_elements | 512 |

### Scale Configurations

| Scale | PEs | Description |
|-------|-----|-------------|
| 10K | 10,000 | Small-scale PIM system |
| 20K | 20,000 | Medium-scale PIM system |
| 50K | 50,000 | Large-scale PIM system |
| 100K | 100,000 | Very large-scale PIM system |

---

## Running the Tests

### Prerequisites

1. **Build PIMID**:
```bash
cd /home/user/pimid-dev
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

2. **Install Python dependencies**:
```bash
pip3 install pyyaml
```

### Quick Start

**Step 1**: Run quick verification (recommended first):
```bash
cd /home/user/pimid-dev
chmod +x test_quick_execution_models.py
./test_quick_execution_models.py
```

**Step 2**: Run scale comparison:
```bash
chmod +x test_scale_comparison.py
./test_scale_comparison.py
```

**Step 3**: Run comprehensive tests (optional):
```bash
chmod +x test_execution_models_comprehensive.py
./test_execution_models_comprehensive.py --max-tests 100
```

### Interpreting Results

#### Success Criteria
- ✓ Test passes if simulation completes without errors
- Execution time is measured for comparison
- Throughput and performance metrics are extracted

#### Comparison Metrics
1. **Execution Time**: Wall-clock time for simulation
   - ZSim: Typically slower (detailed modeling)
   - Event-Driven: Typically faster (analytical)

2. **Simulated Cycles**: Modeled execution cycles
   - Should be similar between models for same workload
   - Validates analytical model accuracy

3. **Speedup**: Event-Driven time / ZSim time
   - Expected: 10-100x speedup for event-driven
   - Higher at larger scales

---

## Troubleshooting

### Common Issues

**Issue**: Tests timeout
- **Solution**: Reduce `--max-tests` or use smaller workload sizes
- **Check**: System resources (CPU, memory)

**Issue**: Build errors
- **Solution**: Rebuild PIMID from scratch:
  ```bash
  cd /home/user/pimid-dev
  rm -rf build
  mkdir build && cd build
  cmake ..
  make -j$(nproc)
  ```

**Issue**: Configuration errors
- **Solution**: Check YAML syntax in generated configs
- **Verify**: Memory technology settings match hardware model

**Issue**: Inconsistent results between models
- **Solution**: This is expected! Models have different accuracy/speed tradeoffs
- **Note**: Event-driven is approximate, ZSim is cycle-accurate

---

## Advanced Usage

### Custom Test Configurations

Create your own test by modifying the workload parameters:

```python
# In test script
workloads = [
    {
        "name": "bfs",
        "params": {
            "num_vertices": 256,  # Increase for more work
            "avg_degree": 8,      # Denser graph
        },
    },
]
```

### Filtering Tests

Run only specific scales:
```python
scales = [10000, 50000]  # Only 10K and 50K
```

Run only specific workloads:
```python
workloads = [
    ("gemm", {"matrix_size": 64}),
    ("bfs", {"num_vertices": 128, "avg_degree": 4}),
]
```

### Parallel Execution

For faster testing, run multiple test scripts in parallel:
```bash
# Terminal 1
./test_quick_execution_models.py

# Terminal 2
./test_scale_comparison.py --output-dir scale_test_1

# Terminal 3
./test_execution_models_comprehensive.py --max-tests 50 --output-dir comp_test_1
```

---

## Expected Performance

### Typical Execution Times

| Test Suite | # Tests | Estimated Time |
|------------|---------|----------------|
| Quick | 7 | 3-5 minutes |
| Scale Comparison | 64 | 30-60 minutes |
| Comprehensive (100 tests) | 100 | 2-4 hours |
| Comprehensive (all) | 400+ | 8-12 hours |

### Memory Requirements

- **Quick tests**: < 2GB RAM
- **Scale comparison**: < 4GB RAM
- **Comprehensive**: < 8GB RAM
- **Per-test peak**: ~500MB

---

## Validation

### What the Tests Verify

1. **Functional Correctness**:
   - Both execution models complete successfully
   - No crashes or errors
   - Valid output metrics

2. **Model Consistency**:
   - Similar cycle counts (within expected variance)
   - Consistent relative performance trends
   - Proper scaling behavior

3. **Performance Characteristics**:
   - Event-driven is faster than ZSim
   - Speedup increases with scale
   - Reasonable absolute performance

### Known Differences

- **Cycle counts**: May differ by 5-10% (analytical approximation)
- **Energy**: Event-driven uses simplified energy model
- **Cache effects**: ZSim models cache misses, event-driven approximates
- **NoC contention**: Different modeling fidelity

---

## Output Files

### JSON Results
```json
{
  "config_name": "test_0001_10K_zsim_Simple_bfs_DRAM_bank",
  "execution_model": "zsim",
  "num_pes": 10000,
  "workload": "bfs",
  "success": true,
  "execution_time_ms": 142.5,
  "throughput": 2.3
}
```

### CSV Format
```csv
Config,Execution_Model,Num_PEs,Workload,Success,Time_ms,Throughput
test_0001,zsim,10000,bfs,True,142.5,2.3
test_0002,event_driven,10000,bfs,True,11.2,2.4
```

---

## Integration with CI/CD

Add to your continuous integration:

```yaml
# .github/workflows/test.yml
- name: Run Quick Execution Model Tests
  run: |
    cd /home/user/pimid-dev
    ./test_quick_execution_models.py

- name: Run Scale Comparison (limited)
  run: |
    ./test_scale_comparison.py --max-tests 20
```

---

## References

- [USER_GUIDE.md](USER_GUIDE.md) - General PIMID usage
- [EXECUTION_MODEL_CONFIGURATION.md](docs/EXECUTION_MODEL_CONFIGURATION.md) - Execution model details
- [TEST_SUITE_DOCUMENTATION.md](test/TEST_SUITE_DOCUMENTATION.md) - Other test suites
- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture

---

## Support

For issues or questions:
1. Check this documentation
2. Review test output logs
3. Examine generated config files
4. Check PIMID build logs

---

**Last Updated**: 2025-11-23
**Version**: 1.0
**Maintainer**: PIMID Development Team
