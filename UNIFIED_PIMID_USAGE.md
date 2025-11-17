# PIMID Unified Simulator - Usage Guide

## Overview

The `pimid` binary is now a **unified entry point** for all PIMID simulation modes:
- **standalone**: PIM-enabled workload execution (default)
- **host**: Host-only simulation
- **device**: Device-only simulation
- **cosim**: Host/Device co-simulation

This provides a single, consistent interface for all simulation scenarios instead of multiple separate binaries.

## Installation

Build the unified `pimid` binary:

```bash
cd pimid
mkdir build && cd build
cmake .. && make pimid -j$(nproc)
```

The binary will be located at: `pimid/build/pimid`

## Usage

### General Syntax

```bash
pimid --mode <mode> [mode-specific options]
```

### Getting Help

```bash
pimid --help
```

---

## Simulation Modes

### 1. Standalone Mode (Default)

Execute PIM-enabled workloads with configurable memory technologies and PE configurations.

**Usage:**
```bash
pimid --mode standalone --workload <binary> [workload_args...]
pimid --mode standalone --config <config.yaml> --workload <binary>
```

**Examples:**
```bash
# Run BFS workload
pimid --mode standalone --workload ./workloads/bfs/bfs 1000 16

# Run with config file
pimid --mode standalone --config configs/sram_bank.yaml --workload ./bfs

# Default mode (standalone is implicit)
pimid --workload ./bfs 10000 8
```

**Key Features:**
- Forks and executes external workload binaries
- Instruments memory operations
- Simulates PIM execution
- Reports statistics

---

### 2. Host-Only Mode

Simulate host CPU operations independently.

**Usage:**
```bash
pimid --mode host [options]
```

**Options:**
- `--port PORT` - Port to listen on (default: 9999)
- `--cycles CYCLES` - Number of cycles to simulate (default: 10000)

**Examples:**
```bash
# Run host simulation
pimid --mode host --port 9999 --cycles 100000

# Quick host test
pimid --mode host --cycles 50000
```

**Use Cases:**
- Host-side performance analysis
- Network co-simulation (host listens for device connection)
- Baseline measurements

---

### 3. Device-Only Mode

Simulate PIM device operations independently.

**Usage:**
```bash
pimid --mode device [options]
```

**Options:**
- `--host HOST` - Host address to connect to (default: 127.0.0.1)
- `--port PORT` - Port to connect to (default: 9999)
- `--cycles CYCLES` - Number of cycles to simulate (default: 10000)

**Examples:**
```bash
# Run device simulation
pimid --mode device --host 127.0.0.1 --port 9999 --cycles 100000

# Connect to remote host
pimid --mode device --host 192.168.1.100 --port 8888
```

**Use Cases:**
- Device-side performance analysis
- Network co-simulation (device connects to host)
- Distributed simulation

---

### 4. Co-Simulation Mode (NEW!)

Simulate host/device cooperation with automatic data transfers and verification.

**Usage:**
```bash
pimid --mode cosim [options]
```

**Options:**
- `--size SIZE` - Array size for vector operation (default: 1048576)

**Examples:**
```bash
# Run co-simulation with 1M elements
pimid --mode cosim --size 1000000

# Quick co-sim test
pimid --mode cosim --size 100000
```

**What It Does:**
1. Allocates memory on both host and device
2. Initializes data on host
3. Transfers data from host to device
4. Executes PIM kernel (vector addition) on device
5. Transfers results back to host
6. Verifies correctness on host
7. Reports transfer bandwidth and throughput

**Output Example:**
```
========================================
PIMID HOST/DEVICE CO-SIMULATION MODE
========================================
Array size: 100000 elements
Data size: 390.625 KB per array

Step 1: Allocating host memory
[Host] Allocated 400000 bytes
...

Step 5: Executing computation on device
[PIM Kernel] Executing vector addition on 100000 elements
[PIM Kernel] Computation completed in 225 us
[PIM Kernel] Throughput: 444.444 M ops/sec

Step 7: Verifying results on host
[Host] ✓ All results verified successfully!

========================================
HOST/DEVICE TRANSFER STATISTICS
========================================
Host -> Device:
  Total bytes: 800000
  Total time: 463 us
  Bandwidth: 1647.82 MB/s

Device -> Host:
  Total bytes: 400000
  Total time: 237 us
  Bandwidth: 1609.58 MB/s

========================================
CO-SIMULATION PASSED
========================================
```

