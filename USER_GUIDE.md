# PIMID User Guide

## Table of Contents
1. [Introduction](#introduction)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [Configuration](#configuration)
5. [Running Workloads](#running-workloads)
6. [Creating Custom Workloads](#creating-custom-workloads)
7. [Comprehensive Testing](#comprehensive-testing)
8. [Advanced Usage](#advanced-usage)
9. [Troubleshooting](#troubleshooting)

## Introduction

PIMID (Processing-In-Memory Integrated Development environment) is a comprehensive simulator for Processing-In-Memory (PIM) systems. It allows you to:

- Simulate **any workload** with different memory technologies (DRAM, SRAM, NVMs)
- Configure **PE types** and **placement strategies**
- Support **OpenMP and MPI** workloads
- Generate **comprehensive test suites** automatically
- Analyze **performance and energy** characteristics

### Key Features

✅ **Workload-Agnostic**: Run any binary, not just built-in benchmarks  
✅ **Configuration-Driven**: YAML configs for all parameters  
✅ **Multiple Memory Technologies**: DRAM, SRAM, STT-MRAM, PCM, ReRAM  
✅ **Flexible PE Placement**: Subarray, Bank, or Rank level  
✅ **Parallel Support**: OpenMP and MPI workloads  
✅ **Automated Testing**: 240+ pre-generated test configurations  
✅ **Analysis Tools**: Python scripts for result analysis  

## Installation

### Prerequisites

- C++17 compiler (g++ or clang++)
- CMake 3.15+
- Python 3.6+ (for test generation and analysis)
- OpenMP (optional, for parallel workloads)

### Build PIMID

```bash
cd pimid
mkdir build && cd build
cmake ..
make -j$(nproc)
```

This builds:
- `pimid` - Main simulator binary
- `libpimid_lib.a` - Core library
- Memory models and PE simulators

### Build Example Workloads

```bash
cd workloads/bfs
make standalone      # Build without PIMID
make openmp          # Build with OpenMP
make pimid           # Build with PIMID support
make pimid-openmp    # Build with both
```

## Quick Start

### 1. Run BFS Workload Standalone

```bash
cd workloads/bfs
./bfs --vertices 10000 --degree 16
```

### 2. Run BFS Under PIMID

First, create a configuration file `my_sim.yaml`:

```yaml
simulation:
  name: "BFS_SRAM_Bank"

memory:
  technology: "SRAM"
  num_banks: 4
  subarrays_per_bank: 4

processing_elements:
  type: "in_order_core"
  placement_level: "BANK"
  num_pes: 4

workload:
  binary: "./workloads/bfs/bfs_pimid"

output:
  stats_file: "results/bfs_stats.txt"
```

Then run:

```bash
pimid --config my_sim.yaml --workload ./workloads/bfs/bfs_pimid --vertices 10000 --degree 16
```

### 3. Run Comprehensive Test Suite

```bash
# Generate all 240 test configurations
python3 scripts/generate_test_configs.py --mode comprehensive

# Run quick validation (24 tests)
python3 scripts/run_comprehensive_tests.py --suite quick

# Analyze results
python3 scripts/analyze_results.py results/quick_test/results_*.json
```

## Configuration

PIMID uses YAML configuration files. Here's a complete example:

```yaml
simulation:
  name: "My_PIM_Simulation"
  description: "Testing BFS on different memory technologies"
  mode: "standalone"

memory:
  technology: "SRAM"  # DRAM | SRAM | STT_MRAM | PCM | ReRAM
  
  hierarchy:
    num_banks: 4
    subarrays_per_bank: 4
    capacity_mb: 256

processing_elements:
  type: "in_order_core"  # simple_alu | in_order_core
  placement_level: "BANK"  # SUBARRAY | BANK | RANK
  num_pes: 4

  in_order_core:
    pipeline_stages: 5
    fetch_decode_ns: 2.0
    execute_compare_ns: 1.0
    branch_penalty_ns: 3.0
    branch_prediction_accuracy: 0.5

workload:
  binary: "./my_workload"
  arguments: ["--arg1", "value1", "--arg2", "value2"]

output:
  stats_file: "results/stats.txt"
  trace_file: "results/trace.out"
  enable_detailed_stats: true
  enable_tracing: false
```

### Memory Technologies

| Technology | Read (ns) | Write (ns) | Best For |
|------------|-----------|------------|----------|
| **SRAM** | 2.5 | 2.5 | Highest performance, symmetric access |
| **DRAM** | 13.32 | 15.0 | Moderate performance, high capacity |
| **ReRAM** | 5.0 | 12.0 | Best NVM, balanced read/write |
| **STT-MRAM** | 7.0 | 25.0 | Non-volatile, moderate asymmetry |
| **PCM** | 8.0 | 100.0 | High density, very slow writes |

### PE Types

**Simple ALU**:
- Fast compare (0.3ns)
- No branch support
- Best for predictable workloads

**In-Order Core**:
- 5-stage pipeline
- Branch prediction
- Suitable for complex control flow

### Placement Levels

**SUBARRAY** (16 PEs):
- Highest parallelism
- Most PEs, best for large workloads

**BANK** (4 PEs):
- Good balance
- Moderate parallelism and complexity

**RANK** (1 PE):
- Simplest architecture
- Lowest parallelism

## Running Workloads

### Example: BFS with Different Configs

```bash
# SRAM with bank-level PEs
pimid --config configs/bfs_sram_bank.yaml --workload ./bfs --vertices 100000

# PCM with subarray-level PEs
pimid --config configs/bfs_pcm_subarray.yaml --workload ./bfs --vertices 100000

# DRAM with OpenMP parallel
pimid --config configs/bfs_dram_bank.yaml --workload ./bfs_omp --vertices 100000 --parallel
```

### Using Pre-Generated Configs

We've generated 240 comprehensive test configurations:

```bash
# List available configs
ls configs/comprehensive_tests/comprehensive/

# Run a specific config
pimid --config configs/comprehensive_tests/comprehensive/bfs_bank_inorder_sram_16k_deg16.yaml
```

## Creating Custom Workloads

### 1. Basic Workload Structure

```cpp
#include <pimid/api.h>  // Include PIMID API

int main(int argc, char** argv) {
    // Initialize PIMID
    auto ctx = pimid_init();
    
    // Allocate tracked memory
    int* data = (int*)pimid_malloc(n * sizeof(int));
    
    // Mark PIM region
    PIMID_BEGIN_PIM_REGION("My_Kernel");
    
    // Your computation here
    for (int i = 0; i < n; i++) {
        data[i] = compute(data[i]);
    }
    
    PIMID_END_PIM_REGION();
    
    // Cleanup
    pimid_free(data);
    pimid_finalize(ctx);
    
    return 0;
}
```

### 2. Build with PIMID Support

```bash
g++ -std=c++17 -O3 -DPIMID_ENABLED \
    -I../../pimid/include \
    -o my_workload my_workload.cpp \
    -L../../pimid/build -lpimid_lib
```

### 3. With OpenMP

```cpp
#ifdef _OPENMP
#include <omp.h>
#endif

PIMID_BEGIN_PIM_REGION("Parallel_Kernel");

#pragma omp parallel for
for (int i = 0; i < n; i++) {
    data[i] = compute(data[i]);
}

PIMID_END_PIM_REGION();
```

## Comprehensive Testing

### Generate All Test Configurations

```bash
# Generate 240 comprehensive configs
python3 scripts/generate_test_configs.py --mode comprehensive

# Generate 24 quick validation configs
python3 scripts/generate_test_configs.py --mode quick
```

### Run Test Suites

```bash
# Quick validation (< 1 second, 24 tests)
python3 scripts/run_comprehensive_tests.py --suite quick

# Full comprehensive (5-10 minutes, 240 tests)
python3 scripts/run_comprehensive_tests.py --suite comprehensive

# Custom directory
python3 scripts/run_comprehensive_tests.py --suite custom --configs path/to/configs/
```

### Analyze Results

```bash
# Detailed analysis
python3 scripts/analyze_results.py results/comprehensive/results_*.json

# Shows:
# - Performance comparison by memory technology
# - PE type effectiveness
# - Placement level efficiency
# - Top 10 fastest/slowest configurations
# - Design recommendations
```

### Example Output

```
================================================================================
MEMORY TECHNOLOGY PERFORMANCE RANKING
================================================================================

Rank  Technology       Avg Latency    vs SRAM  Configs
-------------------------------------------------------
   1  sram                   20.27       1.00x        8
   2  reram                  43.54       0.47x        8
   3  dram                   68.00       0.30x        8

DESIGN RECOMMENDATIONS
================================================================================
Best Overall Configuration:
  Config: bfs_bank_inorder_sram_1k_deg16
  Latency: 20.27 ms
  Memory: SRAM
  PE Type: inorder
  Placement: bank

Recommendations:
  ⚠️  Avoid PCM for write-heavy workloads like BFS
  ✓ In-Order Core recommended when branch prediction is effective
  ✓ Bank-level PIM offers good balance of parallelism and complexity
```

## Advanced Usage

### Batch Testing

Create a script to test multiple configurations:

```bash
#!/bin/bash
for tech in sram dram reram; do
  for placement in bank subarray; do
    pimid --config configs/bfs_${placement}_${tech}.yaml \
          --workload ./bfs --vertices 100000 \
          > results/bfs_${placement}_${tech}.log
  done
done
```

### Custom Test Generation

Modify `scripts/generate_test_configs.py`:

```python
# Add new memory technology
MEMORY_TECHS['my_tech'] = {
    'name': 'MY_TECH',
    'subarray_read_ns': 10.0,
    'subarray_write_ns': 20.0,
    'inner_bank_ns': 5.0
}

# Add new workload size
WORKLOAD_SIZES['huge'] = {
    'name': 'Huge',
    'num_vertices': 1000000,
    'suffix': '1m'
}
```

### Performance Profiling

Enable detailed statistics and tracing:

```yaml
simulation_control:
  enable_power_modeling: true
  enable_detailed_stats: true
  enable_tracing: true

output:
  stats_file: "detailed_stats.txt"
  trace_file: "memory_trace.out"
```

## Troubleshooting

### Workload Binary Not Found

```
Error: Failed to execute workload: ./my_workload
```

**Solution**: Ensure the workload binary is compiled and executable:
```bash
chmod +x my_workload
file my_workload  # Check it's a valid executable
```

### PIMID Library Not Found

```
error while loading shared libraries: libpimid_lib.so
```

**Solution**: Add PIMID library path:
```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:pimid/build
```

### Config File Parsing Errors

```
Warning: Could not open config file
```

**Solution**: Check file path and format:
```bash
# Verify file exists
ls -l my_config.yaml

# Check YAML syntax
python3 -c "import yaml; yaml.safe_load(open('my_config.yaml'))"
```

### OpenMP Not Available

```
Error: OpenMP not available
```

**Solution**: Install OpenMP support:
```bash
# Ubuntu/Debian
sudo apt-get install libomp-dev

# macOS
brew install libomp
```

## Best Practices

1. **Start with Quick Tests**: Use the quick suite (24 configs) for rapid validation
2. **Use Appropriate Workload Sizes**: Small graphs for testing, large for benchmarking
3. **Match Memory Tech to Workload**: Write-heavy → SRAM/ReRAM, Read-heavy → Any
4. **Compare Multiple Configs**: Use comprehensive suite to explore design space
5. **Analyze Results**: Always use analysis scripts to identify patterns
6. **Version Control Configs**: Keep your YAML configs in git
7. **Document Custom Workloads**: Add README.md to each workload directory

## Getting Help

- **Architecture**: See `ARCHITECTURE.md`
- **API Reference**: See `include/pimid/api.h`
- **Examples**: See `workloads/bfs/` and `workloads/template/`
- **Test Framework**: See `configs/comprehensive_tests/README.md`

## Example Workflow

Complete example from start to finish:

```bash
# 1. Build PIMID
cd pimid/build
cmake .. && make -j4

# 2. Build BFS workload
cd ../../workloads/bfs
make pimid-openmp

# 3. Create config
cat > my_test.yaml << 'YAML'
memory:
  technology: "SRAM"
processing_elements:
  type: "in_order_core"
  placement_level: "BANK"
  num_pes: 4
YAML

# 4. Run simulation
../../pimid/build/pimid --config my_test.yaml \
                        --workload ./bfs_pimid_omp \
                        --vertices 100000 --degree 16 --parallel

# 5. Run comprehensive tests
cd ../../pimid
python3 scripts/run_comprehensive_tests.py --suite quick

# 6. Analyze results
python3 scripts/analyze_results.py results/quick_test/results_*.json
```

This should give you a complete understanding of PIMID capabilities and best performance!
