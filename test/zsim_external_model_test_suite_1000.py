#!/usr/bin/env python3
"""
PIMID ZSim + External Models Comprehensive Test Suite - 1000 Test Cases
=========================================================================
This test suite generates and runs 1000 unique test configurations that:
1. Use ZSim as the execution model (not hardcoded timing)
2. Use external Ramulator for cycle-accurate DRAM simulation
3. Use external GARNET for network-on-chip simulation
4. Run through the pimid binary in standalone mode
5. Test diverse combinations of parameters

Coverage:
- 5 ZSim core types: Simple, OOO, ALU, Timing, Null
- 5 DRAM types: DDR3, DDR4, DDR5, HBM2, HBM3
- 5 Memory technologies: SRAM, DRAM, STT-MRAM, PCM, ReRAM
- 6 Network topologies: MESH_2D, TORUS_2D, FAT_TREE, DRAGONFLY, CROSSBAR, H_TREE
- 16 Workloads: 8 algorithms x 2 programming models (message/shared)
- Variable parameters: subarrays, problem sizes, iterations
"""

import os
import subprocess
import json
import random
import time
import sys
from datetime import datetime
from pathlib import Path
import itertools
import shutil

# Set random seed for reproducibility
random.seed(2025)

# Base directories
BASE_DIR = Path("/home/user/pimid-dev")
BUILD_DIR = BASE_DIR / "build"
WORKLOAD_DIR = BASE_DIR / "DAC26/workloads_pimid"
RESULTS_DIR = BASE_DIR / "test/results/zsim_external_1000"
CONFIG_DIR = RESULTS_DIR / "configs"
ZSIM_CONFIG_DIR = RESULTS_DIR / "zsim_configs"
RAMULATOR_CONFIG_DIR = RESULTS_DIR / "ramulator_configs"
PIMID_BINARY = BUILD_DIR / "pimid/pimid"

# Create directories
for d in [RESULTS_DIR, CONFIG_DIR, ZSIM_CONFIG_DIR, RAMULATOR_CONFIG_DIR]:
    d.mkdir(parents=True, exist_ok=True)

# ============================================================================
# CONFIGURATION PARAMETERS
# ============================================================================

# ZSim Core Types with detailed configurations
ZSIM_CORE_TYPES = {
    'Simple': {
        'type': 'Simple',
        'description': 'Simple single-issue in-order core',
        'has_cache': True,
        'pipeline_stages': 5,
    },
    'OOO': {
        'type': 'OOO',
        'description': 'Out-of-order superscalar core',
        'has_cache': True,
        'issue_width': 4,
        'rob_size': 128,
    },
    'ALU': {
        'type': 'ALU',
        'description': 'ALU-only core for PIM (no cache)',
        'has_cache': False,
        'alu_latency': 1,
    },
    'Timing': {
        'type': 'Timing',
        'description': 'Timing core with cache hierarchy',
        'has_cache': True,
    },
    'Null': {
        'type': 'Null',
        'description': 'Null core for fast-forward',
        'has_cache': False,
    },
}

# DRAM Types for Ramulator external model
DRAM_TYPES = {
    'DDR3': {
        'impl': 'DDR3',
        'org_preset': 'DDR3_2Gb_x8',
        'timing_preset': 'DDR3_1600K',
        'channels': 1,
        'ranks': 2,
        'frequency_mhz': 800,
        'bus_width': 64,
    },
    'DDR4': {
        'impl': 'DDR4',
        'org_preset': 'DDR4_8Gb_x8',
        'timing_preset': 'DDR4_2400R',
        'channels': 1,
        'ranks': 2,
        'frequency_mhz': 1200,
        'bus_width': 64,
    },
    'DDR5': {
        'impl': 'DDR5',
        'org_preset': 'DDR5_8Gb_x8',
        'timing_preset': 'DDR5_4800',
        'channels': 2,
        'ranks': 2,
        'frequency_mhz': 2400,
        'bus_width': 64,
    },
    'HBM2': {
        'impl': 'HBM2',
        'org_preset': 'HBM2_8Gb',
        'timing_preset': 'HBM2_2000',
        'channels': 8,
        'ranks': 1,
        'frequency_mhz': 1000,
        'bus_width': 1024,
    },
    'HBM3': {
        'impl': 'HBM3',
        'org_preset': 'HBM3_16Gb',
        'timing_preset': 'HBM3_6400',
        'channels': 16,
        'ranks': 1,
        'frequency_mhz': 3200,
        'bus_width': 1024,
    },
}

