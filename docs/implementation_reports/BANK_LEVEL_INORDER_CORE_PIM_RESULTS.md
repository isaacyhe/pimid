# Bank-Level PIM with In-Order Core PE: Performance Results

## Test Overview

**Test Name:** `test_bank_inorder_bfs_pim`
**Date:** 2025-11-17
**Workload:** Breadth-First Search (BFS)
**Configuration:**
- **Banks:** 4
- **Processing Elements:** 4 (one In-Order Core PE per bank)
- **Subarrays per bank:** 4
- **PE Type:** In-Order Core (5-stage pipeline with branch support)
- **Graph:** 256K vertices, average degree 16

## In-Order Core PE Specifications

- **Fetch/Decode:** 2 ns
- **Execute (Compare):** 1 ns
- **Branch Penalty:** 3 ns (50% misprediction rate)
- **Pipeline:** 5-stage (IF, ID, EX, MEM, WB)
- **Capabilities:** Full control flow support with conditional execution

## Performance Results

### Summary Table

| Technology | Latency (ms) | Throughput (Mv/s) | Energy/Vtx (nJ) | Speedup vs SRAM |
|------------|--------------|-------------------|-----------------|-----------------|
| **SRAM**   | **20.27**    | **12.93**         | **1.17**        | **1.00x** (baseline) |
| **ReRAM**  | 43.54        | 6.02              | 1.19            | 0.47x |
| **DRAM**   | 68.00        | 3.86              | 0.94            | 0.30x |
| **STT-MRAM** | 66.15      | 3.96              | 1.50            | 0.31x |
| **PCM**    | 150.64       | 1.74              | 2.99            | 0.13x |

### Detailed Per-Vertex Timing Breakdown (ns)

| Technology | Read | Transfer | PE Compute | Branch | Write | **Total** |
|------------|------|----------|------------|--------|-------|-----------|
| **SRAM**   | 5.0  | 90.4     | 16.0       | 56.0   | 40.0  | **309.3** |
| **ReRAM**  | 10.0 | 185.5    | 16.0       | 56.0   | 192.0 | **664.4** |
| **DRAM**   | 26.6 | 219.5    | 16.0       | 56.0   | 240.0 | **1037.5** |
| **STT-MRAM** | 14.0 | 240.9  | 16.0       | 56.0   | 400.0 | **1009.3** |
| **PCM**    | 16.0 | 285.4    | 16.0       | 56.0   | 1600.0 | **2298.7** |

### Memory Characteristics

| Technology | Subarray Read (ns) | Subarray Write (ns) | Inner-Bank H-tree (ns) | Write Asymmetry |
|------------|-------------------|---------------------|------------------------|-----------------|
| **SRAM**   | 2.50              | 2.50                | 2.74                   | 1.0x (symmetric) |
| **DRAM**   | 13.32             | 15.00               | 6.65                   | 1.13x |
| **ReRAM**  | 5.00              | 12.00               | 5.62                   | 2.4x |
| **STT-MRAM** | 7.00            | 25.00               | 7.30                   | 3.57x |
| **PCM**    | 8.00              | 100.00              | 8.65                   | 12.5x |

## Key Findings

### 1. SRAM Performance Leadership
- **Fastest overall:** 20.27ms total latency
- **Low latency:** 2.5ns symmetric read/write
- **Efficient H-tree:** 2.74ns inner-bank transfer
- **Balanced:** No write asymmetry bottleneck

### 2. ReRAM is Second Best (47% of SRAM performance)
- **Total latency:** 43.54ms (0.47x speedup vs SRAM)
- **Moderate asymmetry:** 2.4x write/read ratio manageable for BFS
- **Efficient transfers:** 5.62ns H-tree latency
- **Best NVM option:** Outperforms other emerging memories

### 3. DRAM and STT-MRAM Similar Performance (~30% of SRAM)
- **DRAM:** 68.00ms (0.30x speedup)
  - Higher read latency (13.32ns) from DRAM protocol overhead
  - Transfer latency (6.65ns) adds significant overhead
- **STT-MRAM:** 66.15ms (0.31x speedup)
  - 3.57x write asymmetry (25ns writes)
  - Write operations dominate: 400ns per vertex

### 4. PCM Catastrophically Slow (13% of SRAM performance)
- **Total latency:** 150.64ms (0.13x speedup)
- **Write bottleneck dominates:** 1600ns per vertex (70% of total time!)
- **Root cause:** 100ns write latency × 16 neighbors × 2 writes = massive overhead
- **Conclusion:** PCM is unsuitable for write-heavy workloads like BFS

