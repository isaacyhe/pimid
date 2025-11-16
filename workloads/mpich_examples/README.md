# MPICH-based Workloads for PIMID

This directory contains example MPICH-based workloads demonstrating both **host-side** and **device-side (PIM)** execution in the PIMID simulator.

## Overview

PIMID supports two primary programming models for workloads:
- **OpenMP**: Shared memory with hardware coherence
- **MPICH**: Message passing for distributed execution

These examples use **MPICH** for message passing between processing elements, which is recommended for PIM-side workloads as documented in the PIMID papers.

## Workloads

### 1. Vector Dot Product (`vector_dotproduct.c`)
**Description**: Distributed vector dot product computation

**Characteristics**:
- Simple memory-intensive operation
- Good for testing basic PIM functionality
- Vector size: 1,000,000 elements
- Demonstrates MPI_Reduce for aggregation

**Key Features**:
- Host-side and PIM-side execution modes
- Configurable via `USE_PIM` macro
- PIM annotations mark compute-intensive regions

### 2. Matrix Multiplication (`matrix_multiply.c`)
**Description**: Parallel dense matrix multiplication (C = A × B)

**Characteristics**:
- Memory-intensive computation
- 512×512 matrices (configurable)
- Row-wise distribution across MPI processes
- Demonstrates benefits of near-memory processing

**Key Features**:
- Performance metrics (GFLOPS)
- Result verification via checksum
- Ideal for evaluating PIM memory bandwidth benefits

### 3. Graph PageRank (`graph_pagerank.c`)
**Description**: PageRank algorithm for graph analytics

**Characteristics**:
- Irregular memory access patterns
- 10,000 vertices with average degree 10
- Synthetic graph generation (CSR format)
- Iterative convergence-based algorithm

**Key Features**:
- Realistic graph analytics workload
- Demonstrates PIM benefits for random memory accesses
- Convergence tracking
- Top-K vertex ranking output

## Building the Workloads

### Prerequisites
```bash
# Install MPICH
sudo apt-get install mpich  # Debian/Ubuntu
# or
sudo yum install mpich      # RHEL/CentOS

# Verify installation
mpicc --version
```

### Build All Workloads
```bash
make all
```

### Build Individual Workloads
```bash
make vector_dotproduct
make matrix_multiply
make graph_pagerank
```

### Clean Build Artifacts
```bash
make clean
```

## Running the Workloads

### Quick Test (4 MPI processes)
```bash
# Vector dot product
mpirun -np 4 ./vector_dotproduct

# Matrix multiplication
mpirun -np 4 ./matrix_multiply

# Graph PageRank
mpirun -np 4 ./graph_pagerank
```

### Using Make Targets
```bash
make run-vector      # Run vector dot product
make run-matrix      # Run matrix multiplication
make run-pagerank    # Run graph PageRank
```

### Custom Process Count
```bash
# Run with 8 MPI processes
mpirun -np 8 ./matrix_multiply

# Run with 16 processes
mpirun -np 16 ./graph_pagerank
```

### Running with PIMID Simulator

**Important**: These workloads are designed for PIMID simulation. To run them with PIMID:

1. Configure PIMID host and device simulation engines
2. Set up the socket-based communication interface
3. Configure PE placement (subarrays, banks, ranks, or logic dies)
4. Run through PIMID's simulation framework

```bash
# Example PIMID simulation command (adjust based on your setup)
# ./pimid-sim --host-config host.cfg --device-config device.cfg \
#             --workload ./matrix_multiply --np 8
```

## Configuration Options

Each workload has configurable parameters at the top of the source file:

### vector_dotproduct.c
```c
#define VECTOR_SIZE 1000000  // Total vector size
#define USE_PIM 1            // 1=PIM-side, 0=Host-side
```

### matrix_multiply.c
```c
#define MATRIX_SIZE 512      // N×N matrix dimensions
#define USE_PIM 1            // 1=PIM-side, 0=Host-side
```

### graph_pagerank.c
```c
#define NUM_VERTICES 10000           // Number of graph vertices
#define AVG_DEGREE 10                // Average vertex degree
#define MAX_ITERATIONS 20            // Maximum iterations
#define DAMPING_FACTOR 0.85          // PageRank damping factor
#define CONVERGENCE_THRESHOLD 0.0001 // Convergence criterion
#define USE_PIM 1                    // 1=PIM-side, 0=Host-side
```

