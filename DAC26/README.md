# DAC'26 LIBCom vs H-tree Evaluation

## Overview
Comparative evaluation of LIBCom interconnect vs baseline H-tree for bank-level PIM operations.

## Bank Configurations

| Config | Size | Subarrays | Layout |
|--------|------|-----------|--------|
| Bank 1 | 32KB | 8 | 8 × 4KB subarrays |
| Bank 2 | 64KB | 16 | 16 × 4KB subarrays |
| Bank 3 | 128KB | 32 | 32 × 4KB subarrays |

## Memory Hierarchy

- **Subarray size**: 4KB (1024 × 32-bit words)
- **Word size**: 32 bits
- **Clock frequency**: 500MHz (2ns cycle time)

## Interconnect Comparison

### Baseline H-tree
- **Topology**: Hierarchical tree
- **Latency**: log₂(num_subarrays) cycles
- **Access**: Read + Write through H-tree for inter-subarray transfers
- **Total latency**: 2× subarray access + H-tree latency

### LIBCom
- **Topology**: Direct interconnect with switches
- **Switch latency**: 1 cycle per hop
- **Access**: Direct path between subarrays
- **Total latency**: 1× subarray access
- **Energy savings**:
  - Copy: -45%
  - Move: -47%

## Operations

1. **Read/Write**: Traditional DRAM operations
   - Baseline: Standard subarray access (1 cycle)

2. **Copy**: Inter-subarray data copy
   - Baseline: Read + H-tree + Write (2× subarray + H-tree latency)
   - LIBCom: Direct transfer (1× subarray access)

3. **Move**: Inter-subarray data move
   - Baseline: Read + H-tree + Write (2× subarray + H-tree latency)
   - LIBCom: Direct transfer (1× subarray access)

## Workloads

### 1. GEMM (Matrix Multiplication)
- **Pattern**: Matrix blocks distributed across subarrays
- **Communication**: Block transfers between subarrays
- **Key metric**: Inter-subarray data movement

### 2. BFS (Breadth-First Search)
- **Pattern**: Graph vertices partitioned across subarrays
- **Communication**: Frontier expansion across partitions
- **Key metric**: Cross-partition edge traversals

### 3. SpMV (Sparse Matrix-Vector Multiply)
- **Pattern**: Sparse matrix rows in different subarrays
- **Communication**: Partial sum accumulation
- **Key metric**: Row-to-row data transfers

### 4. Reduction
- **Pattern**: Tree reduction across all subarrays
- **Communication**: Hierarchical aggregation
- **Key metric**: All-to-all reduction paths

## Metrics

1. **Total execution cycles**
2. **Inter-subarray communication count**
3. **H-tree traversals** (baseline only)
4. **Direct transfers** (LIBCom only)
5. **Energy per operation**
   - Baseline: 100%
   - LIBCom copy: 55% (45% reduction)
   - LIBCom move: 53% (47% reduction)

## Critical Test Case

**All-to-all communication in 32-subarray config**:
- Tests worst-case H-tree bottleneck
- Demonstrates LIBCom's advantage in high-connectivity scenarios
- Each subarray communicates with all others (31 × 32 = 992 transfers)

## Directory Structure

```
DAC26/
├── configs/          # Memory configurations for each bank size
├── workloads/        # Benchmark implementations
├── scripts/          # Analysis and plotting scripts
├── results/          # Simulation output data
└── analysis/         # Performance analysis reports
```

## Running Simulations

```bash
# Run all configurations
./scripts/run_all.sh

# Run specific workload
./scripts/run_workload.sh gemm bank1

# Generate results
./scripts/analyze_results.sh
```
