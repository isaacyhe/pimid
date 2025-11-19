# Message Passing PIMID Workloads

This directory contains PIMID-integrated versions of all 8 DAC26 message passing workloads.

## Overview

All message passing workloads have been integrated with the PIMID simulator for accurate energy and timing modeling at 45nm technology node and 1GHz operating frequency.

## Created Workloads

### 1. **reduction_message_pimid.cpp**
- **Description**: Tree reduction with hierarchical message passing
- **Communication Pattern**: Binary tree reduction across subarrays
- **Key Features**:
  - Explicit data transfers between subarrays using `simulateNetworkTransfer()`
  - Each level halves active subarrays
  - Local reduction computation with `simulateCompute()`

### 2. **dotproduct_message_pimid.cpp**
- **Description**: Vector dot product with distributed data
- **Communication Pattern**: Tree reduction of partial sums
- **Key Features**:
  - Local MAC operations on assigned vector elements
  - Tree reduction for combining partial sums
  - Minimal inter-subarray communication

### 3. **histogram_message_pimid.cpp**
- **Description**: Histogram computation with local bins
- **Communication Pattern**: Tree reduction for merging histograms
- **Key Features**:
  - Local histogram construction (no synchronization)
  - Transfer entire histograms between subarrays
  - Element-wise addition during merge

### 4. **prefixsum_message_pimid.cpp**
- **Description**: Segmented scan with boundary exchange
- **Communication Pattern**: Sequential offset propagation
- **Key Features**:
  - Local prefix sum computation
  - Sequential offset transfer between neighbors
  - Adjustment phase with propagated offsets

### 5. **stencil1d_message_pimid.cpp**
- **Description**: 1D stencil computation with halo exchange
- **Communication Pattern**: Nearest-neighbor halo exchange
- **Key Features**:
  - Explicit halo cell exchange each iteration
  - 3-point stencil computation
  - Demonstrates iterative nearest-neighbor communication

### 6. **spmv_message_pimid.cpp**
- **Description**: Sparse matrix-vector multiplication
- **Communication Pattern**: On-demand vector element transfers
- **Key Features**:
  - Distributed sparse matrix rows
  - Remote vector element transfers as needed
  - Partial sum transfers for reduction

### 7. **gemm_message_pimid.cpp**
- **Description**: Matrix multiplication with blocked distribution
- **Communication Pattern**: Block-level data movement
- **Key Features**:
  - Round-robin block distribution
  - Transfer A and B blocks to C's subarray
  - Block-level MAC operations

### 8. **bfs_message_pimid.cpp**
- **Description**: Breadth-first search on distributed graph
- **Communication Pattern**: Frontier vertex transfers
- **Key Features**:
  - Distributed vertex ownership
  - Cross-partition edge handling
  - Frontier queue transfers between subarrays

## Key PIMID Integration Patterns

All message passing workloads follow these integration patterns:

### Network Transfer
```cpp
// Transfer data between subarrays
simulator->simulateNetworkTransfer(src_subarray, dst_subarray, num_bytes);
```

### Local Memory Access
```cpp
// Local read
simulator->simulateMemoryAccess(true, true, bytes);
// Local write
simulator->simulateMemoryAccess(true, false, bytes);
```

### Computation
```cpp
// Simulate compute operations
simulator->simulateCompute(num_ops);
```

### Simulation Flow
```cpp
// 1. Initialize simulator with configuration
PIMConfig pim_config;
pim_config.tech_node_nm = 45;
pim_config.frequency_ghz = 1.0;
pim_config.num_subarrays = num_subarrays;
pim_config.topology = topology;  // HTREE_BASELINE or LIBCOM

simulator = std::make_shared<PIMSimulator>(pim_config);
simulator->initialize();

// 2. Reset stats and execute workload
simulator->resetStats();
performWorkload();

// 3. Get results
const SimulationResults& results = simulator->getResults();
```

## Building the Workloads

### Build All Workloads
```bash
cd /home/user/pimid-dev/DAC26/workloads_pimid
make
```

### Build Individual Workloads
```bash
make reduction_message_pimid
make dotproduct_message_pimid
make histogram_message_pimid
make prefixsum_message_pimid
make stencil1d_message_pimid
make spmv_message_pimid
make gemm_message_pimid
make bfs_message_pimid
```

### Clean Build Artifacts
```bash
make clean
```

## Running the Workloads

All workloads accept the same command-line format:

```bash
./<workload>_message_pimid <num_subarrays> <problem_size> <is_libcom>
```

Where:
- `num_subarrays`: Number of subarrays (8, 16, 32, etc.)
- `problem_size`: Problem-specific size parameter
- `is_libcom`: 0 for Baseline H-tree, 1 for LIBCom