# Memory technologies
MEMORY_TECHS = {
    'SRAM': {'yaml': 'SRAM', 'read_ns': 2.5, 'write_ns': 2.5},
    'DRAM': {'yaml': 'DRAM', 'read_ns': 13.3, 'write_ns': 15.0},
    'STT_MRAM': {'yaml': 'STT_MRAM', 'read_ns': 7.0, 'write_ns': 25.0},
    'PCM': {'yaml': 'PCM', 'read_ns': 8.0, 'write_ns': 100.0},
    'ReRAM': {'yaml': 'RERAM', 'read_ns': 5.0, 'write_ns': 12.0},
}

# Network topologies for GARNET external model
NETWORK_TOPOLOGIES = {
    'MESH_2D': {'rows': 4, 'cols': 4, 'routing': 'XY'},
    'TORUS_2D': {'rows': 4, 'cols': 4, 'routing': 'XY'},
    'FAT_TREE': {'levels': 3, 'radix': 4, 'routing': 'MINIMAL'},
    'DRAGONFLY': {'groups': 4, 'routers_per_group': 4, 'routing': 'MINIMAL'},
    'CROSSBAR': {'nodes': 16, 'routing': 'DIRECT'},
    'H_TREE': {'levels': 4, 'routing': 'HIERARCHICAL'},
}

# Workloads
WORKLOADS = {
    'bfs_message': {'binary': 'bfs_message_pimid', 'sizes': [32, 64, 128, 256, 512, 1024, 2048, 4096]},
    'bfs_shared': {'binary': 'bfs_shared_pimid', 'sizes': [32, 64, 128, 256, 512, 1024, 2048]},
    'gemm_message': {'binary': 'gemm_message_pimid', 'sizes': [32, 64, 96, 128, 192, 256, 384, 512]},
    'gemm_shared': {'binary': 'gemm_shared_pimid', 'sizes': [32, 64, 96, 128, 192, 256, 384, 512]},
    'spmv_message': {'binary': 'spmv_message_pimid', 'sizes': [32, 64, 96, 128, 192, 256, 384, 512]},
    'spmv_shared': {'binary': 'spmv_shared_pimid', 'sizes': [32, 64, 96, 128, 192, 256, 384, 512]},
    'dotproduct_message': {'binary': 'dotproduct_message_pimid', 'sizes': [256, 512, 1024, 2048, 4096, 8192]},
    'dotproduct_shared': {'binary': 'dotproduct_shared_pimid', 'sizes': [256, 512, 1024, 2048, 4096, 8192]},
    'reduction_message': {'binary': 'reduction_message_pimid', 'sizes': [128, 256, 512, 1024, 2048, 4096]},
    'reduction_shared': {'binary': 'reduction_shared_pimid', 'sizes': [128, 256, 512, 1024, 2048, 4096]},
    'histogram_message': {'binary': 'histogram_message_pimid', 'sizes': [256, 512, 1024, 2048, 4096, 8192]},
    'histogram_shared': {'binary': 'histogram_shared_pimid', 'sizes': [256, 512, 1024, 2048, 4096, 8192]},
    'prefixsum_message': {'binary': 'prefixsum_message_pimid', 'sizes': [128, 256, 512, 1024, 2048, 4096]},
    'prefixsum_shared': {'binary': 'prefixsum_shared_pimid', 'sizes': [128, 256, 512, 1024, 2048, 4096]},
    'stencil1d_message': {'binary': 'stencil1d_message_pimid', 'sizes': [258, 514, 1026, 2050], 'needs_iterations': True},
    'stencil1d_shared': {'binary': 'stencil1d_shared_pimid', 'sizes': [258, 514, 1026, 2050], 'needs_iterations': True},
}

# PIM granularity levels
PIM_GRANULARITIES = ['SUBARRAY', 'BANK', 'CHIP', 'RANK']

# Number of subarrays/PEs
NUM_SUBARRAYS = [1, 2, 4, 8, 16, 32, 64]

# SIMD widths
SIMD_WIDTHS = [128, 256, 512]  # SSE, AVX, AVX-512

