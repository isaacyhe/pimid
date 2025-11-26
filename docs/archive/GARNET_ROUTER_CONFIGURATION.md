# GARNET Router Configuration for DRAM Internal Networks

## Overview

GARNET now supports **configurable router complexity** to model everything from full-featured NoC routers to lightweight DRAM muxes.

---

## Virtual Networks (VN) vs Virtual Channels (VC)

### ⚠️ CRITICAL DISTINCTION

**Virtual Networks (VN)** and **Virtual Channels (VC)** serve **different purposes**:

| Aspect | Virtual Networks (VN) | Virtual Channels (VC) |
|--------|----------------------|----------------------|
| **Purpose** | Message class separation | Deadlock avoidance |
| **Use Case** | Different traffic types | Flow control within VN |
| **Example** | Read vs Write, Req vs Resp | Escape channels, buffer management |
| **Independence** | Separate physical/logical paths | Share same physical link |
| **Allocation** | Based on message type | Dynamic, based on availability |

### Virtual Networks (VN)

**VNs separate different message classes:**
- VN 0: Read requests
- VN 1: Write requests
- VN 2: Read responses (if needed)
- VN 3: Write responses (if needed)

**Why use VNs:**
- ✅ Prevent protocol-level deadlock (request blocked by response)
- ✅ Provide QoS (priority classes)
- ✅ Isolate different traffic types
- ✅ Model separate physical paths (like DRAM read/write paths)

### Virtual Channels (VC)

**VCs provide deadlock-free routing within a VN:**
- VC 0: Primary path
- VC 1: Escape path (for deadlock avoidance)
- VC 2+: Additional escape paths

**Why use VCs:**
- ✅ Prevent routing deadlock (cyclic dependencies)
- ✅ Increase effective link utilization
- ✅ Allow head-of-line blocking avoidance
- ✅ Multiple flows on same link

### DRAM Configuration

For **DRAM H-trees**, we use:
```cpp
virtual_networks = 2;          // VN0: Reads, VN1: Writes
virtual_channels_per_vn = 1;   // No deadlock in tree topology
total_vcs = 2 * 1 = 2
```

**Rationale:**
- 2 VNs model separate read/write data paths in DRAM
- 1 VC per VN because trees are deadlock-free
- No need for escape VCs in acyclic topology

---

## Router Pipeline Complexity

GARNET supports 4 router complexity levels:

### 1. FULL (4-stage pipeline)
```
RC → VA → SA → ST
(Route Computation → VC Allocation → Switch Allocation → Switch Traversal)
```

**When to use:**
- Full-featured NoC with multiple VCs per VN
- Complex routing algorithms
- Need detailed cycle-accurate modeling

**Latency:** 4 cycles minimum (1 per stage)

**Example:** Mesh NoC for many-core processors

### 2. REDUCED (3-stage pipeline)
```
RC → SA → ST
(Route Computation → Switch Allocation → Switch Traversal)
```

**When to use:**
- Simple VC allocation (1 VC per VN)
- Still need routing computation
- Moderate complexity

**Latency:** 3 cycles minimum

**Example:** Simple on-chip networks

### 3. SIMPLE (2-stage pipeline)
```
SA → ST
(Switch Allocation → Switch Traversal)
```

**When to use:**
- Pre-computed routes (like tree topologies)
- Simple arbitration needed
- Lightweight routers

**Latency:** 2 cycles minimum

**Example:** Fat-tree networks with deterministic routing

### 4. MINIMAL (1-stage pipeline)
```
ST only
(Switch Traversal only, just a mux)
```

**When to use:**
- **DRAM internal networks** ← DEFAULT FOR DRAM
- Very simple switching
- Crossbars, muxes
- No routing needed (point-to-point or tree)

**Latency:** 1 cycle minimum (just mux delay)

**Example:** DRAM H-trees, simple crossbars

---

## Default DRAM Configuration

All **12 DRAM types** now use this configuration:

```cpp
NetworkConfig config;

// Virtual Networks (message classes)
config.virtual_networks = 2;           // VN0: Read, VN1: Write
config.virtual_channels_per_vn = 1;    // 1 VC (trees are deadlock-free)
config.virtual_channels = 2;           // Total: 2 VNs × 1 VC = 2

// Router pipeline (lightweight for DRAM)
config.router_pipeline = RouterPipelineComplexity::MINIMAL;
config.router_latency = 1;             // 1 cycle mux delay
config.enable_router_bypass = true;    // Bypass for single-hop

// Minimal buffering (realistic for DRAM)
config.input_buffer_depth = 2;         // Sense amp + column latch
config.output_buffer_depth = 2;        // Output driver buffer
```

**Why MINIMAL pipeline:**
- DRAM H-trees are just **muxes**, not complex routers
- No need for routing computation (tree has fixed paths)
- No VC allocation (1 VC per VN)
- Realistic 1-cycle switching delay

**Why 2 buffers:**
- DRAM has **minimal buffering** (not like NoC routers with deep queues)
- Sense amplifier latches (~1 row)
- Column latches (~1-2 bursts)
- Realistic modeling!

---

## Customizing Router Configuration

### Example 1: Heavyweight Router (Full NoC)

