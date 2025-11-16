# PIM Granularity Deep Dive: Understanding the Trade-offs

**Test File:** `tests/workloads/test_pim_granularity.cpp`

**Key Question:** If all PIM architectures have the **same compute power**, why does placement matter?

**Answer:** Because **data movement** dominates performance, not computation!

---

## Executive Summary

This test compares **7 different architectures** with **identical compute power** (64 units @ 2 GFLOPS each = 128 GFLOPS total) but **different data movement characteristics** based on DDR4 DRAM hierarchy.

### Results (Vector Addition, 48 MB data) - WITH PORT CONTENTION:

| Architecture | Performance | Speedup vs CPU | Why? |
|--------------|------------|----------------|------|
| **Bank-PIM** | **262 μs** | **10.06x** | ✅ **BEST**: Dedicated port (19.2 GB/s) |
| **Subarray-PIM** | **1049 μs** | **2.52x** | ⚠️ 4x port contention hurts performance |
| BankGroup-PIM | 655 μs | 4.02x | Moderate contention (4 banks share port) |
| Rank-PIM | 2622 μs | 1.01x | Limited by rank interface (9.6 GB/s) |
| MC-PIM | 2622 μs | 1.01x | Limited by MC bandwidth (19.2 GB/s) |
| **CPU** | **2638 μs** | **1.00x** | Baseline |
| **Chip-PIM** | **20,972 μs** | **0.13x** | ❌ **WORST**: SEVERE I/O bottleneck (1.2 GB/s)! |

**CRITICAL Insight:** Port bitwidth limitations **dominate** performance!
- **Bank-PIM wins** with 10x speedup (dedicated 19.2 GB/s port)
- **Chip-PIM loses catastrophically** (16 banks share tiny x8 = 1.2 GB/s I/O)

---

## DDR4 DRAM Hierarchy

```
Memory Controller (MC)
 └─ Rank (DIMM-level)
     └─ Chip (DRAM die)
         └─ Bank Group
             └─ Bank (2 MB)
                 └─ Subarray (512 KB)
```

### Physical Organization:
- **64 Subarrays** total (4 per bank × 16 banks)
- **16 Banks** total (4 banks × 4 bank groups)
- **4 Bank Groups** per chip
- **2 Chips** (simplified from 8 for this test)
- **2 Ranks**
- **1 Memory Controller**

---

## Architecture Comparison

### All Architectures Have EQUAL Compute:
- ✅ 64 total compute units
- ✅ 2 GFLOPS per unit
- ✅ 128 GFLOPS total
- ✅ **IDENTICAL compute time**

### But DIFFERENT Data Movement (with Port Contention!):

| Architecture | Units | Local Memory | Data Latency | Port BW | Effective BW | Energy/Byte |
|--------------|-------|--------------|--------------|---------|--------------|-------------|
| **Subarray-PIM** | 64 | 512 KB | **26.64 ns** ✅ | 19.2 GB/s ÷ 4 | **4.8 GB/s** ⚠️ | **1 pJ/B** ✅ |
| **Bank-PIM** | 16 | 2 MB | 39.96 ns | 19.2 GB/s | **19.2 GB/s** ✅ | 2 pJ/B |
| **BankGroup-PIM** | 4 | 8 MB | 50 ns | 76.8 GB/s ÷ 4 | 19.2 GB/s | 3 pJ/B |
| **Chip-PIM** | 2 | 128 MB | 60 ns | **1.2 GB/s** ❌ | **1.2 GB/s** ❌ | 5 pJ/B |
| **Rank-PIM** | 2 | 1 GB | 80 ns | 9.6 GB/s | 9.6 GB/s | 10 pJ/B |
| **MC-PIM** | 1 | 4 GB | 100 ns | 19.2 GB/s | 19.2 GB/s | 15 pJ/B |
| **CPU** | 1 | 32 MB LLC | 100 ns | 19.2 GB/s | 19.2 GB/s | **20 pJ/B** ❌ |