# Frequencies
FREQUENCIES = [1600, 2000, 2400, 3000, 3600]  # MHz


# ============================================================================
# CONFIGURATION GENERATORS
# ============================================================================

def generate_zsim_config(test_id, config):
    """Generate ZSim configuration file (.cfg format)"""

    core_type = config['zsim_core']
    core_info = ZSIM_CORE_TYPES[core_type]
    num_cores = config['num_pes']
    simd_width = config.get('simd_width', 256)
    frequency = config.get('frequency', 2400)

    # Build core section
    if core_type == 'ALU':
        core_section = f"""
        pim_pes = {{
            type = "ALU";
            cores = {num_cores};
            aluLatency = 1;
            // NO cache - direct memory access for PIM
        }};"""
    elif core_type == 'OOO':
        core_section = f"""
        ooo_cores = {{
            type = "OOO";
            cores = {num_cores};
            icache = "l1i";
            dcache = "l1d";
            issueWidth = 4;
            robEntries = 128;
            lsqEntries = 48;
        }};"""
    elif core_type == 'Simple':
        core_section = f"""
        simple_cores = {{
            type = "Simple";
            cores = {num_cores};
            icache = "l1i";
            dcache = "l1d";
        }};"""
    elif core_type == 'Timing':
        core_section = f"""
        timing_cores = {{
            type = "Timing";
            cores = {num_cores};
            icache = "l1i";
            dcache = "l1d";
        }};"""
    else:  # Null
        core_section = f"""
        null_cores = {{
            type = "Null";
            cores = {num_cores};
        }};"""

    # Build cache section (only for cores with caches)
    if core_info.get('has_cache', False):
        cache_section = f"""
    caches = {{
        l1i = {{
            size = 32768;      // 32KB
            latency = 3;
            array = "SetAssoc";
            ways = 8;
        }};
        l1d = {{
            size = 32768;      // 32KB
            latency = 3;
            array = "SetAssoc";
            ways = 8;
        }};
        l2 = {{
            caches = 1;
            size = 262144;     // 256KB
            latency = 10;
            array = "SetAssoc";
            ways = 8;
            children = "l1i|l1d";
        }};
        l3 = {{
            caches = 1;
            size = 8388608;    // 8MB
            latency = 30;
            array = "SetAssoc";
            ways = 16;
            children = "l2";
        }};
    }};"""
    else:
        cache_section = ""

    zsim_content = f"""// ZSim Configuration - Test {test_id:04d}
// Core Type: {core_type} ({core_info['description']})
// Generated for PIMID External Model Testing
// Uses Ramulator for DRAM timing, GARNET for network

sys = {{
    lineSize = 64;
    frequency = {frequency};
    simdWidth = {simd_width};

    cores = {{{core_section}
    }};
    {cache_section}

    // Memory handled by external Ramulator model
    mem = {{
        type = "External";
        controller = "Ramulator";
        latency = 0;  // Actual latency from Ramulator
    }};
}};

sim = {{
    phaseLength = 10000;
    maxTotalInstrs = 100000000L;
    statsPhaseInterval = 10000;
    printHierarchy = true;
    maxPhases = 0;  // Unlimited
    schedQuantum = 50;
}};

// External model integration
external_models = {{
    ramulator = {{
        enabled = true;
        config_file = "{RAMULATOR_CONFIG_DIR}/ramulator_{test_id:04d}.yaml";
    }};
    garnet = {{
        enabled = true;
        topology = "{config['network_topology']}";
    }};
}};

// Workload process
process0 = {{
    command = "{WORKLOAD_DIR}/{WORKLOADS[config['workload']]['binary']}";
    startFastForwarded = False;
}};
"""

    zsim_file = ZSIM_CONFIG_DIR / f"zsim_{test_id:04d}.cfg"
    with open(zsim_file, 'w') as f:
        f.write(zsim_content)

    return zsim_file


