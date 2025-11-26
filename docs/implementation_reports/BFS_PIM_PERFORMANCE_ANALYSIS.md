# BFS PIM Performance Analysis

## Overview

This document analyzes **Breadth-First Search (BFS) graph traversal** performance across all memory technologies with different PIM configurations. BFS is a critical workload for testing PIM systems because it features:

- **Irregular memory access patterns** (not data-parallel)
- **Control flow** (conditional branches)
- **Read-modify-write** operations (update visited flags)
- **Real-world relevance** (social networks, web graphs)

## Test Configurations

### Subarray-Level PIM
- **PE**: Simple ALU per subarray
- **Subarrays per bank**: 4
- **Technologies**: SRAM, STT-MRAM, PCM, ReRAM, **DRAM**

### Bank-Level PIM
- **PE**: Simple ALU + In-Order Core (both tested)
- **Banks**: 4
- **Subarrays per bank**: 4  
- **Technologies**: SRAM, STT-MRAM, PCM, ReRAM, **DRAM**

## BFS Workload

**Graph**: 256K vertices, average degree 16

**Operations per vertex**:
1. Read vertex ID from queue (1 read)
2. Read adjacency list pointer (1 read)
3. For each of 16 neighbors:
   - Read neighbor ID (1 read)
   - Read visited flag (1 read)
   - Compare: is visited? (ALU/Core operation)
   - **Branch**: skip if visited (In-Order Core only!)
   - Write visited flag if not visited (1 write, 50% probability)
   - Enqueue neighbor if not visited (1 write, 50% probability)

**Total per vertex**:
- **34 reads** (2 + 16×2)
- **16 writes** (16×2×0.5, assuming 50% already visited)
- **16 compares**
- **16 branches** (In-Order Core only)

## Why BFS Matters for PIM Testing

### 1. Irregular Memory Access
Unlike vector/matrix operations, BFS has:
- Random access to vertices (cache-unfriendly)
- Unpredictable adjacency list locations
- Tests memory latency, not just bandwidth

**Impact**: Memory bottleneck more visible than in data-parallel workloads

### 2. Control Flow Sensitivity
BFS requires:
- Conditional checks (is vertex visited?)
- Early exit from neighbor processing
- Queue management logic

**Impact**: **In-Order Core has advantage** over Simple ALU despite instruction overhead!

### 3. Read-Modify-Write Patterns
Update visited flags:
- Read current value
- Check state
- Write new value

**Impact**: Write asymmetry matters! PCM penalty is severe.

### 4. Real-World Relevance
BFS is used in:
- Social network analysis (shortest paths)
- Web graph crawling
- Recommendation systems
- Graph databases

**Impact**: Validates PIM for actual applications, not just benchmarks

---

## Expected Performance Results

### Subarray-Level BFS (Simple ALU PE)

| Technology | Latency (ms) | Speedup | Why? |
|-----------|-------------|---------|------|
| **SRAM** | 2-3 | 1.0x | Fastest R/W (2ns), no asymmetry |
| **DRAM** | 4-5 | 0.5x | Moderate (13.32ns read, 15ns write) |
| **ReRAM** | 9-11 | 0.25x | 2.8x write asymmetry |
| **STT-MRAM** | 10-12 | 0.2x | 3.5x write asymmetry (MTJ switching) |
| **PCM** | 60-80 | 0.04x | TERRIBLE! 108ns writes kill performance |

**Key Insights**:
- **SRAM dominates** (2ns symmetric access)
- **DRAM competitive** despite slower access (realistic for main memory PIM)
- **PCM catastrophic** for BFS (16 writes per vertex × 108ns = 1,728ns overhead per vertex!)

### Detailed Timing (SRAM Subarray-Level)

```
Per vertex:
  Read vertex + adjacency:   2 × 2.0ns = 4.0ns
  Process 16 neighbors:
    Read neighbor + visited: 16 × 2 × 2.0ns = 64.0ns
    Compare (ALU):           16 × 0.3ns = 4.8ns
    Write (50% prob):        16 × 2 × 0.5 × 2.0ns = 32.0ns
  ──────────────────────────────────────────
  Total per vertex:          104.8ns

Vertices per subarray: 256K / (8 banks × 4 subarrays) = 8,192
Total latency: 104.8ns × 8,192 = 0.858 ms ≈ 0.9 ms
```

### Detailed Timing (PCM Subarray-Level)

```
Per vertex:
  Read vertex + adjacency:   2 × 8.7ns = 17.4ns
  Process neighbors:
    Read neighbor + visited: 16 × 2 × 8.7ns = 278.4ns
    Compare (ALU):           16 × 0.3ns = 4.8ns
    Write (SLOW!):           16 × 2 × 0.5 × 108ns = 1,728ns ⚠️
  ──────────────────────────────────────────
  Total per vertex:          2,028.6ns

Total latency: 2,028.6ns × 8,192 = 16.6 ms
```

