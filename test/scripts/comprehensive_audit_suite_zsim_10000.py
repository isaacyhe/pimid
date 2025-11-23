#!/usr/bin/env python3
"""
Comprehensive PIMID Audit & Verification Suite - 10,000 Test Configurations
Entry Point: pimid binary with ZSim execution model
Purpose: Extensive verification, testing, and auditing across all configurations
"""

import os
import subprocess
import json
import random
import time
from datetime import datetime
from pathlib import Path
import itertools
import hashlib

# Set random seed for reproducibility
random.seed(2025)

# Base directories
BASE_DIR = Path("/home/user/pimid-dev")
WORKLOAD_DIR = BASE_DIR / "DAC26/workloads_pimid"
RESULTS_DIR = BASE_DIR / "test/results/test_results_zsim_10000"
CONFIG_DIR = RESULTS_DIR / "configs"
AUDIT_DIR = RESULTS_DIR / "audit"
PIMID_BINARY = BASE_DIR / "build/pimid/pimid"

# Create directories
RESULTS_DIR.mkdir(exist_ok=True)
CONFIG_DIR.mkdir(exist_ok=True)
AUDIT_DIR.mkdir(exist_ok=True)

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
        'sizes': [32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384],
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
        'sizes': [128, 256, 512, 1024, 2048, 4096, 8192, 16384],
        'supports_mem_tech': False,
    },
    'reduction_shared': {
        'binary': 'reduction_shared_pimid',
        'params': ['num_subarrays', 'elements_per_subarray', 'is_libcom'],
        'size_param': 'elements_per_subarray',
        'sizes': [128, 256, 512, 1024, 2048, 4096, 8192, 16384],
        'supports_mem_tech': False,
    },
    'histogram_message': {
        'binary': 'histogram_message_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [256, 512, 1024, 2048, 4096, 8192, 16384, 32768],
        'supports_mem_tech': False,
    },
    'histogram_shared': {
        'binary': 'histogram_shared_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [256, 512, 1024, 2048, 4096, 8192, 16384, 32768],
        'supports_mem_tech': False,
    },
    'prefixsum_message': {
        'binary': 'prefixsum_message_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [128, 256, 512, 1024, 2048, 4096, 8192, 16384],
        'supports_mem_tech': False,
    },
    'prefixsum_shared': {
        'binary': 'prefixsum_shared_pimid',
        'params': ['num_subarrays', 'num_elements', 'is_libcom'],
        'size_param': 'num_elements',
        'sizes': [128, 256, 512, 1024, 2048, 4096, 8192, 16384],
        'supports_mem_tech': False,
    },
    'stencil1d_message': {
        'binary': 'stencil1d_message_pimid',
        'params': ['num_subarrays', 'num_points', 'num_iters', 'is_libcom'],
        'size_param': 'num_points',
        'sizes': [258, 514, 1026, 2050, 4098, 8194],
        'extra_param': 'num_iters',
        'extra_values': [5, 10, 20, 50, 100, 200, 500, 1000],
        'supports_mem_tech': False,
    },
    'stencil1d_shared': {
        'binary': 'stencil1d_shared_pimid',
        'params': ['num_subarrays', 'num_points', 'num_iters', 'is_libcom'],
        'size_param': 'num_points',
        'sizes': [258, 514, 1026, 2050, 4098, 8194],
        'extra_param': 'num_iters',
        'extra_values': [5, 10, 20, 50, 100, 200, 500, 1000],
        'supports_mem_tech': False,
    },
}

# Parameter ranges
NUM_SUBARRAYS = [1, 2, 4, 8, 16, 32, 64]
IS_LIBCOM = [0, 1]  # 0 = baseline, 1 = LIBCom

