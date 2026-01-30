#!/usr/bin/env python3
"""
Comprehensive PIMID Testing Suite - 1000 Test Configurations
Tests workloads directly with various parameter combinations
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
random.seed(42)

# Base directories
BASE_DIR = Path("/home/user/pimid-dev")
WORKLOAD_DIR = BASE_DIR / "DAC26/workloads_pimid"
RESULTS_DIR = BASE_DIR / "test/results/test_results_1000_v2"

# Create directories
RESULTS_DIR.mkdir(exist_ok=True)

# Workload definitions with their parameter ranges
WORKLOADS = {
    'bfs_message': {
        'binary': 'bfs_message_pimid',
        'params': ['num_subarrays', 'num_vertices', 'is_libcom'],
        'size_param': 'num_vertices',
        'sizes': [64, 128, 256, 512, 1024, 2048, 4096],
    },
    'bfs_shared': {
        'binary': 'bfs_shared_pimid',
        'params': ['num_subarrays', 'num_vertices', 'is_libcom'],
        'size_param': 'num_vertices',
        'sizes': [64, 128, 256, 512, 1024, 2048, 4096],
    },
    'gemm_message': {
        'binary': 'gemm_message_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [64, 128, 256, 512, 1024],
    },
    'gemm_shared': {
        'binary': 'gemm_shared_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [64, 128, 256, 512, 1024],
    },
    'spmv_message': {
        'binary': 'spmv_message_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [64, 128, 256, 512, 1024],
    },
    'spmv_shared': {
        'binary': 'spmv_shared_pimid',
        'params': ['num_subarrays', 'matrix_size', 'is_libcom'],
        'size_param': 'matrix_size',
        'sizes': [64, 128, 256, 512, 1024],
    },
    'dotproduct_message': {
        'binary': 'dotproduct_message_pimid',
        'params': ['num_subarrays', 'vector_length', 'is_libcom'],
        'size_param': 'vector_length',
        'sizes': [512, 1024, 2048, 4096, 8192, 16384],
    },
    'dotproduct_shared': {
        'binary': 'dotproduct_shared_pimid',
        'params': ['num_subarrays', 'vector_length', 'is_libcom'],
        'size_param': 'vector_length',
        'sizes': [512, 1024, 2048, 4096, 8192, 16384],
    },
    'reduction_message': {
        'binary': 'reduction_message_pimid',
        'params': ['num_subarrays', 'elements_per_subarray', 'is_libcom'],
        'size_param': 'elements_per_subarray',
        'sizes': [256, 512, 1024, 2048, 4096],
    },
    'reduction_shared': {
        'binary': 'reduction_shared_pimid',
        'params': ['num_subarrays', 'elements_per_subarray', 'is_libcom'],
        'size_param': 'elements_per_subarray',
        'sizes': [256, 512, 1024, 2048, 4096],
    },
    'histogram_message': {
        'binary': 'histogram_message_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [512, 1024, 2048, 4096, 8192],
    },
    'histogram_shared': {
        'binary': 'histogram_shared_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [512, 1024, 2048, 4096, 8192],
    },
    'prefixsum_message': {
        'binary': 'prefixsum_message_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [256, 512, 1024, 2048, 4096],
    },
    'prefixsum_shared': {
        'binary': 'prefixsum_shared_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [256, 512, 1024, 2048, 4096],
    },
    'stencil1d_message': {
        'binary': 'stencil1d_message_pimid',
        'params': ['num_subarrays', 'num_points', 'num_iters', 'is_libcom'],
        'size_param': 'num_points',
        'sizes': [258, 514, 1026, 2050],
        'extra_param': 'num_iters',
        'extra_values': [10, 50, 100, 200],
    },
    'stencil1d_shared': {
        'binary': 'stencil1d_shared_pimid',
        'params': ['num_subarrays', 'num_points', 'num_iters', 'is_libcom'],
        'size_param': 'num_points',
        'sizes': [258, 514, 1026, 2050],
        'extra_param': 'num_iters',
        'extra_values': [10, 50, 100, 200],
    },
}

# Parameter ranges
NUM_SUBARRAYS = [1, 2, 4, 8, 16, 32]
IS_LIBCOM = [0, 1]  # 0 = baseline, 1 = LIBCom

def generate_test_configs(num_tests=1000):
    """Generate 1000 unique test configurations"""

    print(f"Generating {num_tests} unique test configurations...")

    configs = []

    # Strategy: Mix systematic and random sampling
    # First half: systematic coverage
    # Second half: random combinations with edge cases

    # Generate all possible combinations for systematic sampling
    systematic_combinations = []

    for workload_name, workload_info in WORKLOADS.items():
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

    # Sample from systematic combinations
    num_systematic = min(500, len(systematic_combinations))
    systematic_sample = random.sample(systematic_combinations, num_systematic)

    for i, combo in enumerate(systematic_sample):
        configs.append((i, combo))

    # Random sampling for remaining tests
    for i in range(len(configs), num_tests):
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

        # Handle stencil1d special case
        if 'extra_param' in workload_info:
            combo['extra_param'] = random.choice(workload_info['extra_values'])

        # Add edge cases
        if i % 50 == 0:
            # Try extreme num_subarrays
            combo['num_subarrays'] = random.choice([1, 32])
        if i % 30 == 0:
            # Try extreme sizes
            combo['size'] = random.choice([
                workload_info['sizes'][0],  # smallest
                workload_info['sizes'][-1],  # largest
            ])

        configs.append((i, combo))

    print(f"Generated {len(configs)} unique configurations")
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

    # Run with timeout
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,  # 2 minute timeout per test
            cwd=str(WORKLOAD_DIR)
        )

        return {
            'test_id': test_id,
            'workload': workload_name,
            'config': config,
            'command': ' '.join(cmd),
            'status': 'completed' if result.returncode == 0 else 'failed',
            'returncode': result.returncode,
            'stdout': result.stdout,
            'stderr': result.stderr,
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
            'stderr': 'Test exceeded 2 minute timeout',
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
        'total_energy_nj': None,
        'network_energy_nj': None,
        'execution_time_ns': None,
        'bandwidth_gbps': None,
    }

    for line in stdout.split('\n'):
        if 'Total energy:' in line:
            try:
                metrics['total_energy_nj'] = float(line.split(':')[1].split('nJ')[0].strip())
            except:
                pass
        elif 'Network energy:' in line:
            try:
                metrics['network_energy_nj'] = float(line.split(':')[1].split('nJ')[0].strip())
            except:
                pass
        elif 'Execution time:' in line:
            try:
                metrics['execution_time_ns'] = float(line.split(':')[1].split('ns')[0].strip())
            except:
                pass
        elif 'Bandwidth:' in line:
            try:
                metrics['bandwidth_gbps'] = float(line.split(':')[1].split('GB/s')[0].strip())
            except:
                pass

    return metrics

def main():
    """Main test execution"""

    print("=" * 80)
    print("PIMID Comprehensive Test Suite - 1000 Configurations")
    print("=" * 80)
    print()

    start_time = time.time()

    # Generate configurations
    configs = generate_test_configs(1000)

    # Run tests
    print("\n" + "=" * 80)
    print("Running Tests")
    print("=" * 80)

    results = []
    passed = 0
    failed = 0
    timeout_count = 0
    errors = 0

    for i, (test_id, config) in enumerate(configs):
        workload_name = config['workload']
        arch = "LIBCom" if config['is_libcom'] else "Baseline"

        print(f"\n[{i+1:4d}/1000] Test {test_id:04d}: {workload_name}")
        print(f"  Params: subarrays={config['num_subarrays']}, "
              f"size={config['size']}, arch={arch}")

        result = run_test(test_id, config)
        results.append(result)

        # Parse metrics if successful
        if result['status'] == 'completed':
            metrics = parse_output(result['stdout'])
            result['metrics'] = metrics

        # Update counters
        if result['status'] == 'completed':
            passed += 1
            print(f"  ✓ PASSED", end='')
            if 'metrics' in result and result['metrics']['total_energy_nj'] is not None:
                m = result['metrics']
                print(f" | Energy: {m['total_energy_nj']:.2f}nJ, "
                      f"Time: {m['execution_time_ns']:.0f}ns")
            else:
                print()
        elif result['status'] == 'failed':
            failed += 1
            print(f"  ✗ FAILED (code {result['returncode']})")
            if result['stderr']:
                stderr_preview = result['stderr'][:150].replace('\n', ' ')
                print(f"  Error: {stderr_preview}")
        elif result['status'] == 'timeout':
            timeout_count += 1
            print(f"  ⏱ TIMEOUT")
        else:
            errors += 1
            print(f"  ⚠ ERROR: {result['stderr'][:150]}")

        # Progress update every 100 tests
        if (i + 1) % 100 == 0:
            elapsed = time.time() - start_time
            avg_time = elapsed / (i + 1)
            remaining = avg_time * (1000 - i - 1)
            pass_rate = (passed / (i + 1)) * 100

            print(f"\n  ═══ Progress Report ═══")
            print(f"  Tests: {i+1}/1000 ({(i+1)/10:.1f}%)")
            print(f"  Passed: {passed} ({pass_rate:.1f}%), Failed: {failed}, "
                  f"Timeout: {timeout_count}, Errors: {errors}")
            print(f"  Time: {elapsed/60:.1f}m elapsed, {remaining/60:.1f}m remaining")
            print(f"  ═════════════════════════")

    # Save detailed results
    results_file = RESULTS_DIR / 'all_results.json'
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

    # Save summary
    summary_file = RESULTS_DIR / 'test_summary.json'
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
        print(f"  {wl:25s}: {stats['passed']:3d}/{stats['total']:3d} passed ({pass_rate:5.1f}%)")

    print()
    print("Per-Architecture Statistics:")
    print("-" * 80)
    for arch, stats in arch_stats.items():
        pass_rate = (stats['passed'] / stats['total'] * 100) if stats['total'] > 0 else 0
        print(f"  {arch:10s}: {stats['passed']:3d}/{stats['total']:3d} passed ({pass_rate:5.1f}%)")

    print()
    print(f"Detailed results: {results_file}")
    print(f"Summary:         {summary_file}")
    print("=" * 80)

    return 0 if (failed + errors) == 0 else 1

if __name__ == '__main__':
    exit(main())
