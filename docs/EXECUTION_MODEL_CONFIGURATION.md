# Execution Model Configuration Guide

## Overview

PIMID now supports **multiple execution models** for both HOST and DEVICE domains, giving you full flexibility to balance simulation speed vs. accuracy:

- **Option 2**: Event-Driven Analytical (FAST, good for design space exploration)
- **Option 3**: ZSim Execution-Driven (ACCURATE, detailed instruction-level simulation)

You can mix and match ANY combination!

## Configuration Options

### In `pimid_config.yaml`:

```yaml
simulation:
  # =========================================================================
  # EXECUTION MODEL SELECTION
  # =========================================================================

  # Host execution model (choose one):
  # - "zsim" or "execution_driven"    : ZSim-based execution-driven (Option 3)
  # - "event_driven" or "analytical"  : Event-driven analytical (Option 2)
  # - "hybrid"                        : Hybrid approach
  host_execution_model: "zsim"          # Default: ZSim for host

  # Device execution model (choose one):
  device_execution_model: "event_driven"  # Default: Event-driven for device

  # ZSim configuration files (used when zsim model is selected)
  host_zsim_config: "configs/zsim_host.cfg"
  device_zsim_config: "configs/zsim_device.cfg"

  # Event-driven analytical model parameters
  event_driven:
    # Performance model type:
    # - "roofline"         : Roofline model (compute vs memory bound)
    # - "configurable_ipc" : Simple IPC-based model
    # - "plugin_based"     : Use PE plugin for latency
    performance_model: "roofline"

    # Core parameters for analytical modeling
    host:
      num_cores: 4
      frequency_mhz: 2400.0
      ipc: 2.0                  # Instructions per cycle
      vector_width: 8           # SIMD width (AVX-256)
      pipeline_depth: 14        # Pipeline stages
      memory_bandwidth_gbps: 25.0  # DDR4 bandwidth

    device:
      num_cores: 256            # Number of PIM PEs
      frequency_mhz: 1000.0
      ipc: 1.0                  # Simple in-order cores
      vector_width: 4           # Smaller SIMD
      pipeline_depth: 5         # Shallow pipeline
      memory_bandwidth_gbps: 300.0  # HBM2 bandwidth
```

## Common Configurations

### Configuration 1: Both ZSim (Maximum Accuracy, SLOW)

```yaml
simulation:
  host_execution_model: "zsim"
  device_execution_model: "zsim"
  host_zsim_config: "configs/zsim_host.cfg"
  device_zsim_config: "configs/zsim_pim.cfg"
```

**Use Case:**
- Detailed accuracy studies
- Small-scale simulations (few cores)
- Validating analytical models

**Speed:** SLOW (hours for complex workloads)
**Accuracy:** HIGHEST

---

### Configuration 2: Both Event-Driven (Maximum Speed, FAST)

```yaml
simulation:
  host_execution_model: "event_driven"
  device_execution_model: "event_driven"

  event_driven:
    performance_model: "roofline"
    host:
      num_cores: 4
      ipc: 2.0
      frequency_mhz: 2400.0
    device:
      num_cores: 256
      ipc: 1.0
      frequency_mhz: 1000.0
```

**Use Case:**
- Design space exploration
- Large-scale studies (1000+ PEs)
- Early-stage architecture evaluation

**Speed:** VERY FAST (minutes to seconds)
**Accuracy:** GOOD (trends accurate, absolute numbers approximate)

---

### Configuration 3: Hybrid (Best of Both Worlds) - **RECOMMENDED**

```yaml
simulation:
  host_execution_model: "zsim"              # Detailed host simulation
  device_execution_model: "event_driven"    # Fast PIM simulation

  host_zsim_config: "configs/zsim_host.cfg"

  event_driven:
    performance_model: "roofline"
    device:
      num_cores: 256
      ipc: 1.0
      frequency_mhz: 1000.0
```

**Use Case:** (MOST COMMON)
- Realistic workload characterization (host needs accuracy)
- Large PIM arrays (device needs speed)
- Production research simulations

**Speed:** MODERATE (10-100x faster than full zsim)
**Accuracy:** HIGH for host, GOOD for device

---

### Configuration 4: Event-Driven Host + ZSim Device

```yaml
simulation:
  host_execution_model: "event_driven"
  device_execution_model: "zsim"
  device_zsim_config: "configs/zsim_pim.cfg"
```

**Use Case:**
- Detailed PIM core studies
- PIM microarchitecture optimization
- Small number of PEs (< 16)

**Speed:** MODERATE
**Accuracy:** HIGH for device, GOOD for host

---

## ZSim Configuration

### Example `zsim_host.cfg`:

```cfg
sys = {
  frequency = 2400;  // MHz
  cores = {
    type = "OOO";
    cores = 4;
    icache = 32768;
    dcache = 32768;
  };

  caches = {
    l1i = {
      size = 32768;
      array = "SetAssoc";
      ways = 8;
      latency = 2;
    };
    l1d = {
      size = 32768;
      ways = 8;
      latency = 2;
    };
    l2 = {
      size = 262144;
      ways = 8;
      latency = 7;
    };
    l3 = {
      size = 8388608;
      ways = 16;
      latency = 27;
    };
  };

  mem = {
    type = "DDR";
    controllers = 1;
    tech = "DDR4-2400";
  };
};
```

