# External Model Integration Examples

This directory contains examples of integrating external network and memory models with PIMID using the lightweight adapter interface.

## Directory Structure

```
examples/external_model_integration/
├── README.md                          # This file
├── simple_network/                    # Simple network model example
│   ├── SimpleNetwork.h                # Existing model (unchanged)
│   ├── SimpleNetwork.cpp              # Existing model (unchanged)
│   ├── simple_network_adapter.cpp     # PIMID adapter (NEW, ~50 lines)
│   └── test_integration.cpp           # Test program
├── dramsim3_adapter/                  # DRAMSim3 integration
│   ├── dramsim3_adapter.cpp           # Adapter for DRAMSim3
│   └── README.md                      # Integration notes
└── booksim_adapter/                   # BookSim integration
    ├── booksim_adapter.cpp            # Adapter for BookSim
    └── README.md                      # Integration notes
```

## Quick Start

###Step 1: Build the adapter

```bash
cd simple_network
g++ -shared -fPIC simple_network_adapter.cpp SimpleNetwork.cpp \
    -o libsimple_network.so \
    -I../../../pimid/external_models/include
```

### Step 2: Load in PIMID

```cpp
#include "external_models/include/external_model_interface.h"

int main() {
    // Load the model
    pimid_load_model("libsimple_network.so");

    // Use it
    const PimidModelDescriptor* model = pimid_get_model("SimpleNetwork");
    model->callbacks.network.init(model->handle, "config.txt");
}
```

That's it! Your existing model now works with PIMID.

## Examples

See subdirectories for specific examples:
- `simple_network/` - Minimal example (good starting point)
- `dramsim3_adapter/` - Real-world memory simulator
- `booksim_adapter/` - Real-world NoC simulator