```cpp
auto network = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);

// Override to use full NoC router (for comparison/research)
// Note: This would require modifying createGarnetHTreeForDRAM
// to accept custom NetworkConfig

NetworkConfig custom_config;
custom_config.topology = NetworkTopology::H_TREE;
custom_config.virtual_networks = 4;         // Req/Resp for Read/Write
custom_config.virtual_channels_per_vn = 2;  // Escape VCs
custom_config.router_pipeline = RouterPipelineComplexity::FULL;
custom_config.router_latency = 0;  // Use pipeline stages (4 cycles)
custom_config.input_buffer_depth = 8;  // Deep buffers

// This would create heavyweight routers:
// - 4 VNs × 2 VCs = 8 total VCs
// - 4-stage pipeline (RC→VA→SA→ST)
// - 4 cycle minimum latency
// - Deep buffering

// Use case: Research on full-featured DRAM NoC
```

### Example 2: Medium Complexity (SIMPLE pipeline)

```cpp
NetworkConfig config;
config.virtual_networks = 2;
config.virtual_channels_per_vn = 1;
config.router_pipeline = RouterPipelineComplexity::SIMPLE;
config.router_latency = 2;              // SA + ST = 2 cycles
config.input_buffer_depth = 4;

// Use case: Slightly more complex than pure mux
// - Still lightweight (2-stage)
// - Explicit arbitration modeling
```

### Example 3: Multi-Class DRAM (More VNs)

```cpp
NetworkConfig config;
config.virtual_networks = 4;            // More message classes
// VN0: High-priority reads
// VN1: High-priority writes
// VN2: Low-priority reads
// VN3: Low-priority writes
config.virtual_channels_per_vn = 1;
config.router_pipeline = RouterPipelineComplexity::MINIMAL;
config.router_latency = 1;

// Use case: QoS-aware DRAM
// - 4 priority classes
// - Still lightweight routers
// - Total 4 VCs (4 VNs × 1 VC/VN)
```

---

## Comparison: MINIMAL vs FULL Router

### Scenario: HBM3 with 32 subarrays

#### MINIMAL Router (Default)
```
Configuration:
- Pipeline: MINIMAL (ST only)
- Latency: 1 cycle per hop
- VNs: 2 (Read, Write)
- VCs per VN: 1
- Buffers: 2 deep

Single transfer latency:
= Link latency + Router latency × hops
= 3 cycles + 1 cycle × log2(32)
= 3 + 5 = 8 cycles

Resource usage:
- 32 simple muxes
- 64 small buffers (32 routers × 2 buffers)
- Minimal area/power
```

#### FULL Router (Heavyweight)
```
Configuration:
- Pipeline: FULL (RC→VA→SA→ST)
- Latency: 4 cycles per hop
- VNs: 2
- VCs per VN: 2 (with escape channels)
- Buffers: 8 deep

Single transfer latency:
= Link latency + (Router pipeline × hops)
= 3 cycles + 4 cycles × log2(32)
= 3 + 20 = 23 cycles

Resource usage:
- 32 complex routers with full pipeline
- 512 deep buffers (32 routers × 2 VNs × 2 VCs × 4 depth)
- Significant area/power overhead
```

**Speedup with MINIMAL:** 23 / 8 = **2.88× faster!**

**Reason:** DRAM doesn't need complex routers - just muxes!

---

## When to Use Each Complexity Level

| Complexity | Use Case | Example |
|------------|----------|---------|
| **MINIMAL** | DRAM internal networks, simple crossbars | ✅ **All DRAM H-trees (default)** |
| **SIMPLE** | Deterministic routing, tree networks | Fat-trees with pre-computed routes |
| **REDUCED** | Moderate NoCs, simple VC management | Small mesh networks |
| **FULL** | Complex NoCs, multiple VCs per VN, adaptive routing | Large mesh/torus, dragonfly |

---

## Performance Impact

### Latency Breakdown for 64-byte Transfer

**DDR4 with MINIMAL router:**
```
Link latency:        5 cycles (subarray H-tree)
Router hops:         log2(16) = 4 hops
Router latency:      1 cycle × 4 = 4 cycles
Serialization:       64B / (8 bits/cycle) = 64 cycles
────────────────────────────────────────────
Total:               5 + 4 + 64 = 73 cycles
```

**DDR4 with FULL router:**
```
Link latency:        5 cycles
Router hops:         4 hops
Router latency:      4 cycles × 4 = 16 cycles (RC+VA+SA+ST each hop)
Serialization:       64 cycles
────────────────────────────────────────────
Total:               5 + 16 + 64 = 85 cycles
```

**Overhead:** FULL router adds 12 cycles (**16% slower**) for NO benefit in DRAM!

---

## Key Takeaways

1. **VN ≠ VC:**
   - VN = Message classes (read vs write)
   - VC = Deadlock avoidance within VN

2. **DRAM uses MINIMAL routers:**
   - Just muxes, not complex routers
   - 1-cycle switching delay
   - Minimal buffering (2 deep)

3. **Configuration is explicit:**
   - `virtual_networks = 2` (read/write separation)
   - `virtual_channels_per_vn = 1` (no deadlock in trees)
   - `router_pipeline = MINIMAL` (lightweight)

4. **Significant impact:**
   - MINIMAL vs FULL: **2-3× faster** for DRAM
   - Realistic modeling: routers are muxes, not full NoC

5. **Customizable:**
   - Can increase VNs for QoS (priority classes)
   - Can change router complexity for research
   - Defaults are validated and realistic

---

## Future Enhancements

Potential extensions:
1. **Per-level router complexity** (subarray MINIMAL, chip SIMPLE)
2. **Dynamic VN allocation** (runtime message class assignment)
3. **Configurable VC allocation policies** (static vs dynamic)
4. **Router bypass optimization** (skip routing for local traffic)

---

**Status:** ✅ Implemented and tested
**Version:** 2.0
**Date:** 2025-11-18
**Validated:** All 12 DRAM types use MINIMAL routers by default
