# PIMID Current Architecture: Design Intent vs Reality

## TL;DR

**Question**: Is PIMID really zsim-based or is it an in-house event-driven model?

**Answer**:
- 📋 **Design Intent**: zsim-based execution-driven simulation
- ✅ **Current Reality**: In-house **simple cycle-based analytical model**
- 🔧 **Status**: zsim integration is **planned but not implemented yet**

---

## What the Documentation Says

From `docs/ARCHITECTURE.md`:

```
Host Engine:
- Uses ZSim for cycle-accurate CPU simulation ✓ (documented)

Device Engine:
- Uses ZSim configured as PIM PEs ✓ (documented)
```

**But this is the DESIGN INTENT, not the current implementation!**

---

## What the Code Actually Does

### Current Implementation: Simple Analytical Model

#### 1. Host Engine (pimid/src/host_engine/host_engine.cpp)

```cpp
void HostEngine::initializeZSim() {
    std::cout << "Initializing ZSim for host simulation..." << std::endl;
    // TODO: Actual ZSim initialization
    // For now, just set to nullptr to indicate it's a placeholder
    zsim_instance_ = nullptr;  // ❌ No actual zsim!
}

void HostEngine::run(Cycle num_cycles) {
    while (current_cycle_ < target_cycle) {
        handleDeviceMessages();

        // In a real implementation, this would call ZSim's tick/advance
        // For now, just advance the cycle counter
        advanceCycle();  // ❌ Just incrementing a counter!

        stats_.total_cycles = current_cycle_;
    }
}
```

**Reality**: No zsim, just a simple cycle counter!

#### 2. Device Engine (pimid/src/device_engine/device_engine.cpp)

```cpp
void DeviceEngine::initializeZSim() {
    std::cout << "Initializing ZSim for device PEs..." << std::endl;
    // ZSim initialization for PEs will be implemented using zsim.enabled
    // and zsim.config_file from device configuration when ZSim integration
    // is complete
    zsim_instance_ = nullptr;  // ❌ No actual zsim!
    std::cout << "ZSim placeholder set (full integration pending)" << std::endl;
}

void DeviceEngine::run(Cycle num_cycles) {
    while (current_cycle_ < target_cycle) {
        handleHostMessages();

        // In a real implementation, this would call ZSim's tick/advance for all PEs
        // For now, just advance the cycle counter
        advanceCycle();  // ❌ Just incrementing a counter!

        // Process active offloads
        for (auto& offload : active_offloads_) {
            if (!offload.completed) {
                // Simulate offload execution
                // In a real implementation, this would execute on PEs

                // ⚠️ ANALYTICAL MODEL: Just check if enough cycles elapsed!
                if (current_cycle_ - offload.start_cycle >= offload_completion_cycles_) {
                    offload.completion_cycle = current_cycle_;
                    offload.completed = true;
                    completeOffload(offload.offload_id);
                }
            }
        }

        stats_.total_cycles = current_cycle_;
    }
}
```

**Reality**: Simple analytical model with configurable completion time!

---

## Current Architecture: What's Actually Implemented

```
┌─────────────────────────────────────────────────────────────┐
│  PIMID Current Implementation (as of now)                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Host Engine                    Device Engine              │
│  ┌──────────────┐              ┌──────────────┐           │
│  │ Simple       │              │ Simple       │           │
│  │ Cycle Loop   │◄────────────►│ Cycle Loop   │           │
│  └──────────────┘   Socket     └──────────────┘           │
│        │           Communication       │                   │
│        │                              │                   │
│        │ NO zsim!                     │ NO zsim!          │
│        │                              │                   │
│        ↓                              ↓                   │
│  ┌──────────────┐              ┌──────────────┐           │
│  │ Analytical   │              │ Analytical   │           │
│  │ Model        │              │ Model        │           │
│  │ (configurable│              │ (cycles =    │           │
│  │  latency)    │              │  config val) │           │
│  └──────────────┘              └──────────────┘           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### What It Actually Simulates

**Host Engine**:
```cpp
// Configuration from pimid_config.yaml
host.execution.latency = X cycles  // Placeholder value

