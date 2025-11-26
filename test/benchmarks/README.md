# PIMID Benchmarks

Config-driven benchmark system for evaluating PIM performance across different memory technologies and workloads.

## Overview

This benchmark framework uses YAML configuration files to define benchmark parameters, making it easy to:
- Run the same workload across different memory technologies
- Compare performance without recompiling
- Maintain reproducible benchmark configurations
- Easily add new benchmarks

## Building

The benchmark runner is built as part of the PIMID build process:

```bash
cd pimid/build
cmake ..
make benchmark_runner
```

## Usage

### Running a Single Benchmark

```bash
./benchmark_runner --config configs/benchmarks/bfs_bank_inorder_sram.yaml
```

### Running All Benchmarks (Batch Mode)

```bash
./benchmark_runner --batch configs/benchmarks/
```

This will run all available benchmark configurations and produce a summary table.

## Available Benchmarks

### BFS (Breadth-First Search) with Bank-Level In-Order Core PE

Tests graph traversal performance with bank-level processing elements.

**Configuration files:**
- `bfs_bank_inorder_dram.yaml` - BFS on DRAM (DDR4)
- `bfs_bank_inorder_sram.yaml` - BFS on SRAM
- `bfs_bank_inorder_sttmram.yaml` - BFS on STT-MRAM
- `bfs_bank_inorder_pcm.yaml` - BFS on PCM
- `bfs_bank_inorder_reram.yaml` - BFS on ReRAM

**Workload parameters:**
- 256K vertices (262,144 total)
- Average degree: 16 neighbors per vertex
- 4 banks, 4 PEs (one In-Order Core per bank)

## Configuration File Format

Benchmark configurations use YAML format with the following structure:

```yaml
benchmark:
  name: "Benchmark_Name"
  description: "Description of the benchmark"
  workload_type: "bfs"  # Workload type

workload:
  graph:
    num_vertices: 262144
    avg_degree: 16

memory:
  technology: "SRAM"  # DRAM | SRAM | STT_MRAM | PCM | ReRAM
  hierarchy:
    num_banks: 4
    num_subarrays_per_bank: 4

processing_element:
  type: "in_order_core"
  placement_level: "BANK"
  num_pes: 4

  pipeline:
    fetch_decode_ns: 2.0
    execute_compare_ns: 1.0
    branch_penalty_ns: 3.0

output:
  stats_file: "results/stats.txt"
  summary_format: "table"  # table | json | csv
```

## Example Output

```
========================================
Running: BFS_Bank_InOrder_SRAM
========================================

Memory Characteristics:
  Subarray read:  2.5 ns
  Subarray write: 2.5 ns
  Inner-bank H-tree: 2.74 ns

Results:
  Total latency: 20.27 ms
  Throughput: 12.93 M vertices/sec
  Edges/sec: 206.89 M edges/sec
  Per-vertex time: 309.34 ns
```

## Adding New Benchmarks

1. Create a new YAML config file in `configs/benchmarks/`
2. Define benchmark parameters following the format above
3. Run with `--config <your_config.yaml>`

No recompilation needed!

## Performance Results

See the results documentation:
- `BANK_LEVEL_INORDER_CORE_PIM_RESULTS.md` - Detailed results for bank-level BFS tests

## Architecture

The benchmark framework consists of:

1. **Benchmark Runner** (`benchmark_runner.cpp`)
   - Parses YAML configurations
   - Creates appropriate memory models
   - Executes workloads
   - Collects and reports metrics

2. **Configuration Files** (`configs/benchmarks/*.yaml`)
   - Define benchmark parameters
   - Specify memory technology
   - Configure PE characteristics
   - Set output options

3. **Workload Implementations**
   - BFS: Graph traversal with configurable vertex count and degree
   - (More workloads can be added as needed)

## Future Enhancements

- [ ] Full YAML-CPP integration for richer config parsing
- [ ] Additional workloads (SpMV, DNN inference, etc.)
- [ ] JSON and CSV output formats
- [ ] Automated regression testing
- [ ] Performance visualization tools
