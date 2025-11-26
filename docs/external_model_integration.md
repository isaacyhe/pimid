# External Model Integration Guide

## Overview

PIMID uses a **lightweight adapter interface** that allows you to integrate existing network and memory models **without modifying them**. You only need to provide a thin wrapper that maps your model's API to PIMID's interface.

## Design Philosophy

✅ **Your model stays unchanged** - No need to inherit from base classes or rewrite code
✅ **Simple C-style callbacks** - Maximum compatibility with any language/model
✅ **Dynamic loading** - Load models at runtime via shared libraries
✅ **Minimal adapter** - ~50-100 lines of glue code

## Integration Methods

### Method 1: Direct Registration (Simplest)

For models already linked into your binary:

```cpp
#include "external_models/include/external_model_interface.h"
#include "your_model.h"  // Your existing model header

// Step 1: Write thin wrapper functions
static int my_model_init(PimidModelHandle handle, const char* config_file) {
    YourModel* model = (YourModel*)handle;
    return model->initialize(config_file) ? 0 : -1;
}

static int my_model_send(PimidModelHandle handle, const PimidNetworkPacket* pkt) {
    YourModel* model = (YourModel*)handle;
    // Translate PIMID packet to your model's format
    YourPacket your_pkt = {pkt->src_id, pkt->dst_id, pkt->size_bytes};
    return model->send(your_pkt);
}

static uint64_t my_model_latency(PimidModelHandle handle, uint32_t src, uint32_t dst, uint64_t bytes) {
    YourModel* model = (YourModel*)handle;
    return model->get_latency(src, dst, bytes);
}

static void my_model_tick(PimidModelHandle handle) {
    YourModel* model = (YourModel*)handle;
    model->advance_cycle();
}

static void my_model_stats(PimidModelHandle handle, PimidNetworkStats* stats) {
    YourModel* model = (YourModel*)handle;
    YourStats s = model->get_statistics();
    stats->total_packets = s.packets;
    stats->total_bytes = s.bytes;
    stats->avg_latency = s.latency;
}

static void my_model_destroy(PimidModelHandle handle) {
    YourModel* model = (YourModel*)handle;
    delete model;
}

// Step 2: Register with PIMID
void register_my_model() {
    // Create your model instance
    YourModel* model = new YourModel();

    // Fill descriptor
    PimidModelDescriptor desc;
    desc.name = "YourModel";
    desc.version = "1.0";
    desc.type = PIMID_MODEL_NETWORK;
    desc.handle = (PimidModelHandle)model;

    // Set callbacks
    desc.callbacks.network.init = my_model_init;
    desc.callbacks.network.send_packet = my_model_send;
    desc.callbacks.network.tick = my_model_tick;
    desc.callbacks.network.get_latency = my_model_latency;
    desc.callbacks.network.get_stats = my_model_stats;
    desc.callbacks.network.destroy = my_model_destroy;

    // Register
    pimid_register_model(&desc);
}

// Step 3: Use in PIMID
int main() {
    register_my_model();

    // Now PIMID can use your model
    const PimidModelDescriptor* model = pimid_get_model("YourModel");
    model->callbacks.network.init(model->handle, "config.yaml");

    // Send packet
    PimidNetworkPacket pkt = {0, 1, 64, 0x1000, NULL};
    model->callbacks.network.send_packet(model->handle, &pkt);
}
```

### Method 2: Dynamic Loading (Most Flexible)

For models provided as shared libraries (.so/.dll):

**Step 1: Create adapter shared library**

