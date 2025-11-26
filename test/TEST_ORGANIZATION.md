# Test Directory Organization

This directory contains all test, verification, and audit-related files for PIMID.

## Directory Structure

```
test/
├── scripts/          # Test execution scripts (21 files)
├── reports/          # Test reports and documentation (12 files)
├── results/          # Test execution results (10 result directories)
├── logs/             # Test execution logs
├── data/             # Test data and configuration samples
├── configs/          # Test configuration files
├── benchmarks/       # Benchmark workloads (BFS, GEMM, SpMV, etc.)
├── unit/             # Unit tests
├── integration/      # Integration tests
├── functional/       # Functional tests
├── memory/           # Memory subsystem tests
├── compatibility/    # Compatibility tests
├── workloads/        # Workload-specific tests
├── tools/            # Test utilities and helper tools
└── config_generator/ # Configuration generation tools
```

## Test Scripts (`test/scripts/`)

### Comprehensive Test Suites
- **comprehensive_audit_suite_zsim_10000.py** - 10,000 configuration audit (100% pass rate)
- **comprehensive_test_suite_zsim_1000.py** - 1,000 configuration test suite
- **comprehensive_test_suite_5000.py** - 5,000 test comprehensive suite
- **comprehensive_test_1000_v2.py** - Version 2 of 1000-test suite

### Execution Model Testing
- **test_execution_models_comprehensive.py** - Comprehensive execution model validation
- **test_scale_comparison.py** - Scale comparison tests (16-1024 PEs)
- **test_quick_execution_models.py** - Quick smoke tests for execution models
- **run_execution_model_tests.sh** - Master execution model test runner
- **compare_execution_models.sh** - Compare ZSim vs Event-Driven models

### Benchmark Testing
- **run_comprehensive_benchmark_tests.sh** - Full benchmark suite
- **test_existing_benchmarks.sh** - Quick benchmark validation
- **benchmark_runner** - Benchmark execution tool

### Specialized Tests
- **test_all_dram_types_5000.py** - DRAM technology coverage
- **test_pimid_comprehensive_5000.py** - PIMID comprehensive validation
- **test_corner_cases_5000.py** - Edge case testing
- **large_scale_zsim_test.py** - Large-scale ZSim tests
- **retest_reduction_shared.py** - Regression test for reduction_shared fix

### Monitoring & Utilities
- **monitor_tests.sh** - Real-time test monitoring
- **run_all_tests.sh** - Run all tests in sequence

## Test Reports (`test/reports/`)

### Audit & Verification Reports
- **COMPREHENSIVE_AUDIT_REPORT_10000.md** - 10K test audit (100% success, production-ready)
- **PE_MODEL_REMOVAL_AUDIT.md** - PE plugin removal audit report
- **BUGFIX_REDUCTION_SHARED.md** - Bug fix documentation for missing binary

### Test Results Documentation
- **TEST_RESULTS_ZSIM_1000.md** - 1000 ZSim test results
- **TEST_RESULTS_5000_SUMMARY.md** - 5000 test summary
- **TEST_RESULTS_SUMMARY.md** - General test results summary
- **BENCHMARK_TEST_RESULTS.md** - Benchmark testing results

### Execution Model Documentation
- **TEST_EXECUTION_MODELS_README.md** - Quick start guide for execution model testing
- **EXECUTION_MODEL_TESTING.md** - Comprehensive execution model testing guide
- **EXECUTION_MODEL_TEST_RESULTS.md** - Execution model test outcomes

### Analysis & Summaries
- **WORKLOAD_TEST_ANALYSIS.md** - Workload-specific analysis
- **COVERAGE_SUMMARY.md** - Test coverage summary
- **TOPOLOGY_TEST_SUMMARY.md** - Network topology test results

## Test Results (`test/results/`)

All test execution results are organized by test suite:

