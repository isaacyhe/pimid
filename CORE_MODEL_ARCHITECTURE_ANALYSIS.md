# Core Model Architecture Analysis: zsim Traces vs Event-Driven PE Models

## Executive Summary

**Question**: zsim feeds traces to other models in PIMID - would event-driven core models work for PIMID through the PE plugin system?

**Short Answer**:
- ✅ **YES** - Event-driven core models can work with PIMID
- ⚠️ **BUT** - The current PE plugin system focuses on **PE architecture specification**, not **simulation methodology**
- 🔧 **Needs Extension** - To fully support event-driven core models, we need to extend the plugin interface

---

## Current Architecture

### 1. How zsim Integrates with PIMID

#### zsim's Role

**zsim is EXECUTION-DRIVEN, not trace-driven:**

```cpp
// zsim/src/core.h - Base Core class
class Core {
    virtual uint64_t getInstrs() const = 0;
    virtual uint64_t getCycles() const = 0;
    virtual InstrFuncPtrs GetFuncPtrs() = 0;  // PIN instrumentation
};
```

**How it works:**
1. **PIN Instrumentation**: zsim uses Intel PIN to intercept actual program execution
2. **BBL Analysis**: Breaks execution into Basic Blocks (BBL)
3. **Cycle-Accurate Simulation**: Simulates each instruction's timing
4. **Memory Traces**: Generates memory access traces as a *byproduct* of execution

```
Application Binary
      ↓ (PIN instrumentation)
   zsim Core
      ↓ (execution simulation)
   Memory Accesses → fed to PIMID memory models
      ↓
   PIMID Memory Hierarchy (Ramulator, GARNET, etc.)
```

#### Current Integration Points

**Host Engine** (pimid/src/host_engine/host_engine.cpp:64):
```cpp
// Initialize ZSim for host CPU simulation
void HostEngine::initializeZSim() {
    // zsim simulates host cores
    // Generates memory access traces
}
```

**Device Engine** (pimid/src/device_engine/device_engine.cpp:58):
```cpp
// Initialize ZSim for PE simulation
void DeviceEngine::initializeZSim() {
    // zsim configured to simulate PEs
    // PEs execute PIM kernels
}
```

---

### 2. PIMID's Event-Driven Infrastructure

**Good News**: PIMID already has event-driven infrastructure!

**EventQueue** (pimid/include/common/event_queue.h):
```cpp
class EventQueue {
public:
    void scheduleEvent(EventType type, Cycle cycle,
                      uint32_t priority,
                      std::function<void()> callback);

    void processEvents(Cycle until_cycle);
    Cycle getNextEventCycle() const;
};
```

**Supported Event Types**:
```cpp
enum class EventType {
    MEMORY_RESPONSE,    // Memory operation completion
    NETWORK_ARRIVAL,    // NoC packet arrival
    OFFLOAD_COMPLETE,   // PIM task completion
    SYNCHRONIZATION,    // Host-device sync
    CUSTOM              // User-defined events
};
```

**Simulation Engine** (pimid/include/common/simulation_engine.h:58):
```cpp
class SimulationEngine {
protected:
    std::shared_ptr<EventQueue> event_queue_;  // ✅ Already has event queue!
};
```

---

## PE Plugin System (What I Just Implemented)

### Current Scope: Architecture Specification

The PE plugin system I created focuses on **describing PE characteristics**, not **how to simulate them**:

```cpp
// pimid/include/plugin/pe_plugin.h
struct PEArchitecture {
    std::string name;
    std::vector<PECapability> capabilities;  // What ops it can do
    uint32_t num_registers;                  // Register file size
    uint32_t pipeline_stages;                // Pipeline depth
    double frequency_mhz;                    // Clock frequency
    double area_mm2;                         // Physical area
    double power_mw;                         // Power consumption
};

class ProcessingElement {
    // Represents a PE instance
    virtual Cycle getOperationLatency(const std::string& op) const;
    virtual double getOperationEnergy(const std::string& op) const;
};
```

**What it provides:**
- ✅ PE architectural parameters
- ✅ Operation latencies
- ✅ Energy per operation
- ✅ Capability description

**What it doesn't provide:**
- ❌ Instruction-level simulation
- ❌ Execution model (in-order, out-of-order, etc.)
- ❌ Functional simulation
- ❌ Integration with event queue

---

## The Gap: Execution-Driven vs Event-Driven

### Current: Execution-Driven (zsim)

