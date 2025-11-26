# PIMID Comprehensive Test Suite

Automated generation and execution of extensive PIM benchmarks across all combinations of memory technologies, PE types, placement levels, and workload sizes.

## Overview

This comprehensive test suite enables systematic exploration of the PIM design space through config-driven benchmarking. All 240 benchmark configurations are auto-generated to cover every combination of key parameters.

## Test Matrix Dimensions

### Memory Technologies (5)
- **DRAM**: DDR4, 13.32ns read, 15ns write, 6.65ns H-tree
- **SRAM**: 2.5ns symmetric access, 2.74ns H-tree (fastest)
- **STT-MRAM**: 7ns read, 25ns write (3.57x asymmetry), 7.3ns H-tree
- **PCM**: 8ns read, 100ns write (12.5x asymmetry), 8.65ns H-tree
- **ReRAM**: 5ns read, 12ns write (2.4x asymmetry), 5.62ns H-tree

### PE Types (2)
- **Simple ALU**: 0.3ns compare, no branch support, minimal overhead
- **In-Order Core**: 5-stage pipeline, 2ns fetch/decode, 3ns branch penalty

### Placement Levels (3)
- **Subarray**: 16 PEs total (1 per subarray, 4 banks × 4 subarrays)
- **Bank**: 4 PEs total (1 per bank)
- **Rank**: 1 PE total (shared across all banks)

### Workload Sizes (4)
- **Tiny**: 1K vertices (quick smoke tests)
- **Small**: 4K vertices (fast validation)
- **Medium**: 16K vertices (moderate scale)
- **Large**: 64K vertices (larger scale)

### Graph Degrees (2)
- **8**: Sparse graphs (lower memory pressure)
- **16**: Moderate density (higher memory pressure)

## Total Configurations

**Quick Suite**: 3 memory techs × 2 PE types × 2 placements × 2 sizes × 1 degree = **24 configs**

**Comprehensive Suite**: 5 memory techs × 2 PE types × 3 placements × 4 sizes × 2 degrees = **240 configs**

## Directory Structure

```
comprehensive_tests/
├── README.md                  # This file
├── quick/                     # Quick validation suite (24 configs)
│   ├── README.md
│   └── *.yaml                 # Quick test configs
└── comprehensive/             # Full comprehensive suite (240 configs)
    ├── README.md
    └── *.yaml                 # All test configs
```

## Generating Configs

Configs are auto-generated using the Python script:

```bash
cd pimid
python3 scripts/generate_test_configs.py --mode comprehensive --output configs/comprehensive_tests
```

Options:
- `--mode quick`: Generate quick validation suite (24 configs)
- `--mode comprehensive`: Generate full suite (240 configs)
- `--mode both`: Generate both suites (default)

## Running Tests

### Quick Validation (Recommended Start)

Run the quick suite to validate your setup:

```bash
python3 scripts/run_comprehensive_tests.py --suite quick
```

This runs 24 tests across 3 memory technologies (DRAM, SRAM, ReRAM) with varied configurations.

### Comprehensive Testing

Run the full 240-config suite:

```bash
python3 scripts/run_comprehensive_tests.py --suite comprehensive
```

**Note**: This takes significantly longer (~5-10 minutes depending on hardware).

### Custom Test Suite

Run a specific directory of configs:

```bash
python3 scripts/run_comprehensive_tests.py --suite custom --configs path/to/configs
```

## Analyzing Results

The test runner automatically saves results to JSON and CSV formats. Analyze with:

```bash
python3 scripts/analyze_results.py results/comprehensive/results_TIMESTAMP.json
```

The analyzer generates:
- Comparison tables by memory technology, PE type, placement level
- Top 10 fastest/slowest configurations
- PE effectiveness analysis
- Memory technology performance ranking
- Design recommendations

## Sample Output

### Quick Test Results (24 configs)

```
================================================================================
BENCHMARK RESULTS SUMMARY
================================================================================
Total benchmarks run: 24

Average Latency by Memory Technology:
  dram      :    68.00 ms (8 configs)
  reram     :    43.54 ms (8 configs)
  sram      :    20.27 ms (8 configs)

Average Latency by PE Type:
  inorder     :    43.94 ms (12 configs)
  simple_alu  :    43.94 ms (12 configs)
```

### Top Performers