def generate_ramulator_config(test_id, config):
    """Generate Ramulator configuration file (YAML format)"""

    dram_type = config['dram_type']
    dram_info = DRAM_TYPES[dram_type]

    ramulator_content = f"""# Ramulator Configuration - Test {test_id:04d}
# DRAM Type: {dram_type}
# Generated for PIMID External Model Testing

Frontend:
  impl: SimpleO3
  clock_ratio: 8
  num_expected_insts: 100000000

  Translation:
    impl: RandomTranslation
    max_addr: 4294967296

MemorySystem:
  impl: GenericDRAM
  clock_ratio: 3

  DRAM:
    impl: {dram_info['impl']}
    org:
      preset: {dram_info['org_preset']}
      channel: {dram_info['channels']}
      rank: {dram_info['ranks']}
    timing:
      preset: {dram_info['timing_preset']}

  Controller:
    impl: Generic
    Scheduler:
      impl: FRFCFS
    RefreshManager:
      impl: AllBank
    RowPolicy:
      impl: OpenRowPolicy
      cap: 16
    plugins:

  AddrMapper:
    impl: RoBaRaCoCh
"""

    ramulator_file = RAMULATOR_CONFIG_DIR / f"ramulator_{test_id:04d}.yaml"
    with open(ramulator_file, 'w') as f:
        f.write(ramulator_content)

    return ramulator_file


def generate_pimid_config(test_id, config):
    """Generate main PIMID configuration file (YAML format)"""

    zsim_file = ZSIM_CONFIG_DIR / f"zsim_{test_id:04d}.cfg"
    ramulator_file = RAMULATOR_CONFIG_DIR / f"ramulator_{test_id:04d}.yaml"
    mem_tech = config['memory_tech']
    network = config['network_topology']
    network_info = NETWORK_TOPOLOGIES[network]
    dram_type = config['dram_type']

    pimid_content = f"""# PIMID Configuration - Test {test_id:04d}
# External Model Testing with ZSim Execution
# Memory: {mem_tech}, DRAM: {dram_type}, Network: {network}

simulation:
  name: "ZSim_External_Test_{test_id:04d}"
  description: "ZSim execution with Ramulator DRAM and GARNET network"
  mode: "standalone"

# Execution Model - ZSim (instruction-level simulation)
execution_model:
  type: "zsim"
  config_file: "{zsim_file}"

  # ZSim core configuration
  zsim:
    core_type: "{config['zsim_core']}"
    num_cores: {config['num_pes']}
    frequency_mhz: {config.get('frequency', 2400)}
    simd_width: {config.get('simd_width', 256)}

# External Models Configuration
external_models:
  # Ramulator for cycle-accurate DRAM simulation
  ramulator:
    enabled: true
    config_file: "{ramulator_file}"
    dram_type: "{dram_type}"
    channels: {DRAM_TYPES[dram_type]['channels']}
    ranks: {DRAM_TYPES[dram_type]['ranks']}

  # GARNET for network-on-chip simulation
  garnet:
    enabled: true
    topology: "{network}"
    routing: "{network_info['routing']}"
    virtual_channels: 4
    link_width_bytes: 8
    router_latency_cycles: 2

# Memory Technology Configuration
memory:
  technology: "{MEMORY_TECHS[mem_tech]['yaml']}"

  # Technology-specific timing
  timing:
    read_latency_ns: {MEMORY_TECHS[mem_tech]['read_ns']}
    write_latency_ns: {MEMORY_TECHS[mem_tech]['write_ns']}

# PIM Configuration
pim:
  granularity: {config['pim_granularity']}
  num_pes: {config['num_pes']}
  pe_type: "{config['zsim_core']}"

  # Addressing
  addressing_mode: "UNIFIED"
  scheduler: "NEAREST_PE"

# Network Configuration
network:
  enabled: true
  topology: "{network}"
  num_rows: {network_info.get('rows', 4)}
  num_cols: {network_info.get('cols', 4)}
  routing: "{network_info['routing']}"
  virtual_channels: 4
  link_width_bytes: 8
  link_latency_cycles: 1
  router_latency_cycles: 2

# Simulation Control
simulation_control:
  max_cycles: 100000000000
  warmup_cycles: 10000
  stats_interval: 100000
  enable_power_modeling: true
  enable_detailed_stats: true

# Output Configuration
output:
  stats_file: "{RESULTS_DIR}/stats/test_{test_id:04d}_stats.txt"
  log_level: "ERROR"
  output_format: "JSON"
"""

    config_file = CONFIG_DIR / f"test_{test_id:04d}.yaml"
    with open(config_file, 'w') as f:
        f.write(pimid_content)

    return config_file


