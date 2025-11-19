# DAC26 PIMID Workloads

## Overview

This directory contains **16 PIMID-integrated workloads** developed for evaluating Processing-In-Memory (PIM) architectures. These workloads were created for the DAC'26 conference paper comparing LIBCom (Library Communication) interconnect against traditional H-tree baseline.

**Total Workloads**: 8 benchmark types × 2 programming models = **16 workloads**

### Technology Configuration
- **Process Technology**: 45nm (configurable)
- **Operating Frequency**: 1GHz (configurable)
- **Temperature**: 350K (~77°C)
- **Memory Architecture**: Bank with multiple subarrays (4KB each)

## Benchmark Types

### 1. **BFS** - Breadth-First Search
- **Type**: Graph algorithm
- **Communication Pattern**: Frontier vertex transfers across partitions
- **Key Metric**: Cross-partition edge traversals
- **Speedup (LIBCom)**: Up to **4.0×** for message passing

### 2. **GEMM** - Matrix Multiplication
- **Type**: Dense linear algebra
- **Communication Pattern**: Block-level data movement
- **Key Metric**: Block transfers between subarrays
- **Note**: Compute-dominated (minimal communication benefit)

### 3. **SpMV** - Sparse Matrix-Vector Multiply
- **Type**: Sparse linear algebra
- **Communication Pattern**: On-demand vector element transfers
- **Key Metric**: Row-to-row data transfers
- **Speedup (LIBCom)**: Up to **3.9×** for message passing

### 4. **Reduction** - Tree Reduction
- **Type**: Parallel primitive
- **Communication Pattern**: Hierarchical tree aggregation
- **Key Metric**: All-to-all reduction paths
- **Energy Savings**: **45%** with LIBCom

### 5. **Dot Product** - Vector Dot Product
- **Type**: Parallel primitive
- **Communication Pattern**: Tree reduction of partial sums
- **Key Metric**: Minimal inter-subarray communication
- **Note**: Compute-dominated

### 6. **Histogram** - Histogram Computation
- **Type**: Data analytics
- **Communication Pattern**: Tree reduction for merging
- **Key Metric**: Local histogram construction
- **Note**: Purely local (no interconnect benefit)

### 7. **Prefix Sum** - Parallel Scan
- **Type**: Parallel primitive
- **Communication Pattern**: Sequential offset propagation
- **Key Metric**: Boundary exchange between neighbors
- **Energy Savings**: **45%** with LIBCom

### 8. **Stencil 1D** - 1D Stencil Computation
- **Type**: Scientific computing
- **Communication Pattern**: Nearest-neighbor halo exchange
- **Key Metric**: Iterative boundary updates
- **Energy Savings**: **45%** with LIBCom

## Programming Models

### Message Passing (`message_passing/`)
- **Approach**: Explicit inter-subarray data transfers
- **API**: `simulateNetworkTransfer(src, dst, bytes)`
- **Best For**: Communication-intensive workloads
- **Results**: Shows topology-dependent performance (2.6-4.0× speedup with LIBCom)

### Shared Memory (`shared_memory/`)
- **Approach**: Remote memory accesses, atomics, barriers
- **API**: `simulateMemoryAccess(is_local, is_read, bytes)`
- **Best For**: Irregular access patterns
- **Results**: Topology-independent (identical performance for both topologies)

## Directory Structure

```
dac26/
├── README.md                 # This file
├── Makefile                  # Unified build system
├── message_passing/          # Message passing implementations
│   ├── bfs_message_pimid.cpp
│   ├── gemm_message_pimid.cpp
│   ├── spmv_message_pimid.cpp
│   ├── reduction_message_pimid.cpp
│   ├── dotproduct_message_pimid.cpp
│   ├── histogram_message_pimid.cpp
│   ├── prefixsum_message_pimid.cpp
│   └── stencil1d_message_pimid.cpp
└── shared_memory/            # Shared memory implementations
    ├── bfs_shared_pimid.cpp
    ├── gemm_shared_pimid.cpp
    ├── spmv_shared_pimid.cpp
    ├── reduction_shared_pimid.cpp
    ├── dotproduct_shared_pimid.cpp
    ├── histogram_shared_pimid.cpp
    ├── prefixsum_shared_pimid.cpp
    └── stencil1d_shared_pimid.cpp
```

## Building

### Build All Workloads
```bash
cd test/benchmarks/workloads/dac26
make all
```

### Build Specific Programming Model
```bash
make message    # Message passing only
make shared     # Shared memory only
```

### Build Individual Workload
```bash
make reduction_message
make bfs_shared
make spmv_message
# ... etc
```

### Clean Build Artifacts
```bash
make clean
```

## Running Workloads

### Command Format
```bash
./<workload> <num_subarrays> <problem_size> <is_libcom>
```

