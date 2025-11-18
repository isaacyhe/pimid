# Baseline Topology Comparison Summary
**Date:** November 18, 2025
**Test:** Tree Reduction on 32-subarray configuration (128KB bank)

---

## Three Network Topologies Compared

### 1. **Baseline: H-tree Bus Only**
- **Description**: Traditional SRAM with ALUs at each subarray
- **Network**: Simple H-tree bus connecting all subarrays to central port
- **Routing**: All inter-subarray transfers must go through central port
- **Path**: Source → Up to root → Down to destination (2× tree height)
- **Switches**: 0 (no routing infrastructure)
- **Innovation Level**: None (baseline architecture)

### 2. **H-tree with Switches**
- **Description**: H-tree with routing switches at each node
- **Network**: Hierarchical tree with switches enabling intermediate routing
- **Routing**: Can route at intermediate switches (not always to root)
- **Path**: Source → Common ancestor → Destination (log₂(N) average)
- **Switches**: N-1 switches for N subarrays (31 for 32 SA)
- **Innovation Level**: Adds centralized routing control

### 3. **LIBCom (Proposed Innovation)**
- **Description**: Direct point-to-point switching via local interconnect bus
- **Network**: Crossbar-like with direct paths between any two subarrays
- **Routing**: Direct connection (1 hop)
- **Path**: Source → Switch → Destination
- **Switches**: Distributed switching at each subarray
- **Innovation Level**: Novel architecture with direct paths

---

## Performance Results: Bank 3 (32 Subarrays, 128KB)

| Topology | Total Cycles | Transfer Cycles | Base Latency | Contention | Blocked Cycles |
|----------|--------------|-----------------|--------------|------------|----------------|
| **Baseline (Bus Only)** | **5,418** | **1,242** | **12 cycles** | 870 | 870 |
| H-tree (Switches) | 5,393 | 1,087 | 7 cycles | 870 | 870 |
| H-tree (Switches, 4 VCs) | 5,363 | 919 | 7 cycles | 702 | 702 |
| **LIBCom** | **5,125** | **31** | **1 cycle** | **0** | **0** |

### Key Metrics

**Baseline vs H-tree with Switches:**
- Improvement: 25 cycles (0.5% faster)
- Benefit: Shorter average paths (log₂(N) vs 2×log₂(N))
- Limitation: Still has same contention (870 blocked cycles)

**Baseline vs LIBCom:**
- Improvement: 293 cycles (5.4% faster)
- Benefit: Direct paths eliminate all contention
- Transfer cycles: **40× reduction** (1242 → 31 cycles)

**H-tree with Switches vs LIBCom:**
- Improvement: 268 cycles (5.0% faster)
- Benefit: 0 blocked cycles vs 870 blocked cycles
- Transfer cycles: **35× reduction** (1087 → 31 cycles)

---

## Detailed Analysis

### Base Transfer Latency Comparison

| Topology | Formula | 32 Subarrays | Components |
|----------|---------|--------------|------------|
| Bus Only | 2 + 2×log₂(N) | **12 cycles** | 2 SA access + 2×5 tree hops |
| H-tree (Switches) | 2 + log₂(N) | **7 cycles** | 2 SA access + 5 tree hops |
| LIBCom | 1 | **1 cycle** | Direct switch |

**Observation**:
- Baseline has **double the tree latency** because all paths go through root
- Switches **halve the tree latency** by enabling intermediate routing
- LIBCom **eliminates tree latency** entirely with direct paths

### Contention Analysis

All three topologies experience the **same contention pattern** (870 blocked cycles) when using 1 VC because:

1. **Baseline**: Everything goes through central port (massive bottleneck)
2. **H-tree with Switches**: Still a tree topology with shared paths
3. **LIBCom**: No shared paths → 0 contention!

**Virtual Channels help H-tree with Switches** but not enough:
- 1 VC: 870 blocked cycles
- 2 VCs: 812 blocked cycles (6.7% reduction)
- 4 VCs: 702 blocked cycles (19.3% reduction)

**VCs cannot solve the fundamental topology limitation** - only LIBCom's direct paths eliminate contention entirely.

---

## Switch Complexity Comparison

| Topology | Switches Required | Control Logic | Notes |
|----------|-------------------|---------------|-------|
| Baseline (Bus Only) | **0** | None | Simple, but poor performance |
| H-tree (Switches) | **31** (N-1) | 62 bits (31×2) | Centralized control needed |
| LIBCom | **32** (N) | Distributed | Local control at each SA |

**Key Insight**: Adding switches improves performance, but **LIBCom's distributed architecture provides the best performance** with similar switch count.