### ZSim Execution Model Results
- **test_results_zsim_10000/** - 10,000 configuration audit results
  - `all_results_zsim_10000.json` - Complete results dataset
  - `audit_summary_zsim_10000.json` - Summary statistics
  - `configs/` - 10,000 YAML + ZSim config files

- **test_results_zsim_1000/** - 1,000 configuration test results
  - `all_results_zsim_1000.json`
  - `test_summary_zsim_1000.json`
  - `configs/` - 1,000 test configurations

### Benchmark Results
- **comprehensive_benchmark_results/** - Full benchmark suite results
  - `comparison.csv` - Performance comparison across memory technologies
  - Individual test logs

### Large-Scale Tests
- **large_scale_zsim_results/** - Large-scale ZSim execution
- **pimid_comprehensive_results/** - PIMID comprehensive testing

### Specialized Test Results
- **test_results_5000/** - 5000 general tests
- **test_results_5000_pimid/** - 5000 PIMID-specific tests
- **test_results_all_dram_5000/** - DRAM technology tests
- **test_results_corner_cases_5000/** - Edge case results
- **test_results_1000_v2/** - Version 2 test results

## Test Data (`test/data/`)

Sample configurations and analysis data:
- **test_minimal.yaml** - Minimal test configuration
- **architecture_analysis.json** - Architecture analysis data (v1)
- **architecture_analysis_v2.json** - Architecture analysis data (v2)
- **comprehensive_test_report.json** - Comprehensive test report data
- **BENCHMARK_RESULTS.csv** - Benchmark results in CSV format
- **pim_sim_results.csv** - PIM simulation results

## Key Test Achievements

### 10,000 Configuration Audit (Production-Ready ✅)
- **Tests**: 10,000 unique configurations
- **Success Rate**: 100.0% (10,000/10,000 passed)
- **Execution Time**: 4.5 minutes (37 tests/second)
- **Coverage**:
  - 16 workloads (message passing + shared memory)
  - 5 memory technologies (SRAM, DRAM, STT-MRAM, PCM, ReRAM)
  - 7 PE counts (1, 2, 4, 8, 16, 32, 64)
  - 2 architectures (Baseline H-tree, LIBCom)
- **Status**: Production-ready, zero failures

### 1,000 Configuration Test Suite
- **Tests**: 1,000 configurations
- **Success Rate**: 100.0% (after reduction_shared binary fix)
- **Bug Fixed**: Missing reduction_shared_pimid binary
- **Coverage**: All workloads, all memory techs, realistic PE counts

## LIBCom Experiments

LIBCom (Low-latency Interconnect for Banked Compute-in-Memory) experiments are located in:

**DAC26/** directory (separate from test/ to preserve research artifact structure):
- **DAC26/configs/**: LIBCom configuration files
  - `bank1_32KB_libcom.yaml` - 8 subarrays (32KB)
  - `bank2_64KB_libcom.yaml` - 16 subarrays (64KB)
  - `bank3_128KB_libcom.yaml` - 32 subarrays (128KB)

- **DAC26/README.md**: LIBCom vs H-tree evaluation guide
- **DAC26/workloads_pimid/**: Compiled workload binaries for experiments
- **DAC26/\*ANALYSIS\*.md**: Various analysis reports

### LIBCom Test Coverage
LIBCom configurations are included in comprehensive test suites:
- 10K audit: Tests both Baseline H-tree and LIBCom topologies
- Benchmark tests: Compare performance across architectures
- Memory tech tests: Validate LIBCom with all memory technologies

## Running Tests

### Quick Verification (Recommended First)
```bash
cd /home/user/pimid-dev
./test/scripts/test_quick_execution_models.py
```

### Comprehensive Benchmark Suite
```bash
./test/scripts/run_comprehensive_benchmark_tests.sh
```

### Full 10K Audit (Production Validation)
```bash
./test/scripts/comprehensive_audit_suite_zsim_10000.py
```

### Monitor Running Tests
```bash
./test/scripts/monitor_tests.sh
```

## Test Categories

### By Scope
- **Unit Tests** (`unit/`): Component-level testing
- **Integration Tests** (`integration/`): Multi-component testing
- **Functional Tests** (`functional/`): Feature validation
- **Comprehensive Tests** (scripts/): Full system validation

### By Component
- **Memory Tests** (`memory/`): Memory subsystem validation
- **Workload Tests** (`benchmarks/`, `workloads/`): Application testing
- **Config Tests** (`configs/`): Configuration validation
- **Compatibility Tests** (`compatibility/`): Cross-version testing

### By Execution Model
- **ZSim Tests**: Cycle-accurate execution-driven simulation
- **Event-Driven Tests**: Analytical performance model
- **Hybrid Tests**: Combined execution models

## Workload Coverage

All workloads tested in both programming models:

### Graph & Sparse Workloads
- **BFS** (Breadth-First Search) - Message passing & Shared memory
- **SpMV** (Sparse Matrix-Vector) - Message passing & Shared memory

### Dense Linear Algebra
- **GEMM** (Matrix Multiplication) - Message passing & Shared memory
- **Dot Product** - Message passing & Shared memory

### Reduction Operations
- **Reduction** - Message passing & Shared memory
- **Prefix Sum** - Message passing & Shared memory

### Data Analytics
- **Histogram** - Message passing & Shared memory

### Stencil Computations
- **1D Stencil** - Message passing & Shared memory

## Memory Technology Coverage

All tests validate across:
- **SRAM** - Fastest, 3.4× faster than DRAM
- **DRAM** - Baseline reference
- **ReRAM** - 1.6× faster than DRAM, non-volatile
- **STT-MRAM** - Similar to DRAM, non-volatile
- **PCM** - Highest density, 2.2× slower than DRAM

## PE Scale Coverage

Tests cover realistic PE counts (1 PE per bank or subarray):
- **1-16 PEs**: Bank-level (1 PE per bank, up to 16 banks)
- **64 PEs**: Small subarray-level (4 banks × 16 subarrays)
- **256 PEs**: Medium subarray-level (16 banks × 16 subarrays)
- **1024 PEs**: Large subarray-level (16 banks × 64 subarrays)

## Bug Fixes & Regression Tests

### Fixed Issues
1. **reduction_shared Binary Missing** (`BUGFIX_REDUCTION_SHARED.md`)
   - Impact: 52 tests failing (5.2%)
   - Fix: Compiled missing binary
   - Verification: `retest_reduction_shared.py` - 52/52 passed

### Regression Test Suite
- Re-run verification scripts after each fix
- Automated regression detection in comprehensive suites
- 100% success required for production approval

## Documentation Standards

All test reports follow consistent format:
- **Executive Summary**: Key findings and metrics
- **Test Configuration**: What was tested
- **Results**: Detailed outcomes with pass/fail rates
- **Analysis**: Performance insights and comparisons
- **Issues**: Any failures or warnings
- **Recommendations**: Next steps or improvements

## Test Maintenance

### Adding New Tests
1. Create test script in appropriate category (`unit/`, `integration/`, etc.)
2. Add to master test runner (`run_all_tests.sh`)
3. Document in this README
4. Update relevant test reports

### Updating Test Data
1. Place new test data in `test/data/`
2. Update configuration generators if needed
3. Re-run affected test suites
4. Update reports with new results

### Archiving Old Results
- Keep recent comprehensive results (last 3 runs)
- Archive older results to `test/results/archive/`
- Maintain at least one production-validated result set

## Support & Resources

- **Test Documentation**: See `test/reports/` for detailed guides
- **Configuration Examples**: See `test/configs/` and `test/data/`
- **PIMID User Guide**: `/home/user/pimid-dev/docs/USER_GUIDE.md`
- **Architecture Guide**: `/home/user/pimid-dev/docs/ARCHITECTURE.md`

---

**Last Updated**: 2025-11-23
**Organization Version**: 1.0
**Production Status**: ✅ Validated (10K audit, 100% success)
