# Execution Model Testing - Quick Start

This directory contains a comprehensive test suite for validating ZSim and Event-Driven execution models across different scales (10K-100K PEs) using your workloads.

## 🚀 Quick Start

```bash
# 1. Build PIMID (if not already built)
mkdir -p build && cd build
cmake ..
make -j$(nproc)
cd ..

# 2. Run quick verification test (recommended first!)
./run_execution_model_tests.sh quick

# 3. Run scale comparison (10K, 20K, 50K, 100K PEs)
./run_execution_model_tests.sh scale

# 4. Run comprehensive tests
./run_execution_model_tests.sh comprehensive 100
```

## 📋 Test Scripts

| Script | Purpose | Duration | Output |
|--------|---------|----------|--------|
| `run_execution_model_tests.sh` | Master test runner | Varies | Console |
| `test_quick_execution_models.py` | Quick smoke test | ~5 min | Console |
| `test_scale_comparison.py` | Scale comparison | ~30-60 min | JSON + CSV |
| `test_execution_models_comprehensive.py` | Full test suite | ~2-4 hrs | JSON + CSV |

## 🎯 What Gets Tested

### Execution Models
- ✅ **ZSim (execution-driven)** - Cycle-accurate simulation
  - Simple cores (in-order)
  - ALU cores (no cache)
- ✅ **Event-Driven (analytical)** - Fast roofline model

### Scales (Realistic: 1 PE per bank or per subarray)
- ✅ 16 PEs (bank-level: 1 PE per bank, 16 banks)
- ✅ 64 PEs (4 banks × 16 subarrays, or 64 banks)
- ✅ 256 PEs (16 banks × 16 subarrays)
- ✅ 1024 PEs (16 banks × 64 subarrays)

### Workloads (with small inputs for speed)
- ✅ BFS (64-128 vertices)
- ✅ GEMM (32-64x32-64 matrices)
- ✅ SpMV (128x128 sparse matrix)
- ✅ Dot Product (256 elements)
- ✅ Reduction (256 elements)
- ✅ Histogram (512 elements)

### Memory Technologies
- ✅ DRAM
- ✅ SRAM
- ✅ ReRAM

### PE Placements
- ✅ Bank-level
- ✅ Subarray-level

## 📊 Example Results

```
Scale: 16 PEs (bank-level)
  ZSim avg time: 145.2ms (16 tests)
  Event-Driven avg time: 12.3ms (16 tests)
  Speedup (Event/ZSim): 11.8x

Scale: 256 PEs (subarray-level)
  ZSim avg time: 687.4ms (16 tests)
  Event-Driven avg time: 18.7ms (16 tests)
  Speedup (Event/ZSim): 36.7x

Scale: 1024 PEs (subarray-level)
  ZSim avg time: 1342.5ms (16 tests)
  Event-Driven avg time: 25.4ms (16 tests)
  Speedup (Event/ZSim): 52.9x
```

## 📁 Output Files

### Quick Tests
- Console output only

### Scale Comparison
- `scale_comparison_results.json` - Detailed results with all metrics
- `scale_comparison.csv` - CSV format for plotting/analysis
- `test_configs_scale/*.yaml` - Generated configuration files

### Comprehensive Tests
- `test_results_execution_models/test_results.json` - All test results
- `test_results_execution_models/execution_model_comparison.csv` - Comparison table
- `test_results_execution_models/*.yaml` - Individual test configs

## 🔍 Understanding the Results

### Success Criteria
- ✅ Simulation completes without errors
- ✅ Valid output metrics generated
- ✅ Performance within expected ranges

### Key Metrics
1. **Execution Time (ms)** - Wall-clock simulation time
   - ZSim: Slower (detailed modeling)
   - Event-Driven: Faster (analytical)

2. **Simulated Cycles** - Modeled execution cycles
   - Should be similar between models
   - Validates analytical model accuracy

3. **Throughput (GOPS)** - Operations per second
   - Application-dependent
   - Should scale with PE count

4. **Speedup** - Event-Driven time / ZSim time
   - Expected: 10-100x at scale
   - Higher at larger scales

### Expected Differences
- **Cycle Counts**: May differ by 5-10% (analytical approximation)
- **Cache Effects**: ZSim models precisely, event-driven approximates
- **NoC Contention**: Different modeling approaches
- **Energy**: Event-driven uses simplified model

## 🛠️ Troubleshooting

### PIMID not built
```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Python dependencies missing
```bash
pip3 install pyyaml
```

### Tests timeout
- Reduce test count: `./run_execution_model_tests.sh comprehensive 50`
- Use smaller workload sizes in scripts
- Check system resources

### Inconsistent results
- This is expected! Models trade accuracy for speed
- ZSim is cycle-accurate
- Event-driven is analytical approximation

## 📖 Full Documentation

See [EXECUTION_MODEL_TESTING.md](EXECUTION_MODEL_TESTING.md) for:
- Detailed test descriptions
- Advanced configuration options
- Custom test creation
- Troubleshooting guide
- Integration with CI/CD

## 🎓 Usage Examples

### Run everything in sequence
```bash
./run_execution_model_tests.sh all
```

### Quick verification before commit
```bash
./run_execution_model_tests.sh quick
```

### Compare specific scales
```bash
# Edit test_scale_comparison.py to set:
scales = [10000, 100000]  # Only test 10K and 100K
./run_execution_model_tests.sh scale
```

### Limited comprehensive test
```bash
./run_execution_model_tests.sh comprehensive 50
```

### Run individual scripts
```bash
# Quick test
./test_quick_execution_models.py

# Scale comparison
./test_scale_comparison.py

# Comprehensive (100 tests)
./test_execution_models_comprehensive.py --max-tests 100

# Comprehensive with custom output
./test_execution_models_comprehensive.py --output-dir my_test_results
```

## 📈 Interpreting CSV Output

The CSV files can be imported into spreadsheet tools or plotting libraries:

```python
import pandas as pd
import matplotlib.pyplot as plt

# Load results
df = pd.read_csv('scale_comparison.csv')

# Plot execution time vs scale
zsim_data = df[df['Execution_Model'] == 'zsim']
event_data = df[df['Execution_Model'] == 'event_driven']

plt.plot(zsim_data['Scale'], zsim_data['Time_ms'], label='ZSim')
plt.plot(event_data['Scale'], event_data['Time_ms'], label='Event-Driven')
plt.xlabel('Number of PEs')
plt.ylabel('Execution Time (ms)')
plt.legend()
plt.savefig('execution_time_comparison.png')
```

## ✅ Pre-commit Checklist

Before committing changes that affect execution models:

1. ✅ Run quick test: `./run_execution_model_tests.sh quick`
2. ✅ Verify all tests pass
3. ✅ Check for performance regressions
4. ✅ Review any new warnings or errors

## 🤝 Contributing

When adding new workloads or execution model features:

1. Add test cases to the appropriate test script
2. Use small input sizes for fast testing
3. Update this README if adding new test scripts
4. Run full test suite before submitting PR

## 📞 Support

- **Documentation**: [EXECUTION_MODEL_TESTING.md](EXECUTION_MODEL_TESTING.md)
- **PIMID Guide**: [USER_GUIDE.md](USER_GUIDE.md)
- **Architecture**: [ARCHITECTURE.md](ARCHITECTURE.md)

---

**Last Updated**: 2025-11-23
**Test Suite Version**: 1.0
