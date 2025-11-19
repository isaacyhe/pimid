# PIMID Workloads Summary

This directory contains workloads for PIMID benchmarking and PIM architecture evaluation.

## Workload Collections

### 1. DAC26 Workloads (`dac26/`)

**16 PIMID-integrated workloads** (8 benchmarks × 2 programming models) with actual energy and timing simulation.

**Benchmarks**:
- BFS (Breadth-First Search) - Graph traversal
- GEMM (Matrix Multiplication) - Dense linear algebra
- SpMV (Sparse Matrix-Vector) - Sparse linear algebra
- Reduction - Tree reduction primitive
- Dot Product - Vector primitive
- Histogram - Data analytics
- Prefix Sum - Parallel scan
- Stencil 1D - Scientific computing

**Programming Models**:
- Message Passing: Explicit inter-subarray transfers
- Shared Memory: Remote memory accesses and atomics

**Key Results**:
- Energy savings: 45% with LIBCom interconnect
- Performance: Up to 4× speedup for communication-intensive workloads
- Validation: 90 configurations tested

See `dac26/README.md` for complete documentation.

### 2. MPICH Examples (`mpich_examples/`)

Basic MPICH workloads for testing distributed PIM execution.

### 3. BFS (Breadth-First Search)
- **Location:** `workloads/bfs/`
- **Algorithm:** Parallel graph traversal using CSR format
- **Versions:**
  - Device-side: `device/bfs_pim.c` (with PIM annotations)
  - Host baseline: `host/bfs_baseline.c` (pure host execution)

## Directory Structure

```
workloads/
├── WORKLOADS_SUMMARY.md    # This file
├── dac26/                  # DAC26 PIMID-integrated workloads (16 total)
│   ├── README.md           # Complete documentation
│   ├── Makefile            # Unified build system
│   ├── message_passing/    # 8 message passing workloads
│   │   ├── bfs_message_pimid.cpp
│   │   ├── gemm_message_pimid.cpp
│   │   ├── spmv_message_pimid.cpp
│   │   ├── reduction_message_pimid.cpp
│   │   ├── dotproduct_message_pimid.cpp
│   │   ├── histogram_message_pimid.cpp
│   │   ├── prefixsum_message_pimid.cpp
│   │   └── stencil1d_message_pimid.cpp
│   └── shared_memory/      # 8 shared memory workloads
│       ├── bfs_shared_pimid.cpp
│       ├── gemm_shared_pimid.cpp
│       ├── spmv_shared_pimid.cpp
│       ├── reduction_shared_pimid.cpp
│       ├── dotproduct_shared_pimid.cpp
│       ├── histogram_shared_pimid.cpp
│       ├── prefixsum_shared_pimid.cpp
│       └── stencil1d_shared_pimid.cpp
├── mpich_examples/         # Basic MPICH workloads
│   ├── vector_dotproduct.c
│   ├── graph_pagerank.c
│   └── matrix_multiply.c
└── bfs/                    # Legacy BFS with PIM annotations
    ├── README.md
    ├── Makefile
    ├── device/             # Device-side version
    │   ├── bfs_pim.c
    │   └── Makefile
    └── host/               # Host-only baseline
        ├── bfs_baseline.c
        └── Makefile
```

## Key Features

### Device-Side Versions
- **PIM Annotations:** `#pragma pim offload begin/end`
- **Toggleable:** `USE_PIM` macro (1 = device, 0 = host)
- **Ready for PIMID:** Compatible with PIMID simulator
- **Offloading:** Computation offloaded to PIM device PEs

### Host-Only Baselines
- **Pure host execution:** No PIM annotations
- **Identical algorithms:** Fair performance comparison
- **Benchmarking:** Reference for speedup calculations

## Building

### Prerequisites
- MPICH 3.0 or higher
- GCC with C99 support

### Build commands
```bash
cd workloads/bfs
make all           # Build both versions
make run-compare   # Run both for comparison
```

### Verification
```bash
cd workloads/bfs
./verify_build.sh  # Checks MPICH and builds both versions
```

## Running

### Quick test (4 processes, 1000 nodes)
```bash
cd workloads/bfs
make run-device    # Device version
make run-host      # Host baseline
make run-compare   # Both versions
```

### Custom configuration
```bash
# Device version
cd workloads/bfs/device
mpirun -np 8 ./bfs_pim 10000

# Host baseline
cd workloads/bfs/host
mpirun -np 8 ./bfs_baseline 10000
```

## PIMID Integration

### Using device-side version with PIMID simulator
```bash
pimid-gcc -o bfs_pim bfs_pim.c
pimid-sim --config pim.cfg mpirun -np 4 ./bfs_pim 10000
```

### How PIM annotations work
- **Standard compilation:** Annotations are ignored (host-only execution)
- **PIMID compilation:** Annotations recognized, code offloaded to device
- **Host-device split:** Separate ZSim instances communicate via sockets

## Performance Metrics

Both versions report:
- BFS traversal levels
- Nodes visited count
- Execution time (seconds)
- Throughput (million edges/sec)

## Comparison Benchmarking

To compare device vs host performance:

1. Build both versions: `make all`
2. Run comparison: `make run-compare`
3. Analyze output metrics:
   - Execution time difference
   - Throughput comparison
   - Scalability analysis

## Design Decisions

### Why Two Versions?

1. **Fair comparison:** Identical algorithms, only PIM annotations differ
2. **Baseline reference:** Host-only version provides speedup baseline
3. **Debugging:** Host version easier to debug and profile
4. **Portability:** Host version runs anywhere with MPICH

### Graph Format

- **CSR (Compressed Sparse Row):** Memory-efficient for sparse graphs
- **MPI-friendly:** Easy to partition across processes
- **PIM-optimized:** Sequential memory access patterns

### MPI Design

- **Node-based partitioning:** Each process owns subset of nodes
- **Frontier synchronization:** MPI_Allreduce for global frontier
- **Visited array exchange:** MPI_Allreduce with logical OR

## Future Work

Planned additional workloads:
- Matrix multiplication (dense/sparse)
- Heat diffusion (stencil computation)
- Jacobi iteration (iterative solver)
- SpMV (Sparse Matrix-Vector multiplication)

Each with device and host versions for comparison.

## Requirements

### Minimum
- MPICH 3.0+
- GCC 4.9+ (C99)
- 4+ CPU cores for meaningful parallelism

### Recommended
- MPICH 3.3+
- GCC 7.0+
- PIMID simulator for device-side execution

## Troubleshooting

### mpicc not found
Install MPICH:
- Ubuntu/Debian: `sudo apt-get install mpich`
- RHEL/CentOS: `sudo yum install mpich mpich-devel`
- macOS: `brew install mpich`

### Build errors
Run verification script:
```bash
cd workloads/bfs
./verify_build.sh
```

### Runtime errors
Check MPI installation:
```bash
mpicc --version
mpirun --version
```
