# PIMID Configuration Guide

Complete guide to configuring the PIMID simulator for your research needs.

## Table of Contents

1. [Quick Start](#quick-start)
2. [Configuration File Structure](#configuration-file-structure)
3. [Main Configuration](#main-configuration)
4. [Host Configuration](#host-configuration)
5. [Device Configuration](#device-configuration)
6. [Memory Configuration](#memory-configuration)
7. [Network Configuration](#network-configuration)
8. [Power Configuration](#power-configuration)
9. [Configuration Presets](#configuration-presets)
10. [Interactive Configuration](#interactive-configuration)
11. [Validation and Troubleshooting](#validation-and-troubleshooting)

---

## Quick Start

### Minimal Configuration

The absolute minimum configuration to run PIMID:

```yaml
# minimal_config.yaml
simulation:
  mode: "standalone"

memory:
  technology: "DRAM"
  capacity: "4GB"

pim:
  pe_placement_level: "BANK"
  num_pes_per_level: 1
```

Run with:
```bash
./pimid_standalone minimal_config.yaml ./workload
```

### Using Configuration Presets

Start with a preset and customize:

```bash
# List available presets
./pimid_config --list-presets

# Generate configuration from preset
./pimid_config --preset high_performance --output my_config.yaml

# Edit and use
vim my_config.yaml
./pimid_standalone my_config.yaml ./workload
```

---

## Configuration File Structure

PIMID uses hierarchical YAML configuration:

```
pimid_config.yaml (main)
├── host_config.yaml
├── device_config.yaml
├── memory_config.yaml
├── network_config.yaml
└── power_config.yaml
```

You can use either:
1. **Single file**: Everything in one YAML file
2. **Multiple files**: Separate files for each component (recommended)

### Single File Example

```yaml
# all_in_one.yaml
simulation:
  mode: "co-simulation"

host:
  cores:
    num_cores: 4
  caches:
    l1d:
      size_kb: 32

device:
  processing_elements:
    placement:
      level: "BANK"

# ... etc
```

### Multiple Files Example

```yaml
# pimid_config.yaml
components:
  host_config: "config/host_config.yaml"
  device_config: "config/device_config.yaml"
  memory_config: "config/memory_config.yaml"
```

---

## Main Configuration

### Simulation Mode

Choose between different simulation modes:

```yaml
simulation:
  # co-simulation: Full host + device simulation
  # standalone: Combined simulation in one process
  # host-only: Only simulate host
  # device-only: Only simulate device
  mode: "co-simulation"
```

#### Co-Simulation Mode

Most accurate, runs host and device in separate processes:

```yaml
simulation:
  mode: "co-simulation"

communication:
  port: 50000
  host_address: "127.0.0.1"
  timeout: 30
```

Start device first, then host:
```bash
# Terminal 1
./pimid_device config.yaml

# Terminal 2
./pimid_host config.yaml ./workload
```

#### Standalone Mode

Faster simulation, single process:

```yaml
simulation:
  mode: "standalone"
```

```bash
./pimid_standalone config.yaml ./workload
```

### Simulation Control

```yaml
simulation:
  # Maximum cycles (0 = unlimited)
  max_cycles: 10000000

  # Warmup before statistics
  warmup_cycles: 100000

  # Fast-forward non-ROI regions
  fast_forward: true

  # Logging
  debug_mode: false
  log_level: "INFO"  # DEBUG, INFO, WARNING, ERROR
  log_file: "logs/pimid.log"

  # Output
  output_dir: "results/"
  stats_interval: 10000
```

---

## Host Configuration

### Core Configuration

```yaml
# host_config.yaml
cores:
  num_cores: 4

  default:
    type: "out-of-order"
    frequency_mhz: 3000
    num_threads: 2

    # Pipeline
    pipeline_depth: 14
    issue_width: 4
    retire_width: 4

    # Execution units
    num_int_alu: 4
    num_fp_alu: 2
    num_load_units: 2
    num_store_units: 1
```

### Cache Hierarchy

```yaml
caches:
  l1i:
    size_kb: 32
    line_size_bytes: 64
    associativity: 8
    latency_cycles: 4
    replacement_policy: "LRU"

  l1d:
    size_kb: 32
    line_size_bytes: 64
    associativity: 8
    latency_cycles: 4
    prefetcher: "stream"

  l2:
    size_kb: 256
    latency_cycles: 12

  l3:
    size_kb: 8192
    latency_cycles: 30
    num_banks: 16
```

### Performance Tuning

**High Performance:**
```yaml
cores:
  default:
    issue_width: 8
    rob_size: 256

caches:
  l3:
    size_kb: 16384
    prefetcher: "stride"
```

**Low Power:**
```yaml
cores:
  default:
    type: "in-order"
    frequency_mhz: 1000
    issue_width: 2

caches:
  l1d:
    size_kb: 16
  l2:
    size_kb: 128
```

---

## Device Configuration

### PE Placement

Choose where to place processing elements in the memory hierarchy:

#### Subarray-Level PEs

```yaml
processing_elements:
  placement:
    level: "SUBARRAY"
    num_pes_per_level: 1

memory_hierarchy:
  subarray:
    num_subarrays_per_bank: 16
```

- **Pros**: Maximum data locality, lowest memory access latency
- **Cons**: Many PEs, higher cost, complex scheduling
- **Use case**: Fine-grained parallelism, small working sets

#### Bank-Level PEs

```yaml
processing_elements:
  placement:
    level: "BANK"
    num_pes_per_level: 1
```

- **Pros**: Good balance of locality and cost
- **Cons**: Less locality than subarray-level
- **Use case**: Most workloads, recommended default

#### Logic Die PEs

```yaml
processing_elements:
  placement:
    level: "LOGIC_DIE"
    num_pes_per_level: 4

memory_hierarchy:
  logic_die:
    has_logic_die: true
    logic_die_frequency_mhz: 2000
```

- **Pros**: Highest performance PEs, most flexible
- **Cons**: Less data locality, more data movement
- **Use case**: HBM/HMC systems, compute-intensive workloads

### PE Architecture

```yaml
processing_elements:
  architecture:
    type: "in-order"
    frequency_mhz: 1000
    num_cores: 1

    # Instruction set
    isa: "RISC-V"

    # Vector support
    vector_support: true
    vector_width: 256

  local_memory:
    scratchpad_size_kb: 256
    has_l1_cache: true
    l1_size_kb: 32
```

### Scheduler Selection

```yaml
scheduler:
  # Built-in schedulers
  policy: "NEAREST_PE"    # Data locality
  # policy: "ROUND_ROBIN"  # Simple round-robin
  # policy: "LOAD_BALANCED" # Balance PE utilization

  # Custom plugin scheduler
  # policy: "MyScheduler"
  # See Plugin Development Guide

  # Parameters
  load_balance_threshold: 20
  dynamic_load_balancing: true
```

---

## Memory Configuration

### DRAM Configuration

```yaml
technology:
  type: "DRAM"
  model: "ramulator"

dram:
  standard: "DDR4"
  speed_grade: "2400"

  organization:
    channels: 4
    ranks_per_channel: 2
    banks_per_chip: 16
    bank_groups: 4

  row_buffer:
    policy: "open_page"  # or "close_page"
```

#### DDR4 Example

```yaml
dram:
  standard: "DDR4"
  speed_grade: "3200"

  timing:
    tCL: 16
    tRCD: 16
    tRP: 16
    tRAS: 39
```

#### HBM Example

```yaml
dram:
  standard: "HBM2"

  organization:
    channels: 8
    ranks_per_channel: 1
    banks_per_chip: 16

memory_hierarchy:
  logic_die:
    has_logic_die: true
```

### SRAM Configuration

```yaml
technology:
  type: "SRAM"
  model: "cacti"

sram:
  capacity_mb: 16

  timing:
    read_latency_ns: 1.0
    write_latency_ns: 1.0

  cacti:
    cache_size_bytes: 16777216
    line_size_bytes: 64
    banks: 16
```

### STT-MRAM Configuration

```yaml
technology:
  type: "STT_MRAM"
  model: "nvsim"

stt_mram:
  capacity_mb: 16

  timing:
    read_latency_ns: 5.0
    write_latency_ns: 20.0

  reliability:
    write_endurance: 1e15
    retention_time_years: 10
```

### Hybrid Memory

```yaml
hybrid:
  enabled: true

  tiers:
    - technology: "SRAM"
      capacity_mb: 256
      role: "cache"

    - technology: "DRAM"
      capacity_gb: 16
      role: "main"

    - technology: "STT_MRAM"
      capacity_gb: 64
      role: "storage"

  migration:
    policy: "hot_cold"
    threshold_accesses: 100
```

---

## Network Configuration

### Topology Selection

#### 2D Mesh

```yaml
topology:
  type: "MESH_2D"

  dimensions:
    rows: 4
    cols: 4

routing:
  algorithm: "XY"
```

#### Crossbar

```yaml
topology:
  type: "CROSSBAR"

routing:
  algorithm: "MINIMAL"
```

#### Dragonfly

```yaml
topology:
  type: "DRAGONFLY"

  dragonfly:
    num_groups: 4
    routers_per_group: 4
    nodes_per_router: 4
    global_links_per_router: 3

routing:
  algorithm: "ADAPTIVE"
```

### Router Configuration

```yaml
router:
  architecture: "VC"

  pipeline:
    num_stages: 4

  latency_cycles: 4

virtual_channels:
  num_vcs: 4

links:
  width_bytes: 8
  latency_cycles: 1

buffers:
  input_buffer_depth: 8
```

---

## Power Configuration

### Enable Power Modeling

```yaml
power_modeling:
  enabled: true
  model: "McPAT"
  granularity: "component"
  update_interval: 10000
```

### Technology Parameters

```yaml
technology:
  tech_node_nm: 22
  device_type: "HP"
  temperature_k: 350
  voltage: 1.0
  frequency_ghz: 2.0
```

### Component Power

```yaml
host:
  enabled: true

  core:
    dynamic_power_w: 5.0
    leakage_power_w: 0.5

  cache:
    l1:
      dynamic_power_per_access_nj: 0.1
      leakage_power_w: 0.05

device:
  enabled: true

  pe:
    dynamic_power_w: 1.0
    leakage_power_w: 0.1

memory:
  enabled: true

  dram:
    background_power_w: 1.5
    read_energy_nj: 2.5
    write_energy_nj: 3.0
```

---

## Configuration Presets

### High Performance

```bash
./pimid_config --preset high_performance
```

```yaml
# Optimized for maximum performance
cores:
  num_cores: 8
  default:
    frequency_mhz: 4000
    issue_width: 8

caches:
  l3:
    size_kb: 32768

memory:
  dram:
    standard: "HBM2"
    channels: 8

pim:
  pe_placement_level: "LOGIC_DIE"
  scheduler: "LOAD_BALANCED"
```

### Low Power

```bash
./pimid_config --preset low_power
```

```yaml
# Optimized for energy efficiency
cores:
  num_cores: 2
  default:
    type: "in-order"
    frequency_mhz: 1000

memory:
  technology: "STT_MRAM"

pim:
  scheduler: "ENERGY_AWARE"

power_management:
  dvfs:
    enabled: true
    policy: "power_save"
```

### Accuracy

```bash
./pimid_config --preset accuracy
```

```yaml
# Maximum modeling accuracy
simulation:
  detailed_simulation: true

power:
  enabled: true
  detailed_breakdown: true

network:
  detailed_model: true
```

---

## Interactive Configuration

### Using the Configuration Wizard

```bash
./pimid_config_wizard
```

The wizard will guide you through:
1. Simulation mode selection
2. Memory technology choice
3. PE placement strategy
4. Network topology
5. Power modeling options

### Quick Setup

```bash
# For machine learning workloads
./pimid_config_wizard --workload ml

# For graph analytics
./pimid_config_wizard --workload graph

# For streaming applications
./pimid_config_wizard --workload streaming
```

---

## Validation and Troubleshooting

### Validate Configuration

```bash
./pimid_standalone --validate config.yaml
```

Output:
```
Validating configuration...
✓ Main configuration valid
✓ Host configuration valid
✓ Device configuration valid
✓ Memory configuration valid
✓ Network configuration valid
✗ Power configuration has warnings:
  - Warning: tech_node_nm (14) smaller than recommended (22) for device_type HP
✓ All configurations valid with 1 warning(s)
```

### Common Issues

#### Issue: "Cannot find configuration file"

```bash
Error: Could not open file: config/host_config.yaml
```

**Solution:** Ensure paths in main config are correct:
```yaml
components:
  host_config: "./config/host_config.yaml"  # Use relative path
```

#### Issue: "Invalid parameter value"

```bash
Error: memory.dram.timing.tCL: Invalid value '14.5'
       Expected integer, got float
```

**Solution:** Check parameter types in schema:
```yaml
dram:
  timing:
    tCL: 14  # Integer, not 14.5
```

#### Issue: "Plugin not found"

```bash
Error: Plugin 'MyScheduler' not found
```

**Solution:** Ensure plugin is loaded:
```yaml
plugins:
  plugin_dir: "plugins/"
  loaded_plugins:
    - name: "MyScheduler"
      library: "plugins/libmy_scheduler.so"
```

### Debug Configuration

Enable detailed logging:

```yaml
simulation:
  debug_mode: true
  log_level: "DEBUG"

validation:
  strict_mode: true
  check_ranges: true
  verify_paths: true
```

---

## Configuration Examples

See `config/examples/` for complete examples:

- `high_performance.yaml` - Maximum performance
- `low_power.yaml` - Energy efficient
- `balanced.yaml` - Performance/power balance
- `dram_only.yaml` - Using DRAM
- `hybrid_memory.yaml` - DRAM + STT-MRAM
- `mesh_network.yaml` - 2D mesh NoC
- `crossbar_network.yaml` - Crossbar interconnect

---

## Configuration Schema

Full parameter documentation:

```bash
./pimid_config --generate-docs > configuration_reference.md
```

This generates complete documentation of all configuration parameters with:
- Parameter descriptions
- Valid value ranges
- Default values
- Examples

---

## Next Steps

1. Start with a preset matching your use case
2. Customize key parameters
3. Validate configuration
4. Run simulation
5. Iterate based on results

For more details:
- Plugin development: See [PLUGIN_DEVELOPMENT_GUIDE.md](PLUGIN_DEVELOPMENT_GUIDE.md)
- Architecture details: See [ARCHITECTURE.md](ARCHITECTURE.md)
- Getting started: See [GETTING_STARTED.md](../pimid/GETTING_STARTED.md)
