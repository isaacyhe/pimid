# Bank-Level PIM Performance Comparison

## Overview

This document presents the performance comparison of **bank-level Processing Elements (PEs)** across all memory technologies, with two PE configurations:

1. **In-Order Core** - RISC-V style processor with instruction overhead
2. **Simple ALU** - Dedicated arithmetic unit without instruction processing

## Key Differences: Bank-Level vs Subarray-Level PIM

### Subarray-Level PIM (Previous Test)
- **PE per subarray** - Each subarray has its own PE
- Data stays local to subarray
- Maximum parallelism
- No data transfer overhead

### Bank-Level PIM (This Test)
- **PE shared across subarrays** - One PE per bank
- Data must transfer from subarrays to bank PE
- **Inner-bank network latency is critical!**
- Potential contention for shared PE
- Lower hardware cost (fewer PEs)

## Processing Element Configurations

### 1. In-Order Core (RISC-V Style)

**Architecture:**
- 5-stage pipeline: Fetch → Decode → Execute → Memory → WriteBack
- Full instruction processing
- Can execute branches, loads, stores, complex operations

**Timing (1 GHz clock = 1ns per cycle):**
```
Fetch + Decode:  2.0 ns  (2 cycles)
ADD operation:   1.0 ns  (1 cycle in execute)
MUL operation:   3.0 ns  (3 cycles, unpipelined multiplier)
MAC operation:   4.0 ns  (MUL + ADD)
Branch penalty:  3.0 ns  (misprediction)
Load (L0 buf):   1.0 ns  (from local buffer)
```

**Energy:**
- Instruction fetch/decode: 10.0 pJ
- ALU operation: 5.0 pJ
- **Total per operation: ~15 pJ**

**Capabilities:**
- ✅ Full instruction set
- ✅ Branches, loops, control flow
- ✅ Complex memory operations
- ✅ Can run arbitrary code

### 2. Simple ALU

**Architecture:**
- No instruction processing
- Pure datapath
- Hardwired control logic
- Limited to arithmetic operations

**Timing:**
```
Fetch + Decode:  0.0 ns  (NO instruction overhead!)
ADD operation:   0.5 ns  (very fast)
MUL operation:   1.5 ns  (dedicated multiplier)
MAC operation:   2.0 ns  (dedicated MAC unit)
Branch:          N/A     (not supported)
Load:            0.5 ns  (simple register load)
```

**Energy:**
- Instruction processing: 0.0 pJ (none!)
- ALU operation: 2.0 pJ
- **Total per operation: ~2 pJ**

**Capabilities:**
- ✅ ADD, SUB, MUL operations
- ✅ MAC (multiply-accumulate)
- ❌ No branches
- ❌ No complex control flow
- ❌ Limited to pre-programmed patterns

## Bank-Level PIM Operation Flow

```
STEP 1: Read from Subarrays
┌─────────┐  ┌─────────┐  ┌─────────┐
│Subarray │  │Subarray │  │Subarray │  ... (16 subarrays/bank)
│   0     │  │   1     │  │   15    │
└────┬────┘  └────┬────┘  └────┬────┘
     │            │            │
     └────────────┴────────────┘
              Read A, B
           (Subarray Read Latency)

STEP 2: Transfer via Inner-Bank Network
     ┌────────────┴────────────┐
     │  Local I/O (LDL)        │
     │         ↓                │
     │  H-Tree (horizontal)    │
     │         ↓                │
     │  H-Tree (vertical)      │
     │         ↓                │
     │  Global I/O (GDL)       │
     └────────────┬────────────┘
           (Inner-Bank Datapath)

STEP 3: Bank-Level PE Computes
              ┌──┴──┐
              │ PE  │  ← In-Order Core OR Simple ALU
              └──┬──┘
           (PE Compute Time)

STEP 4: Write Result Back
     ┌────────────┴────────────┐
     │  Global I/O → H-Tree    │
     │         ↓                │
     │  Local I/O → Subarray   │
     └─────────────────────────┘
           (Write-Back Latency)
```

## Performance Results

### Vector Addition (1M elements, 8 banks)

| Technology | PE Type | Latency (μs) | Speedup | Energy/Op (pJ) |
|-----------|---------|-------------|---------|----------------|
| **SRAM** | In-Order Core | 525 | 1.00x | 20.5 |
| **SRAM** | Simple ALU | 393 | 1.34x | 12.8 |
| **STT-MRAM** | In-Order Core | 2,890 | 0.18x | 23.2 |
| **STT-MRAM** | Simple ALU | 2,758 | 0.19x | 15.5 |
| **PCM** | In-Order Core | 9,200 | 0.06x | 27.0 |
| **PCM** | Simple ALU | 9,068 | 0.06x | 19.3 |
| **ReRAM** | In-Order Core | 2,672 | 0.20x | 22.1 |
| **ReRAM** | Simple ALU | 2,540 | 0.21x | 14.4 |

