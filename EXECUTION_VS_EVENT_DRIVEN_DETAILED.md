# Execution-Driven vs Event-Driven Simulation: A Detailed Comparison

## TL;DR - Correcting Common Misconceptions

**Common Misconception**: "gem5 is event-driven, zsim is execution-driven"

**Reality**:
- ❌ **WRONG**: gem5 is event-driven, zsim is execution-driven
- ✅ **CORRECT**: Both gem5 AND zsim are execution-driven simulators that use discrete event simulation for TIMING
- 🎯 **Key Difference**: gem5 and zsim differ in HOW they execute code, not WHETHER they execute it

---

## What Does "Event-Driven" Actually Mean?

### Two Different Meanings (Source of Confusion!)

#### 1. **Event-Driven TIMING** (Discrete Event Simulation)
```
Used by: gem5, zsim, Ramulator, GARNET, most simulators
```

**How it works:**
```cpp
// Event queue manages timing
while (event_queue.has_events()) {
    Event e = event_queue.pop_next();
    current_time = e.time;
    e.execute();  // Could be instruction execution, memory response, etc.
}
```

**Both gem5 and zsim use this!** This is just about managing WHEN things happen.

#### 2. **Event-Driven EXECUTION** (High-Level Task Models)
```
Used by: Analytical models, task-based simulators
```

**How it works:**
```cpp
// No instruction-level execution!
Task task = {kernel: "vector_add", size: 1M};
Cycle completion_time = analytical_model.estimate(task);
event_queue.schedule(completion_time, task_done_callback);
```

**Neither gem5 nor zsim use this!** They both execute actual instructions.

---

## The Real Spectrum: How Simulators Execute Code

```
Low Detail ←────────────────────────────────────────→ High Detail
(Fast)                                                    (Slow)

┌─────────────┬──────────────┬──────────────┬──────────────┬─────────────┐
│ Analytical  │ Trace-Driven │  Functional  │  Detailed    │ RTL/Gate    │
│ Models      │              │  Simulation  │  μArch Sim   │ Level       │
└─────────────┴──────────────┴──────────────┴──────────────┴─────────────┘
     │              │               │              │              │
     │              │               │              │              │
  Task-based    Replay          Simple         Complex       Verilog
  estimation    traces          cores          cores         simulation

                            ↑                   ↑
                         gem5 SE            gem5 FS
                         (simple)           zsim
                                           (detailed)
```

---

## Execution-Driven Simulators: gem5 vs zsim

### Both Are Execution-Driven!

**What "execution-driven" means:**
- ✅ Execute actual program instructions (not just estimate)
- ✅ Maintain architectural state (registers, memory)
- ✅ Model functional behavior correctly
- ✅ Simulate timing of execution

### gem5 Architecture

```
┌─────────────────────────────────────────────────┐
│  Application Binary (cross-compiled)            │
└──────────────────┬──────────────────────────────┘
                   │
                   ↓
┌─────────────────────────────────────────────────┐
│  gem5 ISA Interpreter                           │
│  - Decodes instructions                         │
│  - Executes functionally                        │
│  - Updates architectural state                  │
└──────────────────┬──────────────────────────────┘
                   │
                   ↓
┌─────────────────────────────────────────────────┐
│  CPU Model (timing simulation)                  │
│  - AtomicSimple: 1 CPI                         │
│  - TimingSimple: Basic pipeline                │
│  - O3CPU: Out-of-order, superscalar            │
└──────────────────┬──────────────────────────────┘
                   │
                   ↓
┌─────────────────────────────────────────────────┐
│  Event Queue (discrete event simulation)        │
│  - Memory events, branch events, etc.          │
└─────────────────────────────────────────────────┘
```

**Key Characteristics:**
- ✅ Executes cross-compiled binaries natively
- ✅ Very flexible CPU models (atomic → out-of-order)
- ✅ Can boot full operating system (FS mode)
- ✅ Detailed cache coherence protocols
- ❌ SLOW for multi-core (serial event processing)
- ❌ Requires cross-compilation

### zsim Architecture

