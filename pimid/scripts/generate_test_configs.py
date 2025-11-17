#!/usr/bin/env python3
"""
Generate comprehensive test configurations for PIMID benchmarks.
Creates YAML config files for all combinations of:
- Memory technologies
- PE types
- PE placement levels
- Workload sizes
- Graph degrees
"""

import os
import itertools
from pathlib import Path

# Test dimensions
MEMORY_TECHS = {
    'dram': {
        'name': 'DRAM',
        'subarray_read_ns': 13.32,
        'subarray_write_ns': 15.0,
        'inner_bank_ns': 6.65
    },
    'sram': {
        'name': 'SRAM',
        'subarray_read_ns': 2.5,
        'subarray_write_ns': 2.5,
        'inner_bank_ns': 2.74
    },
    'sttmram': {
        'name': 'STT_MRAM',
        'subarray_read_ns': 7.0,
        'subarray_write_ns': 25.0,
        'inner_bank_ns': 7.3
    },
    'pcm': {
        'name': 'PCM',
        'subarray_read_ns': 8.0,
        'subarray_write_ns': 100.0,
        'inner_bank_ns': 8.65
    },
    'reram': {
        'name': 'ReRAM',
        'subarray_read_ns': 5.0,
        'subarray_write_ns': 12.0,
        'inner_bank_ns': 5.62
    }
}

PE_TYPES = {
    'simple_alu': {
        'name': 'Simple ALU',
        'type': 'simple_alu',
        'fetch_decode_ns': 0.0,
        'compare_ns': 0.3,
        'branch_ns': 0.0,
        'alu_ns': 0.5
    },
    'inorder': {
        'name': 'In-Order Core',
        'type': 'in_order_core',
        'fetch_decode_ns': 2.0,
        'compare_ns': 1.0,
        'branch_ns': 3.0,
        'alu_ns': 1.0
    }
}

PLACEMENT_LEVELS = {
    'subarray': {
        'level': 'SUBARRAY',
        'num_banks': 4,
        'subarrays_per_bank': 4,
        'num_pes': 16  # 4 banks * 4 subarrays
    },
    'bank': {
        'level': 'BANK',
        'num_banks': 4,
        'subarrays_per_bank': 4,
        'num_pes': 4  # 1 per bank
    },
    'rank': {
        'level': 'RANK',
        'num_banks': 4,
        'subarrays_per_bank': 4,
        'num_pes': 1  # 1 per rank
    }
}

WORKLOAD_SIZES = {
    'tiny': {
        'name': 'Tiny',
        'num_vertices': 1024,  # 1K
        'suffix': '1k'
    },
    'small': {
        'name': 'Small',
        'num_vertices': 4096,  # 4K
        'suffix': '4k'
    },
    'medium': {
        'name': 'Medium',
        'num_vertices': 16384,  # 16K
        'suffix': '16k'
    },
    'large': {
        'name': 'Large',
        'num_vertices': 65536,  # 64K
        'suffix': '64k'
    }
}

GRAPH_DEGREES = [8, 16]