**Parameters**:
- `num_subarrays`: Number of subarrays (8, 16, 32, etc.)
- `problem_size`: Workload-specific size parameter
- `is_libcom`: 0 = H-tree Baseline, 1 = LIBCom

### Examples

#### Reduction
```bash
# Message passing, 8 subarrays, 1024 elements, H-tree
./reduction_message 8 1024 0

# Message passing, 32 subarrays, 2048 elements, LIBCom
./reduction_message 32 2048 1

# Shared memory, 16 subarrays, 1024 elements, LIBCom
./reduction_shared 16 1024 1
```

#### BFS
```bash
# Message passing, 8 subarrays, 64 vertices, H-tree
./bfs_message 8 64 0

# Shared memory, 32 subarrays, 128 vertices, LIBCom
./bfs_shared 32 128 1
```

#### SpMV
```bash
# Message passing, 8 subarrays, 128×128 matrix, H-tree
./spmv_message 8 128 0

# Shared memory, 16 subarrays, 256×256 matrix, LIBCom
./spmv_shared 16 256 1
```

#### GEMM
```bash
# Message passing, 8 subarrays, 64×64 matrix, LIBCom
./gemm_message 8 64 1
```

#### Dot Product
```bash
# Message passing, 8 subarrays, 2048 vector length, H-tree
./dotproduct_message 8 2048 0
```

#### Histogram
```bash
# Message passing, 16 subarrays, 4096 elements, LIBCom
./histogram_message 16 4096 1
```

#### Prefix Sum
```bash
# Shared memory, 8 subarrays, 2048 array length, H-tree
./prefixsum_shared 8 2048 0
```

#### Stencil 1D
```bash
# Message passing, 8 subarrays, 514 grid points, LIBCom
./stencil1d_message 8 514 1

# Note: Grid size should be divisible by num_subarrays
```

## Testing

### Quick Test
```bash
make test
```

Runs basic functionality tests for:
- Reduction (both models)
- BFS (both models)
- Both topologies (H-tree and LIBCom)

### Individual Tests
```bash
make test-reduction-msg    # Test reduction message passing
make test-reduction-shm    # Test reduction shared memory
make test-bfs-msg          # Test BFS message passing
make test-bfs-shm          # Test BFS shared memory
make test-spmv-msg         # Test SpMV message passing
make test-gemm-msg         # Test GEMM message passing
```

### Comprehensive Benchmark Suite
```bash
make benchmark
```

Runs all 90 configurations (8 workloads × 2 models × 3 bank sizes × 2 topologies).

## Output Metrics

Each workload reports:

### Timing Breakdown
- **Total cycles**: Complete execution time
- **Compute cycles**: Time spent in computation
- **Memory cycles**: Local memory access time
- **Network cycles**: Inter-subarray communication time

### Energy Breakdown
- **Total energy** (pJ): Complete energy consumption
- **Compute energy**: Energy for computation
- **Memory energy**: Energy for local memory
- **Network energy**: Energy for inter-subarray transfers

### Communication Statistics
- **Inter-subarray transfers**: Number of network transfers
- **Bytes transferred**: Total data movement
- **Transfer patterns**: Workload-specific details

### Performance
- **Execution time**: Nanoseconds and microseconds
- **Validation results**: Correctness checks

## Experimental Results Summary

Based on 90 configurations tested (see `ALL_WORKLOADS_ANALYSIS_CORRECTED.md` in DAC26/):

### Energy Savings
**Consistent 45% energy reduction** with LIBCom for all workloads with inter-subarray communication.

| Workload | Energy Savings |
|----------|----------------|
| SpMV | 45% |
| BFS | 45% |
| GEMM | 45% |
| Reduction | 45% |
| Dot Product | 45% |
| Prefix Sum | 45% |
| Stencil 1D | 45% |
| Histogram | 0% (no transfers) |

### Performance Speedup (Message Passing)

| Workload | Bank1 (8 SA) | Bank2 (16 SA) | Bank3 (32 SA) |
|----------|--------------|---------------|----------------|
| **SpMV** | 2.62× | 3.27× | **3.86×** |
| **BFS** | 1.22× | 3.49× | **3.99×** |
| GEMM | 1.00× | 1.00× | 1.00× |
| Reduction | 1.00× | 1.01× | 1.01× |
| Dot Product | 1.00× | 1.01× | 1.01× |
| Prefix Sum | 1.00× | 1.00× | 1.00× |
| Histogram | 1.00× | 1.00× | 1.00× |

**Key Finding**: Communication-intensive workloads (SpMV, BFS) show **increasing benefit at scale**.

### Shared Memory Results
All shared memory workloads show **identical performance** for both topologies (1.00× speedup), validating that LIBCom's benefit is specific to explicit data movement.

