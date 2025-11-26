#!/usr/bin/env python3
"""
All DRAM Types Long Simulation Test Suite - 5000 Tests
Comprehensive coverage of ALL DRAM types and ALL PIM granularity levels
Uses PIMID binary as the ONLY entry point with proper YAML configs
"""

import os
import subprocess
import json
import random
import time
from datetime import datetime
from pathlib import Path
import itertools

# Set random seed for reproducibility
random.seed(20251120)

# Base directories
BASE_DIR = Path("/home/user/pimid-dev")
WORKLOAD_DIR = BASE_DIR / "DAC26/workloads_pimid"
RESULTS_DIR = BASE_DIR / "test/results/test_results_all_dram_5000"
CONFIG_DIR = RESULTS_DIR / "configs"
DRAM_CONFIG_DIR = BASE_DIR / "pimid/configs/dram"
PIMID_BINARY = BASE_DIR / "build/pimid/pimid"

# Create directories
RESULTS_DIR.mkdir(exist_ok=True)
CONFIG_DIR.mkdir(exist_ok=True)

# ALL DRAM Types with their configuration files
DRAM_TYPES = {
    'DDR3': {
        'type': 'DDR3',
        'config_file': 'ddr3_htree.yaml',
        'description': 'DDR3 with H-tree',
    },
    'DDR4': {
        'type': 'DDR4',
        'config_file': 'ddr4_htree.yaml',
        'description': 'DDR4 with H-tree',
    },
    'DDR4_VRR': {
        'type': 'DDR4',
        'config_file': 'ddr4_vrr_htree.yaml',
        'description': 'DDR4 VRR with H-tree',
    },
    'DDR4_RVRR': {
        'type': 'DDR4',
        'config_file': 'ddr4_rvrr_htree.yaml',
        'description': 'DDR4 RVRR with H-tree',
    },
    'DDR5': {
        'type': 'DDR5',
        'config_file': 'ddr5_htree.yaml',
        'description': 'DDR5 with H-tree',
    },
    'DDR5_VRR': {
        'type': 'DDR5',
        'config_file': 'ddr5_vrr_htree.yaml',
        'description': 'DDR5 VRR with H-tree',
    },
    'DDR5_RVRR': {
        'type': 'DDR5',
        'config_file': 'ddr5_rvrr_htree.yaml',
        'description': 'DDR5 RVRR with H-tree',
    },
    'LPDDR5': {
        'type': 'LPDDR5',
        'config_file': 'lpddr5_htree.yaml',
        'description': 'LPDDR5 with H-tree',
    },
    'GDDR6': {
        'type': 'GDDR6',
        'config_file': 'gddr6_htree.yaml',
        'description': 'GDDR6 with H-tree',
    },
    'HBM': {
        'type': 'HBM',
        'config_file': 'hbm_htree.yaml',
        'description': 'HBM with H-tree',
    },
    'HBM2': {
        'type': 'HBM2',
        'config_file': 'hbm2_htree.yaml',
        'description': 'HBM2 with H-tree',
    },
    'HBM3': {
        'type': 'HBM3',
        'config_file': 'hbm3_htree.yaml',
        'description': 'HBM3 with H-tree',
    },
}

# PIM Granularity levels (ALL 6 levels for DRAM-based PIM)
PIM_GRANULARITIES = {
    'MEMORY_CONTROLLER': {'name': 'MC-PIM', 'level': 'MEMORY_CONTROLLER'},
    'RANK': {'name': 'Rank-PIM', 'level': 'RANK'},
    'CHIP': {'name': 'Chip-PIM', 'level': 'CHIP'},
    'BANK_GROUP': {'name': 'BG-PIM', 'level': 'BANK_GROUP'},
    'BANK': {'name': 'Bank-PIM', 'level': 'BANK'},
    'SUBARRAY': {'name': 'Subarray-PIM', 'level': 'SUBARRAY'},
}