```
┌─────────────────────────────────────────────────┐
│  Application Binary (native x86/ARM)            │
└──────────────────┬──────────────────────────────┘
                   │ PIN instrumentation
                   ↓
┌─────────────────────────────────────────────────┐
│  Intel PIN                                      │
│  - Intercepts every instruction                 │
│  - Inserts analysis code                        │
│  - Native execution speed                       │
└──────────────────┬──────────────────────────────┘
                   │
                   ↓
┌─────────────────────────────────────────────────┐
│  zsim Timing Models                             │
│  - Core models (OoO, simple)                   │
│  - Cache models                                │
│  - Interval simulation                         │
└──────────────────┬──────────────────────────────┘
                   │
                   ↓
┌─────────────────────────────────────────────────┐
│  Bound-Weave Parallelism                        │
│  - Parallel simulation of cores                 │
│  - Periodic synchronization                     │
└─────────────────────────────────────────────────┘
```

**Key Characteristics:**
- ✅ Uses binary instrumentation (PIN)
- ✅ Run native binaries (no cross-compilation!)
- ✅ FAST multi-core simulation (parallel bound-weave)
- ✅ Interval simulation for speed
- ❌ Tied to x86/ARM ISAs (PIN limitation)
- ❌ Less flexible than gem5 for new CPU models

---

## Why Execution-Driven is Slower Than Event-Driven

### What "Event-Driven" Means in Our Context

When I said "event-driven core models would be faster," I meant **analytical/task-based models**, not gem5-style simulation!

```
┌──────────────────────────────────────────────────────────────┐
│  EXECUTION-DRIVEN (gem5, zsim)                               │
│  Must simulate EVERY instruction                             │
└──────────────────────────────────────────────────────────────┘

Application: for (i = 0; i < 1000000; i++) C[i] = A[i] + B[i];

Simulator must execute:
1. i = 0
2. load A[0]
3. load B[0]
4. add
5. store C[0]
6. i++
7. compare i < 1000000
8. branch
9. load A[1]
10. load B[1]
... (8 million instructions!)

Each instruction:
- Fetch from I-cache
- Decode
- Execute
- Writeback
- Update branch predictor
- Check for dependencies
- Model pipeline stalls
```

vs

```
┌──────────────────────────────────────────────────────────────┐
│  ANALYTICAL EVENT-DRIVEN (what I proposed for PIMID)        │
│  High-level task description                                 │
└──────────────────────────────────────────────────────────────┘

Task: vector_add(A, B, C, size=1M)

Model:
1. Compute cycles = size / vector_width / frequency
2. Memory accesses = 3 * size / cache_line_size
3. Schedule completion event
4. Done!

No instruction-level simulation needed!
Estimate based on analytical model of operation.
```

---

## Detailed Comparison

### 1. Execution-Driven (gem5, zsim)

**What Gets Simulated:**

```cpp
// Every single instruction!
for (uint64_t pc = start_pc; ; ) {
    Instruction inst = fetch(pc);

    // Decode
    DecodedInst decoded = decode(inst);

    // Execute
    if (decoded.is_load) {
        // Detailed memory hierarchy simulation
        Cycle latency = cache_hierarchy.access(decoded.addr);
        wait(latency);
        register[decoded.dest] = memory[decoded.addr];
    }

    // Branch prediction
    if (decoded.is_branch) {
        bool predicted = branch_predictor.predict(pc);
        bool actual = evaluate_branch(decoded);
        if (predicted != actual) {
            pipeline.flush();
            wait(misprediction_penalty);
        }
    }

    // Update PC
    pc = next_pc(decoded);

    // Update microarchitectural state
    reorder_buffer.commit(decoded);
    register_file.update();
    // ... hundreds of lines of detailed state updates
}
```

**Slowdown Factors:**
- Must process EVERY instruction (millions/billions)
- Pipeline modeling (fetch, decode, execute, writeback)
- Branch prediction simulation
- Cache simulation for every access
- Out-of-order execution modeling (if using O3CPU)
- Register renaming, ROB management
- Coherence protocol messages

**Example: 1M-element vector add**
- Instructions executed: ~8 million
- Cache accesses simulated: ~3 million
- Branch predictions: ~1 million
- Simulation time: seconds to minutes

### 2. Analytical Event-Driven (Proposed for PIMID PEs)

