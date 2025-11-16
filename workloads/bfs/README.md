# BFS MPICH Workloads

Parallel Breadth-First Search (BFS) implementations for PIMID benchmarking.

## Directory Structure

```
bfs/
├── device/          # Device-side version with PIM annotations
│   ├── bfs_pim.c
│   └── Makefile
├── host/            # Host-only baseline version
│   ├── bfs_baseline.c
│   └── Makefile
├── Makefile         # Top-level build system
└── README.md        # This file
```

## Versions

### Device-Side (PIM-Annotated)
- **Location:** `device/bfs_pim.c`
- **Features:**
  - Uses `#pragma pim offload begin/end` annotations
  - Toggleable via `USE_PIM` macro (1 = device, 0 = host)
  - Ready for PIMID simulator integration
  - Graph traversal offloaded to PIM device

### Host-Only Baseline
- **Location:** `host/bfs_baseline.c`
- **Features:**
  - Pure host-side execution
  - No PIM annotations
  - Identical algorithm for fair comparison
  - Baseline for performance benchmarking

## Algorithm

Both versions implement parallel BFS using:
- **Graph format:** Compressed Sparse Row (CSR)
- **Parallelization:** MPI for distributed computation
- **Work distribution:** Node-based partitioning
- **Synchronization:** MPI_Allreduce for frontier exchange

## Building

### Build both versions:
```bash
make all
```

### Build specific version:
```bash
make device    # Device-side with PIM
make host      # Host-only baseline
```

### Clean build artifacts:
```bash
make clean
```

## Running

### Run device version:
```bash
cd device
make run         # 1000 nodes, 4 processes
make run-large   # 10000 nodes, 4 processes

# Custom configuration
mpirun -np <num_procs> ./bfs_pim <num_nodes>
```

### Run host baseline:
```bash
cd host
make run         # 1000 nodes, 4 processes
make run-large   # 10000 nodes, 4 processes

# Custom configuration
mpirun -np <num_procs> ./bfs_baseline <num_nodes>
```

### Compare both versions:
```bash
make run-compare
```

## Performance Metrics

Both versions report:
- **BFS levels:** Number of iterations required
- **Nodes visited:** Total reachable nodes from source
- **Execution time:** Wall-clock time in seconds
- **Throughput:** Millions of edges processed per second

## Usage with PIMID Simulator

### Device-Side Integration
The device version is designed for PIMID:

1. **Compile with PIMID:**
   ```bash
   pimid-gcc -o bfs_pim bfs_pim.c
   ```

2. **Run through simulator:**
   ```bash
   pimid-sim --config pim.cfg mpirun -np 4 ./bfs_pim 10000
   ```

3. **PIM annotations recognized:**
   - Code within `PIM_OFFLOAD_START/END` runs on device PEs
   - Host and device communicate via PIMID interface
   - Separate ZSim instances for host and device

### Switching Modes
In `bfs_pim.c`, change:
```c
#define USE_PIM 1  // 1 = PIM device, 0 = Host only
```

Then rebuild:
```bash
make clean && make all
```

## Graph Generation

Both versions generate random graphs with:
- **Average edges per node:** 10
- **Distribution:** Uniform random
- **Format:** CSR (Compressed Sparse Row)
- **Seed:** Time-based + process rank

## MPI Communication

### Collective operations:
- `MPI_Allreduce`: Frontier size gathering
- `MPI_Allreduce`: Visited array synchronization
- `MPI_Reduce`: Final statistics collection
- `MPI_Barrier`: Timing synchronization

## Comparison Benchmarking

To compare performance:

1. **Build both:**
   ```bash
   make all
   ```

2. **Run comparison:**
   ```bash
   make run-compare
   ```

3. **Analyze metrics:**
   - Execution time difference
   - Throughput comparison
   - Scalability (weak/strong scaling)

## Requirements

- **MPICH:** Version 3.0 or higher
- **GCC:** C99 support
- **Optional:** PIMID simulator for device-side execution

## Future Workloads

Planned additions:
- Matrix multiplication
- Heat diffusion (stencil)
- Jacobi iteration
- SpMV (Sparse Matrix-Vector)