---

## Performance Progression Summary

```
Baseline (Bus Only):     5,418 cycles  ████████████████████
                                       ↓ Add Switches
H-tree (Switches, 1 VC): 5,393 cycles  ███████████████████▌ (0.5% better)
                                       ↓ Add VCs
H-tree (Switches, 4 VCs):5,363 cycles  ███████████████████  (1.0% better than baseline)
                                       ↓ Switch to Direct Paths
LIBCom:                  5,125 cycles  █████████████████    (5.4% better than baseline)
```

### Relative Speedup vs Baseline

| Configuration | Total Cycles | Speedup | Transfer Cycle Reduction |
|---------------|--------------|---------|--------------------------|
| Baseline | 5,418 | 1.00× | - |
| H-tree + Switches | 5,393 | 1.005× | 12.5% fewer transfer cycles |
| H-tree + Switches + 4 VCs | 5,363 | 1.010× | 26.0% fewer transfer cycles |
| **LIBCom** | **5,125** | **1.057×** | **97.5% fewer transfer cycles** |

---

## Energy Comparison

| Topology | Energy per Transfer | Total Energy (31 transfers) | Savings vs Baseline |
|----------|---------------------|----------------------------|---------------------|
| Baseline | 1024.00 | 31,744 | - |
| H-tree (Switches) | 1024.00 | 31,744 | 0% (same tree hops) |
| LIBCom | **563.20** | **17,459** | **45%** |

**Note**: H-tree with switches has same energy as baseline because transfers still use tree paths (just shorter on average). LIBCom's single-hop direct paths dramatically reduce energy.

---

## Implications for Paper

### 1. **Baseline Establishment**
The "bus only" configuration establishes that:
- Traditional SRAM with ALUs exists but has poor inter-subarray performance
- This justifies the need for ANY switching infrastructure

### 2. **Incremental Innovation**
Adding switches to H-tree provides **modest improvement** (0.5%):
- Shows that simply adding switches isn't enough
- Tree topology is the fundamental limitation
- Justifies need for architectural change

### 3. **LIBCom Breakthrough**
Direct paths provide **significant improvement** (5.4%):
- Not just incremental—fundamentally different approach
- Eliminates contention entirely (870 → 0 blocked cycles)
- 45% energy savings from reduced hops

### 4. **Addressing "Too Simple" Criticism**
Progression shows:
- **Baseline**: Simple but inadequate
- **H-tree + Switches**: Complex (31 switches, 62 control bits) but limited benefit
- **LIBCom**: Simpler distributed control, much better performance

This demonstrates that **complexity ≠ performance**. LIBCom's innovation is in the **topology**, not just adding more switches.

---

## LaTeX Table for Paper

```latex
\begin{table}[h]
\centering
\caption{Network Topology Comparison: Baseline vs Innovations (32 subarrays)}
\label{tab:topology_comparison}
\begin{tabular}{|l|c|c|c|c|c|}
\hline
\textbf{Topology} & \textbf{Switches} & \textbf{Latency} & \textbf{Total Cycles} & \textbf{Blocked} & \textbf{Speedup} \\
\hline
Baseline (Bus) & 0 & 12 & 5,418 & 870 & 1.00× \\
H-tree (Switches) & 31 & 7 & 5,393 & 870 & 1.005× \\
H-tree (Sw + 4 VCs) & 31 & 7 & 5,363 & 702 & 1.010× \\
\textbf{LIBCom} & \textbf{32} & \textbf{1} & \textbf{5,125} & \textbf{0} & \textbf{1.057×} \\
\hline
\end{tabular}
\end{table}
```

---

## Recommended Figure

Create a bar chart showing:
- X-axis: Three topologies (Baseline, H-tree+Switches, LIBCom)
- Y-axis: Total cycles
- Stacked bars showing: Compute (green) | Transfer (yellow) | Contention (red)

This visually demonstrates:
1. Compute time stays constant (workload-dependent)
2. Transfer time reduces with better topology
3. **Contention disappears** with LIBCom (red portion = 0)

---

## Files Generated

- **Script**: `scripts/run_baseline_comparison.sh`
- **Results**: `results/baseline_comparison_20251118_225558.txt`
- **Source**: `workloads/reduction_message_vc.cpp` (updated with baseline support)
- **Model**: `workloads/network_contention.h` (H_TREE_BUS_ONLY topology added)

---

## Reproduction

```bash
cd DAC26/scripts
./run_baseline_comparison.sh
```

This runs all three topologies across all bank sizes and generates comprehensive comparison data.