**PCM is 19x slower than SRAM for BFS!**

---

## Bank-Level BFS: ALU vs In-Order Core

### Simple ALU (No Branches)

**Problem**: Cannot skip already-visited vertices
- Must process all neighbors unconditionally
- Wastes read/write operations on visited vertices
- 50% of neighbors already visited → 50% wasted work

**Operations per vertex**:
```
Read vertex:              2 reads
Transfer to bank PE:      inner_bank_latency
For each neighbor:
  Read neighbor:          1 read + inner_bank_transfer
  Read visited:           1 read + inner_bank_transfer  
  Compare (ALU):          0.3ns (fast!)
  Write UNCONDITIONALLY:  2 writes (even if visited!) ← WASTE!
  Transfer back:          inner_bank_latency
```

**Wasted**: 50% of writes are unnecessary

### In-Order Core (With Branches)

**Advantage**: Can branch and skip visited vertices!
- Conditional check: if (visited) continue;
- Saves read/write operations
- 50% of neighbors skipped

**Operations per vertex**:
```
Read vertex:              2 reads
Transfer to bank PE:      inner_bank_latency
For each neighbor:
  Read neighbor:          1 read + transfer
  Read visited:           1 read + transfer
  Fetch instruction:      2.0ns (overhead!)
  Compare (Core):         1.0ns
  Branch:                 3.0ns × 0.5 (50% misprediction) ← PENALTY!
  If NOT visited (50%):
    Write visited:        1 write + transfer
    Enqueue:              1 write + transfer
```

**Saved**: 50% reduction in writes!

### Performance Comparison (SRAM Bank-Level)

**Simple ALU**:
```
Per vertex time:
  Read/transfer:     ~150ns
  Compare:           16 × 0.3ns = 4.8ns
  Write all:         16 × 2 × (transfer + write) = ~300ns
  ──────────────────────────
  Total:             ~455ns

Latency: 455ns × (256K / 4 banks) = 29.1 ms
```

**In-Order Core**:
```
Per vertex time:
  Read/transfer:     ~150ns
  Fetch/decode:      16 × 2.0ns = 32ns (overhead!)
  Compare:           16 × 1.0ns = 16ns
  Branch:            16 × 3.0ns × 0.5 = 24ns (penalty!)
  Write (50% only):  8 × 2 × (transfer + write) = ~150ns ← SAVED!
  ──────────────────────────
  Total:             ~372ns

Latency: 372ns × (256K / 4 banks) = 23.8 ms
```

**In-Order Core is 1.22x FASTER for BFS!**

Despite instruction overhead, saving 50% of writes wins!

---

## Technology Comparison with DRAM

### Why DRAM Inclusion Matters

**SRAM**: Fast but small
- Use case: L1/L2/L3 cache-level PIM
- Graph size: Limited (few MB)

**DRAM**: Slower but large
- Use case: Main memory PIM (realistic!)
- Graph size: GB-scale graphs

### DRAM Performance Characteristics

**Timing**:
- Subarray read: tRCD + tCAS = 13.32ns (DDR4-2400)
- Subarray write: tRCD + tCAS + tWR = 15.0ns
- Inner-bank: H-tree + Global I/O = 6.65ns

**BFS Impact**:
```
SRAM per vertex:  ~105ns
DRAM per vertex:  ~500ns (5x slower)
```

**But**: DRAM enables **1000x larger graphs**!

### DRAM vs NVM Comparison

| Technology | Read (ns) | Write (ns) | Write/Read | Capacity | Use Case |
|-----------|-----------|------------|------------|----------|----------|
| **SRAM** | 2.0 | 2.0 | 1.0x | MB | Cache PIM |
| **DRAM** | 13.32 | 15.0 | 1.13x | GB | Main memory PIM |
| **STT-MRAM** | 5.4 | 18.7 | 3.5x | MB-GB | Persistent PIM |
| **ReRAM** | 5.6 | 15.8 | 2.8x | MB-GB | Fast NVM PIM |
| **PCM** | 8.7 | 108 | 12.4x | GB | Read-only (avoid!) |

**For BFS**:
1. **SRAM**: Best performance, limited capacity
2. **DRAM**: Good performance, high capacity (**practical choice!**)
3. **ReRAM**: Moderate performance, persistent state
4. **STT-MRAM**: Slower, but non-volatile
5. **PCM**: Terrible (avoid for BFS!)

---

## Energy Analysis

### Energy Breakdown (SRAM Subarray-Level)

**Per vertex**:
```
Read energy:     34 reads × 8 bytes × 0.5 pJ/byte = 136 pJ
Write energy:    16 writes × 8 bytes × 0.8 pJ/byte = 102.4 pJ
Compute energy:  16 compares × 2 pJ = 32 pJ
────────────────────────────────────────────────────
Total:           270.4 pJ per vertex

For 256K vertices: 69.2 nJ total
```