### Example `zsim_pim.cfg`:

```cfg
sys = {
  frequency = 1000;  // MHz
  cores = {
    type = "Simple";  // In-order cores for PIM
    cores = 256;      // Many PIM PEs
    icache = 8192;
    dcache = 8192;
  };

  caches = {
    l1i = {
      size = 8192;
      ways = 2;
      latency = 1;
    };
    l1d = {
      size = 8192;
      ways = 2;
      latency = 1;
    };
  };

  mem = {
    type = "HBM";
    controllers = 16;
    tech = "HBM2";
  };
};
```

---

## API Usage

### C++ API:

```cpp
#include "execution_model/execution_model.h"
#include "execution_model/zsim_execution_model.h"
#include "execution_model/event_driven_execution_model.h"

using namespace pimid;

// Load configuration
PIMIDConfig config = loadConfig("pimid_config.yaml");

// Option 1: Create from factory using config
auto host_model = ExecutionModelFactory::createFromConfig(
    config.host_execution_model,
    config,
    SimulationDomain::HOST
);

auto device_model = ExecutionModelFactory::createFromConfig(
    config.device_execution_model,
    config,
    SimulationDomain::DEVICE
);

// Option 2: Create directly
auto zsim_model = std::make_shared<ZSimExecutionModel>();
zsim_model->initialize("configs/zsim_host.cfg", SimulationDomain::HOST);

auto event_model = std::make_shared<EventDrivenExecutionModel>();
event_model->initialize("pimid_config.yaml", SimulationDomain::DEVICE);

// Option 3: Create hybrid model
auto hybrid = std::make_shared<HybridExecutionModel>(
    zsim_model,
    event_model
);

// Execute tasks
Task task{
    .task_id = 1,
    .kernel_name = "vector_add",
    .input_addresses = {0x10000000, 0x20000000},
    .output_addresses = {0x30000000},
    .input_size = 1024 * 1024,
    .output_size = 1024 * 1024,
    .num_ops = 1000000,
    .pe_id = 0
};

Cycle completion = host_model->executeTask(task);
std::cout << "Task completes at cycle: " << completion << std::endl;
```

---

## Performance Comparison

| Configuration | Host Model | Device Model | Speed | Accuracy | Use Case |
|---------------|------------|--------------|-------|----------|----------|
| **Full ZSim** | ZSim | ZSim | ★☆☆☆☆ | ★★★★★ | Detailed studies, validation |
| **Full Event-Driven** | Event | Event | ★★★★★ | ★★★☆☆ | Design space exploration |
| **Hybrid (Recommended)** | ZSim | Event | ★★★★☆ | ★★★★☆ | Production research |
| **Reverse Hybrid** | Event | ZSim | ★★★☆☆ | ★★★★☆ | PIM core optimization |

### Speed Estimates (for 1M element vector add):

- **Full ZSim (host + device)**: ~30-60 minutes
- **Hybrid (ZSim host + Event device)**: ~5-10 minutes
- **Full Event-Driven**: ~10-30 seconds

**Speedup: 100-1000x for event-driven!**

---

## Validation

### Validating Event-Driven Against ZSim:

1. Run small benchmark with full ZSim:
```yaml
host_execution_model: "zsim"
device_execution_model: "zsim"
```

2. Run same benchmark with event-driven:
```yaml
host_execution_model: "event_driven"
device_execution_model: "event_driven"
```

3. Compare results:
```bash
# Check IPC
# Check total cycles
# Check memory bandwidth utilization
```

4. Tune event-driven parameters to match ZSim:
```yaml
event_driven:
  device:
    ipc: 0.85  # Adjust based on ZSim results
    frequency_mhz: 950.0  # Tune to match
```

---

## References

- **MultiPIM Integration**: Based on https://github.com/Systems-ShiftLab/MultiPIM
- **Ramulator-PIM**: Based on https://github.com/CMU-SAFARI/ramulator-pim
- **ZSim Documentation**: See `pimid/external/zsim/README.md`
- **Event-Driven Model Details**: See `CORE_MODEL_ARCHITECTURE_ANALYSIS.md`
- **Roofline Model**: Williams et al., "Roofline: An Insightful Visual Performance Model"

---

## Troubleshooting

### Issue: ZSim fails to initialize

**Solution:**
- Check zsim config file path
- Verify zsim config syntax
- Check that PIN instrumentation is available

### Issue: Event-driven model too fast/slow

**Solution:**
- Adjust IPC parameter to match target performance
- Tune frequency to realistic values
- Consider switching performance model

### Issue: Results don't match between models

**Solution:**
- This is expected! Event-driven is analytical approximation
- Use event-driven for trends, not absolute values
- Validate critical regions with ZSim

---

## Future Enhancements

1. **Trace-driven mode**: Record ZSim traces, replay with event-driven
2. **ML-based models**: Train performance models on ZSim data
3. **Adaptive hybrid**: Switch models dynamically based on region
4. **GPU execution models**: Add support for GPU cores

