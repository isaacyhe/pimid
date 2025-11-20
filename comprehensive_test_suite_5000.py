#!/usr/bin/env python3
"""
Comprehensive PIMID Testing Suite - 5000 Test Configurations
Tests all memory technologies, workloads, and parameter combinations for comprehensive corner case coverage
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
RESULTS_DIR = BASE_DIR / "test_results_5000"

# Create directories
RESULTS_DIR.mkdir(exist_ok=True)

# Memory technologies (0=SRAM, 1=DRAM, 2=STT-MRAM, 3=PCM, 4=ReRAM)
MEMORY_TECHS = {
    0: {'name': 'SRAM', 'abbr': 'sram'},
    1: {'name': 'DRAM', 'abbr': 'dram'},
    2: {'name': 'STT-MRAM', 'abbr': 'sttmram'},
    3: {'name': 'PCM', 'abbr': 'pcm'},
    4: {'name': 'ReRAM', 'abbr': 'reram'},
}

# Workload definitions with their parameter ranges
WORKLOADS = {
    'bfs_message': {
        'binary': 'bfs_message_pimid',
        'params': ['num_subarrays', 'num_vertices', 'is_libcom', 'mem_tech'],
        'size_param': 'num_vertices',
        'sizes': [32, 64, 128, 256, 512, 1024, 2048, 4096, 8192],
        'supports_mem_tech': True,  # Modified to support all memory techs
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

# Parameter ranges
NUM_SUBARRAYS = [1, 2, 4, 8, 16, 32]
IS_LIBCOM = [0, 1]  # 0 = baseline, 1 = LIBCom

def generate_test_configs(num_tests=5000):
    """Generate 5000 unique test configurations with extensive corner case coverage"""

    print(f"Generating {num_tests} unique test configurations...")
    print("Coverage:")
    print("  - 16 workload types (8 workloads × 2 programming models)")
    print("  - 5 memory technologies (for BFS message passing)")
    print("  - 6 subarray counts: 1, 2, 4, 8, 16, 32")
    print("  - 2 topologies: Baseline H-tree, LIBCom")
    print("  - Wide range of problem sizes")
    print("  - Edge cases and corner cases\n")

    configs = []

    # Strategy 1: Systematic coverage for BFS with all memory technologies (1000 tests)
    print("Phase 1: BFS with all memory technologies (1000 tests)...")
    bfs_mem_tech_count = 0
    for mem_tech in range(5):  # All 5 memory techs
        for num_subs in NUM_SUBARRAYS:
            for is_lib in IS_LIBCOM:
                for size in WORKLOADS['bfs_message']['sizes']:
                    if bfs_mem_tech_count >= 1000:
                        break
                    configs.append((len(configs), {
                        'workload': 'bfs_message',
                        'num_subarrays': num_subs,
                        'size': size,
                        'is_libcom': is_lib,
                        'mem_tech': mem_tech,
                    }))
                    bfs_mem_tech_count += 1
                if bfs_mem_tech_count >= 1000:
                    break
            if bfs_mem_tech_count >= 1000:
                break
        if bfs_mem_tech_count >= 1000:
            break

    # Strategy 2: Systematic sampling of all other workloads (2500 tests)
    print("Phase 2: Systematic coverage of all workloads (2500 tests)...")
    systematic_combinations = []

    for workload_name, workload_info in WORKLOADS.items():
        if workload_name == 'bfs_message':
            continue  # Already covered above

        for num_subs in NUM_SUBARRAYS:
            for is_lib in IS_LIBCOM:
                for size in workload_info['sizes']:
                    # Handle stencil1d special case
                    if 'extra_param' in workload_info:
                        for extra_val in workload_info['extra_values']:
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

    # Sample 2500 from systematic combinations
    num_systematic = min(2500, len(systematic_combinations))
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
        if len(configs) % 20 == 0 and 'extra_param' in combo:
            # Extreme iterations for stencil
            combo['extra_param'] = random.choice([5, 500])

        configs.append((len(configs), combo))

    print(f"Generated {len(configs)} unique configurations\n")
    return configs

def run_test(test_id, config):
    """Run a single test configuration"""

    workload_name = config['workload']
    workload_info = WORKLOADS[workload_name]
    workload_binary = WORKLOAD_DIR / workload_info['binary']

    # Build command
    cmd = [str(workload_binary)]

    # Add parameters in order
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
            timeout=180,  # 3 minute timeout per test
            cwd=str(WORKLOAD_DIR)
        )

        return {
            'test_id': test_id,
            'workload': workload_name,
            'config': config,
            'command': ' '.join(cmd),
            'status': 'completed' if result.returncode == 0 else 'failed',
            'returncode': result.returncode,
            'stdout': result.stdout[-2000:] if len(result.stdout) > 2000 else result.stdout,  # Last 2KB
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
            'stderr': 'Test exceeded 3 minute timeout',
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
    """Parse energy and timing metrics from output"""

    metrics = {
        'total_energy_pj': None,
        'network_energy_pj': None,
        'execution_time_ns': None,
        'total_cycles': None,
    }

    for line in stdout.split('\n'):
        if 'Total energy:' in line:
            try:
                metrics['total_energy_pj'] = float(line.split(':')[1].split('pJ')[0].strip())
            except:
                pass
        elif 'Network energy:' in line:
            try:
                metrics['network_energy_pj'] = float(line.split(':')[1].split('pJ')[0].strip())
            except:
                pass
        elif 'Execution time:' in line and 'ns' in line:
            try:
                metrics['execution_time_ns'] = float(line.split(':')[1].split('ns')[0].strip())
            except:
                pass
        elif 'Total cycles:' in line:
            try:
                metrics['total_cycles'] = int(line.split(':')[1].strip())
            except:
                pass

    return metrics

def main():
    """Main test execution"""

    print("=" * 80)
    print("PIMID COMPREHENSIVE TEST SUITE - 5000 CONFIGURATIONS")
    print("Testing all memory technologies and corner cases")
    print("=" * 80)
    print()

    start_time = time.time()

    # Generate configurations
    configs = generate_test_configs(5000)

    # Run tests
    print("\n" + "=" * 80)
    print("Running Tests")
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

        print(f"\n[{i+1:4d}/5000] Test {test_id:04d}: {workload_name}")
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
            print(f"  ✗ FAILED (code {result['returncode']})")
            if result['stderr']:
                stderr_preview = result['stderr'][:100].replace('\n', ' ')
                print(f"  Error: {stderr_preview}")
        elif result['status'] == 'timeout':
            timeout_count += 1
            print(f"  ⏱ TIMEOUT")
        else:
            errors += 1
            print(f"  ⚠ ERROR: {result['stderr'][:100]}")

        # Progress update every 500 tests
        if (i + 1) % 500 == 0:
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
    results_file = RESULTS_DIR / 'all_results_5000.json'
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

    # Save summary
    summary_file = RESULTS_DIR / 'test_summary_5000.json'
    with open(summary_file, 'w') as f:
        json.dump(summary, f, indent=2)

    # Print final summary
    print("\n" + "=" * 80)
    print("Test Summary")
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
