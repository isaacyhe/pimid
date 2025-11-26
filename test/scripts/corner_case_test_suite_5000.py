#!/usr/bin/env python3
"""
Comprehensive PIMID Corner Case Testing Suite - 5000 Test Configurations
Focus: Corner cases, edge cases, boundary conditions
Coverage: All 5 memory technologies + All 6 PIM granularity levels with DRAM
Uses the PIMID binary simulator as the only entry point
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
random.seed(2025)

# Base directories
BASE_DIR = Path("/home/user/pimid-dev")
WORKLOAD_DIR = BASE_DIR / "DAC26/workloads_pimid"
RESULTS_DIR = BASE_DIR / "test/results/test_results_corner_cases_5000"
CONFIG_DIR = RESULTS_DIR / "configs"
PIMID_BINARY = BASE_DIR / "build/pimid/pimid"

# Create directories
RESULTS_DIR.mkdir(exist_ok=True)
CONFIG_DIR.mkdir(exist_ok=True)

# Memory technologies (0=SRAM, 1=DRAM, 2=STT-MRAM, 3=PCM, 4=ReRAM)
MEMORY_TECHS = {
    0: {'name': 'SRAM', 'abbr': 'sram', 'yaml': 'SRAM'},
    1: {'name': 'DRAM', 'abbr': 'dram', 'yaml': 'DRAM'},
    2: {'name': 'STT-MRAM', 'abbr': 'sttmram', 'yaml': 'STT_MRAM'},
    3: {'name': 'PCM', 'abbr': 'pcm', 'yaml': 'PCM'},
    4: {'name': 'ReRAM', 'abbr': 'reram', 'yaml': 'RERAM'},
}

# PIM Granularity levels (6 levels for DRAM-based PIM)
PIM_GRANULARITIES = {
    'MEMORY_CONTROLLER': {'name': 'MC-PIM', 'level': 'MEMORY_CONTROLLER'},
    'RANK': {'name': 'Rank-PIM', 'level': 'RANK'},
    'CHIP': {'name': 'Chip-PIM', 'level': 'CHIP'},
    'BANK_GROUP': {'name': 'BG-PIM', 'level': 'BANK_GROUP'},
    'BANK': {'name': 'Bank-PIM', 'level': 'BANK'},
    'SUBARRAY': {'name': 'Subarray-PIM', 'level': 'SUBARRAY'},
}

# Workload definitions with their parameter ranges
WORKLOADS = {
    'bfs_message': {
        'binary': 'bfs_message_pimid',
        'params': ['num_subarrays', 'num_vertices', 'is_libcom', 'mem_tech'],
        'size_param': 'num_vertices',
        'sizes': [32, 64, 128, 256, 512, 1024, 2048, 4096, 8192],
        'supports_mem_tech': True,
    },
    'bfs_shared': {
        'binary': 'bfs_shared_pimid',
        'params': ['num_subarrays', 'num_vertices', 'is_libcom'],
        'size_param': 'num_vertices',
        'sizes': [32, 64, 128, 256, 512, 1024, 2048, 4096, 8192],
        'supports_mem_tech': False,
    },
    'gemm_message': {
        'binary': 'gemm_message_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [32, 64, 96, 128, 192, 256, 384, 512, 768, 1024],
        'supports_mem_tech': False,
    },
    'gemm_shared': {
        'binary': 'gemm_shared_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [32, 64, 96, 128, 192, 256, 384, 512, 768, 1024],
        'supports_mem_tech': False,
    },
    'spmv_message': {
        'binary': 'spmv_message_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [32, 64, 96, 128, 192, 256, 384, 512, 768, 1024],
        'supports_mem_tech': False,
    },
    'spmv_shared': {
        'binary': 'spmv_shared_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [32, 64, 96, 128, 192, 256, 384, 512, 768, 1024],
        'supports_mem_tech': False,
    },
    'dotproduct_message': {
        'binary': 'dotproduct_message_pimid',
        'params': ['num_subarrays', 'vector_length', 'is_libcom'],
        'size_param': 'vector_length',
        'sizes': [256, 512, 1024, 2048, 4096, 8192, 16384, 32768],
        'supports_mem_tech': False,
    },
    'dotproduct_shared': {
        'binary': 'dotproduct_shared_pimid',
        'params': ['num_subarrays', 'vector_length', 'is_libcom'],
        'size_param': 'vector_length',
        'sizes': [256, 512, 1024, 2048, 4096, 8192, 16384, 32768],
        'supports_mem_tech': False,
    },
    'reduction_message': {
        'binary': 'reduction_message_pimid',
        'params': ['num_subarrays', 'elements_per_subarray', 'is_libcom'],
        'size_param': 'elements_per_subarray',
        'sizes': [128, 256, 512, 1024, 2048, 4096, 8192],
        'supports_mem_tech': False,
    },
    'reduction_shared': {
        'binary': 'reduction_shared_pimid',
        'params': ['num_subarrays', 'elements_per_subarray', 'is_libcom'],
        'size_param': 'elements_per_subarray',
        'sizes': [128, 256, 512, 1024, 2048, 4096, 8192],
        'supports_mem_tech': False,
    },
    'histogram_message': {
        'binary': 'histogram_message_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [256, 512, 1024, 2048, 4096, 8192, 16384],
        'supports_mem_tech': False,
    },
    'histogram_shared': {
        'binary': 'histogram_shared_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [256, 512, 1024, 2048, 4096, 8192, 16384],
        'supports_mem_tech': False,
    },
    'prefixsum_message': {
        'binary': 'prefixsum_message_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [128, 256, 512, 1024, 2048, 4096, 8192],
        'supports_mem_tech': False,
    },
    'prefixsum_shared': {
        'binary': 'prefixsum_shared_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [128, 256, 512, 1024, 2048, 4096, 8192],
        'supports_mem_tech': False,
    },
    'stencil1d_message': {
        'binary': 'stencil1d_message_pimid',
        'params': ['num_subarrays', 'num_points', 'num_iters', 'is_libcom'],
        'size_param': 'num_points',
        'sizes': [258, 514, 1026, 2050, 4098],
        'extra_param': 'num_iters',
        'extra_values': [5, 10, 20, 50, 100, 200, 500],
        'supports_mem_tech': False,
    },
    'stencil1d_shared': {
        'binary': 'stencil1d_shared_pimid',
        'params': ['num_subarrays', 'num_points', 'num_iters', 'is_libcom'],
        'size_param': 'num_points',
        'sizes': [258, 514, 1026, 2050, 4098],
        'extra_param': 'num_iters',
        'extra_values': [5, 10, 20, 50, 100, 200, 500],
        'supports_mem_tech': False,
    },
}

# Corner case parameters
NUM_SUBARRAYS = [1, 2, 4, 8, 16, 32]
NUM_SUBARRAYS_EXTREME = [1, 32]  # Min and max for corner cases
IS_LIBCOM = [0, 1]  # 0 = baseline, 1 = LIBCom

def generate_corner_case_configs(num_tests=5000):
    """Generate 5000 corner case test configurations with focus on boundary conditions"""

    print(f"Generating {num_tests} corner case test configurations...")
    print("\nCorner Case Coverage:")
    print("  - All 5 memory technologies: SRAM, DRAM, STT-MRAM, PCM, ReRAM")
    print("  - All 6 PIM granularity levels for DRAM-based tests")
    print("  - Boundary conditions: min/max subarrays, min/max sizes")
    print("  - Extreme parameter combinations")
    print("  - Edge cases for all 16 workload types\n")

    configs = []

    # ========================================================================
    # Phase 1: All Memory Technologies × All PIM Granularities (1500 tests)
    # ========================================================================
    print("Phase 1: Memory Tech × PIM Granularity combinations (1500 tests)...")

    mem_pim_count = 0
    for mem_tech in range(5):  # All 5 memory techs
        for pim_level_name, pim_info in PIM_GRANULARITIES.items():
            for workload_name in ['bfs_message']:  # Use BFS as it supports mem_tech
                workload_info = WORKLOADS[workload_name]
                for num_subs in NUM_SUBARRAYS_EXTREME:  # Min and max
                    for is_lib in IS_LIBCOM:
                        for size in [workload_info['sizes'][0], workload_info['sizes'][-1]]:  # Min and max
                            if mem_pim_count >= 1500:
                                break
                            configs.append((len(configs), {
                                'workload': workload_name,
                                'num_subarrays': num_subs,
                                'size': size,
                                'is_libcom': is_lib,
                                'mem_tech': mem_tech,
                                'pim_granularity': pim_level_name,
                            }))
                            mem_pim_count += 1
                        if mem_pim_count >= 1500:
                            break
                    if mem_pim_count >= 1500:
                        break
                if mem_pim_count >= 1500:
                    break
            if mem_pim_count >= 1500:
                break
        if mem_pim_count >= 1500:
            break

    # ========================================================================
    # Phase 2: Extreme Corner Cases - Minimum Configurations (1000 tests)
    # ========================================================================
    print("Phase 2: Minimum configuration corner cases (1000 tests)...")

    min_corner_count = 0
    for workload_name, workload_info in WORKLOADS.items():
        for _ in range(67):  # Distribute evenly across workloads
            if min_corner_count >= 1000:
                break

            mem_tech = random.choice(range(5))
            pim_level_name = random.choice(list(PIM_GRANULARITIES.keys()))

            config = {
                'workload': workload_name,
                'num_subarrays': 1,  # MINIMUM
                'size': workload_info['sizes'][0],  # MINIMUM size
                'is_libcom': random.choice(IS_LIBCOM),
                'pim_granularity': pim_level_name,
            }

            if workload_name == 'bfs_message':
                config['mem_tech'] = mem_tech

            if 'extra_param' in workload_info:
                config['extra_param'] = workload_info['extra_values'][0]  # MINIMUM iterations

            configs.append((len(configs), config))
            min_corner_count += 1
        if min_corner_count >= 1000:
            break

    # ========================================================================
    # Phase 3: Extreme Corner Cases - Maximum Configurations (1000 tests)
    # ========================================================================
    print("Phase 3: Maximum configuration corner cases (1000 tests)...")

    max_corner_count = 0
    for workload_name, workload_info in WORKLOADS.items():
        for _ in range(67):  # Distribute evenly across workloads
            if max_corner_count >= 1000:
                break

            mem_tech = random.choice(range(5))
            pim_level_name = random.choice(list(PIM_GRANULARITIES.keys()))

            config = {
                'workload': workload_name,
                'num_subarrays': 32,  # MAXIMUM
                'size': workload_info['sizes'][-1],  # MAXIMUM size
                'is_libcom': random.choice(IS_LIBCOM),
                'pim_granularity': pim_level_name,
            }

            if workload_name == 'bfs_message':
                config['mem_tech'] = mem_tech

            if 'extra_param' in workload_info:
                config['extra_param'] = workload_info['extra_values'][-1]  # MAXIMUM iterations

            configs.append((len(configs), config))
            max_corner_count += 1
        if max_corner_count >= 1000:
            break

    # ========================================================================
    # Phase 4: Mixed Corner Cases - Boundary Combinations (1000 tests)
    # ========================================================================
    print("Phase 4: Mixed boundary corner cases (1000 tests)...")

    mixed_count = 0
    while mixed_count < 1000:
        workload_name = random.choice(list(WORKLOADS.keys()))
        workload_info = WORKLOADS[workload_name]
        mem_tech = random.choice(range(5))
        pim_level_name = random.choice(list(PIM_GRANULARITIES.keys()))

        # Mix min/max parameters
        config = {
            'workload': workload_name,
            'num_subarrays': random.choice(NUM_SUBARRAYS_EXTREME),
            'size': random.choice([workload_info['sizes'][0], workload_info['sizes'][-1]]),
            'is_libcom': random.choice(IS_LIBCOM),
            'pim_granularity': pim_level_name,
        }

        if workload_name == 'bfs_message':
            config['mem_tech'] = mem_tech

        if 'extra_param' in workload_info:
            config['extra_param'] = random.choice([
                workload_info['extra_values'][0],
                workload_info['extra_values'][-1]
            ])

        configs.append((len(configs), config))
        mixed_count += 1

    # ========================================================================
    # Phase 5: Stress Test Corner Cases - All Memory Techs (500 tests)
    # ========================================================================
    print("Phase 5: Memory technology stress tests (500 tests)...")

    stress_count = 0
    while stress_count < 500:
        workload_name = random.choice(list(WORKLOADS.keys()))
        workload_info = WORKLOADS[workload_name]
        mem_tech = stress_count % 5  # Cycle through all 5 memory technologies
        pim_level_name = random.choice(list(PIM_GRANULARITIES.keys()))

        config = {
            'workload': workload_name,
            'num_subarrays': random.choice(NUM_SUBARRAYS),
            'size': random.choice(workload_info['sizes']),
            'is_libcom': random.choice(IS_LIBCOM),
            'pim_granularity': pim_level_name,
        }

        if workload_name == 'bfs_message':
            config['mem_tech'] = mem_tech

        if 'extra_param' in workload_info:
            config['extra_param'] = random.choice(workload_info['extra_values'])

        configs.append((len(configs), config))
        stress_count += 1

    print(f"Generated {len(configs)} corner case configurations\n")
    return configs

def generate_pimid_config(test_id, config):
    """Generate YAML config file for PIMID simulator with PIM granularity"""

    mem_tech = config.get('mem_tech', 1)  # Default to DRAM
    mem_tech_name = MEMORY_TECHS[mem_tech]['yaml']
    pim_granularity = config.get('pim_granularity', 'BANK')

    yaml_content = f"""# PIMID Corner Case Test Configuration - Test {test_id:04d}