```cpp
// my_model_adapter.cpp
#include "external_models/include/external_model_interface.h"
#include "your_model.h"

// Wrapper functions (same as above)
// ... [wrapper functions here] ...

// Entry point for dynamic loading
extern "C" {
    PimidModelDescriptor* pimid_model_create() {
        static PimidModelDescriptor desc;
        static YourModel* model = new YourModel();

        desc.name = "YourModel";
        desc.version = "1.0";
        desc.type = PIMID_MODEL_NETWORK;
        desc.handle = (PimidModelHandle)model;

        // Set callbacks
        desc.callbacks.network.init = my_model_init;
        desc.callbacks.network.send_packet = my_model_send;
        desc.callbacks.network.tick = my_model_tick;
        desc.callbacks.network.get_latency = my_model_latency;
        desc.callbacks.network.get_stats = my_model_stats;
        desc.callbacks.network.destroy = my_model_destroy;

        return &desc;
    }
}
```

**Step 2: Build shared library**

```bash
g++ -shared -fPIC my_model_adapter.cpp -o libmymodel_adapter.so \
    -I/path/to/pimid/include \
    -I/path/to/your/model \
    -L/path/to/your/model/lib -lyourmodel
```

**Step 3: Load at runtime**

```cpp
// In your PIMID simulation
pimid_load_model("libmymodel_adapter.so");

// Use the model
const PimidModelDescriptor* model = pimid_get_model("YourModel");
model->callbacks.network.init(model->handle, "config.yaml");
```

### Method 3: Config-Based Loading (Zero Code)

For even less coding, use config files:

**models.yaml:**
```yaml
models:
  - name: BookSim
    type: network
    library: /path/to/libbooksim_adapter.so
    config: booksim_config.txt

  - name: DRAMSim3
    type: memory
    library: /path/to/libdramsim3_adapter.so
    config: dramsim3.ini
```

**In PIMID:**
```cpp
pimid_load_models_from_config("models.yaml");
```

## Real-World Examples

### Example 1: Integrating BookSim (NoC Simulator)

**BookSim adapter** (~60 lines):

```cpp
// booksim_adapter.cpp
#include "external_models/include/external_model_interface.h"
#include "booksim.hpp"  // BookSim header

static int booksim_init(PimidModelHandle h, const char* cfg) {
    BookSimWrapper* bs = (BookSimWrapper*)h;
    return bs->Init(cfg) ? 0 : -1;
}

static int booksim_send(PimidModelHandle h, const PimidNetworkPacket* pkt) {
    BookSimWrapper* bs = (BookSimWrapper*)h;
    Flit* f = Flit::New();
    f->src = pkt->src_id;
    f->dest = pkt->dst_id;
    f->size = pkt->size_bytes;
    return bs->_InjectFlits(&f, 1);
}

static uint64_t booksim_latency(PimidModelHandle h, uint32_t src, uint32_t dst, uint64_t bytes) {
    BookSimWrapper* bs = (BookSimWrapper*)h;
    return bs->GetAverageLatency(src, dst);
}

static void booksim_tick(PimidModelHandle h) {
    BookSimWrapper* bs = (BookSimWrapper*)h;
    bs->_Step();
}

static void booksim_stats(PimidModelHandle h, PimidNetworkStats* stats) {
    BookSimWrapper* bs = (BookSimWrapper*)h;
    stats->total_packets = bs->GetSentFlits();
    stats->total_bytes = bs->GetSentFlits() * 64;
    stats->avg_latency = bs->GetAverageLatency();
}

static void booksim_destroy(PimidModelHandle h) {
    delete (BookSimWrapper*)h;
}

extern "C" {
    PimidModelDescriptor* pimid_model_create() {
        static PimidModelDescriptor desc;
        desc.name = "BookSim";
        desc.version = "2.0";
        desc.type = PIMID_MODEL_NETWORK;
        desc.handle = new BookSimWrapper();

        desc.callbacks.network.init = booksim_init;
        desc.callbacks.network.send_packet = booksim_send;
        desc.callbacks.network.tick = booksim_tick;
        desc.callbacks.network.get_latency = booksim_latency;
        desc.callbacks.network.get_stats = booksim_stats;
        desc.callbacks.network.destroy = booksim_destroy;

        return &desc;
    }
}
```

### Example 2: Integrating DRAMSim3 (Memory Simulator)

**DRAMSim3 adapter** (~50 lines):