### Energy Breakdown (PCM Subarray-Level)

**Per vertex**:
```
Read energy:     34 reads × 8 bytes × 1.0 pJ/byte = 272 pJ
Write energy:    16 writes × 8 bytes × 30 pJ/byte = 3,840 pJ ⚠️
Compute energy:  16 compares × 2 pJ = 32 pJ
────────────────────────────────────────────────────
Total:           4,144 pJ per vertex (15x worse than SRAM!)

For 256K vertices: 1,061 nJ total
```

**PCM write energy dominates!** 93% of energy is writes.

### Energy: ALU vs In-Order Core

**Simple ALU** (unnecessary writes):
```
Memory energy:   100 pJ
Transfer:        50 pJ
PE compute:      16 × 2 pJ = 32 pJ
────────────────────────
Total:           182 pJ per vertex
```

**In-Order Core** (50% fewer writes):
```
Memory energy:   75 pJ (saved 25% from fewer writes!)
Transfer:        37.5 pJ
PE compute:      16 × 15 pJ = 240 pJ (instruction overhead!)
────────────────────────
Total:           352.5 pJ per vertex
```

**ALU is 1.9x more energy efficient** (instruction overhead hurts In-Order Core).

**Trade-off**: In-Order Core is faster but uses more energy.

---

## Key Findings

### 1. SRAM Dominates Performance
- 2-3ms for 256K vertex BFS
- But limited to small graphs (cache-level PIM)

### 2. DRAM is Practical Choice
- 4-5ms for 256K vertex BFS (2x slower than SRAM)
- **But enables GB-scale graphs** (realistic workloads!)
- Standard for main memory PIM systems

### 3. PCM is Unusable for BFS
- 60-80ms for 256K vertex BFS (20-30x slower!)
- Write asymmetry (12.4x) kills performance
- **Only use for read-only graph algorithms**

### 4. In-Order Core Wins Despite Overhead
- 1.22x faster for BFS (fewer operations)
- Branch capability saves 50% of writes
- **Control flow > instruction overhead for BFS**

### 5. Write Asymmetry Critical
- BFS has 16 writes per vertex
- Technologies with slow writes severely penalized
- SRAM/DRAM best, PCM worst

---

## Recommendations

### For Graph Algorithms (BFS, DFS, etc.)

**Memory Technology**:
- **DRAM**: Best overall (performance + capacity) ✅
- **SRAM**: Fastest but limited capacity
- **ReRAM**: Alternative for persistent graphs
- **STT-MRAM**: If non-volatile needed
- **PCM**: Avoid! (only for read-only graphs)

**PE Configuration**:
- **In-Order Core**: Preferred for control flow ✅
- **Simple ALU**: Only if power-constrained

### For Different Graph Sizes

| Graph Size | Best Technology | Why? |
|-----------|----------------|------|
| < 10MB | SRAM | Fits in cache, maximum performance |
| 10MB - 1GB | **DRAM** | Practical size, good performance |
| > 1GB | DRAM or PCM | Capacity needed, PCM only if read-only |

### Design Guidelines

**For PIM Architects**:
1. **Support DRAM** for realistic graph sizes
2. **Include In-Order Core** for control-heavy workloads
3. **Optimize writes** (critical for BFS performance)
4. **Minimize inner-bank latency** (data movement overhead)

**For Application Developers**:
1. **Use DRAM** unless graph fits in SRAM
2. **Use In-Order Core PE** for graph algorithms
3. **Avoid PCM** for write-heavy graph updates
4. **Consider ReRAM** if persistence needed

---

## Test Implementation

**Files**:
- `test_subarray_bfs_pim.cpp` - Subarray-level with ALU PE
- `test_bank_bfs_pim.cpp` - Bank-level with ALU + In-Order Core

**Build and run**:
```bash
cd build
cmake ..
make test_subarray_bfs_pim test_bank_bfs_pim
./tests/test_subarray_bfs_pim
./tests/test_bank_bfs_pim
```

**Output metrics**:
- Latency (ms)
- Throughput (M vertices/sec, M edges/sec)
- Energy per vertex (nJ)
- Operation counts (reads, writes, compares, branches)
- Timing breakdown

---

## Conclusion

BFS testing reveals critical insights for PIM design:

1. **DRAM is essential** for practical graph sizes
2. **In-Order Core beats ALU** for control-heavy workloads
3. **Write asymmetry is critical** (PCM unusable for BFS)
4. **Irregular workloads stress memory** differently than vector ops

The addition of DRAM support makes these tests **realistic** for main memory PIM systems, complementing the existing SRAM/NVM tests for different PIM deployment scenarios.