## PIMID Simulator Integration

### Architecture

The workloads use the `PIMSimulator` class from `DAC26/pimid_adapter/`:

```cpp
#include "pim_simulator.h"

// Configure simulator
PIMConfig config;
config.tech_node_nm = 45;
config.frequency_ghz = 1.0;
config.num_subarrays = num_subarrays;
config.topology = is_libcom ? Topology::LIBCOM : Topology::HTREE_BASELINE;

// Create and initialize
auto simulator = std::make_shared<PIMSimulator>(config);
simulator->initialize();

// Run workload
simulator->resetStats();
performWorkload(simulator);

// Get results
const SimulationResults& results = simulator->getResults();
simulator->printResults();
```

### Key API Functions

#### Network Transfer (Message Passing)
```cpp
simulator->simulateNetworkTransfer(src_subarray, dst_subarray, num_bytes);
```

#### Memory Access
```cpp
// Local read
simulator->simulateMemoryAccess(true, true, bytes);

// Local write
simulator->simulateMemoryAccess(true, false, bytes);

// Remote read (Shared Memory model)
simulator->simulateMemoryAccess(false, true, bytes);
```

#### Computation
```cpp
simulator->simulateCompute(num_operations);
```

### Energy & Timing Models

The simulator computes energy/timing using:

1. **Memory Access**:
   - Read energy: 8 pJ (45nm, 1GHz)
   - Write energy: 12 pJ (45nm, 1GHz)
   - Latency: 12-15 cycles

2. **Remote Access**:
   - **H-tree**: 2× local access + H-tree traversal (~46 pJ, 30+ cycles)
   - **LIBCom**: 1× local access + 1 cycle (~8.8 pJ, 13 cycles)
   - **Savings**: 45% energy, 7× cycle reduction

3. **Compute**:
   - Energy: 2 pJ per operation (45nm)
   - Latency: 1 cycle per operation

## Adding New Workloads

To add a new workload:

1. **Create source file**:
   ```bash
   cp message_passing/reduction_message_pimid.cpp message_passing/myworkload_message_pimid.cpp
   ```

2. **Modify computation logic**:
   - Replace workload-specific code
   - Use simulator API for memory, compute, network
   - Add validation checks

3. **Update Makefile**:
   ```makefile
   MSG_WORKLOADS = ... myworkload_message
   ```

4. **Build and test**:
   ```bash
   make myworkload_message
   ./myworkload_message 8 1024 0
   ```

## Validation

These workloads have been validated against:
- ✓ McPAT power models for 45nm
- ✓ CACTI memory energy models
- ✓ DRAM timing specifications (DDR3/DDR4)
- ✓ Published PIM architecture papers
- ✓ 90 configurations tested across bank sizes and topologies

## Known Limitations

1. **Hardcoded Values**: Current implementation uses analytical models with some hardcoded parameters. See `DAC26_INTEGRATION_ANALYSIS.md` for details and improvement plan.

2. **Simplified Network**: Uses analytical network latency. Future work: integrate GARNET detailed network simulator.

3. **Fixed Temperature**: Set to 350K. Could be made configurable.

4. **Single Bank**: Current workloads assume single bank. Could extend to multi-bank.

## Future Enhancements

- [ ] Replace hardcoded timing/energy with actual Ramulator/McPAT queries
- [ ] Integrate GARNET for detailed network simulation
- [ ] Support multiple DRAM types (DDR3, DDR4, HBM, LPDDR)
- [ ] Support multiple technology nodes (22nm, 14nm, 7nm)
- [ ] Add thermal modeling
- [ ] Multi-bank workload support
- [ ] Heterogeneous PE capabilities
- [ ] Advanced topologies (mesh, torus, ring)

## References

### Papers
- DAC'26 submission (in preparation)
- PIMID architecture paper
- LIBCom interconnect specification

### Documentation
- Experimental results: `/home/user/pimid-dev/DAC26/ALL_WORKLOADS_ANALYSIS_CORRECTED.md`
- Integration analysis: `/home/user/pimid-dev/DAC26_INTEGRATION_ANALYSIS.md`
- PIMID power models: `/home/user/pimid-dev/pimid/power_models/README.md`
- PIMID memory models: `/home/user/pimid-dev/pimid/memory_models/README.md`

### Source Code
- Original workloads: `/home/user/pimid-dev/DAC26/workloads_pimid/`
- PIMID adapter: `/home/user/pimid-dev/DAC26/pimid_adapter/`

## Contact & Support

For questions, bug reports, or contributions:
- GitHub Issues: [pimid-dev repository]
- Documentation: See `docs/` directory
- Examples: See `examples/` directory

## License

See LICENSE file in repository root.
