# Network Topology Configuration Schema

## Overview

This document describes the YAML configuration schema for specifying custom network topologies and external network/memory models in PIMID config files.

## Custom Topology Configuration

### Basic Structure

```yaml
memory:
  dram_type: DDR4
  channels: 1
  ranks_per_channel: 1
  subarrays_per_bank: 16
  banks_per_bg: 4
  bg_per_chip: 4
  chips_per_rank: 8

  # Custom topology configuration
  network:
    custom_topology:
      enabled: true

      # Level 0: Bank level
      l0:
        use_default: false
        topology: H_TREE
        num_switches: 15
        ports_per_switch: 4
        num_endpoints: 16

      # Level 1: Bank group level
      l1:
        use_default: false
        topology: BUS
        num_switches: 4
        ports_per_switch: 8
        num_endpoints: 32

      # Level 2: Chip level
      l2:
        use_default: false
        topology: MESH_2D
        num_switches: 8
        ports_per_switch: 5
        num_endpoints: 8

      # Level 3: Rank level
      l3:
        use_default: true

      # Level 4: Channel level
      l4:
        use_default: true

      # Level 5: System level
      l5:
        use_default: true
```

### Topology Types

| Value | Description |
|-------|-------------|
| `BUS` | Shared bus (1 switch, N ports) |
| `CROSSBAR` | Full crossbar (N×N connections) |
| `MESH_2D` | 2D mesh grid |
| `TORUS_2D` | 2D torus with wrap-around |
| `FAT_TREE` | Hierarchical fat-tree |
| `H_TREE` | Binary H-tree structure |
| `CUSTOM` | User-defined |

### Level Configuration Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `use_default` | boolean | Yes | Use auto-calculated values |
| `topology` | string | If !use_default | Topology type |
| `num_switches` | integer | If !use_default | Number of switches |
| `ports_per_switch` | integer | If !use_default | Ports per switch |
| `num_endpoints` | integer | If !use_default | Number of endpoints |

## External Model Configuration

### Network Model Integration

```yaml
external_models:
  network:
    enabled: true
    library_path: ./lib/libsimple_network_adapter.so
    model_name: SimpleNetwork
    config_file: ./config/network_config.txt

    # Optional: Override specific parameters
    parameters:
      num_nodes: 16
      topology: mesh
      link_bandwidth: 10.0
      link_latency: 10
```

### Memory Model Integration

```yaml
external_models:
  memory:
    enabled: true
    library_path: ./lib/libcustom_dram_adapter.so
    model_name: CustomDRAM
    config_file: ./config/dram_config.ini

    # Optional: Override specific parameters
    parameters:
      frequency_mhz: 3200
      burst_length: 8
      row_buffer_policy: open_page
```

### Combined Configuration

```yaml
external_models:
  # External network model
  network:
    enabled: true
    library_path: ./lib/libbooksim_adapter.so
    model_name: BookSim2
    config_file: ./config/booksim.cfg

  # External memory model
  memory:
    enabled: true
    library_path: ./lib/libdramsim3_adapter.so
    model_name: DRAMSim3
    config_file: ./config/DDR4_8Gb_x8_3200.ini
```

## Complete Example

```yaml
# PIMID Configuration File
simulation:
  name: custom_topology_test
  mode: standalone
  duration_cycles: 100000

memory:
  dram_type: DDR4
  channels: 2
  ranks_per_channel: 2
  chips_per_rank: 8
  bg_per_chip: 4
  banks_per_bg: 4
  subarrays_per_bank: 16

  network:
    custom_topology:
      enabled: true

      l0:
        use_default: false
        topology: H_TREE
        num_switches: 15
        ports_per_switch: 4
        num_endpoints: 16

      l1:
        use_default: false
        topology: CROSSBAR
        num_switches: 4
        ports_per_switch: 8
        num_endpoints: 32

      l2:
        use_default: false
        topology: MESH_2D
        num_switches: 8
        ports_per_switch: 5
        num_endpoints: 8

      l3:
        use_default: false
        topology: TORUS_2D
        num_switches: 4
        ports_per_switch: 5
        num_endpoints: 4

      l4:
        use_default: false
        topology: FAT_TREE
        num_switches: 2
        ports_per_switch: 8
        num_endpoints: 2

      l5:
        use_default: false
        topology: CROSSBAR
        num_switches: 1
        ports_per_switch: 2
        num_endpoints: 2

external_models:
  network:
    enabled: false

  memory:
    enabled: false

pim:
  enabled: true
  num_pes: 128
  pe_placement: bank
  scheduling_policy: nearest
```

## Validation Rules

1. **Custom Topology**:
   - If `enabled: true`, at least one level must have `use_default: false`
   - If `use_default: false`, all fields (topology, num_switches, ports_per_switch, num_endpoints) must be specified
   - `num_switches` and `ports_per_switch` must be positive integers
   - `num_endpoints` must match the calculated value for that level based on DRAM hierarchy

2. **External Models**:
   - If `enabled: true`, `library_path` and `model_name` must be specified
   - `library_path` must point to an existing `.so` file
   - `config_file` is optional but recommended
   - If both internal network and external network are enabled, external takes precedence

3. **Topology Consistency**:
   - For `MESH_2D` and `TORUS_2D`: `num_endpoints` should be a perfect square
   - For `H_TREE`: `num_switches` should be (num_endpoints - 1) for binary tree
   - For `BUS` and `CROSSBAR`: typically 1 switch with multiple ports

## Usage with pimid Binary

```bash
# Run with custom topology config
pimid --config ./configs/custom_topology_test.yaml

# Run with external model config
pimid --config ./configs/external_model_test.yaml

# Run with both features
pimid --config ./configs/combined_test.yaml
```

## See Also

- `docs/switch_topology_customization.md` - Topology customization API
- `docs/external_model_integration.md` - External model integration guide
- `test/configs/` - Example configuration files