def generate_test_configs(num_tests=10000):
    """Generate 10,000 unique test configurations with maximum coverage"""

    print(f"Generating {num_tests} unique test configurations for comprehensive audit...")
    print("Coverage:")
    print("  - 16 workload types (8 workloads × 2 programming models)")
    print("  - 5 memory technologies")
    print("  - 7 subarray counts: 1, 2, 4, 8, 16, 32, 64")
    print("  - 2 topologies: Baseline H-tree, LIBCom")
    print("  - Extended problem sizes")
    print("  - ZSim as execution model\n")

    configs = []
    config_hashes = set()  # Track unique configs

    # Strategy 1: Comprehensive memory technology coverage (2000 tests)
    print("Phase 1: Comprehensive memory technology coverage (2000 tests)...")
    for mem_tech in range(5):  # All 5 memory techs
        target_per_tech = 400
        tech_count = 0

        for workload_name in list(WORKLOADS.keys()):
            if tech_count >= target_per_tech:
                break

            workload_info = WORKLOADS[workload_name]
            for num_subs in NUM_SUBARRAYS:
                for is_lib in IS_LIBCOM:
                    for size in workload_info['sizes']:
                        if tech_count >= target_per_tech:
                            break

                        combo = {
                            'workload': workload_name,
                            'num_subarrays': num_subs,
                            'size': size,
                            'is_libcom': is_lib,
                        }

                        # Add memory tech if workload supports it
                        if workload_name == 'bfs_message':
                            combo['mem_tech'] = mem_tech
                        else:
                            combo['mem_tech'] = mem_tech if random.random() < 0.2 else 0

                        # Add extra params for stencil
                        if 'extra_param' in workload_info:
                            combo['extra_param'] = random.choice(workload_info['extra_values'])

                        # Check uniqueness
                        config_hash = hashlib.md5(str(sorted(combo.items())).encode()).hexdigest()
                        if config_hash not in config_hashes:
                            config_hashes.add(config_hash)
                            configs.append((len(configs), combo))
                            tech_count += 1

    # Strategy 2: Systematic all-workload coverage (3000 tests)
    print("Phase 2: Systematic all-workload coverage (3000 tests)...")
    systematic_target = 3000
    systematic_count = 0

    while systematic_count < systematic_target:
        for workload_name, workload_info in WORKLOADS.items():
            if systematic_count >= systematic_target:
                break

            for num_subs in NUM_SUBARRAYS:
                for is_lib in IS_LIBCOM:
                    if systematic_count >= systematic_target:
                        break

                    size = random.choice(workload_info['sizes'])

                    combo = {
                        'workload': workload_name,
                        'num_subarrays': num_subs,
                        'size': size,
                        'is_libcom': is_lib,
                        'mem_tech': random.choice([0, 1, 2, 3, 4]),
                    }

                    if 'extra_param' in workload_info:
                        combo['extra_param'] = random.choice(workload_info['extra_values'])

                    config_hash = hashlib.md5(str(sorted(combo.items())).encode()).hexdigest()
                    if config_hash not in config_hashes:
                        config_hashes.add(config_hash)
                        configs.append((len(configs), combo))
                        systematic_count += 1

    # Strategy 3: Extreme edge cases and stress tests (2000 tests)
    print("Phase 3: Extreme edge cases and stress tests (2000 tests)...")
    edge_case_count = 0
    edge_case_target = 2000

    while edge_case_count < edge_case_target:
        workload_name = random.choice(list(WORKLOADS.keys()))
        workload_info = WORKLOADS[workload_name]

        # Bias towards extreme values
        if random.random() < 0.5:
            num_subs = random.choice([1, 64])  # Extreme PE counts
        else:
            num_subs = random.choice(NUM_SUBARRAYS)

        if random.random() < 0.5:
            size = random.choice([workload_info['sizes'][0], workload_info['sizes'][-1]])  # Min/Max
        else:
            size = random.choice(workload_info['sizes'])

        is_lib = random.choice(IS_LIBCOM)
        mem_tech = random.choice([0, 1, 2, 3, 4])

        combo = {
            'workload': workload_name,
            'num_subarrays': num_subs,
            'size': size,
            'is_libcom': is_lib,
            'mem_tech': mem_tech,
        }

        if 'extra_param' in workload_info:
            if random.random() < 0.5:
                combo['extra_param'] = random.choice([workload_info['extra_values'][0],
                                                      workload_info['extra_values'][-1]])
            else:
                combo['extra_param'] = random.choice(workload_info['extra_values'])

        config_hash = hashlib.md5(str(sorted(combo.items())).encode()).hexdigest()
        if config_hash not in config_hashes:
            config_hashes.add(config_hash)
            configs.append((len(configs), combo))
            edge_case_count += 1

    # Strategy 4: Random comprehensive coverage (fill to 10000)
    print("Phase 4: Random comprehensive coverage (remaining tests)...")
    while len(configs) < num_tests:
        workload_name = random.choice(list(WORKLOADS.keys()))
        workload_info = WORKLOADS[workload_name]

        num_subs = random.choice(NUM_SUBARRAYS)
        is_lib = random.choice(IS_LIBCOM)
        size = random.choice(workload_info['sizes'])
        mem_tech = random.choice([0, 1, 2, 3, 4])

        combo = {
            'workload': workload_name,
            'num_subarrays': num_subs,
            'size': size,
            'is_libcom': is_lib,
            'mem_tech': mem_tech,
        }

        if 'extra_param' in workload_info:
            combo['extra_param'] = random.choice(workload_info['extra_values'])

        config_hash = hashlib.md5(str(sorted(combo.items())).encode()).hexdigest()
        if config_hash not in config_hashes:
            config_hashes.add(config_hash)
            configs.append((len(configs), combo))

    print(f"Generated {len(configs)} unique configurations\n")
    return configs

