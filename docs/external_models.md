# Integrating external models

PIMID can host an **existing, unchanged** external network or memory model
through a lightweight adapter interface:

- API header: `include/external_model_interface.h`
- Registry: `src/model_registry.cpp`
- Worked example: `examples/integration/external_model_integration/simple_network/`

The original model class is not modified -- only a thin adapter layer
(~50 lines) is needed.

## The adapter

An adapter provides callback functions that forward to the existing model,
plus one registration function:

```cpp
// 1. Initialize model
int my_init(handle, config_file)            -> model->Initialize(config_file)
// 2. Send packet (with format conversion)
int my_send_packet(handle, packet)          -> model->SendPacket(converted)
// 3. Advance simulation
void my_tick(handle)                        -> model->AdvanceCycle()
// 4. Query latency
uint64_t my_get_latency(...)                -> model->GetLatency(...)
// 5. Get statistics (with format conversion)
void my_get_stats(...)                      -> model->GetStatistics()
// 6. Cleanup
void my_destroy(handle)                     -> delete model
```

```cpp
extern "C" int pimid_register_my_network() {
    PimidNetworkCallbacks callbacks = { /* fill in callbacks */ };
    PimidModelDescriptor descriptor = { /* fill in metadata */ };
    return pimid_register_model(&descriptor);
}
```

## Step by step

### 1. Keep your model unchanged

Your existing model (`MyNetwork.h`, `MyNetwork.cpp`) stays exactly as is.

### 2. Write the adapter

```cpp
#include "MyNetwork.h"
#include "external_model_interface.h"

static int my_network_init(PimidModelHandle handle, const char* config_file) {
    MyNetwork* net = static_cast<MyNetwork*>(handle);
    return net->yourInitMethod(config_file) ? 0 : -1;
}

static int my_network_send_packet(PimidModelHandle handle,
                                  const PimidNetworkPacket* packet) {
    MyNetwork* net = static_cast<MyNetwork*>(handle);
    // convert packet format if needed
    return net->yourSendMethod(/* ... */) ? 0 : -1;
}

static void my_network_tick(PimidModelHandle handle) {
    static_cast<MyNetwork*>(handle)->yourTickMethod();
}

// ... get_latency, get_stats, destroy

extern "C" {
    PimidModelHandle create_my_network() { return new MyNetwork(); }

    int pimid_register_my_network() {
        PimidNetworkCallbacks callbacks = { /* ... */ };
        PimidModelDescriptor descriptor = { /* ... */ };
        return pimid_register_model(&descriptor);
    }
}
```

### 3. Compile as a shared library

```bash
g++ -shared -fPIC my_network_adapter.cpp MyNetwork.cpp \
    -o libmy_network.so -I/path/to/pimid/include
```

### 4. Use in PIMID

```cpp
pimid_load_model("libmy_network.so");
const PimidModelDescriptor* model = pimid_get_model("MyNetwork");
model->callbacks.network.init(model->handle, "config.txt");
```

## The simple_network example

`examples/integration/external_model_integration/simple_network/` contains a
complete minimal example:

| File | Purpose | Modified? |
|------|---------|-----------|
| `SimpleNetwork.h` / `.cpp` | the "existing" model | no |
| `simple_network_adapter.cpp` | the adapter (the only new code) | new, ~50 lines |
| `test_integration.cpp` | test program exercising the full lifecycle | new |

Build it by hand (it is not part of the CMake build):

```bash
cd examples/integration/external_model_integration/simple_network
g++ -shared -fPIC simple_network_adapter.cpp SimpleNetwork.cpp \
    -o libsimple_network.so -I../../../../include
```

## Why this design

1. **No modifications to existing code** -- the external model stays pristine
2. **Minimal integration effort** -- ~50 lines of adapter code
3. **Dynamic loading** -- models load at runtime via `pimid_load_model()`
4. **Independent maintenance** -- update the model without touching PIMID