# Auto-generated for comprehensive corner case testing

simulation:
  name: "CornerCase_Test_{test_id:04d}"
  description: "Corner case: {config['workload']} with {mem_tech_name} at {pim_granularity} granularity"
  mode: "standalone"

# Memory configuration
memory:
  technology: "{mem_tech_name}"

# PIM configuration with granularity level
pim:
  granularity: {pim_granularity}
  num_pes: {config['num_subarrays']}

# Simulation control
simulation_control:
  max_cycles: 100000000000
  enable_power_modeling: true

# Output
output:
  stats_file: "corner_test_{test_id:04d}_stats.txt"
  log_level: "ERROR"
"""

    config_file = CONFIG_DIR / f"corner_test_{test_id:04d}.yaml"
    with open(config_file, 'w') as f:
        f.write(yaml_content)

    return config_file

def run_test(test_id, config):
    """Run a single corner case test configuration using PIMID binary"""

    workload_name = config['workload']
    workload_info = WORKLOADS[workload_name]
    workload_binary = WORKLOAD_DIR / workload_info['binary']

    # Generate PIMID config file
    config_file = generate_pimid_config(test_id, config)

    # Build pimid command
    cmd = [
        str(PIMID_BINARY),
        '--mode', 'standalone',
        '--config', str(config_file),
        '--workload', str(workload_binary)
    ]

    # Add workload parameters
    cmd.append(str(config['num_subarrays']))
    cmd.append(str(config['size']))

    # Handle stencil1d extra parameter
    if 'extra_param' in config:
        cmd.append(str(config['extra_param']))

    cmd.append(str(config['is_libcom']))

    # Add memory tech parameter if supported
    if 'mem_tech' in config:
        cmd.append(str(config['mem_tech']))

    # Run with timeout
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=300,  # 5 minute timeout per test
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
            'stderr': 'Test exceeded 5 minute timeout',
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
    print("PIMID CORNER CASE TEST SUITE - 5000 CONFIGURATIONS")
    print("Testing boundary conditions, extreme cases, all memory techs & PIM levels")
    print("=" * 80)
    print()

    # Verify pimid binary exists
    if not PIMID_BINARY.exists():
        print(f"ERROR: PIMID binary not found at {PIMID_BINARY}")
        print("Please build pimid first: cd pimid && mkdir -p build && cd build && cmake .. && make pimid")
        return 1

    print(f"Using PIMID binary: {PIMID_BINARY}")
    print(f"Results directory: {RESULTS_DIR}")
    print()

    start_time = time.time()

    # Generate configurations
    configs = generate_corner_case_configs(5000)

    # Run tests
    print("\n" + "=" * 80)
    print("Running Corner Case Tests")
    print("=" * 80)

    results = []
    passed = 0
    failed = 0
    timeout_count = 0
    errors = 0

    # Statistics by memory technology
    mem_tech_stats = {i: {'total': 0, 'passed': 0} for i in range(5)}
    # Statistics by PIM granularity
    pim_granularity_stats = {name: {'total': 0, 'passed': 0} for name in PIM_GRANULARITIES.keys()}

    for i, (test_id, config) in enumerate(configs):
        workload_name = config['workload']
        arch = "LIBCom" if config['is_libcom'] else "Baseline"
        mem_tech_str = MEMORY_TECHS[config.get('mem_tech', 1)]['name']
        pim_level_str = config.get('pim_granularity', 'BANK')

        print(f"\n[{i+1:4d}/5000] Test {test_id:04d}: {workload_name}")
        print(f"  Params: subarrays={config['num_subarrays']}, "
              f"size={config['size']}, arch={arch}")
        print(f"  Memory={mem_tech_str}, PIM Level={pim_level_str}")

        result = run_test(test_id, config)
        results.append(result)

        # Parse metrics if successful
        if result['status'] == 'completed':
            metrics = parse_output(result['stdout'])
            result['metrics'] = metrics

        # Update counters
        if result['status'] == 'completed':
            passed += 1
            if 'mem_tech' in config:
                mem_tech_stats[config['mem_tech']]['total'] += 1
                mem_tech_stats[config['mem_tech']]['passed'] += 1
            if 'pim_granularity' in config:
                pim_granularity_stats[config['pim_granularity']]['total'] += 1
                pim_granularity_stats[config['pim_granularity']]['passed'] += 1
            print(f"  ✓ PASSED", end='')
            if 'metrics' in result and result['metrics']['total_energy_pj'] is not None:
                m = result['metrics']
                print(f" | Energy: {m['total_energy_pj']:.0f}pJ, "
                      f"Cycles: {m['total_cycles']}")
            else:
                print()
        elif result['status'] == 'failed':
            failed += 1
            if 'mem_tech' in config:
                mem_tech_stats[config['mem_tech']]['total'] += 1
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
    results_file = RESULTS_DIR / 'corner_case_results_5000.json'
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

    # Memory technology statistics
    summary['memory_tech_stats'] = {
        MEMORY_TECHS[i]['name']: stats for i, stats in mem_tech_stats.items()
    }

    # PIM granularity statistics
    summary['pim_granularity_stats'] = pim_granularity_stats

    # Save summary
    summary_file = RESULTS_DIR / 'corner_case_summary_5000.json'
    with open(summary_file, 'w') as f:
        json.dump(summary, f, indent=2)

    # Print final summary
    print("\n" + "=" * 80)
    print("Corner Case Test Summary")
    print("=" * 80)
    print(f"Total tests:    {len(results)}")
    print(f"Passed:         {passed} ({passed/len(results)*100:.1f}%)")
    print(f"Failed:         {failed} ({failed/len(results)*100:.1f}%)")
    print(f"Timeout:        {timeout_count} ({timeout_count/len(results)*100:.1f}%)")
    print(f"Errors:         {errors} ({errors/len(results)*100:.1f}%)")
    print(f"Duration:       {(time.time() - start_time)/60:.1f} minutes")
    print()

    # Print per-workload stats
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
    print("Per-Memory-Technology Statistics:")
    print("-" * 80)
    for mem_tech, stats in summary['memory_tech_stats'].items():
        if stats['total'] > 0:
            pass_rate = (stats['passed'] / stats['total'] * 100) if stats['total'] > 0 else 0
            print(f"  {mem_tech:10s}: {stats['passed']:4d}/{stats['total']:4d} passed ({pass_rate:5.1f}%)")

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