# Workload definitions with LONGER sizes for extended simulations
WORKLOADS = {
    'bfs_message': {
        'binary': 'bfs_message_pimid',
        'params': ['num_subarrays', 'num_vertices', 'is_libcom', 'mem_tech'],
        'size_param': 'num_vertices',
        # Larger sizes for longer simulations
        'sizes': [2048, 4096, 8192, 16384, 32768, 65536],
        'supports_mem_tech': True,
    },
    'bfs_shared': {
        'binary': 'bfs_shared_pimid',
        'params': ['num_subarrays', 'num_vertices', 'is_libcom'],
        'size_param': 'num_vertices',
        'sizes': [2048, 4096, 8192, 16384, 32768, 65536],
        'supports_mem_tech': False,
    },
    'gemm_message': {
        'binary': 'gemm_message_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        # Larger matrix sizes for longer compute
        'sizes': [384, 512, 768, 1024, 1536, 2048],
        'supports_mem_tech': False,
    },
    'gemm_shared': {
        'binary': 'gemm_shared_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [384, 512, 768, 1024, 1536, 2048],
        'supports_mem_tech': False,
    },
    'spmv_message': {
        'binary': 'spmv_message_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [384, 512, 768, 1024, 1536, 2048],
        'supports_mem_tech': False,
    },
    'spmv_shared': {
        'binary': 'spmv_shared_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [384, 512, 768, 1024, 1536, 2048],
        'supports_mem_tech': False,
    },
    'dotproduct_message': {
        'binary': 'dotproduct_message_pimid',
        'params': ['num_subarrays', 'vector_length', 'is_libcom'],
        'size_param': 'vector_length',
        # Larger vectors for longer simulations
        'sizes': [16384, 32768, 65536, 131072, 262144],
        'supports_mem_tech': False,
    },
    'dotproduct_shared': {
        'binary': 'dotproduct_shared_pimid',
        'params': ['num_subarrays', 'vector_length', 'is_libcom'],
        'size_param': 'vector_length',
        'sizes': [16384, 32768, 65536, 131072, 262144],
        'supports_mem_tech': False,
    },
    'reduction_message': {
        'binary': 'reduction_message_pimid',
        'params': ['num_subarrays', 'elements_per_subarray', 'is_libcom'],
        'size_param': 'elements_per_subarray',
        'sizes': [4096, 8192, 16384, 32768, 65536],
        'supports_mem_tech': False,
    },
    'reduction_shared': {
        'binary': 'reduction_shared_pimid',
        'params': ['num_subarrays', 'elements_per_subarray', 'is_libcom'],
        'size_param': 'elements_per_subarray',
        'sizes': [4096, 8192, 16384, 32768, 65536],
        'supports_mem_tech': False,
    },
    'histogram_message': {
        'binary': 'histogram_message_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [8192, 16384, 32768, 65536, 131072],
        'supports_mem_tech': False,
    },
    'histogram_shared': {
        'binary': 'histogram_shared_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [8192, 16384, 32768, 65536, 131072],
        'supports_mem_tech': False,
    },
    'prefixsum_message': {
        'binary': 'prefixsum_message_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [4096, 8192, 16384, 32768, 65536],
        'supports_mem_tech': False,
    },
    'prefixsum_shared': {
        'binary': 'prefixsum_shared_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [4096, 8192, 16384, 32768, 65536],
        'supports_mem_tech': False,
    },
    'stencil1d_message': {
        'binary': 'stencil1d_message_pimid',
        'params': ['num_subarrays', 'num_points', 'num_iters', 'is_libcom'],
        'size_param': 'num_points',
        'sizes': [4098, 8194, 16386, 32770],
        'extra_param': 'num_iters',
        # More iterations for longer simulations
        'extra_values': [200, 500, 1000, 2000, 5000],
        'supports_mem_tech': False,
    },
    'stencil1d_shared': {
        'binary': 'stencil1d_shared_pimid',
        'params': ['num_subarrays', 'num_points', 'num_iters', 'is_libcom'],
        'size_param': 'num_points',
        'sizes': [4098, 8194, 16386, 32770],
        'extra_param': 'num_iters',
        'extra_values': [200, 500, 1000, 2000, 5000],
        'supports_mem_tech': False,
    },
}