def generate_config(memory_tech, pe_type, placement, workload_size, degree, output_dir):
    """Generate a single YAML config file."""

    mem = MEMORY_TECHS[memory_tech]
    pe = PE_TYPES[pe_type]
    place = PLACEMENT_LEVELS[placement]
    workload = WORKLOAD_SIZES[workload_size]

    # Generate filename
    filename = f"bfs_{placement}_{pe_type}_{memory_tech}_{workload['suffix']}_deg{degree}.yaml"
    filepath = output_dir / filename

    # Benchmark name
    benchmark_name = f"BFS_{place['level']}_{pe['name'].replace(' ', '')}_{mem['name']}"

    # Generate YAML content
    yaml_content = f"""# BFS Benchmark Configuration - Auto-generated
# Placement: {place['level']}, PE: {pe['name']}, Memory: {mem['name']}
# Workload: {workload['name']} ({workload['num_vertices']} vertices, degree {degree})

benchmark:
  name: "{benchmark_name}"
  description: "BFS with {place['level']}-level {pe['name']} PE on {mem['name']}"
  workload_type: "bfs"

# Workload parameters
workload:
  graph:
    num_vertices: {workload['num_vertices']}
    avg_degree: {degree}
    topology: "random"

  bfs:
    root_vertex: 0
    traversal_mode: "level_synchronous"

# Memory configuration
memory:
  technology: "{mem['name']}"

  hierarchy:
    num_banks: {place['num_banks']}
    num_subarrays_per_bank: {place['subarrays_per_bank']}

  # Memory timing characteristics
  timing:
    subarray_read_ns: {mem['subarray_read_ns']}
    subarray_write_ns: {mem['subarray_write_ns']}
    inner_bank_htree_ns: {mem['inner_bank_ns']}

# Processing element configuration
processing_element:
  type: "{pe['type']}"
  placement_level: "{place['level']}"
  num_pes: {place['num_pes']}

"""

    # Add PE-specific config
    if pe_type == 'inorder':
        yaml_content += f"""  # In-Order Core specifications
  pipeline:
    stages: 5
    fetch_decode_ns: {pe['fetch_decode_ns']}
    execute_compare_ns: {pe['compare_ns']}
    execute_alu_ns: {pe['alu_ns']}
    memory_access_ns: 1.0
    writeback_ns: 0.5
    branch_penalty_ns: {pe['branch_ns']}
    branch_prediction_accuracy: 0.5

  energy:
    fetch_decode_pj: 10.0
    alu_pj: 5.0
    branch_pj: 8.0
"""
    else:
        yaml_content += f"""  # Simple ALU specifications
  timing:
    compare_ns: {pe['compare_ns']}
    alu_ns: {pe['alu_ns']}
    load_ns: 0.5

  energy:
    alu_pj: 2.0
"""

    yaml_content += f"""
# Simulation control
simulation:
  max_cycles: 1000000000
  enable_power_modeling: true
  enable_detailed_stats: true

# Output configuration
output:
  stats_file: "results/{filename.replace('.yaml', '_stats.txt')}"
  detailed_log: "results/{filename.replace('.yaml', '_detailed.log')}"
  summary_format: "table"

# Test metadata
metadata:
  placement_level: "{placement}"
  pe_type: "{pe_type}"
  memory_tech: "{memory_tech}"
  workload_size: "{workload_size}"
  graph_degree: {degree}
  auto_generated: true
"""

    # Write file
    with open(filepath, 'w') as f:
        f.write(yaml_content)

    return filename


def generate_all_configs(output_dir, filters=None):
    """Generate all config combinations."""

    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    # Define which combinations to generate
    if filters is None:
        filters = {
            'memory_techs': list(MEMORY_TECHS.keys()),
            'pe_types': list(PE_TYPES.keys()),
            'placements': list(PLACEMENT_LEVELS.keys()),
            'workload_sizes': list(WORKLOAD_SIZES.keys()),
            'degrees': GRAPH_DEGREES
        }

    configs_generated = []

    # Generate all combinations
    for memory_tech in filters['memory_techs']:
        for pe_type in filters['pe_types']:
            for placement in filters['placements']:
                for workload_size in filters['workload_sizes']:
                    for degree in filters['degrees']:
                        filename = generate_config(
                            memory_tech, pe_type, placement,
                            workload_size, degree, output_dir
                        )
                        configs_generated.append(filename)

    return configs_generated


def generate_quick_test_suite(output_dir):
    """Generate a smaller quick test suite for rapid validation."""

    filters = {
        'memory_techs': ['sram', 'dram', 'reram'],  # 3 techs
        'pe_types': ['simple_alu', 'inorder'],  # 2 types
        'placements': ['subarray', 'bank'],  # 2 placements
        'workload_sizes': ['tiny', 'small'],  # 2 sizes
        'degrees': [16]  # 1 degree
    }

    return generate_all_configs(output_dir, filters)