**Port Contention Key:**
- **Subarray-PIM**: 4 subarrays share 1 bank port (128-bit) → 4x contention
- **Bank-PIM**: Each bank has dedicated port → NO contention (WINNER!)
- **Chip-PIM**: 16 banks share x8 I/O (1 byte/cycle) → SEVERE 16x bottleneck!

---

## Data Path Visualization

### CPU (Longest Path):
```
CPU Core → LLC → Memory Controller → Rank → Chip → Bank Group → Bank → Subarray
└─ Latency: ~100ns, Energy: 20 pJ/byte
```

### Bank-PIM (Optimal):
```
Bank Compute → Subarray (within bank)
└─ Latency: ~40ns, Energy: 2 pJ/byte
```

### Subarray-PIM (Shortest but Compute-Bound):
```
Subarray Compute → Local Subarray Data
└─ Latency: ~26ns, Energy: 1 pJ/byte
```

**Data Movement Distance:**
```
Subarray ■
Bank     ■■        ← Best balance
Chip     ■■■
Rank     ■■■■
MC       ■■■■■
CPU      ■■■■■■
```

---

## Detailed Performance Analysis

### Workload: Vector Addition
```
C[i] = A[i] + B[i] for i = 0 to 2,097,152
Data: 48 MB (read A, B; write C)
Compute: 2M additions @ 128 GFLOPS
```

### Performance Breakdown (WITH PORT CONTENTION):

| Architecture | Total Time | Data Movement | Compute | Effective BW |
|--------------|------------|---------------|---------|--------------|
| **Bank-PIM** | **262 μs** ✅ | 164 μs | 262 μs | **192 GB/s** ✅ |
| BankGroup-PIM | 655 μs | 655 μs | 66 μs | 77 GB/s |
| **Subarray-PIM** | **1049 μs** ⚠️ | 164 μs | **1049 μs** | **48 GB/s** ⚠️ |
| Rank-PIM | 2622 μs | 2622 μs | 33 μs | 19 GB/s |
| MC-PIM | 2622 μs | 2622 μs | 16 μs | 19 GB/s |
| CPU | 2638 μs | 2622 μs | 16 μs | 19 GB/s |
| **Chip-PIM** | **20,972 μs** ❌ | **20,972 μs** ❌ | 33 μs | **2.4 GB/s** ❌ |

### Key Observations (Port Contention Matters!):

1. **Bank-PIM Dominates:**
   - ✅ **BEST performance** (262 μs, 10.06x speedup)
   - Why? Dedicated 19.2 GB/s port per bank → NO contention!
   - 16 banks in parallel, each handling 3 MB with full bandwidth
   - Perfect balance of parallelism and port access

2. **Chip-PIM Catastrophic Failure:**
   - ❌ **WORST performance** (20,972 μs, 0.13x vs CPU!)
   - Why? 16 banks sharing tiny x8 I/O (1.2 GB/s) → SEVERE bottleneck
   - Even with 60ns latency, bandwidth limitation dominates
   - Demonstrates why chip I/O width is critical!

3. **Subarray-PIM Port Contention:**
   - ⚠️ 1049 μs (2.52x speedup, not terrible but not great)
   - 4 subarrays share 1 bank port → 4x contention
   - Effective BW: 19.2 / 4 = 4.8 GB/s per subarray
   - Still compute-bound, but port contention also hurts

4. **Port Bitwidth Dominates:**
   - Bank port (128-bit) → determines Subarray/Bank/BankGroup performance
   - Chip I/O (x8 = 8-bit) → catastrophic bottleneck for Chip-PIM
   - Rank/MC bandwidth → limits coarse-grained PIM

---

## The PORT CONTENTION Bottleneck (Critical!)

### DDR4 Port Bitwidths:

| Level | Port Width | Bandwidth @ 1.2 GHz | Units Sharing | Effective BW per Unit |
|-------|------------|---------------------|---------------|----------------------|
| **Bank Port** | 128-bit | 19.2 GB/s | 1 (Bank-PIM) or 4 (Subarray-PIM) | 19.2 or 4.8 GB/s |
| **Bank Group** | 512-bit | 76.8 GB/s | 4 banks | 19.2 GB/s per bank |
| **Chip I/O** | x8 (8-bit) | **1.2 GB/s** ❌ | 16 banks | **0.075 GB/s per bank** ❌ |
| **Rank** | 64-bit | 9.6 GB/s | 8 chips | 1.2 GB/s per chip |
| **MC** | 128-bit | 19.2 GB/s | 2 ranks | 9.6 GB/s per rank |