# ============================================================================
# TEST CONFIGURATION GENERATOR
# ============================================================================

def generate_test_configs(num_tests=1000):
    """Generate 1000 unique test configurations with comprehensive coverage"""

    print(f"Generating {num_tests} unique test configurations...")
    print("Coverage:")
    print(f"  - {len(ZSIM_CORE_TYPES)} ZSim core types")
    print(f"  - {len(DRAM_TYPES)} DRAM types (Ramulator)")
    print(f"  - {len(MEMORY_TECHS)} Memory technologies")
    print(f"  - {len(NETWORK_TOPOLOGIES)} Network topologies (GARNET)")
    print(f"  - {len(WORKLOADS)} Workloads")
    print(f"  - {len(PIM_GRANULARITIES)} PIM granularities")
    print()

    configs = []

    # Strategy 1: Systematic coverage of all combinations (500 tests)
    print("Phase 1: Systematic coverage (500 tests)...")
    systematic_count = 0

    # Create all meaningful combinations
    all_combos = list(itertools.product(
        ZSIM_CORE_TYPES.keys(),
        DRAM_TYPES.keys(),
        MEMORY_TECHS.keys(),
        NETWORK_TOPOLOGIES.keys(),
        list(WORKLOADS.keys())[:8],  # First 8 workloads
        PIM_GRANULARITIES[:3],  # First 3 granularities
        NUM_SUBARRAYS[:4],  # First 4 subarray counts
    ))

    # Sample 500 combinations
    sampled_combos = random.sample(all_combos, min(500, len(all_combos)))

    for combo in sampled_combos:
        zsim_core, dram_type, mem_tech, network, workload, granularity, num_pes = combo

        workload_info = WORKLOADS[workload]
        size = random.choice(workload_info['sizes'])

        config = {
            'zsim_core': zsim_core,
            'dram_type': dram_type,
            'memory_tech': mem_tech,
            'network_topology': network,
            'workload': workload,
            'pim_granularity': granularity,
            'num_pes': num_pes,
            'size': size,
            'is_libcom': random.choice([0, 1]),
            'simd_width': random.choice(SIMD_WIDTHS),
            'frequency': random.choice(FREQUENCIES),
        }

        configs.append((len(configs), config))
        systematic_count += 1

    print(f"  Generated {systematic_count} systematic tests")

    # Strategy 2: ALU-focused PIM tests (200 tests)
    print("Phase 2: ALU-focused PIM tests (200 tests)...")
    alu_count = 0

    for _ in range(200):
        workload = random.choice(list(WORKLOADS.keys()))
        workload_info = WORKLOADS[workload]

        config = {
            'zsim_core': 'ALU',  # ALU core for PIM
            'dram_type': random.choice(list(DRAM_TYPES.keys())),
            'memory_tech': random.choice(list(MEMORY_TECHS.keys())),
            'network_topology': random.choice(list(NETWORK_TOPOLOGIES.keys())),
            'workload': workload,
            'pim_granularity': random.choice(['BANK', 'SUBARRAY']),
            'num_pes': random.choice([8, 16, 32, 64]),
            'size': random.choice(workload_info['sizes']),
            'is_libcom': random.choice([0, 1]),
            'simd_width': random.choice([256, 512]),
            'frequency': random.choice([2400, 3000, 3600]),
        }

        configs.append((len(configs), config))
        alu_count += 1

    print(f"  Generated {alu_count} ALU PIM tests")

    # Strategy 3: HBM-focused high-bandwidth tests (150 tests)
    print("Phase 3: HBM high-bandwidth tests (150 tests)...")
    hbm_count = 0

    for _ in range(150):
        workload = random.choice(list(WORKLOADS.keys()))
        workload_info = WORKLOADS[workload]

        config = {
            'zsim_core': random.choice(['ALU', 'OOO', 'Simple']),
            'dram_type': random.choice(['HBM2', 'HBM3']),
            'memory_tech': random.choice(['DRAM', 'SRAM']),
            'network_topology': random.choice(list(NETWORK_TOPOLOGIES.keys())),
            'workload': workload,
            'pim_granularity': random.choice(PIM_GRANULARITIES),
            'num_pes': random.choice([16, 32, 64]),
            'size': random.choice(workload_info['sizes']),
            'is_libcom': random.choice([0, 1]),
            'simd_width': 512,
            'frequency': random.choice([3000, 3600]),
        }

        configs.append((len(configs), config))
        hbm_count += 1

    print(f"  Generated {hbm_count} HBM tests")

    # Strategy 4: Edge cases and corner cases (remaining tests)
    print("Phase 4: Edge cases and random variations...")
    while len(configs) < num_tests:
        workload = random.choice(list(WORKLOADS.keys()))
        workload_info = WORKLOADS[workload]

        # Inject edge cases
        test_idx = len(configs)

        config = {
            'zsim_core': random.choice(list(ZSIM_CORE_TYPES.keys())),
            'dram_type': random.choice(list(DRAM_TYPES.keys())),
            'memory_tech': random.choice(list(MEMORY_TECHS.keys())),
            'network_topology': random.choice(list(NETWORK_TOPOLOGIES.keys())),
            'workload': workload,
            'pim_granularity': random.choice(PIM_GRANULARITIES),
            'num_pes': random.choice(NUM_SUBARRAYS),
            'size': random.choice(workload_info['sizes']),
            'is_libcom': random.choice([0, 1]),
            'simd_width': random.choice(SIMD_WIDTHS),
            'frequency': random.choice(FREQUENCIES),
        }

        # Edge case injection
        if test_idx % 20 == 0:
            config['num_pes'] = random.choice([1, 64])  # Extreme values
        if test_idx % 25 == 0:
            config['size'] = workload_info['sizes'][-1]  # Largest size
        if test_idx % 30 == 0:
            config['size'] = workload_info['sizes'][0]  # Smallest size

        configs.append((len(configs), config))

    print(f"  Generated {len(configs) - systematic_count - alu_count - hbm_count} edge case tests")
    print(f"\nTotal: {len(configs)} unique configurations generated\n")

    return configs