```
┌─────────────────────────────────────┐
│  Application (actual execution)     │
└──────────────┬──────────────────────┘
               │ PIN instrumentation
               ↓
┌─────────────────────────────────────┐
│  zsim Core (execution simulation)   │
│  - Fetch/Decode/Execute pipeline    │
│  - Functional simulation             │
│  - Timing simulation                 │
└──────────────┬──────────────────────┘
               │ Memory accesses
               ↓
┌─────────────────────────────────────┐
│  PIMID Memory Models                │
│  (Ramulator, GARNET, etc.)          │
└─────────────────────────────────────┘
```

**Characteristics:**
- ✅ Functional correctness (executes real code)
- ✅ Detailed microarchitecture simulation
- ❌ Requires binary instrumentation (PIN)
- ❌ Harder to model custom accelerators
- ❌ Tightly coupled to x86/ARM ISA

### Desired: Event-Driven Core Models

```
┌─────────────────────────────────────┐
│  High-Level Task Description        │
│  (kernel launch, dependencies)      │
└──────────────┬──────────────────────┘
               │ Task parameters
               ↓
┌─────────────────────────────────────┐
│  Event-Driven PE Model              │
│  - Schedules completion event       │
│  - Models operation latencies       │
│  - Generates memory accesses        │
└──────────────┬──────────────────────┘
               │ Events + Memory traces
               ↓
┌─────────────────────────────────────┐
│  PIMID EventQueue + Memory Models   │
└─────────────────────────────────────┘
```

**Characteristics:**
- ✅ Flexible for custom accelerators
- ✅ Fast simulation (no instruction-level detail)
- ✅ Easy to model novel architectures
- ❌ Less detailed than execution-driven
- ❌ Requires analytical models for operations

---

## Would Event-Driven Core Models Work?

### YES, But Needs Extension

**What Works Today:**

1. **EventQueue Infrastructure** ✅
   - Already exists in PIMID
   - Can schedule and process events
   - Supports custom event types

2. **PE Architecture Specification** ✅
   - PE plugin defines capabilities and latencies
   - Can specify operation characteristics

3. **Memory Model Integration** ✅
   - Memory models already work with event-driven timing
   - Ramulator and GARNET use event-driven simulation

**What's Missing:**

1. **Event-Driven Execution Model** ❌
   ```cpp
   // Need to add to PE plugin interface:
   class IEventDrivenPE {
       // Launch a kernel/task
       virtual void launchKernel(const Task& task,
                                EventQueue* eq,
                                Cycle current_cycle) = 0;

       // Generate memory access events
       virtual void generateMemoryAccesses(
           const Task& task,
           std::vector<MemoryAccess>& accesses) = 0;
   };
   ```

2. **Task-Based Execution** ❌
   ```cpp
   // Need task descriptor:
   struct Task {
       std::string kernel_name;
       uint64_t input_size;
       std::vector<uint64_t> input_addresses;
       std::vector<uint64_t> output_addresses;
       std::map<std::string, double> parameters;
   };
   ```

3. **Hybrid Mode Support** ❌
   - Switch between execution-driven (zsim) and event-driven
   - Allow mixing both models in same simulation

---

## Proposed Extension: Event-Driven PE Plugin Interface

### Extended PE Plugin Interface

