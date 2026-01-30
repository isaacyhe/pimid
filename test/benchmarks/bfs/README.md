# BFS Workload for PIMID

This is a Breadth-First Search (BFS) workload implementation that can run:
- **Standalone**: As a regular program
- **With OpenMP**: Parallel execution using OpenMP
- **Under PIMID**: Simulated on configured PIM architecture
- **PIMID + OpenMP**: Parallel PIM simulation

## Building

```bash
# Build standalone version (no dependencies)
make standalone

# Build with OpenMP support
make openmp

# Build with PIMID support
make pimid

# Build with both PIMID and OpenMP
make pimid-openmp
```

## Running

### Standalone
```bash
./bfs --vertices 10000 --degree 16 --root 0
```

### With OpenMP
```bash
./bfs_omp --vertices 100000 --degree 16 --parallel
```

### Under PIMID
```bash
# First, create a PIMID configuration file (config.yaml)
pimid --config config.yaml --workload ./bfs_pimid --vertices 100000 --degree 16
```

## Command Line Options

- `--vertices N`: Number of vertices in the graph (default: 1024)
- `--degree D`: Average degree (edges per vertex) (default: 16)
- `--root R`: Starting vertex for BFS (default: 0)
- `--parallel`: Use OpenMP parallel version (requires OpenMP build)
- `--help`: Show help message

## Graph Generation

The workload generates a random graph with the specified number of vertices and average degree. The graph uses Compressed Sparse Row (CSR) format for efficient storage and traversal.

## BFS Algorithm

Two implementations are provided:

1. **Sequential BFS**: Queue-based traversal
2. **Parallel BFS (OpenMP)**: Level-synchronous parallel traversal with work-sharing

## PIMID Integration

When compiled with `-DPIMID_ENABLED` and linked with PIMID library, the workload:
- Uses PIMID API to mark PIM regions
- Reports simulation statistics (cycles, time, energy)
- Can query PIM configuration at runtime

### PIM Regions

The core BFS traversal is marked as a PIM region:
```cpp
PIMID_BEGIN_PIM_REGION("BFS_Sequential");
// BFS code here
PIMID_END_PIM_REGION();
```

PIMID simulates memory operations within this region using the configured memory technology and PE placement.

## Performance

Example performance on 256K vertices, degree 16:
- Sequential: ~50 M vertices/sec (varies by hardware)
- OpenMP (4 threads): ~150 M vertices/sec
- Under PIMID: depends on memory technology and PE configuration

## Example PIMID Config

```yaml
simulation:
  name: "BFS_Test"

memory:
  technology: "SRAM"
  banks: 4
  subarrays_per_bank: 4

processing_elements:
  type: "in_order_core"
  placement_level: "BANK"
  num_pes: 4

workload:
  binary: "./bfs_pimid_omp"
  arguments: ["--vertices", "256000", "--degree", "16", "--parallel"]
```

## Dependencies

- **C++17 compiler** (g++, clang++)
- **OpenMP** (optional, for parallel version)
- **PIMID library** (optional, for PIM simulation)

## Notes

- Graph is generated randomly with a fixed seed (42) for reproducibility
- BFS visits all reachable vertices from the root
- Memory usage: O(V + E) where V = vertices, E = edges
- For large graphs (> 1M vertices), use the OpenMP version for better performance
