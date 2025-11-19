# PIMID Workloads Summary

This directory contains workloads for PIMID benchmarking and PIM architecture evaluation.

## Available Workloads

### BFS - Breadth-First Search
- **Location:** `bfs/`
- **Type:** Graph traversal algorithm
- **Versions:** Device-side PIM, Host baseline
- **Format:** CSR (Compressed Sparse Row)

### GEMM - Matrix Multiplication
- **Location:** `gemm/`
- **Type:** Dense linear algebra
- **Versions:** Message passing, Shared memory
- **Format:** Block-based matrix multiplication

### Reduction - Tree Reduction
- **Location:** `reduction/`
- **Type:** Parallel primitive
- **Versions:** Message passing, Shared memory
- **Pattern:** Hierarchical tree reduction

### SpMV - Sparse Matrix-Vector Multiply
- **Location:** `spmv/`
- **Type:** Sparse linear algebra
- **Versions:** Message passing, Shared memory
- **Format:** CSR sparse matrices

### Dot Product
- **Location:** `dotproduct/`
- **Type:** Vector primitive
- **Versions:** Message passing, Shared memory
- **Pattern:** Parallel reduction

### Histogram
- **Location:** `histogram/`
- **Type:** Data analytics
- **Versions:** Message passing, Shared memory
- **Pattern:** Local construction with merge

### Prefix Sum
- **Location:** `prefixsum/`
- **Type:** Parallel scan primitive
- **Versions:** Message passing, Shared memory
- **Pattern:** Hierarchical scan

### Stencil 1D
- **Location:** `stencil1d/`
- **Type:** Scientific computing
- **Versions:** Message passing, Shared memory
- **Pattern:** Nearest-neighbor communication

## Directory Structure

```
workloads/
├── WORKLOADS_SUMMARY.md    # This file
├── bfs/                    # Breadth-First Search
│   ├── README.md
│   ├── Makefile
│   ├── bfs.cpp
│   ├── device/
│   │   ├── bfs_pim.c
│   │   └── Makefile
│   └── host/
│       ├── bfs_baseline.c
│       └── Makefile
├── gemm/                   # Matrix Multiplication
│   ├── README.md
│   ├── Makefile
│   ├── gemm_message.cpp
│   └── gemm_shared.cpp
├── reduction/              # Tree Reduction
│   ├── README.md
│   ├── Makefile
│   ├── reduction_message.cpp
│   └── reduction_shared.cpp
├── spmv/                   # Sparse Matrix-Vector
│   ├── README.md
│   ├── Makefile
│   ├── spmv_message.cpp
│   └── spmv_shared.cpp
├── dotproduct/             # Dot Product
│   ├── README.md
│   ├── Makefile
│   ├── dotproduct_message.cpp
│   └── dotproduct_shared.cpp
├── histogram/              # Histogram
│   ├── README.md
│   ├── Makefile
│   ├── histogram_message.cpp
│   └── histogram_shared.cpp
├── prefixsum/              # Prefix Sum
│   ├── README.md
│   ├── Makefile
│   ├── prefixsum_message.cpp
│   └── prefixsum_shared.cpp
├── stencil1d/              # 1D Stencil
│   ├── README.md
│   ├── Makefile
│   ├── stencil1d_message.cpp
│   └── stencil1d_shared.cpp
└── mpich_examples/         # Basic MPICH examples
    ├── vector_dotproduct.c
    ├── graph_pagerank.c
    └── matrix_multiply.c
```

## Programming Models

### Message Passing
- **Approach:** Explicit inter-subarray data transfers
- **Communication:** Network transfers between subarrays
- **Best For:** Communication-intensive workloads
- **Benefit:** Shows topology-dependent performance (2.6-4.0× speedup with LIBCom)

### Shared Memory
- **Approach:** Remote memory accesses
- **Communication:** Read/write to remote subarray memory
- **Best For:** Irregular access patterns
- **Benefit:** Simpler programming model, topology-independent

## Building Workloads

### Individual Workload
```bash
cd <workload_name>/
make                # Build both versions
make message        # Build message passing only
make shared         # Build shared memory only
```

### Example
```bash
cd reduction/
make                # Builds reduction_message and reduction_shared
```

## Running Workloads

All workloads use the same command-line interface:

```bash
./<workload>_message <num_subarrays> <problem_size> <is_libcom>
./<workload>_shared <num_subarrays> <problem_size> <is_libcom>
```

Parameters:
- `num_subarrays`: Number of PIM subarrays (8, 16, 32, etc.)
- `problem_size`: Workload-specific size parameter
- `is_libcom`: 0 = H-tree baseline, 1 = LIBCom interconnect

### Examples

```bash
# Reduction with H-tree
./reduction_message 8 1024 0

# GEMM with LIBCom
./gemm_shared 16 128 1

# SpMV with H-tree
./spmv_message 8 256 0
```

## Technology Configuration

All workloads use consistent technology parameters:
- **Process Technology:** 45nm (configurable)
- **Operating Frequency:** 1GHz (configurable)
- **Temperature:** 350K (~77°C)
- **Memory:** SRAM-like subarrays (4KB each)

Energy and timing values are computed using:
- **CACTI:** Memory access energy and timing
- **McPAT:** Compute energy estimation
- **Analytical Models:** Network latency and energy

## Key Results

### Energy Savings (LIBCom vs H-tree)
- **Communication-intensive workloads:** Up to 45% energy savings
- **Compute-intensive workloads:** Minimal difference (~1-2%)

### Performance Speedup (LIBCom vs H-tree, Message Passing)
- **BFS:** Up to 4.0× speedup
- **SpMV:** Up to 3.9× speedup
- **Reduction:** Up to 3.2× speedup
- **Prefix Sum:** Up to 2.9× speedup
- **Stencil 1D:** Up to 2.6× speedup

### Shared Memory Model
- **All workloads:** ~1.0× speedup (topology-independent)
- **Reason:** Remote accesses hide interconnect topology

## Prerequisites

- **C++17 compiler** (g++, clang++)
- **Standard libraries** (pthread, math)
- **PIMID infrastructure** (CACTI, McPAT wrappers)

## Clean All Workloads

```bash
# From workloads directory
for dir in gemm reduction spmv dotproduct histogram prefixsum stencil1d; do
    (cd $dir && make clean)
done
```

## Build All Workloads

```bash
# From workloads directory
for dir in gemm reduction spmv dotproduct histogram prefixsum stencil1d; do
    (cd $dir && make)
done
```

## Notes

- Workloads integrate directly with PIMID's analytical models (CACTI, McPAT)
- No hardcoded energy or timing values
- Technology-aware parameter computation
- Values automatically adapt to technology node and frequency

## References

- **CACTI:** Memory access energy and timing
- **McPAT:** Power modeling framework
- **LIBCom:** Library communication interconnect (DAC'26)