// Simulation
Task completes after X cycles  // No actual instruction execution!
```

**Device Engine**:
```cpp
// Configuration from pimid_config.yaml
device.execution.offload_completion_cycles = 100  // Configurable

// Simulation
Offload completes after 100 cycles  // No PE instruction execution!
```

**It's an ANALYTICAL model**, not execution-driven!

---

## What's Implemented vs What's Planned

### ✅ Currently Working (Implemented)

| Component | Status | Type |
|-----------|--------|------|
| **Cycle-based loop** | ✅ Working | Simple cycle counter |
| **Host-Device Communication** | ✅ Working | Socket-based messaging |
| **Memory Models** | ✅ Working | Ramulator, CACTI, NVSim |
| **Network Models** | ✅ Working | GARNET integration |
| **EventQueue** | ✅ Implemented | But NOT currently used! |
| **Configuration System** | ✅ Working | YAML-based config |
| **Plugin System** | ✅ Working | Memory, scheduler, PE, network |

### ⚠️ Partially Implemented (Placeholders)

| Component | Status | Reality |
|-----------|--------|---------|
| **PE Execution** | ⚠️ Placeholder | Just counts cycles |
| **Host Execution** | ⚠️ Placeholder | Just counts cycles |
| **Instruction Simulation** | ⚠️ Placeholder | None! |

### ❌ Planned But Not Implemented

| Component | Status | Notes |
|-----------|--------|-------|
| **zsim Integration** | ❌ Not implemented | zsim_instance_ = nullptr |
| **PIN Instrumentation** | ❌ Not implemented | Would need for zsim |
| **Execution-Driven Host** | ❌ Not implemented | Currently analytical |
| **Execution-Driven Device** | ❌ Not implemented | Currently analytical |

---

## So What Kind of Model Is PIMID Currently?

### Current Model: Cycle-Based Analytical

```cpp
// This is what PIMID currently does:

Cycle offload_completion_cycles_ = 100;  // From config file

void run(Cycle num_cycles) {
    while (current_cycle_ < target_cycle) {
        // Check if task should complete
        if (current_cycle_ - start >= offload_completion_cycles_) {
            task_complete();  // No actual execution!
        }
        current_cycle_++;  // Just advance counter
    }
}
```

**Category**:
- ✅ Cycle-accurate (tracks cycles)
- ✅ Configurable (latencies from YAML)
- ❌ NOT execution-driven (doesn't run instructions)
- ❌ NOT event-driven (doesn't use EventQueue)
- ✅ Simple analytical model (cycle counts from config)

### Comparison to My Proposed "Event-Driven" Model

**Current PIMID**:
```cpp
// Cycle-by-cycle loop (inefficient for large simulations)
for (cycle = 0; cycle < 1000000; cycle++) {
    if (offload started && cycle == completion_cycle) {
        complete_offload();
    }
}
// Problem: Must iterate through EVERY cycle!
```

**My Proposed Event-Driven**:
```cpp
// Jump directly to next event (efficient!)
event_queue.schedule(completion_cycle, complete_offload);
while (event_queue.has_events()) {
    event = event_queue.pop_next();
    current_cycle = event.cycle;  // Jump to event time!
    event.execute();
}
// Benefit: Skip idle cycles!
```

**Both are analytical, but event-driven is FASTER for sparse events!**

---

## Why the Confusion?

### 1. Documentation vs Implementation Gap

**Documentation says**: "Uses ZSim"
**Code says**: `zsim_instance_ = nullptr; // TODO`

This is normal for research projects - document the vision, implement incrementally.

### 2. EventQueue Exists But Unused

```cpp
// pimid/include/common/simulation_engine.h
class SimulationEngine {
    std::shared_ptr<EventQueue> event_queue_;  // ✅ Exists!
};

// But in pimid/src/device_engine/device_engine.cpp
void run(Cycle num_cycles) {
    // EventQueue is created but NEVER USED!
    // Just uses simple cycle loop instead
}
```

The infrastructure is there, but not utilized yet!