---

## Comparison with Previous Approach

### Before (Multiple Binaries)

```bash
# Standalone
pimid_standalone --workload ./bfs

# Host-only
pimid_host --port 9999

# Device-only
pimid_device --host 127.0.0.1

# Co-simulation (separate test binary)
./tests/host_device_cooperation_test 100000
```

### After (Single Binary)

```bash
# Standalone
pimid --mode standalone --workload ./bfs

# Host-only
pimid --mode host --port 9999

# Device-only
pimid --mode device --host 127.0.0.1

# Co-simulation (integrated!)
pimid --mode cosim --size 100000
```

---

## Advanced Usage

### Config-Driven Simulation

You can specify YAML configuration files for detailed control:

```bash
pimid --mode standalone \
      --config configs/comprehensive_tests_extended/comprehensive/bfs_bank_ooo_sram_1k_deg16.yaml \
      --workload ./workloads/bfs/bfs
```

### Environment Variables

Standalone mode sets environment variables for the workload:

```bash
PIMID_ACTIVE=1
PIMID_MEMORY_TECH=SRAM
PIMID_PLACEMENT_LEVEL=BANK
PIMID_NUM_PES=4
```

### Distributed Simulation

Run host and device on separate machines:

**On Host Machine (192.168.1.100):**
```bash
pimid --mode host --port 9999 --cycles 1000000
```

**On Device Machine:**
```bash
pimid --mode device --host 192.168.1.100 --port 9999 --cycles 1000000
```

---

## Testing All Modes

Quick test script to verify all modes:

```bash
#!/bin/bash

echo "Testing Standalone Mode..."
pimid --mode standalone --workload ./workloads/bfs/bfs 1000 16

echo -e "\nTesting Host Mode..."
pimid --mode host --cycles 10000

echo -e "\nTesting Device Mode..."
pimid --mode device --cycles 10000

echo -e "\nTesting Co-Simulation Mode..."
pimid --mode cosim --size 100000

echo -e "\nAll modes tested successfully!"
```

---

## Workload Integration

Your workloads can detect if they're running under PIMID:

```cpp
#include <cstdlib>
#include <iostream>

int main() {
    // Check if running under PIMID
    const char* pimid_active = std::getenv("PIMID_ACTIVE");

    if (pimid_active && std::string(pimid_active) == "1") {
        std::cout << "Running under PIMID simulation" << std::endl;

        // Get PIM configuration
        const char* memory_tech = std::getenv("PIMID_MEMORY_TECH");
        const char* num_pes = std::getenv("PIMID_NUM_PES");

        std::cout << "Memory: " << memory_tech << std::endl;
        std::cout << "PEs: " << num_pes << std::endl;

        // Use PIM-optimized code path
        run_pim_optimized();
    } else {
        // Use standard code path
        run_standard();
    }
}
```

---

## Troubleshooting

### "Unknown simulation mode"

Make sure you specify a valid mode: `standalone`, `host`, `device`, or `cosim`

```bash
# Wrong:
pimid --mode sim

# Correct:
pimid --mode standalone
```

### "No workload binary specified"

Standalone mode requires a workload:

```bash
# Wrong:
pimid --mode standalone

# Correct:
pimid --mode standalone --workload ./bfs
```

### "Host/Device engine not available"

This is expected. The full host/device engines are optional components. The simulator will fall back to a simulated mode that still demonstrates the architecture.

To enable full engines, build with:
```bash
cmake .. -DHAVE_HOST_ENGINE=ON -DHAVE_DEVICE_ENGINE=ON
```

---

## Future Enhancements

Planned improvements for the unified pimid binary:

1. **Network co-simulation**: Actual host/device network communication
2. **Trace-driven mode**: Replay memory traces
3. **Interactive mode**: REPL for exploration
4. **Batch mode**: Run multiple configs automatically
5. **Plugin system**: Custom simulation modes

---

## Summary

The unified `pimid` binary provides:

✅ **Single entry point** for all simulation modes
✅ **Consistent interface** across standalone, host, device, and co-sim
✅ **Integrated host/device cooperation** testing
✅ **Backward compatible** with existing workloads
✅ **Easy to extend** with new modes

**Before you needed:**
- `pimid_standalone`
- `pimid_host`
- `pimid_device`
- `host_device_cooperation_test`

**Now you only need:**
- `pimid --mode <mode>`

Simple, clean, unified!