```cpp
// pimid/include/plugin/pe_plugin_extended.h

namespace pimid {
namespace plugin {

/**
 * @brief Event-driven PE execution model
 *
 * Extends the base PE plugin to support event-driven simulation
 * where PEs execute tasks/kernels rather than individual instructions.
 */
class IEventDrivenPEPlugin : public IPEPlugin {
public:
    virtual ~IEventDrivenPEPlugin() = default;

    // -------------------------------------------------------------------------
    // Execution Model
    // -------------------------------------------------------------------------

    /**
     * @brief Launch a task/kernel on this PE
     *
     * @param task Task descriptor
     * @param pe_id PE identifier
     * @param event_queue Event queue for scheduling completion
     * @param current_cycle Current simulation cycle
     * @return Estimated completion cycle
     */
    virtual Cycle launchTask(
        const Task& task,
        uint32_t pe_id,
        EventQueue* event_queue,
        Cycle current_cycle) = 0;

    /**
     * @brief Generate memory access pattern for a task
     *
     * This allows the PE model to describe what memory accesses
     * the task will generate without executing individual instructions.
     *
     * @param task Task descriptor
     * @param accesses Output: generated memory accesses
     */
    virtual void generateMemoryAccessPattern(
        const Task& task,
        std::vector<MemoryAccess>& accesses) = 0;

    /**
     * @brief Estimate task execution time
     *
     * @param task Task descriptor
     * @param pe_arch PE architecture
     * @return Estimated execution cycles
     */
    virtual Cycle estimateExecutionCycles(
        const Task& task,
        const PEArchitecture& pe_arch) const = 0;

    // -------------------------------------------------------------------------
    // Performance Modeling
    // -------------------------------------------------------------------------

    /**
     * @brief Get compute intensity (ops/byte) for a task
     */
    virtual double getComputeIntensity(const Task& task) const = 0;

    /**
     * @brief Get memory bandwidth requirement (bytes/sec)
     */
    virtual double getMemoryBandwidth(const Task& task) const = 0;

    /**
     * @brief Check if task is compute-bound or memory-bound
     */
    virtual bool isComputeBound(const Task& task) const = 0;

    // -------------------------------------------------------------------------
    // Event Handling
    // -------------------------------------------------------------------------

    /**
     * @brief Callback when task completes
     */
    virtual void onTaskComplete(
        uint32_t pe_id,
        const Task& task,
        Cycle completion_cycle) = 0;

    /**
     * @brief Callback when memory operation completes
     */
    virtual void onMemoryComplete(
        uint32_t pe_id,
        const MemoryAccess& access,
        Cycle completion_cycle) = 0;
};

/**
 * @brief Task descriptor for event-driven simulation
 */
struct Task {
    // Task identification
    uint64_t task_id;
    std::string kernel_name;  // e.g., "vector_add", "matmul", "reduction"

    // Input/output data
    uint64_t input_size;      // bytes
    uint64_t output_size;     // bytes
    std::vector<uint64_t> input_addresses;
    std::vector<uint64_t> output_addresses;

    // Operation counts (for analytical modeling)
    uint64_t num_ops;         // Total operations
    uint64_t num_loads;       // Load operations
    uint64_t num_stores;      // Store operations
    uint64_t num_flops;       // Floating-point ops

    // Dependencies
    std::vector<uint64_t> depends_on;  // Task IDs this depends on

    // User-defined parameters
    std::map<std::string, double> parameters;
};

/**
 * @brief Memory access descriptor
 */
struct MemoryAccess {
    uint64_t address;
    uint64_t size;            // bytes
    bool is_read;             // true = read, false = write
    Cycle issue_cycle;        // When issued
    uint32_t pe_id;           // Which PE issued it

    // Optional: access pattern info
    enum class Pattern {
        SEQUENTIAL,
        STRIDED,
        RANDOM
    } pattern = Pattern::SEQUENTIAL;

    uint64_t stride = 0;      // For strided accesses
};

} // namespace plugin
} // namespace pimid
```

### Example Event-Driven PE Plugin

```cpp
/**
 * @brief Simple vector PE with event-driven execution
 */
class EventDrivenVectorPEPlugin : public IEventDrivenPEPlugin {
public:
    Cycle launchTask(const Task& task,
                    uint32_t pe_id,
                    EventQueue* event_queue,
                    Cycle current_cycle) override {

        // Estimate execution time based on task
        Cycle exec_cycles = estimateExecutionCycles(task, arch_);

        // Generate memory accesses
        std::vector<MemoryAccess> accesses;
        generateMemoryAccessPattern(task, accesses);

        // Schedule memory accesses as events
        for (const auto& access : accesses) {
            event_queue->scheduleEvent(
                EventType::MEMORY_RESPONSE,
                current_cycle + access.issue_cycle,
                0,  // priority
                [this, pe_id, access, event_queue]() {
                    // Handle memory access completion
                    this->onMemoryComplete(pe_id, access,
                                          event_queue->getCurrentCycle());
                }
            );
        }

        // Schedule task completion
        Cycle completion_cycle = current_cycle + exec_cycles;
        event_queue->scheduleEvent(
            EventType::CUSTOM,
            completion_cycle,
            1,  // higher priority
            [this, pe_id, task, completion_cycle]() {
                this->onTaskComplete(pe_id, task, completion_cycle);
            }
        );

        return completion_cycle;
    }

    void generateMemoryAccessPattern(const Task& task,
                                     std::vector<MemoryAccess>& accesses) override {

        if (task.kernel_name == "vector_add") {
            // Generate sequential read pattern
            uint64_t elements = task.input_size / sizeof(double);
            uint64_t vector_width = arch_.vector_width;

            for (uint64_t i = 0; i < elements; i += vector_width) {
                // Read A[i:i+vector_width]
                accesses.push_back({
                    task.input_addresses[0] + i * sizeof(double),
                    vector_width * sizeof(double),
                    true,  // read
                    i / vector_width,  // issue cycle
                    0      // pe_id (set by caller)
                });

                // Read B[i:i+vector_width]
                accesses.push_back({
                    task.input_addresses[1] + i * sizeof(double),
                    vector_width * sizeof(double),
                    true,  // read
                    i / vector_width,
                    0
                });

                // Write C[i:i+vector_width]
                accesses.push_back({
                    task.output_addresses[0] + i * sizeof(double),
                    vector_width * sizeof(double),
                    false, // write
                    i / vector_width + 1,  // after computation
                    0
                });
            }
        }
        // Add other kernels (matmul, reduction, etc.)
    }

    Cycle estimateExecutionCycles(const Task& task,
                                 const PEArchitecture& pe_arch) const override {

        if (task.kernel_name == "vector_add") {
            uint64_t elements = task.input_size / sizeof(double);
            uint64_t vector_width = pe_arch.vector_width;

            // Vector add: one cycle per vector
            Cycle compute_cycles = (elements + vector_width - 1) / vector_width;

            // Add pipeline latency
            Cycle total_cycles = compute_cycles + pe_arch.pipeline_stages;

            return total_cycles;
        }

        // Default: use operation count
        return task.num_ops / (pe_arch.frequency_mhz / 1000.0);
    }
};
```