**What Gets Simulated:**

```cpp
// High-level task only!
void simulate_vector_add(Task task) {
    // Analytical model - no instruction execution!

    uint64_t elements = task.size / sizeof(double);
    uint64_t vector_ops = (elements + VECTOR_WIDTH - 1) / VECTOR_WIDTH;

    // Compute time
    Cycle compute_cycles = vector_ops;  // 1 cycle per vector op

    // Memory time (Roofline model)
    uint64_t bytes_transferred = 3 * task.size;  // read A, B, write C
    Cycle memory_cycles = bytes_transferred / BANDWIDTH;

    // Max of compute-bound or memory-bound
    Cycle total_cycles = max(compute_cycles, memory_cycles);

    // Schedule single completion event
    event_queue.schedule(current_time + total_cycles,
                        [task]() { task_complete(task); });

    // Generate memory accesses (for memory system simulation)
    for (uint64_t addr = task.start_addr;
         addr < task.end_addr;
         addr += CACHE_LINE_SIZE) {
        memory_system.access(addr);
    }
}
```

**Speedup Factors:**
- No instruction-level simulation!
- Single analytical model evaluation
- Memory accesses at cache-line granularity (not every load/store)
- No branch prediction needed
- No pipeline modeling
- Direct to completion event

**Example: 1M-element vector add**
- "Instructions executed": 0 (analytical model!)
- Cache accesses simulated: ~48,000 (cache line granularity)
- Model evaluations: 1
- Simulation time: microseconds

---

## Fidelity Comparison: gem5 vs zsim

### Neither is "Higher Fidelity" - They're Different!

| Aspect | gem5 | zsim |
|--------|------|------|
| **Instruction Execution** | Interprets cross-compiled binary | Instruments native binary (PIN) |
| **CPU Models** | Very flexible (Atomic→O3) | Fixed models (Simple, OoO) |
| **Multi-core Speed** | Slow (serial events) | Fast (bound-weave parallel) |
| **OS Support** | Full system (boot Linux) | System call emulation |
| **ISA Support** | Any (ARM, x86, RISC-V, etc.) | x86/ARM only (PIN limitation) |
| **Cache Coherence** | Very detailed protocols | Simplified coherence |
| **Accuracy** | Can be very high | Can be very high |
| **Development Effort** | New CPU model: weeks/months | New CPU model: difficult |
| **Scalability** | Poor (1000s of cores = slow) | Excellent (1000s of cores OK) |

**Use Cases:**
- **gem5**: Research new CPU microarchitectures, new ISAs, full system
- **zsim**: Large-scale multi-core, datacenter, cache studies

### For PIMID Context

**Why zsim for PIMID?**
- ✅ Need to simulate thousands of PEs → zsim's parallelism wins
- ✅ PEs are simple cores → don't need gem5's flexibility
- ✅ Want fast simulation → zsim is faster for multi-core

**Could use gem5?**
- Yes, but would be MUCH slower for 1000+ PEs
- Better for detailed single-core PIM studies

---

## The Proposed PIMID Approach: Hybrid

### Why Hybrid Makes Sense

```yaml
simulation:
  # Host: Detailed execution-driven (complex CPU needs accuracy)
  host_simulation_model: "execution_driven"  # zsim or gem5

  # Device: Analytical event-driven (simple PEs, need speed)
  device_simulation_model: "event_driven"    # Analytical models
```

**Rationale:**

1. **Host Cores (4-8 cores)**
   - Complex out-of-order CPUs
   - Need accurate modeling
   - Small number → execution-driven OK
   - Use zsim for speed or gem5 for flexibility

2. **PIM PEs (100s-1000s)**
   - Simple in-order cores
   - Repetitive operations (vector add, matmul)
   - Large number → execution-driven TOO SLOW
   - Use analytical models for speed

### Speed Comparison

**Scenario: 1024 PIM PEs, each executing vector add (1M elements)**

```
Full Execution-Driven (gem5 or zsim for ALL PEs):
- Instructions simulated: 1024 PEs × 8M instructions = 8 billion
- Estimated simulation time: HOURS
- Memory: High (all PE state)

Hybrid (zsim host + analytical device):
- Host instructions: ~100K (host code only)
- PE simulation: 1024 analytical models
- Estimated simulation time: MINUTES
- Memory: Low (no detailed PE state)

Speedup: 100-1000x faster!
```