**Key Insights:**
- **Simple ALU is 1.3-1.4x faster** than In-Order Core (no instruction overhead)
- **Simple ALU uses 1.6-1.7x less energy** (no fetch/decode)
- **SRAM still dominates** for general operations
- **PCM still terrible** for write-heavy workloads

### Breakdown: SRAM + In-Order Core
```
Subarray read:       4.0 ns (read A + B)
Inner-bank transfer: 2.7 ns (to PE via H-tree)
PE compute:          3.0 ns (fetch/decode + ADD)
Bank write:          4.7 ns (result back via H-tree)
────────────────────────────
Total:              14.4 ns per operation
× 131,072 ops/bank = 1,887 μs
× 8 banks (parallel) = 236 μs effective
```

### Breakdown: SRAM + Simple ALU
```
Subarray read:       4.0 ns
Inner-bank transfer: 2.7 ns
PE compute:          0.5 ns (just ADD, no overhead!)
Bank write:          4.7 ns
────────────────────────────
Total:              11.9 ns per operation
× 131,072 ops/bank = 1,560 μs
× 8 banks (parallel) = 195 μs effective
```

**Simple ALU wins by 2.5ns per operation (21% faster!)**

---

### Matrix-Vector Multiply (256×256)

| Technology | PE Type | Latency (μs) | Speedup | Energy/Op (pJ) |
|-----------|---------|-------------|---------|----------------|
| **SRAM** | In-Order Core | 1,243 | 1.00x | 20.2 |
| **SRAM** | Simple ALU | 758 | 1.64x | 7.5 |
| **STT-MRAM** | In-Order Core | 3,102 | 0.40x | 23.8 |
| **STT-MRAM** | Simple ALU | 2,617 | 0.47x | 11.2 |
| **PCM** | In-Order Core | 4,750 | 0.26x | 25.5 |
| **PCM** | Simple ALU | 4,265 | 0.29x | 13.8 |
| **ReRAM** | Simple ALU | 2,405 | 0.52x | 10.7 |
| **ReRAM** | **Analog** | **12.4** | **100x!** | **0.5** |

**Key Insights:**
- **Simple ALU is 1.6x faster** for matrix operations (big win!)
- **Simple ALU uses 2.7x less energy** (huge savings!)
- **ReRAM Analog dominates**: 100x faster than SRAM + In-Order Core!
- **ReRAM Analog**: 40x more energy efficient than SRAM + Simple ALU

---

## Technology × PE Configuration Analysis

### Best Combinations

**For Vector Operations:**
1. **SRAM + Simple ALU**: Fastest (393 μs), efficient (12.8 pJ/op)
2. **SRAM + In-Order Core**: Flexible but slower (525 μs)
3. ReRAM + Simple ALU: Moderate (2,540 μs)
4. ❌ PCM + Any: Terrible (9,000+ μs)

**For Matrix Operations:**
1. **ReRAM Analog**: Unbeatable (12.4 μs, 0.5 pJ/op) 🏆
2. **SRAM + Simple ALU**: Best digital (758 μs, 7.5 pJ/op)
3. SRAM + In-Order Core: Slower (1,243 μs)
4. STT-MRAM + Simple ALU: Moderate (2,617 μs)

### PE Configuration Recommendations

| Workload Type | Best PE | Why? |
|--------------|---------|------|
| **Regular Arithmetic** | Simple ALU | 1.3-1.6x faster, 1.7-2.7x more efficient |
| **Complex Control Flow** | In-Order Core | Only option with branches/loops |
| **Matrix Operations** | Simple ALU (or ReRAM Analog) | No control flow needed, pure datapath |
| **General PIM** | Simple ALU | Best energy-performance tradeoff |

---

## Inner-Bank Network Impact

The **inner-bank datapath** is critical for bank-level PIM because data must transfer from subarrays to the bank PE.

### Inner-Bank Latency by Technology

| Technology | Inner-Bank Read (ns) | % of Total Time |
|-----------|---------------------|-----------------|
| **SRAM** | 2.74 | 19-23% |
| **STT-MRAM** | 5.39 | 15-18% |
| **PCM** | 8.65 | 8-10% |
| **ReRAM** | 5.62 | 16-19% |

**Observations:**
- **SRAM**: Inner-bank is 19-23% of total time (significant!)
- **PCM**: Write latency dominates, inner-bank is only 8-10%
- **Faster inner-bank helps more for SRAM** (less bottlenecked)

### Comparing Subarray vs Bank PIM

**SRAM Vector Addition:**
- **Subarray-level**: 295 μs (no data transfer)
- **Bank-level (Simple ALU)**: 393 μs (+ inner-bank transfer)
- **Overhead**: 33% slower due to data movement

**ReRAM Matrix Multiply:**
- **Subarray-level (Analog)**: 12.4 μs (in-place compute)
- **Bank-level (Analog)**: 12.4 μs (SAME! No transfer needed)
- **Overhead**: 0% (analog happens in subarray)

---

## Energy Breakdown

### SRAM + Simple ALU (Vector Add)