## PIM Annotations

All workloads use PIM pragma annotations to mark code regions for device-side execution:

```c
#define PIM_OFFLOAD_START _Pragma("pim offload begin")
#define PIM_OFFLOAD_END _Pragma("pim offload end")

// Usage:
PIM_OFFLOAD_START;
{
    // Computation to offload to PIM
    for (int i = 0; i < size; i++) {
        result[i] = compute(data[i]);
    }
}
PIM_OFFLOAD_END;
```

This annotation approach is inherited from MultiPIM as described in the PIMID papers.

## Host-side vs PIM-side Execution

### Host-side Execution
- Traditional CPU execution
- Data movement between memory and processor cache
- Limited by memory bandwidth

### PIM-side Execution
- Computation offloaded to processing elements near/in memory
- Reduced data movement overhead
- Benefits memory-intensive workloads with irregular access patterns

### Toggling Execution Mode

Set `USE_PIM` to 0 for host-side, 1 for PIM-side:
```c
#define USE_PIM 1  // PIM-side execution
```

Then rebuild:
```bash
make clean && make all
```

## Expected Output

### Vector Dot Product
```
Using PIM-side computation
===== Vector Dot Product Results =====
Vector size: 1000000
Number of MPI processes: 4
Execution mode: PIM-side
Global dot product result: 123456.789012
Execution time: 0.234567 seconds
======================================
```

### Matrix Multiplication
```
===== Matrix Multiplication (MPICH + PIM) =====
Matrix size: 512 x 512
Number of MPI processes: 4
Rows per process: 128
Execution mode: PIM-side
==============================================

===== Computation Results =====
Execution time: 0.456789 seconds
Performance: 12.34 GFLOPS
Result checksum: 987654.321098
Sample result C[0][0]: 123.456789
Sample result C[511][511]: 234.567890
==============================
```

### Graph PageRank
```
===== Graph PageRank (MPICH + PIM) =====
Number of vertices: 10000
Number of edges: 100000
Average degree: 10
Number of MPI processes: 4
Vertices per process: 2500
Execution mode: PIM-side
========================================
Iteration 0: diff = 0.850000
Iteration 5: diff = 0.234567
Iteration 10: diff = 0.012345
Converged at iteration 14

===== PageRank Results =====
Total iterations: 15
Execution time: 0.789012 seconds
Time per iteration: 0.052601 seconds

Top 5 vertices by PageRank:
  Vertex 4567: 0.012345
  Vertex 1234: 0.010234
  Vertex 7890: 0.009876
  Vertex 3456: 0.008765
  Vertex 6789: 0.007654
============================
```

## Performance Considerations

### Scalability
- Matrix size and vertex count should be divisible by number of MPI processes
- Larger problem sizes benefit more from PIM execution
- Test with varying process counts (powers of 2 recommended)

### Memory Requirements
- **Vector**: ~8MB per process (for 1M elements)
- **Matrix**: ~2MB per process (for 512×512)
- **PageRank**: ~80KB per process (for 10K vertices)

### Communication Patterns
- Vector: All-reduce (sum aggregation)
- Matrix: Gather (result collection)
- PageRank: All-reduce (rank synchronization)

## Troubleshooting

### MPI Not Found
```bash
# Check if mpicc is in PATH
which mpicc

# Add MPICH to PATH if needed
export PATH=/usr/lib64/mpich/bin:$PATH
```

### Compilation Errors
```bash
# Check MPICH version
mpicc --version

# Ensure C99 support
mpicc -std=c99 --version
```

### Runtime Errors
```bash
# Verify process count divides problem size
# For matrix_multiply: MATRIX_SIZE % np == 0
# For graph_pagerank: NUM_VERTICES % np == 0
```

## References

- PIMID Paper: "PIMID: A Flexible and Scalable Co-Simulation Framework for PIM System Research"
- IEEE Computer Architecture Letters (CAL)
- Authors: Yuan He (RIKEN), Masaaki Kondo (Keio University)

## Future Extensions

These workloads can be extended to:
- Support different PE placement strategies (subarray, bank, chip, rank, logic die)
- Add support for different memory technologies (DRAM, SRAM, STT-MRAM)
- Implement more complex graph algorithms (BFS, SSSP, etc.)
- Add machine learning workloads (neural networks, convolutions)
- Integrate with GARNET network configuration for topology experiments

## License

See LICENSE file in the repository root.
