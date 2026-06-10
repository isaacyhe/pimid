# Simple Network Integration Example

This example demonstrates how to integrate an **existing, unchanged** network model into PIMID using the lightweight adapter approach.

## Key Insight

**The original `SimpleNetwork` class is NOT modified!** Only a thin adapter layer (~50 lines) is needed for integration.

## Files

| File | Purpose | Lines | Modified? |
|------|---------|-------|-----------|
| `SimpleNetwork.h` | User's existing model header | 54 | ❌ No |
| `SimpleNetwork.cpp` | User's existing model implementation | 70 | ❌ No |
| `simple_network_adapter.cpp` | **Adapter code (THE ONLY NEW FILE)** | ~50 | ✅ New |
| `test_integration.cpp` | Test program | 120 | ✅ New |
| `config.txt` | Config file | 5 | ✅ New |

## What the Adapter Does

The adapter provides 6 simple callback functions that forward to the existing model:

```cpp
// 1. Initialize model
int simple_network_init(handle, config_file) → network->Initialize(config_file)

// 2. Send packet (with format conversion)
int simple_network_send_packet(handle, packet) → network->SendPacket(converted_packet)

// 3. Advance simulation
void simple_network_tick(handle) → network->AdvanceCycle()

// 4. Query latency
uint64_t simple_network_get_latency(...) → network->GetLatency(...)

// 5. Get statistics (with format conversion)
void simple_network_get_stats(...) → network->GetStatistics()

// 6. Cleanup
void simple_network_destroy(handle) → delete network
```

Plus one registration function:
```cpp
extern "C" int pimid_register_simple_network() {
    // Fill in callbacks and register
}
```

That's it! **~50 lines total.**

## Building and Running

```bash
# From pimid-dev root directory
mkdir -p build
cd build
cmake ..
make

# Run the integration test
./examples/external_model_integration/simple_network/test_simple_network_integration
```

## Expected Output

```
╔══════════════════════════════════════════════════════════════╗
║   External Model Integration Test                           ║
╚══════════════════════════════════════════════════════════════╝

=== Step 1: Registering External Model ===
[Adapter] Registering SimpleNetwork with PIMID
✓ Model registered successfully

=== Step 2: Creating Model Instance ===
✓ Model instance created

=== Step 3: Initializing Model ===
[SimpleNetwork] Initializing with config: config.txt
✓ Model initialized

=== Step 4: Sending Packets ===
✓ Sent 10 packets

=== Step 5: Running Simulation ===
✓ Ran 100 cycles

=== Step 6: Querying Latency ===
Latency from node 0 to node 3: 16 cycles
✓ Latency query successful

=== Step 7: Retrieving Statistics ===
Network Statistics:
  Total packets: 10
  Total bytes: 640
  Total cycles: 100
  Avg latency: 10 cycles
✓ Statistics correct

=== Step 8: Cleanup ===
✓ Model destroyed

================================================================
All integration tests passed! ✓
================================================================
```

## Integration Steps for Your Own Model

If you have an existing network model (e.g., BookSim, Garnet, custom NoC), follow these steps:

### 1. Keep Your Model Unchanged

Your existing model (`MyNetwork.h`, `MyNetwork.cpp`) stays exactly as is. No modifications needed!

### 2. Create an Adapter File (~50 lines)

Create `my_network_adapter.cpp`:

```cpp
#include "MyNetwork.h"
#include "pimid/external_models/include/external_model_interface.h"

// Callback functions (forward to your model's methods)
static int my_network_init(PimidModelHandle handle, const char* config_file) {
    MyNetwork* net = static_cast<MyNetwork*>(handle);
    return net->yourInitMethod(config_file) ? 0 : -1;
}

static int my_network_send_packet(PimidModelHandle handle, const PimidNetworkPacket* packet) {
    MyNetwork* net = static_cast<MyNetwork*>(handle);
    // Convert packet format if needed
    return net->yourSendMethod(...) ? 0 : -1;
}

static void my_network_tick(PimidModelHandle handle) {
    MyNetwork* net = static_cast<MyNetwork*>(handle);
    net->yourTickMethod();
}

// ... other callbacks (get_latency, get_stats, destroy)

// Registration
extern "C" {
    PimidModelHandle create_my_network() {
        return new MyNetwork();
    }

    int pimid_register_my_network() {
        PimidNetworkCallbacks callbacks = { /* fill in callbacks */ };
        PimidModelDescriptor descriptor = { /* fill in metadata */ };
        return pimid_register_model(&descriptor);
    }
}
```

### 3. Compile as Shared Library

```cmake
add_library(my_network_adapter SHARED my_network_adapter.cpp)
target_link_libraries(my_network_adapter my_network pimid_external_models)
```

### 4. Use in PIMID

```cpp
pimid_register_my_network();
PimidModelHandle handle = pimid_create_model("MyNetwork");
pimid_model_init(handle, "MyNetwork", "config.ini");
// ... use model
```

## Benefits

1. **No modifications to existing code** - Your model stays pristine
2. **Minimal integration effort** - Only ~50 lines of adapter code
3. **Dynamic loading** - Can load models at runtime
4. **Easy maintenance** - Update your model independently
5. **Multiple versions** - Can register different versions simultaneously

## See Also

- `docs/external_model_integration.md` - Complete integration guide
- `pimid/external_models/include/external_model_interface.h` - API reference
- Real-world examples for BookSim and DRAMSim3 in the guide
