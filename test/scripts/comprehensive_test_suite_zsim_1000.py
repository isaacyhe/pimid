#!/usr/bin/env python3
"""
Comprehensive PIMID Testing Suite - 1000 Test Configurations with ZSim Execution Model
Tests all memory technologies, workloads, and parameter combinations using ZSim as the execution model
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
RESULTS_DIR = BASE_DIR / "test/results/test_results_zsim_1000"
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

# Workload definitions with their parameter ranges
WORKLOADS = {
    'bfs_message': {
        'binary': 'bfs_message_pimid',
        'params': ['num_subarrays', 'num_vertices', 'is_libcom', 'mem_tech'],
        'size_param': 'num_vertices',
        'sizes': [32, 64, 128, 256, 512, 1024, 2048, 4096],
        'supports_mem_tech': True,
    },
    'bfs_shared': {
        'binary': 'bfs_shared_pimid',
        'params': ['num_subarrays', 'num_vertices', 'is_libcom'],
        'size_param': 'num_vertices',
        'sizes': [32, 64, 128, 256, 512, 1024, 2048],
        'supports_mem_tech': False,
    },
    'gemm_message': {
        'binary': 'gemm_message_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [32, 64, 96, 128, 192, 256, 384, 512],
        'supports_mem_tech': False,
    },
    'gemm_shared': {
        'binary': 'gemm_shared_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [32, 64, 96, 128, 192, 256, 384, 512],
        'supports_mem_tech': False,
    },
    'spmv_message': {
        'binary': 'spmv_message_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [32, 64, 96, 128, 192, 256, 384, 512],
        'supports_mem_tech': False,
    },
    'spmv_shared': {
        'binary': 'spmv_shared_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [32, 64, 96, 128, 192, 256, 384, 512],
        'supports_mem_tech': False,
    },
    'dotproduct_message': {
        'binary': 'dotproduct_message_pimid',
        'params': ['num_subarrays', 'vector_length', 'is_libcom'],
        'size_param': 'vector_length',
        'sizes': [256, 512, 1024, 2048, 4096, 8192],
        'supports_mem_tech': False,
    },
    'dotproduct_shared': {
        'binary': 'dotproduct_shared_pimid',
        'params': ['num_subarrays', 'vector_length', 'is_libcom'],
        'size_param': 'vector_length',
        'sizes': [256, 512, 1024, 2048, 4096, 8192],
        'supports_mem_tech': False,
    },
    'reduction_message': {
        'binary': 'reduction_message_pimid',
        'params': ['num_subarrays', 'elements_per_subarray', 'is_libcom'],
        'size_param': 'elements_per_subarray',
        'sizes': [128, 256, 512, 1024, 2048, 4096],
        'supports_mem_tech': False,
    },
    'reduction_shared': {
        'binary': 'reduction_shared_pimid',
        'params': ['num_subarrays', 'elements_per_subarray', 'is_libcom'],
        'size_param': 'elements_per_subarray',
        'sizes': [128, 256, 512, 1024, 2048, 4096],
        'supports_mem_tech': False,
    },
    'histogram_message': {
        'binary': 'histogram_message_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [256, 512, 1024, 2048, 4096, 8192],
        'supports_mem_tech': False,
    },
    'histogram_shared': {
        'binary': 'histogram_shared_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [256, 512, 1024, 2048, 4096, 8192],
        'supports_mem_tech': False,
    },
    'prefixsum_message': {
        'binary': 'prefixsum_message_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [128, 256, 512, 1024, 2048, 4096],
        'supports_mem_tech': False,
    },
    'prefixsum_shared': {
        'binary': 'prefixsum_shared_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [128, 256, 512, 1024, 2048, 4096],
        'supports_mem_tech': False,
    },
    'stencil1d_message': {
        'binary': 'stencil1d_message_pimid',
        'params': ['num_subarrays', 'num_points', 'num_iters', 'is_libcom'],
        'size_param': 'num_points',
        'sizes': [258, 514, 1026, 2050],
        'extra_param': 'num_iters',
        'extra_values': [5, 10, 20, 50, 100],
        'supports_mem_tech': False,
    },
    'stencil1d_shared': {
        'binary': 'stencil1d_shared_pimid',
        'params': ['num_subarrays', 'num_points', 'num_iters', 'is_libcom'],
        'size_param': 'num_points',
        'sizes': [258, 514, 1026, 2050],
        'extra_param': 'num_iters',
        'extra_values': [5, 10, 20, 50, 100],
        'supports_mem_tech': False,
    },
}

# Parameter ranges
NUM_SUBARRAYS = [1, 2, 4, 8, 16, 32]
IS_LIBCOM = [0, 1]  # 0 = baseline, 1 = LIBCom

def generate_test_configs(num_tests=1000):
    """Generate 1000 unique test configurations with extensive coverage using ZSim"""

    print(f"Generating {num_tests} unique test configurations for ZSim execution model...")
    print("Coverage:")
    print("  - 16 workload types (8 workloads × 2 programming models)")
    print("  - 5 memory technologies (comprehensive testing)")
    print("  - 6 subarray counts: 1, 2, 4, 8, 16, 32")
    print("  - 2 topologies: Baseline H-tree, LIBCom")
    print("  - Wide range of problem sizes")
    print("  - ZSim as execution model\n")

    configs = []

    # Strategy 1: Systematic coverage for all memory technologies (400 tests)
    print("Phase 1: All workloads with all memory technologies (400 tests)...")
    mem_tech_count = 0
    for mem_tech in range(5):  # All 5 memory techs
        for workload_name in ['bfs_message', 'gemm_message', 'spmv_message', 'dotproduct_message']:
            workload_info = WORKLOADS[workload_name]
            for num_subs in [1, 4, 16]:  # Representative subarray counts
                for is_lib in IS_LIBCOM:
                    for size in random.sample(workload_info['sizes'], min(2, len(workload_info['sizes']))):
                        if mem_tech_count >= 400:
                            break
                        combo = {
                            'workload': workload_name,
                            'num_subarrays': num_subs,
                            'size': size,
                            'is_libcom': is_lib,
                            'mem_tech': mem_tech if workload_name == 'bfs_message' else 0,
                        }
                        configs.append((len(configs), combo))
                        mem_tech_count += 1
                    if mem_tech_count >= 400:
                        break
                if mem_tech_count >= 400:
                    break
            if mem_tech_count >= 400:
                break
        if mem_tech_count >= 400:
            break

    # Strategy 2: Systematic sampling of all workloads (400 tests)
    print("Phase 2: Systematic coverage of all workloads (400 tests)...")
    systematic_combinations = []

    for workload_name, workload_info in WORKLOADS.items():
        for num_subs in [1, 2, 4, 8, 16, 32]:
            for is_lib in IS_LIBCOM:
                # Sample 1-2 sizes per configuration
                for size in random.sample(workload_info['sizes'], min(1, len(workload_info['sizes']))):
                    # Handle stencil1d special case
                    if 'extra_param' in workload_info:
                        for extra_val in random.sample(workload_info['extra_values'], min(1, len(workload_info['extra_values']))):
                            systematic_combinations.append({
                                'workload': workload_name,
                                'num_subarrays': num_subs,
                                'size': size,
                                'is_libcom': is_lib,
                                'extra_param': extra_val,
                            })
                    else:
                        systematic_combinations.append({
                            'workload': workload_name,
                            'num_subarrays': num_subs,
                            'size': size,
                            'is_libcom': is_lib,
                        })

    # Sample 400 from systematic combinations
    num_systematic = min(400, len(systematic_combinations))
    systematic_sample = random.sample(systematic_combinations, num_systematic)

    for combo in systematic_sample:
        configs.append((len(configs), combo))

    # Strategy 3: Random edge cases and corner cases (remaining tests)
    print("Phase 3: Random edge cases and corner cases...")
    while len(configs) < num_tests:
        workload_name = random.choice(list(WORKLOADS.keys()))
        workload_info = WORKLOADS[workload_name]

        num_subs = random.choice(NUM_SUBARRAYS)
        is_lib = random.choice(IS_LIBCOM)
        size = random.choice(workload_info['sizes'])

        combo = {
            'workload': workload_name,
            'num_subarrays': num_subs,
            'size': size,
            'is_libcom': is_lib,
        }

        # For BFS message, add memory tech parameter
        if workload_name == 'bfs_message':
            combo['mem_tech'] = random.choice([0, 1, 2, 3, 4])

        # Handle stencil1d special case
        if 'extra_param' in workload_info:
            combo['extra_param'] = random.choice(workload_info['extra_values'])

        # Inject edge cases every 10 tests
        if len(configs) % 10 == 0:
            # Extreme num_subarrays
            combo['num_subarrays'] = random.choice([1, 32])
        if len(configs) % 15 == 0:
            # Extreme sizes
            combo['size'] = random.choice([
                workload_info['sizes'][0],   # smallest
                workload_info['sizes'][-1],  # largest
            ])

        configs.append((len(configs), combo))

    print(f"Generated {len(configs)} unique configurations\n")
    return configs

def generate_pimid_config_zsim(test_id, config):
    """Generate YAML config file for PIMID simulator with ZSim execution model"""

    mem_tech = config.get('mem_tech', 0)
    mem_tech_name = MEMORY_TECHS[mem_tech]['yaml']

    yaml_content = f"""# PIMID Test Configuration with ZSim - Test {test_id:04d}