### 5. In-Order Core Branch Overhead
- **Branch overhead:** 56ns per vertex (fetch/decode + branch penalty)
- **PE compute:** 16ns per vertex (comparison operations)
- **Combined overhead:** 72ns per vertex from PE operations
- **Impact:** Makes In-Order Core slower than Simple ALU for this workload
  - Previous test with Simple ALU: SRAM = 15.87ms
  - This test with In-Order Core: SRAM = 20.27ms
  - **28% slowdown** due to branch overhead exceeding conditional execution benefits

## Energy Breakdown

### Total Energy Consumption (μJ)

| Technology | Memory | Transfer | PE Compute | **Total** |
|------------|--------|----------|------------|-----------|
| **SRAM**   | 62.50  | 52.43    | 192.94     | **307.86** |
| **ReRAM**  | 66.52  | 52.43    | 192.94     | **311.89** |
| **DRAM**   | 0.00   | 52.43    | 192.94     | **245.37** |
| **STT-MRAM** | 148.48 | 52.43  | 192.94     | **393.85** |
| **PCM**    | 538.97 | 52.43    | 192.94     | **784.33** |

**Note:** PE energy dominates for fast technologies; memory energy dominates for slow technologies.

### Edge Processing Throughput

| Technology | Edges/sec (M) | Notes |
|------------|---------------|-------|
| **SRAM**   | 206.89        | Highest throughput |
| **ReRAM**  | 96.32         | Good NVM performance |
| **STT-MRAM** | 63.41       | Write asymmetry limits throughput |
| **DRAM**   | 61.68         | Protocol overhead limits throughput |
| **PCM**    | 27.84         | Write latency severely limits throughput |

## Workload Analysis

### BFS Operations per Vertex
- **Reads:** 2 (vertex ID, adjacency ptr) + 16×2 (neighbor ID, visited flag) = **34 reads**
- **Writes:** 16×2 (visited flag, enqueue) × 50% probability = **16 writes**
- **Compares:** 16 (one per neighbor)
- **Branches:** 16 (conditional execution)

### Why PCM Struggles
```
Per-vertex write time = 16 neighbors × 2 writes × 0.5 probability × 100ns
                      = 1600ns (70% of total execution time!)
```

### Why In-Order Core is Slower than Simple ALU
```
Branch overhead per vertex = (2ns fetch/decode + 3ns × 0.5 branch) × 16 neighbors
                            = 56ns

Simple ALU overhead per vertex = 0.3ns compare × 16 neighbors
                                = 4.8ns

Difference = 51.2ns penalty per vertex
```

The In-Order Core's branch penalty (pipeline flush) and instruction fetch overhead exceed any benefits from conditional execution in this BFS implementation.

## Conclusions

### Memory Technology Rankings for Bank-Level BFS PIM
1. **SRAM** - Clear winner (20.27ms)
2. **ReRAM** - Best emerging memory (43.54ms, 0.47x)
3. **DRAM/STT-MRAM** - Moderate performance (~66-68ms, 0.30-0.31x)
4. **PCM** - Unsuitable for write-heavy workloads (150.64ms, 0.13x)

### Design Insights
1. **Write asymmetry is critical** - PCM's 12.5x asymmetry causes catastrophic slowdown
2. **H-tree latency matters** - Inner-bank transfer overhead is 29-40% of per-vertex time
3. **Branch overhead significant** - In-Order Core adds 18% overhead vs Simple ALU for SRAM
4. **ReRAM shows promise** - Balanced read/write characteristics make it best NVM option

### Recommendations
- **For BFS workloads:** Use SRAM or ReRAM for bank-level PIM
- **Avoid PCM** for any write-intensive irregular workloads
- **Consider Simple ALU** over In-Order Core when branch benefits don't outweigh overhead
- **Optimize branch prediction** to reduce 50% misprediction rate in In-Order Core

## Test Execution Details

**Build Command:**
```bash
cd pimid/build
cmake .. && make test_bank_inorder_bfs_pim -j$(nproc)
```

**Run Command:**
```bash
./tests/test_bank_inorder_bfs_pim
```

**Source Files:**
- Test implementation: `pimid/tests/test_bank_inorder_bfs_pim.cpp`
- CMake configuration: `pimid/tests/CMakeLists.txt`

**Branch:** `claude/research-inner-bank-timing-018GLwUE7DZDRRAJp2MDejzf`
