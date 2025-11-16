# MPICH Workloads Summary

This directory contains MPICH-based workloads for PIMID benchmarking, with both device-side (PIM-annotated) and host-only baseline versions for performance comparison.

## Created Workloads

### BFS (Breadth-First Search)
- **Location:** `workloads/bfs/`
- **Algorithm:** Parallel graph traversal using CSR format
- **Versions:**
  - Device-side: `device/bfs_pim.c` (with PIM annotations)
  - Host baseline: `host/bfs_baseline.c` (pure host execution)

## Directory Structure

```
workloads/
├── WORKLOADS_SUMMARY.md    # This file
└── bfs/
    ├── README.md            # BFS workload documentation
    ├── Makefile             # Top-level build system
    ├── verify_build.sh      # Build verification script
    ├── device/              # Device-side version
    │   ├── bfs_pim.c
    │   └── Makefile
    └── host/                # Host-only baseline
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