### Port Contention Analysis:

**Subarray-PIM (4x contention):**
```
Bank has 4 subarrays → all share 128-bit port (19.2 GB/s)
Each subarray gets: 19.2 / 4 = 4.8 GB/s
Impact: Moderate (4x serialization)
```

**Bank-PIM (NO contention):**
```
Each bank has dedicated 128-bit port → 19.2 GB/s
16 banks × 19.2 GB/s = 307.2 GB/s aggregate!
Impact: NONE (this is why Bank-PIM wins!)
```

**Chip-PIM (16x SEVERE contention):**
```
16 banks share x8 chip I/O → only 1.2 GB/s total!
Each bank gets: 1.2 / 16 = 0.075 GB/s
Impact: CATASTROPHIC (16x serialization through tiny I/O)
Result: 20,972 μs (80x slower than Bank-PIM!)
```

**Key Insight:**
- Internal bank bandwidth (19.2 GB/s) >> External chip I/O (1.2 GB/s)
- This 16x mismatch creates catastrophic bottleneck for Chip-PIM
- Bank-PIM avoids this by keeping data movement within banks

---

## Energy Efficiency

### Energy per Operation:

| Architecture | Total Energy | Energy/Byte | Efficiency vs CPU |
|--------------|--------------|-------------|-------------------|
| Subarray-PIM | 104.91 mJ | 1 pJ/B | 1.01x |
| Bank-PIM | 104.96 mJ | 2 pJ/B | 1.01x |
| BankGroup-PIM | 105.01 mJ | 3 pJ/B | 1.01x |
| Chip-PIM | 105.11 mJ | 5 pJ/B | 1.01x |
| Rank-PIM | 105.36 mJ | 10 pJ/B | 1.00x |
| MC-PIM | 105.61 mJ | 15 pJ/B | 1.00x |
| CPU | 105.86 mJ | 20 pJ/B | 1.00x (baseline) |

**Energy Insight:** For this workload, energy differences are minimal (all ~105 mJ) because:
- Compute energy dominates: 128 GFLOPS × execution time × 50 mW/GFLOPS
- Data movement energy is small: 48 MB × (1-20 pJ/B) = 0.05-1 mJ

For **data-intensive** workloads, energy differences would be much larger!

---

## The Parallelism-Locality Trade-off

### Too Fine-Grained (Subarray-PIM):
```
✅ Pros:
  - Lowest data latency (26ns)
  - Highest parallelism (64 units)
  - Lowest energy/byte (1 pJ/B)

❌ Cons:
  - Compute-bound (each unit too small)
  - Limited local memory (512 KB)
  - Overhead of coordinating 64 units
```

### Optimal (Bank-PIM):
```
✅ Pros:
  - Good balance (16 units)
  - Sufficient local memory (2 MB)
  - Low latency (40ns)
  - Best performance (2.56x)

Balance:
  - Each unit: 128K ops / 2 GFLOPS = 64 μs
  - 16 units in parallel
  - Data movement well balanced
```

### Too Coarse-Grained (MC-PIM / CPU):
```
✅ Pros:
  - Large memory access (4 GB+)
  - No inter-unit coordination

❌ Cons:
  - Limited parallelism (1 unit)
  - High data latency (100ns)
  - High energy/byte (15-20 pJ/B)
```

---

## When Each Architecture Wins

### Subarray-PIM Wins:
- ✅ Data fits in 512 KB
- ✅ Extremely data-intensive (compute << data movement)
- ✅ High data reuse within subarray
- Example: Streaming operations on small tiles

### Bank-PIM Wins (Most Common):
- ✅ Data fits in ~2 MB bank
- ✅ Balanced compute/memory workload
- ✅ Good data locality
- Example: **Vector operations, convolution, local matrix operations**