### Examples

```bash
# Reduction: 32 subarrays, 1024 elements per subarray, Baseline
./reduction_message_pimid 32 1024 0

# Dot Product: 8 subarrays, 2048 vector length, LIBCom
./dotproduct_message_pimid 8 2048 1

# Histogram: 16 subarrays, 4096 elements, Baseline
./histogram_message_pimid 16 4096 0

# Prefix Sum: 8 subarrays, 2048 array length, LIBCom
./prefixsum_message_pimid 8 2048 1

# Stencil: 8 subarrays, 514 grid points, 100 iterations, Baseline
./stencil1d_message_pimid 8 514 100 0

# SpMV: 8 subarrays, 128x128 matrix, LIBCom
./spmv_message_pimid 8 128 1

# GEMM: 8 subarrays, 512x512 matrix, Baseline
./gemm_message_pimid 8 512 0

# BFS: 8 subarrays, 64 vertices, LIBCom
./bfs_message_pimid 8 64 1
```

## Output Metrics

Each workload reports:

### Timing Breakdown
- **Total cycles**: Complete execution cycles
- **Compute cycles**: Time spent in computation
- **Memory cycles**: Local memory access time
- **Network cycles**: Inter-subarray communication time

### Energy Breakdown (PIMID-based)
- **Total energy**: Complete energy consumption (pJ)
- **Compute energy**: Energy for computation operations
- **Memory energy**: Energy for local memory accesses
- **Network energy**: Energy for inter-subarray transfers

### Communication Statistics
- **Inter-subarray transfers**: Number of network transfers
- **Bytes transferred**: Total data movement
- **Transfer patterns**: Workload-specific communication details

### Performance
- **Execution time**: Nanoseconds and microseconds
- **Validation results**: Correctness checks

## Comparison with Shared Memory Versions

Both programming models are available:

| Workload | Shared Memory | Message Passing |
|----------|---------------|-----------------|
| Reduction | `reduction_shared_pimid` | `reduction_message_pimid` |
| Dot Product | `dotproduct_shared_pimid` | `dotproduct_message_pimid` |
| Histogram | `histogram_shared_pimid` | `histogram_message_pimid` |
| Prefix Sum | `prefixsum_shared_pimid` | `prefixsum_message_pimid` |
| Stencil 1D | `stencil1d_shared_pimid` | `stencil1d_message_pimid` |
| SpMV | `spmv_shared_pimid` | `spmv_message_pimid` |
| GEMM | `gemm_shared_pimid` | `gemm_message_pimid` |
| BFS | `bfs_shared_pimid` | `bfs_message_pimid` |

**Key Differences:**
- **Shared Memory**: Uses remote memory accesses, atomic operations, barriers
- **Message Passing**: Uses explicit data transfers, no shared state

## Testing

Run test suite for both models:
```bash
make test
```

This runs sample workloads for both shared memory and message passing models with Baseline and LIBCom configurations.

## Technology Parameters

- **Technology Node**: 45nm
- **Operating Frequency**: 1GHz
- **Temperature**: 350K
- **Topologies**:
  - Baseline H-tree (traditional bank-level routing)
  - LIBCom (direct subarray-to-subarray interconnect)

## Files Created

All files are located in `/home/user/pimid-dev/DAC26/workloads_pimid/`:

```
bfs_message_pimid.cpp          (11 KB)
dotproduct_message_pimid.cpp   (9.3 KB)
gemm_message_pimid.cpp         (8.5 KB)
histogram_message_pimid.cpp    (10 KB)
prefixsum_message_pimid.cpp    (11 KB)
reduction_message_pimid.cpp    (9.0 KB)
spmv_message_pimid.cpp         (9.8 KB)
stencil1d_message_pimid.cpp    (11 KB)
```

## Integration Status

✅ All 8 message passing workloads created
✅ PIMID simulator integration complete
✅ Makefile updated with build rules
✅ Test targets added
✅ Help documentation updated

## Next Steps

1. **Build and Test**: Compile all workloads and verify functionality
2. **Run Experiments**: Compare Baseline vs LIBCom performance
3. **Analyze Results**: Study energy and timing breakdowns
4. **Compare Models**: Evaluate shared memory vs message passing trade-offs

## References

- Original message passing workloads: `/home/user/pimid-dev/DAC26/workloads/`
- PIMID adapter: `/home/user/pimid-dev/DAC26/pimid_adapter/`
- Integration guide: `/home/user/pimid-dev/DAC26/PIMID_INTEGRATION_README.md`