def generate_pimid_config_zsim(test_id, config):
    """Generate YAML config file for PIMID with ZSim execution model"""

    mem_tech = config.get('mem_tech', 0)
    mem_tech_name = MEMORY_TECHS[mem_tech]['yaml']

    yaml_content = f"""# PIMID Audit Test Configuration with ZSim - Test {test_id:05d}
# Auto-generated for comprehensive audit (10,000 configs)

simulation:
  name: "ZSim_Audit_{test_id:05d}"
  description: "Comprehensive audit test with ZSim execution model"
  mode: "standalone"

# Execution model - ZSim
execution_model:
  type: "zsim"
  config_file: "{CONFIG_DIR}/zsim_{test_id:05d}.cfg"

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
  stats_file: "test_{test_id:05d}_stats.txt"
  log_level: "ERROR"
"""

    # Generate ZSim config file
    zsim_config_content = f"""// ZSim Configuration for Audit Test {test_id:05d}

sys = {{
    lineSize = 64;
    frequency = 2400;
    simdWidth = 256;

    cores = {{
        bank_pes = {{
            type = "ALU";
            cores = {config['num_subarrays']};
            aluLatency = 1;
        }};
    }};

    mem = {{
        type = "Simple";
        latency = 0;
    }};
}};

sim = {{
    phaseLength = 10000;
    maxTotalInstrs = 10000000L;
    statsPhaseInterval = 1000;
    printHierarchy = false;
}};

process0 = {{
    command = "{WORKLOAD_DIR}/{WORKLOADS[config['workload']]['binary']}";
}};
"""

    config_file = CONFIG_DIR / f"test_{test_id:05d}.yaml"
    with open(config_file, 'w') as f:
        f.write(yaml_content)

    zsim_config_file = CONFIG_DIR / f"zsim_{test_id:05d}.cfg"
    with open(zsim_config_file, 'w') as f:
        f.write(zsim_config_content)

    return config_file