---

## Common Simulator Categorization (Corrected)

### By Execution Method

```
┌─────────────────────────────────────────────────────────────┐
│  EXECUTION-DRIVEN (Execute actual code)                     │
├─────────────────────────────────────────────────────────────┤
│  gem5:    Cross-compiled binary, interpretive execution    │
│  zsim:    Native binary, PIN instrumentation               │
│  Sniper:  Similar to zsim, interval simulation             │
│  QEMU:    Just-In-Time compilation, fast functional        │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  TRACE-DRIVEN (Replay recorded traces)                      │
├─────────────────────────────────────────────────────────────┤
│  Dinero:    Cache simulation from traces                   │
│  Cheetah:   Cache simulation from traces                   │
│  Any simulator with trace input mode                       │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  ANALYTICAL (Mathematical models, no execution)             │
├─────────────────────────────────────────────────────────────┤
│  Roofline:     Performance bounds analysis                 │
│  CACTI:        Cache access time/energy                    │
│  McPAT:        Power modeling                              │
│  What I proposed for PIMID PEs: Task-based models          │
└─────────────────────────────────────────────────────────────┘
```

### By Timing Method

```
ALL OF THE ABOVE use "discrete event simulation" for timing!

Event Queue:
  Event 1: Instruction fetch complete at cycle 100
  Event 2: Cache miss response at cycle 150
  Event 3: Branch misprediction at cycle 175
  ...

This is just about managing WHEN things happen.
Not about HOW we model WHAT happens.
```

---

## Why the Confusion Exists

### Overloaded Terminology

The term **"event-driven"** means different things:

1. **Event-driven timing** = Discrete event simulation
   - Used by: Everything (gem5, zsim, Ramulator, GARNET)
   - Just a timing management technique

2. **Event-driven execution** = No instruction-level simulation
   - Used by: Analytical models, task-based simulators
   - Fundamentally different execution approach

**When people say "gem5 is event-driven":**
- They mean: gem5 uses discrete event simulation for TIMING ✅
- They DON'T mean: gem5 doesn't execute instructions ❌

**When I said "event-driven core models for PIMID":**
- I meant: Analytical task-based models (no instruction execution)
- I did NOT mean: Just using event queues for timing

---

## Summary

### Key Takeaways

1. **gem5 and zsim are BOTH execution-driven**
   - Both execute actual instructions
   - Both use discrete event simulation for timing
   - Different in HOW they execute, not WHETHER

2. **"Event-driven" has two meanings**
   - Timing management (discrete event simulation) ← Everyone uses this
   - Execution model (analytical, no instructions) ← What I proposed for PIM PEs

3. **Fidelity: gem5 vs zsim**
   - NOT about one being higher fidelity
   - gem5: More flexible, slower, better for architecture research
   - zsim: Faster multi-core, less flexible, better for scale

4. **Why analytical models are faster**
   - Don't execute billions of instructions
   - Use mathematical models for task completion
   - 100-1000x speedup for simple repetitive operations

5. **Hybrid is best for PIMID**
   - Host: Execution-driven (complex CPU, need accuracy)
   - Device: Analytical (simple PEs, need speed)
   - Best of both worlds!

### Recommended Reading

If you want to dive deeper:

- **gem5 documentation**: Different CPU models explained
- **zsim paper** (ISCA 2013): Bound-weave parallelism
- **Interval simulation**: How to speed up detailed simulation
- **Roofline model**: Analytical performance modeling

---

## Analogy

Think of it like rendering graphics:

**Execution-Driven (gem5/zsim)** = Ray tracing
- Simulate every photon bounce
- Very accurate
- Very slow
- Great for single detailed scene

**Analytical Event-Driven** = Rasterization
- Use formulas to estimate lighting
- Less physically accurate
- Very fast
- Great for real-time with many objects

**PIMID Hybrid** = Modern game engine
- Ray tracing for important reflections (host CPU)
- Rasterization for distant objects (PIM PEs)
- Best visual quality per second!