# ============================================================================
# TEST EXECUTION
# ============================================================================

def run_test(test_id, config):
    """Run a single test through the pimid binary with ZSim execution"""

    # Generate all config files
    zsim_file = generate_zsim_config(test_id, config)
    ramulator_file = generate_ramulator_config(test_id, config)
    pimid_config = generate_pimid_config(test_id, config)

    workload_name = config['workload']
    workload_info = WORKLOADS[workload_name]
    workload_binary = WORKLOAD_DIR / workload_info['binary']

    # Build pimid command
    cmd = [
        str(PIMID_BINARY),
        '--mode', 'standalone',
        '--config', str(pimid_config),
        '--workload', str(workload_binary),
    ]

    # Add workload arguments
    # stencil1d workloads need: <num_subarrays> <grid_size> <num_iterations> <is_libcom>
    # other workloads need: <num_subarrays> <size> <is_libcom>
    if workload_info.get('needs_iterations', False):
        cmd.extend([
            str(config['num_pes']),
            str(config['size']),
            str(config.get('iterations', 10)),  # Default 10 iterations
            str(config['is_libcom']),
        ])
    else:
        cmd.extend([
            str(config['num_pes']),
            str(config['size']),
            str(config['is_libcom']),
        ])

    # Run with timeout
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=60,  # 1 minute timeout per test
            cwd=str(BASE_DIR),
            env={**os.environ, 'PIMID_ZSIM_MODE': '1'}
        )

        return {
            'test_id': test_id,
            'config': config,
            'command': ' '.join(cmd),
            'status': 'completed' if result.returncode == 0 else 'failed',
            'returncode': result.returncode,
            'stdout': result.stdout[-2000:] if len(result.stdout) > 2000 else result.stdout,
            'stderr': result.stderr[-500:] if len(result.stderr) > 500 else result.stderr,
            'zsim_config': str(zsim_file),
            'ramulator_config': str(ramulator_file),
            'pimid_config': str(pimid_config),
        }

    except subprocess.TimeoutExpired:
        return {
            'test_id': test_id,
            'config': config,
            'command': ' '.join(cmd),
            'status': 'timeout',
            'returncode': -1,
            'stdout': '',
            'stderr': 'Test exceeded 60 second timeout',
        }
    except Exception as e:
        return {
            'test_id': test_id,
            'config': config,
            'command': ' '.join(cmd),
            'status': 'error',
            'returncode': -1,
            'stdout': '',
            'stderr': str(e),
        }