def run_test(test_id, config):
    """Run a single test configuration using PIMID with ZSim"""

    workload_name = config['workload']
    workload_info = WORKLOADS[workload_name]
    workload_binary = WORKLOAD_DIR / workload_info['binary']

    # Verify binary exists
    if not workload_binary.exists():
        return {
            'test_id': test_id,
            'workload': workload_name,
            'config': config,
            'command': '',
            'status': 'error',
            'returncode': -1,
            'stdout': '',
            'stderr': f'Workload binary not found: {workload_binary}',
        }

    # Generate config
    config_file = generate_pimid_config_zsim(test_id, config)

    # Build command
    cmd = [
        str(PIMID_BINARY),
        '--mode', 'standalone',
        '--config', str(config_file),
        '--workload', str(workload_binary)
    ]

    # Add workload parameters
    cmd.append(str(config['num_subarrays']))
    cmd.append(str(config['size']))

    if 'extra_param' in config:
        cmd.append(str(config['extra_param']))

    cmd.append(str(config['is_libcom']))

    if 'mem_tech' in config:
        cmd.append(str(config['mem_tech']))

    # Run with timeout
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=300,
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
    """Parse metrics from PIMID output"""

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
    """Main audit execution"""

    print("=" * 80)
    print("PIMID COMPREHENSIVE AUDIT & VERIFICATION SUITE")
    print("10,000 Test Configurations with ZSim Execution Model")
    print("Entry Point: pimid binary")
    print("=" * 80)
    print()

    # Verify pimid binary
    if not PIMID_BINARY.exists():
        print(f"ERROR: PIMID binary not found at {PIMID_BINARY}")
        return 1

    print(f"Entry Point: {PIMID_BINARY}")
    print(f"Results Directory: {RESULTS_DIR}")
    print(f"Execution Model: ZSim")
    print()

    start_time = time.time()

    # Generate configurations
    configs = generate_test_configs(10000)

    # Run tests
    print("\n" + "=" * 80)
    print("Running 10,000 Comprehensive Audit Tests")
    print("=" * 80)

    results = []
    passed = 0
    failed = 0
    timeout_count = 0
    errors = 0

    # Statistics
    mem_tech_stats = {i: {'total': 0, 'passed': 0} for i in range(5)}
    workload_stats = {}
    arch_stats = {'baseline': {'total': 0, 'passed': 0}, 'libcom': {'total': 0, 'passed': 0}}

    for i, (test_id, config) in enumerate(configs):
        workload_name = config['workload']
        arch = "LIBCom" if config['is_libcom'] else "Baseline"
        mem_tech_str = MEMORY_TECHS[config.get('mem_tech', 0)]['name']

        if (i + 1) % 100 == 0 or i < 10:
            print(f"\n[{i+1:5d}/10000] Test {test_id:05d}: {workload_name}")
            print(f"  Params: PEs={config['num_subarrays']}, "
                  f"size={config['size']}, arch={arch}, mem={mem_tech_str}")

        result = run_test(test_id, config)
        results.append(result)

        # Parse metrics
        if result['status'] == 'completed':
            metrics = parse_output(result['stdout'])
            result['metrics'] = metrics

        # Update counters
        if result['status'] == 'completed':
            passed += 1
            mem_tech_stats[config['mem_tech']]['total'] += 1
            mem_tech_stats[config['mem_tech']]['passed'] += 1

            if (i + 1) % 100 == 0 or i < 10:
                print(f"  ✓ PASSED", end='')
                if 'metrics' in result and result['metrics']['total_energy_pj'] is not None:
                    m = result['metrics']
                    print(f" | Energy: {m['total_energy_pj']:.0f}pJ, Cycles: {m['total_cycles']}")
                else:
                    print()
        elif result['status'] == 'failed':
            failed += 1
            mem_tech_stats[config['mem_tech']]['total'] += 1
            if (i + 1) % 100 == 0 or i < 10:
                print(f"  ✗ FAILED (code {result['returncode']})")
        elif result['status'] == 'timeout':
            timeout_count += 1
            if (i + 1) % 100 == 0 or i < 10:
                print(f"  ⏱ TIMEOUT")
        else:
            errors += 1
            if (i + 1) % 100 == 0 or i < 10:
                print(f"  ⚠ ERROR")

        # Update workload stats
        if workload_name not in workload_stats:
            workload_stats[workload_name] = {'total': 0, 'passed': 0, 'failed': 0}
        workload_stats[workload_name]['total'] += 1
        if result['status'] == 'completed':
            workload_stats[workload_name]['passed'] += 1
        elif result['status'] == 'failed':
            workload_stats[workload_name]['failed'] += 1

        # Update arch stats
        arch = 'libcom' if config['is_libcom'] else 'baseline'
        arch_stats[arch]['total'] += 1
        if result['status'] == 'completed':
            arch_stats[arch]['passed'] += 1

        # Progress every 500 tests
        if (i + 1) % 500 == 0:
            elapsed = time.time() - start_time
            avg_time = elapsed / (i + 1)
            remaining = avg_time * (10000 - i - 1)
            pass_rate = (passed / (i + 1)) * 100

            print(f"\n  {'='*60}")
            print(f"  PROGRESS: {i+1}/10000 ({(i+1)/100:.1f}%)")
            print(f"  Passed: {passed} ({pass_rate:.1f}%), Failed: {failed}, "
                  f"Timeout: {timeout_count}, Errors: {errors}")
            print(f"  Time: {elapsed/60:.1f}m elapsed, {remaining/60:.1f}m remaining")
            print(f"  {'='*60}\n")

    # Save results
    results_file = RESULTS_DIR / 'all_results_zsim_10000.json'
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
        'entry_point': str(PIMID_BINARY),
        'workload_stats': workload_stats,
        'architecture_stats': arch_stats,
        'memory_tech_stats': {
            MEMORY_TECHS[i]['name']: stats for i, stats in mem_tech_stats.items()
        },
    }

    summary_file = RESULTS_DIR / 'audit_summary_zsim_10000.json'
    with open(summary_file, 'w') as f:
        json.dump(summary, f, indent=2)

    # Print final summary
    print("\n" + "=" * 80)
    print("COMPREHENSIVE AUDIT SUMMARY - 10,000 Tests")
    print("=" * 80)
    print(f"Entry Point:    {PIMID_BINARY}")
    print(f"Execution Model: ZSim")
    print(f"Total tests:    {len(results)}")
    print(f"Passed:         {passed} ({passed/len(results)*100:.2f}%)")
    print(f"Failed:         {failed} ({failed/len(results)*100:.2f}%)")
    print(f"Timeout:        {timeout_count} ({timeout_count/len(results)*100:.2f}%)")
    print(f"Errors:         {errors} ({errors/len(results)*100:.2f}%)")
    print(f"Duration:       {(time.time() - start_time)/60:.1f} minutes")
    print()

    print("Per-Workload Results:")
    print("-" * 80)
    for wl, stats in sorted(workload_stats.items()):
        pass_rate = (stats['passed'] / stats['total'] * 100) if stats['total'] > 0 else 0
        print(f"  {wl:30s}: {stats['passed']:5d}/{stats['total']:5d} ({pass_rate:6.2f}%)")

    print()
    print("Per-Architecture Results:")
    print("-" * 80)
    for arch, stats in arch_stats.items():
        pass_rate = (stats['passed'] / stats['total'] * 100) if stats['total'] > 0 else 0
        print(f"  {arch:10s}: {stats['passed']:5d}/{stats['total']:5d} ({pass_rate:6.2f}%)")

    print()
    print("Per-Memory-Technology Results:")
    print("-" * 80)
    for mem_tech, stats in summary['memory_tech_stats'].items():
        if stats['total'] > 0:
            pass_rate = (stats['passed'] / stats['total'] * 100) if stats['total'] > 0 else 0
            print(f"  {mem_tech:10s}: {stats['passed']:5d}/{stats['total']:5d} ({pass_rate:6.2f}%)")

    print()
    print(f"Detailed results: {results_file}")
    print(f"Summary:         {summary_file}")
    print("=" * 80)

    return 0 if (failed + errors + timeout_count) == 0 else 1

if __name__ == '__main__':
    exit(main())