---

## Hybrid Approach: Best of Both Worlds

### Use Case 1: Host Execution-Driven, Device Event-Driven

```yaml
# pimid_config.yaml

simulation:
  # Host uses zsim for detailed CPU simulation
  host_simulation_model: "execution_driven"  # zsim

  # Device uses event-driven for fast PIM simulation
  device_simulation_model: "event_driven"    # Event-driven PE plugins

host:
  cores: 4
  # zsim configuration for host...

device:
  pe_type: "EventDrivenVectorPE"  # Use event-driven plugin
  num_pes: 256
```

**Workflow:**
1. Host executes real binary with zsim (detailed simulation)
2. Host offloads task to device
3. Device uses event-driven PE model (fast simulation)
4. Device generates memory trace to memory models
5. Device sends completion back to host

### Use Case 2: Pure Event-Driven (Fast Simulation)

```yaml
simulation:
  host_simulation_model: "event_driven"
  device_simulation_model: "event_driven"

# Both host and device use analytical models
# Much faster, but less accurate
```

### Use Case 3: Pure Execution-Driven (Detailed Simulation)

```yaml
simulation:
  host_simulation_model: "execution_driven"
  device_simulation_model: "execution_driven"

# Both use zsim
# Slower, but more accurate
```

---

## Implementation Roadmap

### Phase 1: Minimal Event-Driven Support (1-2 weeks)

1. **Extend PE Plugin Interface**
   - Add `IEventDrivenPEPlugin` interface
   - Add `Task` and `MemoryAccess` structures
   - Add event callback methods

2. **Implement Example Plugin**
   - Create `EventDrivenVectorPEPlugin`
   - Implement vector_add, matmul kernels
   - Demonstrate integration with EventQueue

3. **DeviceEngine Integration**
   - Add task submission API
   - Connect event-driven PEs to EventQueue
   - Support both zsim and event-driven modes

### Phase 2: Full Integration (2-4 weeks)

1. **Task Scheduler Extension**
   - Support task-based scheduling
   - Dependency tracking
   - Resource allocation

2. **Trace Generation**
   - Generate memory traces from analytical models
   - Feed to Ramulator/GARNET
   - Validate against execution-driven traces

3. **Hybrid Mode**
   - Allow mixing execution-driven and event-driven
   - Synchronization between modes
   - Performance optimization

### Phase 3: Advanced Features (4+ weeks)

1. **ML-Based Performance Models**
   - Train models on real execution data
   - Use for fast event-driven simulation

2. **Roofline Model Integration**
   - Compute/memory-bound analysis
   - Performance prediction

3. **Trace Replay Mode**
   - Record zsim traces
   - Replay with event-driven models

---

## Conclusion

### ✅ YES, Event-Driven Core Models Can Work

**Current Status:**
- PIMID has EventQueue infrastructure ✅
- PE plugin system defines architecture ✅
- Memory models work event-driven ✅

**What's Needed:**
- Extend PE plugin interface for event-driven execution ⚙️
- Add task-based execution model ⚙️
- Integrate with existing EventQueue ⚙️

**Effort**: ~2-4 weeks for full implementation

**Benefits:**
- ✅ Fast simulation for large-scale PIM systems
- ✅ Easy to model custom accelerators
- ✅ Flexibility in accuracy/speed tradeoff
- ✅ Hybrid execution/event-driven modes

### Recommendation

**Start with hybrid approach:**
1. Keep zsim for host (detailed CPU simulation)
2. Add event-driven support for PIM PEs (fast, flexible)
3. Validate against full execution-driven runs
4. Gradually extend event-driven models

This gives you the best of both worlds: accuracy where you need it (host), speed where it matters (massive PIM arrays).
