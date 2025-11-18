# DAC'26 LIBCom Evaluation - Quick Start Guide

## Overview
This directory contains a complete simulation infrastructure for evaluating LIBCom (direct interconnect) vs Baseline H-tree for in-memory communication.

## Directory Structure
```
DAC26/
├── configs/          # Configuration files for each bank size × interconnect
├── workloads/        # Workload implementations
├── scripts/          # Build and execution scripts
├── results/          # Benchmark results (generated)
├── analysis/         # Analysis tools (future)
├── README.md         # Full specifications
└── QUICKSTART.md     # This file
```

## Quick Start

### 1. Build All Workloads
```bash
cd DAC26/scripts
./build_workloads.sh
```

This compiles all 4 workloads:
- **gemm_workload**: Matrix multiplication
- **bfs_workload**: Graph traversal (BFS)
- **spmv_workload**: Sparse matrix-vector multiply
- **reduction_workload**: Tree reduction

### 2. Run Complete Benchmark Suite
```bash
./run_all.sh
```

This runs all workloads with all bank configurations (8, 16, 32 subarrays) for both baseline and LIBCom, saving results to `results/benchmark_TIMESTAMP.txt`.

### 3. Run Individual Workloads

#### GEMM (Matrix Multiplication)
```bash
cd ../workloads/build

# Baseline: 8 subarrays, 512×512 matrix
./gemm_workload 8 512 0

# LIBCom: 8 subarrays, 512×512 matrix
./gemm_workload 8 512 1
```

**Usage**: `./gemm_workload <num_subarrays> <matrix_size> <is_libcom>`

#### BFS (Graph Traversal)
```bash
# Baseline: 8 subarrays, 64 vertices
./bfs_workload 8 64 0

# LIBCom: 8 subarrays, 64 vertices
./bfs_workload 8 64 1
```

**Usage**: `./bfs_workload <num_subarrays> <num_vertices> <is_libcom>`

#### SpMV (Sparse Matrix-Vector Multiply)
```bash
# Baseline: 8 subarrays, 128×128 matrix
./spmv_workload 8 128 0

# LIBCom: 8 subarrays, 128×128 matrix
./spmv_workload 8 128 1
```

**Usage**: `./spmv_workload <num_subarrays> <matrix_size> <is_libcom>`

#### Reduction (Tree Reduction)
```bash
# Baseline: 8 subarrays, 1024 elements per subarray
./reduction_workload 8 1024 0

# LIBCom: 8 subarrays, 1024 elements per subarray
./reduction_workload 8 1024 1

# CRITICAL TEST: 32 subarrays (worst-case H-tree bottleneck)
./reduction_workload 32 1024 0   # Baseline
./reduction_workload 32 1024 1   # LIBCom
```

**Usage**: `./reduction_workload <num_subarrays> <elements_per_subarray> <is_libcom>`

## Key Parameters

- **is_libcom**:
  - `0` = Baseline (H-tree interconnect with latency = 2 + log₂(subarrays))
  - `1` = LIBCom (direct interconnect with latency = 1 cycle)

## Bank Configurations

| Bank | Size  | Subarrays | H-tree Latency | LIBCom Latency |
|------|-------|-----------|----------------|----------------|
| 1    | 32KB  | 8         | 5 cycles       | 1 cycle        |
| 2    | 64KB  | 16        | 6 cycles       | 1 cycle        |
| 3    | 128KB | 32        | 7 cycles       | 1 cycle        |

## Expected Results

### Performance Improvements (LIBCom vs Baseline)
- **Transfer Latency**: 5-7× faster (1 cycle vs 5-7 cycles)
- **Energy per Transfer**: 45% reduction (0.55 vs 1.0 relative units)
- **Total Cycles**: Varies by workload communication intensity
  - Low communication (GEMM): ~1% improvement
  - High communication (SpMV): ~60% improvement
  - Critical (32-subarray reduction): 7× faster transfers

### Validation Features
All workloads include built-in validation:
- ✓ Transfer count verification
- ✓ Result correctness checks
- ✓ Energy accounting
- ✓ Detailed metrics breakdown

## Troubleshooting

### Build Errors
```bash
cd DAC26/workloads/build
rm -rf *
cd ..
mkdir build && cd build
cmake ..
cmake --build .
```

### Missing Results Directory
```bash
mkdir -p DAC26/results
```

## Next Steps

1. **Run benchmarks**: `./run_all.sh`
2. **Review results**: `cat ../results/benchmark_*.txt`
3. **Analyze trends**: Compare baseline vs LIBCom across bank sizes
4. **Critical test**: Focus on 32-subarray reduction showing worst-case H-tree bottleneck

## Contact
See main README.md for full specifications and design details.