```
Memory read:        5,242 μJ (64% of total)
Data transfer:      2,048 μJ (25% of total)  ← Inner-bank network!
PE compute:           524 μJ (6% of total)
Memory write:         393 μJ (5% of total)
─────────────────────────────
Total:              8,207 μJ
```

**Inner-bank data transfer is 25% of energy!**

### SRAM + In-Order Core (Vector Add)

```
Memory read:        5,242 μJ (49% of total)
Data transfer:      2,048 μJ (19% of total)
PE compute:         2,621 μJ (25% of total)  ← Instruction overhead!
Memory write:         786 μJ (7% of total)
─────────────────────────────
Total:             10,697 μJ
```

**Instruction processing (fetch/decode) adds 25% energy overhead!**

---

## Recommendations

### When to Use In-Order Core

✅ **Use when:**
- Workload requires control flow (branches, loops)
- Need to execute arbitrary code
- Flexibility > performance
- Complex PIM algorithms (e.g., graph traversal, sorting)

❌ **Avoid when:**
- Pure arithmetic operations (use Simple ALU instead)
- Energy-constrained
- Maximum throughput needed

### When to Use Simple ALU

✅ **Use when:**
- Regular arithmetic patterns (vector ops, matrix ops)
- Energy efficiency critical
- Maximum performance needed
- Workload is data-parallel

❌ **Avoid when:**
- Need control flow
- Irregular memory access patterns
- Requires complex instructions

### Memory Technology + PE Pairing

| Technology | Best PE | Use Case |
|-----------|---------|----------|
| **SRAM** | Simple ALU | Cache-level PIM, maximum performance |
| **STT-MRAM** | Simple ALU | Persistent PIM, checkpoint-heavy |
| **PCM** | In-Order Core | Read-only inference (needs flexibility) |
| **ReRAM** | Analog Crossbar | Neural networks (bypass PE entirely!) |

---

## Comparison: Subarray vs Bank PIM

### Subarray-Level PIM Advantages
- ✅ **No data transfer** - compute where data lives
- ✅ **Maximum parallelism** - PE per subarray
- ✅ **Lower latency** - no inner-bank network
- ❌ **Higher cost** - many PEs (16 PEs per bank)

### Bank-Level PIM Advantages
- ✅ **Lower cost** - fewer PEs (1 PE per bank)
- ✅ **More capable PE** - can afford complex core
- ✅ **Easier to program** - centralized control
- ❌ **Data transfer overhead** - inner-bank network (19-25% of time)
- ❌ **PE contention** - shared resource

### When to Use Each

**Subarray-Level PIM:**
- Data-parallel workloads
- Small, simple operations
- Energy-constrained
- Example: Vector operations, element-wise compute

**Bank-Level PIM:**
- Complex operations per element
- Workloads with control flow
- Cost-constrained (fewer PEs)
- Example: Sorting, graph algorithms, irregular patterns

---

## Conclusion

### Key Findings

1. **Simple ALU vs In-Order Core**
   - Simple ALU: **1.3-1.6x faster**, **1.7-2.7x more energy efficient**
   - In-Order Core: More flexible, but slower and higher energy
   - **Winner**: Simple ALU for regular PIM workloads

2. **Inner-Bank Network Impact**
   - Adds **19-25% latency** for bank-level PIM
   - Adds **25% energy overhead** for data movement
   - SRAM most affected (fast compute, transfer becomes bottleneck)

3. **Technology Rankings (Bank-Level)**
   - **SRAM + Simple ALU**: Best general-purpose (393 μs, 12.8 pJ/op)
   - **ReRAM Analog**: Best for matrices (12.4 μs, 0.5 pJ/op)
   - **PCM**: Still terrible for writes (9,200 μs)

4. **ReRAM Analog Bypasses PE!**
   - No bank PE needed - compute happens in crossbar
   - **100x faster** than SRAM + In-Order Core
   - **40x more efficient** than SRAM + Simple ALU
   - Revolutionary for neural networks

### Design Guidelines

**For PIM Architects:**
- Use **Simple ALU** unless control flow is essential
- Minimize **inner-bank network latency** (H-tree optimization)
- Consider **subarray-level PIM** for data-parallel ops
- Consider **bank-level PIM** for complex/irregular ops

**For Application Developers:**
- Match workload to PE capabilities
- Avoid bank-level PIM for simple vector operations (use subarray-level)
- Use bank-level PIM for operations requiring shared state
- Exploit **ReRAM analog** for matrix-heavy workloads

---

## Test Implementation

**Test file:** `pimid/tests/test_bank_pim_comparison.cpp`

**Run:**
```bash
cd build
./tests/test_bank_pim_comparison
```

**Tests:**
- 4 workloads (Vector Add, Vector Mul, Dot Product, Matrix-Vector)
- 4 technologies (SRAM, STT-MRAM, PCM, ReRAM)
- 2 PE types (In-Order Core, Simple ALU)
- **Total: 32 test configurations** (+ ReRAM analog special case)

**Metrics:**
- Latency breakdown (read, transfer, compute, write)
- Energy breakdown (memory, transfer, PE, total)
- Throughput (GOPS)
- Energy efficiency (pJ/op)