# Parameter ranges for comprehensive testing
NUM_SUBARRAYS = [2, 4, 8, 16, 32, 64, 128]  # More parallelism levels
IS_LIBCOM = [0, 1]  # 0 = baseline, 1 = LIBCom

def generate_all_dram_configs(num_tests=5000):
    """Generate 5000 test configurations covering ALL DRAM types and PIM levels"""

    print(f"Generating {num_tests} comprehensive DRAM test configurations...")
    print("\nComprehensive Coverage:")
    print(f"  - ALL {len(DRAM_TYPES)} DRAM types: {', '.join(DRAM_TYPES.keys())}")
    print(f"  - ALL {len(PIM_GRANULARITIES)} PIM granularity levels")
    print("  - Larger problem sizes for extended runtime")
    print("  - Higher iteration counts for iterative workloads")
    print(f"  - All {len(WORKLOADS)} workload types")
    print("  - Systematic + random sampling for full coverage\n")

    configs = []

    # ========================================================================
    # Phase 1: Systematic DRAM × PIM level × Workload coverage (2400 tests)
    # Ensure every DRAM type is tested with every PIM level
    # ========================================================================
    print("Phase 1: Systematic DRAM × PIM × Workload coverage (2400 tests)...")

    systematic_combinations = []
    for dram_name in DRAM_TYPES.keys():
        for pim_level_name in PIM_GRANULARITIES.keys():
            for workload_name, workload_info in WORKLOADS.items():
                for num_subs in [4, 8, 16, 32]:  # Representative subarray counts
                    for is_lib in IS_LIBCOM:
                        # Select larger sizes (upper half of size range)
                        size_options = workload_info['sizes'][len(workload_info['sizes'])//2:]
                        size = random.choice(size_options)

                        combo = {
                            'workload': workload_name,
                            'num_subarrays': num_subs,
                            'size': size,
                            'is_libcom': is_lib,
                            'pim_granularity': pim_level_name,
                            'dram_type': dram_name,
                        }

                        # Handle stencil1d with more iterations
                        if 'extra_param' in workload_info:
                            # Select from upper half of iteration range
                            iter_options = workload_info['extra_values'][len(workload_info['extra_values'])//2:]
                            combo['extra_param'] = random.choice(iter_options)

                        systematic_combinations.append(combo)

    # Sample 2400 from systematic combinations
    num_systematic = min(2400, len(systematic_combinations))
    systematic_sample = random.sample(systematic_combinations, num_systematic)

    for combo in systematic_sample:
        configs.append((len(configs), combo))

    print(f"  Generated {len(configs)} systematic configurations")

    # ========================================================================
    # Phase 2: High-bandwidth memory focused tests (HBM, HBM2, HBM3, GDDR6) (800 tests)
    # ========================================================================
    print("Phase 2: High-bandwidth memory configurations (800 tests)...")

    hbm_types = ['HBM', 'HBM2', 'HBM3', 'GDDR6']
    for _ in range(800):
        workload_name = random.choice(list(WORKLOADS.keys()))
        workload_info = WORKLOADS[workload_name]
        dram_name = random.choice(hbm_types)
        pim_level_name = random.choice(list(PIM_GRANULARITIES.keys()))

        config = {
            'workload': workload_name,
            'num_subarrays': random.choice(NUM_SUBARRAYS),
            'size': random.choice(workload_info['sizes']),
            'is_libcom': random.choice(IS_LIBCOM),
            'pim_granularity': pim_level_name,
            'dram_type': dram_name,
        }

        if 'extra_param' in workload_info:
            config['extra_param'] = random.choice(workload_info['extra_values'])

        configs.append((len(configs), config))

    print(f"  Generated {len(configs)} total configurations")

    # ========================================================================
    # Phase 3: DDR5 family focused tests (latest DDR technology) (600 tests)
    # ========================================================================
    print("Phase 3: DDR5 family configurations (600 tests)...")

    ddr5_types = ['DDR5', 'DDR5_VRR', 'DDR5_RVRR']
    for _ in range(600):
        workload_name = random.choice(list(WORKLOADS.keys()))
        workload_info = WORKLOADS[workload_name]
        dram_name = random.choice(ddr5_types)
        pim_level_name = random.choice(list(PIM_GRANULARITIES.keys()))

        config = {
            'workload': workload_name,
            'num_subarrays': random.choice(NUM_SUBARRAYS),
            'size': random.choice(workload_info['sizes']),
            'is_libcom': random.choice(IS_LIBCOM),
            'pim_granularity': pim_level_name,
            'dram_type': dram_name,
        }

        if 'extra_param' in workload_info:
            config['extra_param'] = random.choice(workload_info['extra_values'])

        configs.append((len(configs), config))

    print(f"  Generated {len(configs)} total configurations")

    # ========================================================================
    # Phase 4: Maximum size + high parallelism stress tests (600 tests)
    # ========================================================================
    print("Phase 4: Maximum size and high parallelism tests (600 tests)...")

    for _ in range(600):
        workload_name = random.choice(list(WORKLOADS.keys()))
        workload_info = WORKLOADS[workload_name]
        dram_name = random.choice(list(DRAM_TYPES.keys()))
        pim_level_name = random.choice(list(PIM_GRANULARITIES.keys()))

        # Maximum size and high parallelism
        config = {
            'workload': workload_name,
            'num_subarrays': random.choice([32, 64, 128]),  # High parallelism
            'size': workload_info['sizes'][-1],  # MAXIMUM size
            'is_libcom': random.choice(IS_LIBCOM),
            'pim_granularity': pim_level_name,
            'dram_type': dram_name,
        }

        if 'extra_param' in workload_info:
            config['extra_param'] = workload_info['extra_values'][-1]  # MAXIMUM iterations

        configs.append((len(configs), config))

    print(f"  Generated {len(configs)} total configurations")

    # ========================================================================
    # Phase 5: Random mixed configurations for comprehensive coverage (600 tests)
    # ========================================================================
    print("Phase 5: Random mixed configurations (600 tests)...")

    while len(configs) < num_tests:
        workload_name = random.choice(list(WORKLOADS.keys()))
        workload_info = WORKLOADS[workload_name]
        dram_name = random.choice(list(DRAM_TYPES.keys()))
        pim_level_name = random.choice(list(PIM_GRANULARITIES.keys()))

        config = {
            'workload': workload_name,
            'num_subarrays': random.choice(NUM_SUBARRAYS),
            'size': random.choice(workload_info['sizes']),
            'is_libcom': random.choice(IS_LIBCOM),
            'pim_granularity': pim_level_name,
            'dram_type': dram_name,
        }

        if 'extra_param' in workload_info:
            config['extra_param'] = random.choice(workload_info['extra_values'])

        configs.append((len(configs), config))

    print(f"\nGenerated {len(configs)} comprehensive DRAM configurations\n")
    return configs

def generate_pimid_config(test_id, config):
    """Generate comprehensive YAML config file for PIMID simulator with specific DRAM type"""

    pim_granularity = config.get('pim_granularity', 'BANK')
    num_pes = config['num_subarrays']
    dram_type_name = config.get('dram_type', 'DDR4')
    dram_info = DRAM_TYPES[dram_type_name]

    # Read the base DRAM config file
    dram_config_file = DRAM_CONFIG_DIR / dram_info['config_file']

    yaml_content = f"""# PIMID All-DRAM Long Simulation Test Configuration - Test {test_id:04d}
# Auto-generated for comprehensive DRAM type coverage
# DRAM Type: {dram_type_name} ({dram_info['description']})

simulation:
  name: "AllDRAM_Test_{test_id:04d}"
  description: "Long sim: {config['workload']} with {dram_type_name} at {pim_granularity}"
  mode: "standalone"

# Import base DRAM configuration
import_dram_config: "{dram_config_file}"

# Memory configuration - Using {dram_type_name}
memory:
  technology: "DRAM"
  dram_type: "{dram_info['type']}"
  capacity: "16GB"
  channels: 4
  ranks_per_channel: 2
  banks_per_rank: 16

# Memory hierarchy configuration
memory_hierarchy:
  subarray:
    size_mb: 2
  bank:
    size_mb: 32
  chip:
    size_gb: 2
  rank:
    size_gb: 16

# PIM configuration with specific granularity level
pim:
  granularity: {pim_granularity}
  num_pes: {num_pes}
  pe_frequency_mhz: 1000
  pe_num_cores: 1
  pe_has_l1_cache: true
  pe_l1_size_kb: 32
  addressing_mode: "UNIFIED"
  scheduler: "NEAREST_PE"

# Simulation control - higher cycle limit for longer simulations
simulation_control:
  max_cycles: 10000000000000
  warmup_cycles: 100000
  enable_power_modeling: true

# Cache hierarchy
caches:
  l1i:
    size_kb: 32
    associativity: 8
    line_size_bytes: 64
  l1d:
    size_kb: 32
    associativity: 8
    line_size_bytes: 64
  l2:
    size_kb: 256
    associativity: 8
    line_size_bytes: 64
  l3:
    size_kb: 8192
    associativity: 16
    line_size_bytes: 64

# Network-on-chip configuration
network:
  enabled: true
  topology: "MESH_2D"
  num_rows: 4
  num_cols: 4
  routing: "XY"
  link_width_bytes: 8
  link_latency_cycles: 1
  router_latency_cycles: 2
  virtual_channels: 4

# Power modeling
power:
  enabled: true
  tech_node_nm: 22
  device_type: "HP"
  temperature_k: 350
  frequency_ghz: 2.0
  model_host: true
  model_device: true
  model_memory: true
  model_network: true

# Output configuration
output:
  stats_file: "all_dram_{test_id:04d}_stats.txt"
  log_level: "ERROR"
  enable_detailed_stats: true
  enable_tracing: false

# Statistics collection
statistics:
  collect_memory_stats: true
  collect_network_stats: true
  collect_power_stats: true
  collect_scheduler_stats: true
  output_formats:
    - "TXT"
    - "JSON"
"""

    config_file = CONFIG_DIR / f"all_dram_{test_id:04d}.yaml"
    with open(config_file, 'w') as f:
        f.write(yaml_content)

    return config_file

def run_test(test_id, config):
    """Run a single test using PIMID binary with comprehensive DRAM config"""

    workload_name = config['workload']
    workload_info = WORKLOADS[workload_name]
    workload_binary = WORKLOAD_DIR / workload_info['binary']

    # Generate comprehensive PIMID config file
    config_file = generate_pimid_config(test_id, config)

    # Build pimid command - binary is the ONLY entry point
    cmd = [
        str(PIMID_BINARY),
        '--mode', 'standalone',
        '--config', str(config_file),
        '--workload', str(workload_binary)
    ]

    # Add workload parameters
    cmd.append(str(config['num_subarrays']))
    cmd.append(str(config['size']))

    # Handle stencil1d extra parameter (iterations)
    if 'extra_param' in config:
        cmd.append(str(config['extra_param']))

    cmd.append(str(config['is_libcom']))

    # Add memory tech parameter if supported (DRAM = 1)
    if workload_name == 'bfs_message':
        cmd.append('1')  # mem_tech = 1 for DRAM

    # Run with longer timeout for longer simulations
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=900,  # 15 minute timeout per test (longer simulations)
            cwd=str(BASE_DIR)
        )

        return {
            'test_id': test_id,
            'workload': workload_name,
            'config': config,
            'command': ' '.join(cmd),
            'status': 'completed' if result.returncode == 0 else 'failed',
            'returncode': result.returncode,
            'stdout': result.stdout[-3000:] if len(result.stdout) > 3000 else result.stdout,
            'stderr': result.stderr[-1000:] if len(result.stderr) > 1000 else result.stderr,
        }

    except subprocess.TimeoutExpired:
        return {
            'test_id': test_id,
            'workload': workload_name,
            'config': config,
            'command': ' '.join(cmd),
            'status': 'timeout',
            'returncode': -1,
            'stdout': '',
            'stderr': 'Test exceeded 15 minute timeout',
        }
    except Exception as e:
        return {
            'test_id': test_id,
            'workload': workload_name,
            'config': config,
            'command': ' '.join(cmd),
            'status': 'error',
            'returncode': -1,
            'stdout': '',
            'stderr': str(e),
        }

def parse_output(stdout):
    """Parse energy and timing metrics from PIMID output"""

    metrics = {
        'total_energy_pj': None,
        'network_energy_pj': None,
        'execution_time_ns': None,
        'total_cycles': None,
        'compute_energy_pj': None,
        'memory_energy_pj': None,
    }

    for line in stdout.split('\n'):
        line_lower = line.lower()

        if 'total energy:' in line_lower:
            try:
                metrics['total_energy_pj'] = float(line.split(':')[1].split('pJ')[0].strip())
            except:
                pass
        elif 'network energy:' in line_lower:
            try:
                metrics['network_energy_pj'] = float(line.split(':')[1].split('pJ')[0].strip())
            except:
                pass
        elif 'compute energy:' in line_lower:
            try:
                metrics['compute_energy_pj'] = float(line.split(':')[1].split('pJ')[0].strip())
            except:
                pass
        elif 'memory energy:' in line_lower:
            try:
                metrics['memory_energy_pj'] = float(line.split(':')[1].split('pJ')[0].strip())
            except:
                pass
        elif 'execution time:' in line_lower and 'ns' in line_lower:
            try:
                metrics['execution_time_ns'] = float(line.split(':')[1].split('ns')[0].strip())
            except:
                pass
        elif 'total cycles:' in line_lower:
            try:
                metrics['total_cycles'] = int(line.split(':')[1].strip())
            except:
                pass

    return metrics

def main():
    """Main test execution"""

    print("=" * 80)
    print("PIMID ALL-DRAM LONG SIMULATION TEST SUITE - 5000 CONFIGURATIONS")
    print("Comprehensive coverage of ALL 12 DRAM types and ALL 6 PIM levels")
    print("Using pimid binary as sole entry point with comprehensive YAML configs")
    print("=" * 80)
    print()

    # Verify pimid binary exists
    if not PIMID_BINARY.exists():
        print(f"ERROR: PIMID binary not found at {PIMID_BINARY}")
        print("Please build pimid first: cd pimid && mkdir -p build && cd build && cmake .. && make pimid")
        return 1

    print(f"Using PIMID binary: {PIMID_BINARY}")
    print(f"Results directory: {RESULTS_DIR}")
    print(f"DRAM configs: {DRAM_CONFIG_DIR}")
    print()

    start_time = time.time()

    # Generate configurations
    configs = generate_all_dram_configs(5000)

    # Run tests
    print("\n" + "=" * 80)
    print("Running All-DRAM Long Simulation Tests")
    print("=" * 80)

    results = []
    passed = 0
    failed = 0
    timeout_count = 0
    errors = 0

    # Statistics by DRAM type
    dram_stats = {name: {'total': 0, 'passed': 0} for name in DRAM_TYPES.keys()}

    # Statistics by PIM granularity
    pim_granularity_stats = {name: {'total': 0, 'passed': 0} for name in PIM_GRANULARITIES.keys()}

    for i, (test_id, config) in enumerate(configs):
        workload_name = config['workload']
        arch = "LIBCom" if config['is_libcom'] else "Baseline"
        pim_level_str = config.get('pim_granularity', 'BANK')
        dram_type_str = config.get('dram_type', 'DDR4')

        print(f"\n[{i+1:4d}/5000] Test {test_id:04d}: {workload_name}")
        print(f"  DRAM: {dram_type_str}, PIM Level: {pim_level_str}, Arch: {arch}")
        print(f"  Params: subarrays={config['num_subarrays']}, size={config['size']}", end='')
        if 'extra_param' in config:
            print(f", iters={config['extra_param']}")
        else:
            print()

        result = run_test(test_id, config)
        results.append(result)

        # Parse metrics if successful
        if result['status'] == 'completed':
            metrics = parse_output(result['stdout'])
            result['metrics'] = metrics

        # Update counters
        if result['status'] == 'completed':
            passed += 1
            if 'dram_type' in config:
                dram_stats[config['dram_type']]['total'] += 1
                dram_stats[config['dram_type']]['passed'] += 1
            if 'pim_granularity' in config:
                pim_granularity_stats[config['pim_granularity']]['total'] += 1
                pim_granularity_stats[config['pim_granularity']]['passed'] += 1
            print(f"  ✓ PASSED", end='')
            if 'metrics' in result and result['metrics']['total_energy_pj'] is not None:
                m = result['metrics']
                print(f" | Energy: {m['total_energy_pj']:.0f}pJ, Cycles: {m['total_cycles']}")
            else:
                print()
        elif result['status'] == 'failed':
            failed += 1
            if 'dram_type' in config:
                dram_stats[config['dram_type']]['total'] += 1
            if 'pim_granularity' in config:
                pim_granularity_stats[config['pim_granularity']]['total'] += 1
            print(f"  ✗ FAILED (code {result['returncode']})")
            if result['stderr']:
                stderr_preview = result['stderr'][:200].replace('\n', ' ')
                print(f"  Error: {stderr_preview}")
        elif result['status'] == 'timeout':
            timeout_count += 1
            print(f"  ⏱ TIMEOUT")
        else:
            errors += 1
            print(f"  ⚠ ERROR: {result['stderr'][:100]}")

        # Progress update every 100 tests
        if (i + 1) % 100 == 0:
            elapsed = time.time() - start_time
            avg_time = elapsed / (i + 1)
            remaining = avg_time * (5000 - i - 1)
            pass_rate = (passed / (i + 1)) * 100

            print(f"\n  ══════════ Progress Report ══════════")
            print(f"  Tests: {i+1}/5000 ({(i+1)/50:.1f}%)")
            print(f"  Passed: {passed} ({pass_rate:.1f}%), Failed: {failed}, "
                  f"Timeout: {timeout_count}, Errors: {errors}")
            print(f"  Time: {elapsed/60:.1f}m elapsed, {remaining/60:.1f}m remaining")
            print(f"  ════════════════════════════════════\n")

    # Save detailed results
    results_file = RESULTS_DIR / 'all_dram_results_5000.json'
    print(f"\nSaving detailed results to {results_file}...")
    with open(results_file, 'w') as f:
        json.dump(results, f, indent=2)

    # Create summary
    summary = {
        'total_tests': len(results),
        'passed': passed,
        'failed': failed,
        'timeout': timeout_count,
        'errors': errors,
        'pass_rate': (passed / len(results)) * 100 if results else 0,
        'duration_seconds': time.time() - start_time,
        'timestamp': datetime.now().isoformat(),
    }

    # Calculate statistics per workload
    workload_stats = {}
    for result in results:
        wl = result['workload']
        if wl not in workload_stats:
            workload_stats[wl] = {'total': 0, 'passed': 0, 'failed': 0}

        workload_stats[wl]['total'] += 1
        if result['status'] == 'completed':
            workload_stats[wl]['passed'] += 1
        elif result['status'] == 'failed':
            workload_stats[wl]['failed'] += 1

    summary['workload_stats'] = workload_stats

    # Calculate statistics per architecture
    arch_stats = {'baseline': {'total': 0, 'passed': 0}, 'libcom': {'total': 0, 'passed': 0}}
    for result in results:
        arch = 'libcom' if result['config']['is_libcom'] else 'baseline'
        arch_stats[arch]['total'] += 1
        if result['status'] == 'completed':
            arch_stats[arch]['passed'] += 1

    summary['architecture_stats'] = arch_stats

    # DRAM type statistics
    summary['dram_type_stats'] = dram_stats

    # PIM granularity statistics
    summary['pim_granularity_stats'] = pim_granularity_stats

    # Calculate average simulation metrics
    total_energy_sum = 0
    total_cycles_sum = 0
    metric_count = 0
    for result in results:
        if result['status'] == 'completed' and 'metrics' in result:
            m = result['metrics']
            if m['total_energy_pj'] is not None:
                total_energy_sum += m['total_energy_pj']
            if m['total_cycles'] is not None:
                total_cycles_sum += m['total_cycles']
                metric_count += 1

    if metric_count > 0:
        summary['avg_energy_pj'] = total_energy_sum / metric_count
        summary['avg_cycles'] = total_cycles_sum / metric_count
    else:
        summary['avg_energy_pj'] = 0
        summary['avg_cycles'] = 0

    # Save summary
    summary_file = RESULTS_DIR / 'all_dram_summary_5000.json'
    with open(summary_file, 'w') as f:
        json.dump(summary, f, indent=2)

    # Print final summary
    print("\n" + "=" * 80)
    print("All-DRAM Long Simulation Test Summary")
    print("=" * 80)
    print(f"Total tests:    {len(results)}")
    print(f"Passed:         {passed} ({passed/len(results)*100:.1f}%)")
    print(f"Failed:         {failed} ({failed/len(results)*100:.1f}%)")
    print(f"Timeout:        {timeout_count} ({timeout_count/len(results)*100:.1f}%)")
    print(f"Errors:         {errors} ({errors/len(results)*100:.1f}%)")
    print(f"Duration:       {(time.time() - start_time)/60:.1f} minutes")
    if metric_count > 0:
        print(f"Avg Energy:     {summary['avg_energy_pj']:.0f} pJ")
        print(f"Avg Cycles:     {summary['avg_cycles']:.0f}")
    print()

    # Print per-DRAM-type stats
    print("Per-DRAM-Type Statistics:")
    print("-" * 80)
    for dram_type, stats in sorted(dram_stats.items()):
        if stats['total'] > 0:
            pass_rate = (stats['passed'] / stats['total'] * 100) if stats['total'] > 0 else 0
            print(f"  {dram_type:20s}: {stats['passed']:4d}/{stats['total']:4d} passed ({pass_rate:5.1f}%)")

    print()
    print("Per-Workload Statistics:")
    print("-" * 80)
    for wl, stats in sorted(workload_stats.items()):
        pass_rate = (stats['passed'] / stats['total'] * 100) if stats['total'] > 0 else 0
        print(f"  {wl:30s}: {stats['passed']:4d}/{stats['total']:4d} passed ({pass_rate:5.1f}%)")

    print()
    print("Per-Architecture Statistics:")
    print("-" * 80)
    for arch, stats in arch_stats.items():
        pass_rate = (stats['passed'] / stats['total'] * 100) if stats['total'] > 0 else 0
        print(f"  {arch:10s}: {stats['passed']:4d}/{stats['total']:4d} passed ({pass_rate:5.1f}%)")

    print()
    print("Per-PIM-Granularity Statistics:")
    print("-" * 80)
    for pim_level, stats in pim_granularity_stats.items():
        if stats['total'] > 0:
            pass_rate = (stats['passed'] / stats['total'] * 100) if stats['total'] > 0 else 0
            print(f"  {pim_level:20s}: {stats['passed']:4d}/{stats['total']:4d} passed ({pass_rate:5.1f}%)")

    print()
    print(f"Detailed results: {results_file}")
    print(f"Summary:         {summary_file}")
    print("=" * 80)

    return 0 if (failed + errors) == 0 else 1

if __name__ == '__main__':
    exit(main())