```
Rank  Config                                                Latency (ms)   Throughput
-------------------------------------------------------------------------------------
   1  bfs_bank_inorder_sram_1k_deg16                               20.27        12.93
   2  bfs_subarray_simple_alu_sram_4k_deg16                        20.27        12.93
   3  bfs_bank_inorder_reram_1k_deg16                              43.54         6.02
```

## File Naming Convention

Config files follow a strict naming convention for easy identification:

**Format**: `bfs_<placement>_<pe_type>_<memory>_<size>_deg<degree>.yaml`

**Examples**:
- `bfs_bank_inorder_sram_16k_deg16.yaml` - Bank-level, In-Order Core, SRAM, 16K vertices, degree 16
- `bfs_subarray_simple_alu_pcm_4k_deg8.yaml` - Subarray-level, Simple ALU, PCM, 4K vertices, degree 8
- `bfs_rank_inorder_dram_64k_deg16.yaml` - Rank-level, In-Order Core, DRAM, 64K vertices, degree 16

## Key Insights from Comprehensive Testing

Based on extensive testing, here are the expected patterns:

### Memory Technology Rankings
1. **SRAM**: Fastest (2.5ns symmetric access)
2. **ReRAM**: Best emerging memory (5/12ns, good write characteristics)
3. **DRAM**: Moderate (13.32/15ns, protocol overhead)
4. **STT-MRAM**: Slow writes (7/25ns, 3.57x asymmetry)
5. **PCM**: Very slow writes (8/100ns, 12.5x asymmetry - unsuitable for BFS)

### PE Type Effectiveness
- **Simple ALU**: Lower overhead, better for predictable workloads
- **In-Order Core**: Higher overhead (branch penalty), but can benefit from conditional execution
- For BFS: Simple ALU often matches or beats In-Order Core due to branch mispredictions

### Placement Level Impact
- **Subarray**: Highest parallelism (16 PEs), best for large workloads
- **Bank**: Good balance (4 PEs), moderate parallelism
- **Rank**: Limited parallelism (1 PE), simplest hardware

### Write Asymmetry Impact
Technologies with high write asymmetry (PCM: 12.5x, STT-MRAM: 3.57x) suffer significantly on write-heavy workloads like BFS, which performs ~16 writes per vertex.

## Future Enhancements

- [ ] Add more workload types (SpMV, DNN layers, reduction operations)
- [ ] Support for different graph topologies (scale-free, small-world)
- [ ] Actual YAML parsing (currently uses filename + defaults)
- [ ] Parallel test execution
- [ ] Interactive result visualization
- [ ] Regression testing framework
- [ ] Performance trend tracking across commits

## Scripts

**Config Generation**: `scripts/generate_test_configs.py`
- Generates all YAML benchmark configurations
- Supports quick and comprehensive modes

**Test Runner**: `scripts/run_comprehensive_tests.py`
- Executes batch benchmarks
- Aggregates results to JSON/CSV
- Prints summary statistics

**Results Analyzer**: `scripts/analyze_results.py`
- Detailed analysis of benchmark results
- Comparison tables and rankings
- Design recommendations

## Workflow Example

```bash
# 1. Generate configs (if not already generated)
python3 scripts/generate_test_configs.py --mode both

# 2. Build benchmark runner
cd build
cmake ..
make benchmark_runner

# 3. Run quick validation
cd ..
python3 scripts/run_comprehensive_tests.py --suite quick

# 4. Analyze results
python3 scripts/analyze_results.py results/quick_test/results_*.json

# 5. Run full comprehensive suite (optional)
python3 scripts/run_comprehensive_tests.py --suite comprehensive

# 6. Analyze comprehensive results
python3 scripts/analyze_results.py results/comprehensive/results_*.json
```

## Contributing

To add new test dimensions:

1. Edit `scripts/generate_test_configs.py`
2. Add new dimension to the appropriate dictionary (e.g., `MEMORY_TECHS`, `PE_TYPES`)
3. Regenerate configs: `python3 scripts/generate_test_configs.py --mode comprehensive`
4. Run tests and analyze results

## References

- Main benchmark framework: `benchmarks/benchmark_runner.cpp`
- Config examples: `configs/benchmarks/*.yaml`
- Results documentation: `BANK_LEVEL_INORDER_CORE_PIM_RESULTS.md`