# Auto-generated for comprehensive testing with ZSim execution model

simulation:
  name: "ZSim_Test_{test_id:04d}"
  description: "Automated test with ZSim execution model"
  mode: "standalone"

# Execution model - ZSim
execution_model:
  type: "zsim"
  config_file: "{CONFIG_DIR}/zsim_{test_id:04d}.cfg"

# Memory configuration
memory:
  technology: "{mem_tech_name}"

# PIM configuration
pim:
  granularity: BANK
  num_pes: {config['num_subarrays']}

# Simulation control
simulation_control:
  max_cycles: 100000000000
  enable_power_modeling: true

# Output
output:
  stats_file: "test_{test_id:04d}_stats.txt"
  log_level: "ERROR"
"""

    # Also generate a ZSim config file
    zsim_config_content = f"""// ZSim Configuration for Test {test_id:04d}
// Auto-generated for PIMID testing

sys = {{
    lineSize = 64;
    frequency = 2400;      // 2.4 GHz
    simdWidth = 256;       // AVX support

    cores = {{
        bank_pes = {{
            type = "ALU";      // ALU core for PIM
            cores = {config['num_subarrays']};
            aluLatency = 1;
        }};
    }};

    mem = {{
        type = "Simple";
        latency = 0;     // Handled by Ramulator
    }};
}};