def generate_comprehensive_test_suite(output_dir):
    """Generate comprehensive test suite with all combinations."""

    return generate_all_configs(output_dir, filters=None)


def generate_test_matrix_doc(configs, output_file):
    """Generate documentation for the test matrix."""

    doc = """# PIMID Comprehensive Test Matrix

This directory contains auto-generated benchmark configurations covering all
combinations of memory technologies, PE types, placement levels, and workload sizes.

## Test Dimensions

### Memory Technologies (5)
- **DRAM**: DDR4, 13.32ns read, 15ns write
- **SRAM**: 2.5ns symmetric access
- **STT-MRAM**: 7ns read, 25ns write (3.57x asymmetry)
- **PCM**: 8ns read, 100ns write (12.5x asymmetry)
- **ReRAM**: 5ns read, 12ns write (2.4x asymmetry)

### PE Types (2)
- **Simple ALU**: Fast compare (0.3ns), no branch support
- **In-Order Core**: 5-stage pipeline, branch support (3ns penalty)

### Placement Levels (3)
- **Subarray**: 16 PEs (1 per subarray), 4 banks × 4 subarrays
- **Bank**: 4 PEs (1 per bank)
- **Rank**: 1 PE (shared across all banks)

### Workload Sizes (4)
- **Tiny**: 1K vertices (quick smoke tests)
- **Small**: 4K vertices (fast validation)
- **Medium**: 16K vertices (moderate scale)
- **Large**: 64K vertices (larger scale)

### Graph Degrees (2)
- **8**: Sparse graphs
- **16**: Moderate density

## Total Configurations

Total configs: 5 × 2 × 3 × 4 × 2 = **{total} configurations**

## Generated Configs

```
{config_list}
```

## Running Tests

### Run single config:
```bash
./benchmark_runner --config configs/comprehensive_tests/<config_name>.yaml
```

### Run all configs (batch mode):
```bash
./benchmark_runner --batch configs/comprehensive_tests/
```

### Run with analysis script:
```bash
python3 scripts/run_comprehensive_tests.py
```

## Expected Results

The comprehensive test suite will reveal:
- Performance impact of write asymmetry (PCM vs SRAM)
- PE placement efficiency (subarray vs bank vs rank)
- Branch overhead costs (Simple ALU vs In-Order Core)
- Scalability characteristics across workload sizes
- Technology-specific bottlenecks

## File Naming Convention

Format: `bfs_<placement>_<pe_type>_<memory_tech>_<size>_deg<degree>.yaml`

Examples:
- `bfs_bank_inorder_sram_16k_deg16.yaml`
- `bfs_subarray_simple_alu_pcm_4k_deg8.yaml`
"""

    total = len(configs)
    config_list = '\n'.join(f"- {c}" for c in sorted(configs))

    with open(output_file, 'w') as f:
        f.write(doc.format(total=total, config_list=config_list))


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='Generate PIMID benchmark configs')
    parser.add_argument('--output', '-o', default='configs/comprehensive_tests',
                        help='Output directory for configs')
    parser.add_argument('--mode', '-m', choices=['quick', 'comprehensive', 'both'],
                        default='both', help='Test suite mode')

    args = parser.parse_args()

    output_dir = Path(args.output)

    if args.mode in ['quick', 'both']:
        print("Generating quick test suite...")
        quick_dir = output_dir / 'quick'
        quick_configs = generate_quick_test_suite(quick_dir)
        print(f"Generated {len(quick_configs)} quick test configs in {quick_dir}")
        generate_test_matrix_doc(quick_configs, quick_dir / 'README.md')

    if args.mode in ['comprehensive', 'both']:
        print("\nGenerating comprehensive test suite...")
        comp_dir = output_dir / 'comprehensive'
        comp_configs = generate_comprehensive_test_suite(comp_dir)
        print(f"Generated {len(comp_configs)} comprehensive test configs in {comp_dir}")
        generate_test_matrix_doc(comp_configs, comp_dir / 'README.md')

    print("\nDone! Config generation complete.")