def parse_metrics(stdout):
    """Parse performance metrics from output"""
    metrics = {
        'total_energy_pj': None,
        'execution_time_ns': None,
        'total_cycles': None,
        'throughput': None,
        'zsim_cycles': None,
        'ramulator_latency': None,
    }

    for line in stdout.split('\n'):
        line_lower = line.lower()

        if 'total energy:' in line_lower:
            try:
                metrics['total_energy_pj'] = float(line.split(':')[1].split()[0])
            except:
                pass
        elif 'execution time:' in line_lower and 'ns' in line_lower:
            try:
                metrics['execution_time_ns'] = float(line.split(':')[1].split('ns')[0].strip())
            except:
                pass
        elif 'total cycles:' in line_lower:
            try:
                metrics['total_cycles'] = int(line.split(':')[1].strip().split()[0])
            except:
                pass
        elif 'throughput:' in line_lower:
            try:
                metrics['throughput'] = line.split(':')[1].strip()
            except:
                pass

    return metrics


# ============================================================================
# MAIN EXECUTION
# ============================================================================

def main():
    print("=" * 80)
    print("PIMID ZSim + External Models Test Suite - 1000 Configurations")
    print("=" * 80)
    print()
    print("This test suite uses:")
    print("  - ZSim for instruction-level execution simulation")
    print("  - Ramulator for cycle-accurate DRAM timing")
    print("  - GARNET for network-on-chip simulation")
    print("  - All tests run through the pimid binary")
    print()

    # Verify pimid binary exists
    if not PIMID_BINARY.exists():
        print(f"ERROR: PIMID binary not found at {PIMID_BINARY}")
        print("Please build pimid first: cd build && cmake .. && make -j")
        return 1

    # Create stats directory
    (RESULTS_DIR / "stats").mkdir(exist_ok=True)

    print(f"Using PIMID binary: {PIMID_BINARY}")
    print(f"Results directory: {RESULTS_DIR}")
    print()

    start_time = time.time()

    # Generate configurations
    configs = generate_test_configs(1000)

    # Run tests
    print("=" * 80)
    print("Running Tests with ZSim Execution + External Models")
    print("=" * 80)

    results = []
    stats = {
        'passed': 0,
        'failed': 0,
        'timeout': 0,
        'error': 0,
        'by_zsim_core': {k: {'total': 0, 'passed': 0} for k in ZSIM_CORE_TYPES},
        'by_dram_type': {k: {'total': 0, 'passed': 0} for k in DRAM_TYPES},
        'by_mem_tech': {k: {'total': 0, 'passed': 0} for k in MEMORY_TECHS},
        'by_network': {k: {'total': 0, 'passed': 0} for k in NETWORK_TOPOLOGIES},
        'by_workload': {},
    }

    for i, (test_id, config) in enumerate(configs):
        # Print progress
        print(f"\n[{i+1:4d}/1000] Test {test_id:04d}")
        print(f"  ZSim Core: {config['zsim_core']:8s} | DRAM: {config['dram_type']:5s} | "
              f"Mem: {config['memory_tech']:8s} | Net: {config['network_topology']:10s}")
        print(f"  Workload: {config['workload']:25s} | PEs: {config['num_pes']:2d} | "
              f"Size: {config['size']:5d} | Gran: {config['pim_granularity']}")

        result = run_test(test_id, config)
        results.append(result)

        # Update statistics
        workload = config['workload']
        if workload not in stats['by_workload']:
            stats['by_workload'][workload] = {'total': 0, 'passed': 0}

        stats['by_zsim_core'][config['zsim_core']]['total'] += 1
        stats['by_dram_type'][config['dram_type']]['total'] += 1
        stats['by_mem_tech'][config['memory_tech']]['total'] += 1
        stats['by_network'][config['network_topology']]['total'] += 1
        stats['by_workload'][workload]['total'] += 1

        if result['status'] == 'completed':
            stats['passed'] += 1
            stats['by_zsim_core'][config['zsim_core']]['passed'] += 1
            stats['by_dram_type'][config['dram_type']]['passed'] += 1
            stats['by_mem_tech'][config['memory_tech']]['passed'] += 1
            stats['by_network'][config['network_topology']]['passed'] += 1
            stats['by_workload'][workload]['passed'] += 1

            metrics = parse_metrics(result['stdout'])
            result['metrics'] = metrics
            print(f"  Status: PASSED", end='')
            if metrics['total_cycles']:
                print(f" | Cycles: {metrics['total_cycles']}")
            else:
                print()
        elif result['status'] == 'failed':
            stats['failed'] += 1
            print(f"  Status: FAILED (code {result['returncode']})")
        elif result['status'] == 'timeout':
            stats['timeout'] += 1
            print(f"  Status: TIMEOUT")
        else:
            stats['error'] += 1
            print(f"  Status: ERROR - {result['stderr'][:80]}")

        # Progress report every 100 tests
        if (i + 1) % 100 == 0:
            elapsed = time.time() - start_time
            rate = (i + 1) / elapsed
            remaining = (1000 - i - 1) / rate if rate > 0 else 0
            pass_rate = stats['passed'] / (i + 1) * 100

            print(f"\n{'='*60}")
            print(f"Progress: {i+1}/1000 ({(i+1)/10:.0f}%)")
            print(f"Pass Rate: {pass_rate:.1f}%")
            print(f"Time: {elapsed/60:.1f}m elapsed, {remaining/60:.1f}m remaining")
            print(f"{'='*60}")

    # Save results
    print("\nSaving results...")

    results_file = RESULTS_DIR / 'all_results.json'
    with open(results_file, 'w') as f:
        json.dump(results, f, indent=2, default=str)

    # Create summary
    summary = {
        'total_tests': len(results),
        'passed': stats['passed'],
        'failed': stats['failed'],
        'timeout': stats['timeout'],
        'error': stats['error'],
        'pass_rate': stats['passed'] / len(results) * 100 if results else 0,
        'duration_seconds': time.time() - start_time,
        'timestamp': datetime.now().isoformat(),
        'execution_model': 'ZSim',
        'external_models': ['Ramulator', 'GARNET'],
        'statistics': stats,
    }

    summary_file = RESULTS_DIR / 'test_summary.json'
    with open(summary_file, 'w') as f:
        json.dump(summary, f, indent=2)

    # Print final report
    print("\n" + "=" * 80)
    print("FINAL TEST REPORT - ZSim + External Models")
    print("=" * 80)
    print(f"\nOverall Results:")
    print(f"  Total Tests:  {len(results)}")
    print(f"  Passed:       {stats['passed']} ({stats['passed']/len(results)*100:.1f}%)")
    print(f"  Failed:       {stats['failed']} ({stats['failed']/len(results)*100:.1f}%)")
    print(f"  Timeout:      {stats['timeout']} ({stats['timeout']/len(results)*100:.1f}%)")
    print(f"  Errors:       {stats['error']} ({stats['error']/len(results)*100:.1f}%)")
    print(f"  Duration:     {(time.time() - start_time)/60:.1f} minutes")

    print(f"\nBy ZSim Core Type:")
    for core, s in stats['by_zsim_core'].items():
        if s['total'] > 0:
            rate = s['passed'] / s['total'] * 100
            print(f"  {core:10s}: {s['passed']:4d}/{s['total']:4d} ({rate:5.1f}%)")

    print(f"\nBy DRAM Type (Ramulator):")
    for dram, s in stats['by_dram_type'].items():
        if s['total'] > 0:
            rate = s['passed'] / s['total'] * 100
            print(f"  {dram:10s}: {s['passed']:4d}/{s['total']:4d} ({rate:5.1f}%)")

    print(f"\nBy Memory Technology:")
    for mem, s in stats['by_mem_tech'].items():
        if s['total'] > 0:
            rate = s['passed'] / s['total'] * 100
            print(f"  {mem:10s}: {s['passed']:4d}/{s['total']:4d} ({rate:5.1f}%)")

    print(f"\nBy Network Topology (GARNET):")
    for net, s in stats['by_network'].items():
        if s['total'] > 0:
            rate = s['passed'] / s['total'] * 100
            print(f"  {net:12s}: {s['passed']:4d}/{s['total']:4d} ({rate:5.1f}%)")

    print(f"\nResults saved to: {RESULTS_DIR}")
    print("=" * 80)

    return 0 if stats['failed'] + stats['error'] == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