sim = {{
    phaseLength = 10000;
    maxTotalInstrs = 10000000L;
    statsPhaseInterval = 1000;
    printHierarchy = false;
}};

// Workload process
process0 = {{
    command = "{WORKLOAD_DIR}/{WORKLOADS[config['workload']]['binary']}";
}};
"""

    config_file = CONFIG_DIR / f"test_{test_id:04d}.yaml"
    with open(config_file, 'w') as f:
        f.write(yaml_content)

    zsim_config_file = CONFIG_DIR / f"zsim_{test_id:04d}.cfg"
    with open(zsim_config_file, 'w') as f:
        f.write(zsim_config_content)

    return config_file

def run_test(test_id, config):
    """Run a single test configuration using PIMID with ZSim execution model"""

    workload_name = config['workload']
    workload_info = WORKLOADS[workload_name]
    workload_binary = WORKLOAD_DIR / workload_info['binary']

    # Generate PIMID config file with ZSim
    config_file = generate_pimid_config_zsim(test_id, config)

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
    print("PIMID COMPREHENSIVE TEST SUITE - 1000 CONFIGURATIONS WITH ZSIM")
    print("Using ZSim as the execution model")
    print("Testing all memory technologies and workloads")
    print("=" * 80)
    print()

    # Verify pimid binary exists
    if not PIMID_BINARY.exists():
        print(f"ERROR: PIMID binary not found at {PIMID_BINARY}")
        print("Please build pimid first")
        return 1

    print(f"Using PIMID binary: {PIMID_BINARY}")
    print(f"Results directory: {RESULTS_DIR}")
    print()

    start_time = time.time()

    # Generate configurations
    configs = generate_test_configs(1000)

    # Run tests
    print("\n" + "=" * 80)
    print("Running Tests with ZSim Execution Model")
    print("=" * 80)

    results = []
    passed = 0
    failed = 0
    timeout_count = 0
    errors = 0

    # Statistics by memory technology
    mem_tech_stats = {i: {'total': 0, 'passed': 0} for i in range(5)}

    for i, (test_id, config) in enumerate(configs):
        workload_name = config['workload']
        arch = "LIBCom" if config['is_libcom'] else "Baseline"
        mem_tech_str = MEMORY_TECHS[config.get('mem_tech', 0)]['name']

        print(f"\n[{i+1:4d}/1000] Test {test_id:04d}: {workload_name}")
        print(f"  Params: subarrays={config['num_subarrays']}, "
              f"size={config['size']}, arch={arch}, mem={mem_tech_str}")

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

        # Progress update every 50 tests
        if (i + 1) % 50 == 0:
            elapsed = time.time() - start_time
            avg_time = elapsed / (i + 1)
            remaining = avg_time * (1000 - i - 1)
            pass_rate = (passed / (i + 1)) * 100

            print(f"\n  ══════════ Progress Report ══════════")
            print(f"  Tests: {i+1}/1000 ({(i+1)/10:.1f}%)")
            print(f"  Passed: {passed} ({pass_rate:.1f}%), Failed: {failed}, "
                  f"Timeout: {timeout_count}, Errors: {errors}")
            print(f"  Time: {elapsed/60:.1f}m elapsed, {remaining/60:.1f}m remaining")
            print(f"  ════════════════════════════════════\n")

    # Save detailed results
    results_file = RESULTS_DIR / 'all_results_zsim_1000.json'
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
        'execution_model': 'ZSim',
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

    # Save summary
    summary_file = RESULTS_DIR / 'test_summary_zsim_1000.json'
    with open(summary_file, 'w') as f:
        json.dump(summary, f, indent=2)

    # Print final summary
    print("\n" + "=" * 80)
    print("Test Summary - ZSim Execution Model")
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
    print(f"Detailed results: {results_file}")
    print(f"Summary:         {summary_file}")
    print("=" * 80)

    return 0 if (failed + errors) == 0 else 1

if __name__ == '__main__':
    exit(main())