### 3. "Event-Driven" Terminology

When I proposed "event-driven core models," I meant:
- Use the existing EventQueue infrastructure
- Replace cycle-by-cycle loop with event-based jumps
- Still analytical (no instruction execution)
- Just MORE EFFICIENT

---

## Timeline: Evolution of PIMID

### Phase 1: Current State (Cycle-Based Analytical)

```
What exists:
✅ Simple cycle loop
✅ Configurable latencies
✅ Memory models (Ramulator, etc.)
✅ Network models (GARNET)
✅ Host-device communication
✅ EventQueue (infrastructure only)
```

### Phase 2: Event-Driven Analytical (My Proposal)

```
What would change:
🔧 Use EventQueue instead of cycle loop
🔧 Jump to events (faster simulation)
✅ Keep analytical models (no instructions)
✅ Add PE plugin execution models
```

**Benefit**: 100-1000x faster for sparse events

### Phase 3: zsim Integration (Future Plan)

```
What would be added:
🔧 Actual zsim integration
🔧 PIN instrumentation
🔧 Execute real binaries
🔧 Instruction-level simulation
```

**Benefit**: Much higher accuracy, but slower

### Phase 4: Hybrid (Ultimate Goal?)

```
Ideal configuration:
🎯 Host: zsim (detailed execution)
🎯 Device: Event-driven analytical (fast)
🎯 Best of both worlds!
```

---

## What This Means for Your Research

### Current Capabilities

**PIMID can currently:**
- ✅ Model memory hierarchies (accurate with Ramulator)
- ✅ Model network topologies (accurate with GARNET)
- ✅ Compare different PIM architectures
- ✅ Study data movement patterns
- ✅ Evaluate different schedulers
- ✅ Test different memory technologies

**PIMID currently cannot:**
- ❌ Execute actual application binaries
- ❌ Simulate instruction-level behavior
- ❌ Model complex CPU microarchitecture
- ❌ Capture instruction dependencies
- ❌ Model branch prediction accurately

### When Is This Sufficient?

**Current model is GOOD for:**
- High-level architecture exploration
- Data movement studies
- Memory bandwidth analysis
- Network topology comparison
- Power/performance trends
- Large design space exploration (fast!)

**Current model is INSUFFICIENT for:**
- Workload characterization (need real apps)
- Detailed CPU-PIM interaction
- Instruction-level optimization
- Accurate IPC modeling
- Cache behavior for complex code

---

## Recommendations

### For Your Use Case

**If you need**:
1. **Fast design space exploration** → Current PIMID is perfect!
2. **Memory hierarchy studies** → Current PIMID works well!
3. **Large-scale PIM arrays (1000s of PEs)** → Event-driven upgrade recommended
4. **Detailed instruction simulation** → Need zsim integration

### Upgrade Path

**Near-term (1-2 weeks)**:
- Implement event-driven simulation (use EventQueue)
- Faster simulation for large-scale studies
- Still analytical, just more efficient

**Mid-term (1-2 months)**:
- Add zsim for host (detailed CPU)
- Keep analytical for device (fast PEs)
- Hybrid approach

**Long-term (3-6 months)**:
- Full zsim integration for both
- Execution-driven throughout
- Maximum accuracy

---

## Summary

| Question | Answer |
|----------|--------|
| **Is PIMID zsim-based?** | ❌ No, not currently. zsim_instance_ = nullptr |
| **What is it then?** | ✅ In-house cycle-based analytical model |
| **Is it event-driven?** | ⚠️ Has EventQueue but doesn't use it yet |
| **Is EventQueue implemented?** | ✅ Yes, infrastructure exists |
| **What does it actually simulate?** | Cycle counts from configuration files |
| **Can it execute binaries?** | ❌ No, analytical model only |
| **Is this a problem?** | Depends on your research goals! |
| **Should we integrate zsim?** | Depends: fast exploration or detailed accuracy? |

**Bottom Line**: PIMID is currently a **fast analytical simulator** with infrastructure ready for both event-driven and execution-driven upgrades. The documentation describes the vision; the code implements a practical starting point.