```cpp
// dramsim3_adapter.cpp
#include "external_models/include/external_model_interface.h"
#include "dramsim3.h"  // DRAMSim3 header

static int dramsim3_init(PimidModelHandle h, const char* cfg) {
    dramsim3::MemorySystem* mem = (dramsim3::MemorySystem*)h;
    // DRAMSim3 initialized in constructor
    return 0;
}

static int dramsim3_send(PimidModelHandle h, const PimidMemoryRequest* req) {
    dramsim3::MemorySystem* mem = (dramsim3::MemorySystem*)h;
    return mem->WillAcceptTransaction(req->addr, req->is_write)
        ? (mem->AddTransaction(req->addr, req->is_write), 0)
        : -1;
}

static bool dramsim3_ready(PimidModelHandle h, const PimidMemoryRequest* req) {
    // Check completion via callback (DRAMSim3 uses callbacks)
    return true;  // Simplified
}

static void dramsim3_tick(PimidModelHandle h) {
    dramsim3::MemorySystem* mem = (dramsim3::MemorySystem*)h;
    mem->ClockTick();
}

static uint64_t dramsim3_latency(PimidModelHandle h, uint64_t addr, uint64_t bytes, bool is_write) {
    // Return typical latency (DRAMSim3 simulates actual timing)
    return is_write ? 100 : 80;  // Rough estimate
}

static void dramsim3_stats(PimidModelHandle h, PimidMemoryStats* stats) {
    dramsim3::MemorySystem* mem = (dramsim3::MemorySystem*)h;
    // Extract stats from DRAMSim3
    stats->total_reads = 0;  // Get from DRAMSim3
    stats->total_writes = 0;
}

static void dramsim3_destroy(PimidModelHandle h) {
    delete (dramsim3::MemorySystem*)h;
}

extern "C" {
    PimidModelDescriptor* pimid_model_create() {
        static PimidModelDescriptor desc;
        desc.name = "DRAMSim3";
        desc.version = "1.0";
        desc.type = PIMID_MODEL_MEMORY;
        desc.handle = new dramsim3::MemorySystem("DDR4_8Gb_x8_3200.ini", ".", nullptr, nullptr);

        desc.callbacks.memory.init = dramsim3_init;
        desc.callbacks.memory.send_request = dramsim3_send;
        desc.callbacks.memory.is_ready = dramsim3_ready;
        desc.callbacks.memory.tick = dramsim3_tick;
        desc.callbacks.memory.get_latency = dramsim3_latency;
        desc.callbacks.memory.get_stats = dramsim3_stats;
        desc.callbacks.memory.destroy = dramsim3_destroy;

        return &desc;
    }
}
```

## Benefits

### ✅ **Minimal Effort**
- 50-100 lines of adapter code
- No changes to your existing model
- Simple C-style callbacks

### ✅ **Maximum Flexibility**
- Use any model (C, C++, even Python via cffi)
- Load/unload at runtime
- Switch models via config files

### ✅ **Zero Performance Overhead**
- Direct function calls (no virtual dispatch)
- Inline-able wrappers
- No serialization/deserialization

### ✅ **Easy Maintenance**
- Your model updates independently
- Adapter rarely needs changes
- Version-independent interface

## Comparison with Other Approaches

| Approach | Code Changes | Effort | Flexibility |
|----------|--------------|--------|-------------|
| **Abstract Base Class** | Inherit + implement 10+ methods | High | Low |
| **Message Passing** | Serialization layer | Medium | Medium |
| **Our Adapter Interface** | 5-10 wrapper functions | **Low** | **High** |

## Getting Started

1. **Identify required callbacks** for your model type (network/memory)
2. **Write 5-10 wrapper functions** that translate PIMID calls to your API
3. **Register model** either directly or via shared library
4. **Done!** PIMID can now use your model

## See Also

- `pimid/external_models/include/external_model_interface.h` - Interface definition
- `examples/adapters/` - Example adapters for popular models
- `docs/api_reference.md` - Complete API documentation