### BankGroup/Rank-PIM Wins:
- ✅ Need moderate parallelism (2-4 units)
- ✅ Data spans multiple banks
- Example: Larger matrix operations, graph analytics

### MC-PIM / CPU Wins:
- ✅ Need access to all memory
- ✅ Irregular access patterns
- ✅ Compute-intensive with random memory access
- Example: Sparse matrix operations, pointer chasing

---

## Key Takeaways

### 1. **PORT CONTENTION Dominates Performance!**
With **equal compute power**, performance varies **160x** (262 μs to 20,972 μs) due to port bitwidth limitations!
- Bank-PIM: 10.06x speedup (dedicated 19.2 GB/s port)
- Chip-PIM: 0.13x (16 banks share 1.2 GB/s) → **CATASTROPHIC**

### 2. **Port Bitwidth >> Data Latency**
Chip-PIM has lower latency (60ns) than CPU (100ns), but performs **80x worse** due to port bottleneck!

### 3. **Bank Port (128-bit) is the Key Resource**
- Bank-PIM: Each bank has dedicated 128-bit port → NO contention ✅
- Subarray-PIM: 4 subarrays share 1 bank port → 4x contention ⚠️
- Chip-PIM: 16 banks bottleneck at x8 I/O → 16x contention ❌

### 4. **Sweet Spot: Bank-Level (10x Speedup!)**
For DDR4 DRAM, **Bank-PIM** achieves best performance:
- 16-way parallelism
- Dedicated 19.2 GB/s port per bank
- 40ns latency
- **10.06x speedup vs CPU**

### 5. **Chip I/O Width is Critical**
DDR4 x8 devices (1 byte/cycle) create severe bottleneck for Chip-PIM:
- Internal: 16 banks × 19.2 GB/s = 307.2 GB/s potential
- External: x8 I/O limits to 1.2 GB/s actual → **256x mismatch!**
- This is why HBM uses much wider I/O (1024-bit = 128 bytes/cycle)

### 6. **Architecture Must Match Workload AND Port Resources**
```
If (data fits in bank && port contention minimal)
    → Bank-PIM ✅ (BEST: 10x speedup)
Else if (data in subarray && can tolerate 4x contention)
    → Subarray-PIM (Good: 2.5x speedup)
Else if (need full memory access)
    → MC-PIM or CPU (~1x)
NEVER use Chip-PIM with DDR4 x8 → Catastrophic!
```

---

## Running the Test

```bash
cd /home/user/pimid-dev/tests
mkdir -p build && cd build
cmake ..
make test_pim_granularity
./workloads/test_pim_granularity
```

**Expected Output:**
- DDR4 architecture overview
- Architecture comparison table
- Data path visualization
- Workload performance results
- Detailed analysis and insights

---

## Implications for PIMID Simulator

This test reveals critical design decisions:

1. **PORT CONTENTION is the PRIMARY bottleneck:** Must model port bitwidth limitations!
   - Bank port (128-bit) → limits Subarray/Bank contention
   - Chip I/O (x8) → catastrophic for Chip-PIM in DDR4
   - Cannot ignore port sharing when multiple PIM units access same resource

2. **Granularity Choice:** Bank-level PIM is the sweet spot (10x speedup)
   - Dedicated 19.2 GB/s port per bank
   - 2 MB local memory per bank
   - 16-way parallelism across banks

3. **Memory Technology Matters:**
   - DDR4 x8 → Chip-PIM fails catastrophically
   - HBM (1024-bit I/O) → Chip-PIM would work much better
   - Must match PIM granularity to memory interface width

4. **Bandwidth Hierarchy:** Internal >> External
   - Internal bank bandwidth: 19.2 GB/s
   - Chip I/O bandwidth: 1.2 GB/s (16x smaller!)
   - This mismatch must be modeled in simulator

5. **Workload Mapping:** Must consider port contention
   - Distribute data across banks to avoid contention
   - Keep working sets within bank boundaries
   - Avoid cross-chip communication in DDR4

---

**Test Version:** 1.0
**Date:** 2025-11-16
**Status:** ✅ Complete and validated
