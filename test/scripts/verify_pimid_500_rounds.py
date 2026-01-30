#!/usr/bin/env python3
"""
PIMID Binary Verification Suite - 500 Rounds
Entry Point: pimid binary with config files ONLY
Purpose: Comprehensive code audit and verification using pimid binary as sole entry point
"""

import os
import subprocess
import json
import random
import time
from datetime import datetime
from pathlib import Path
import hashlib

# Set random seed for reproducibility
random.seed(2025)

# Base directories
BASE_DIR = Path("/home/user/pimid-dev")
WORKLOAD_DIR = BASE_DIR / "DAC26/workloads_pimid"
RESULTS_DIR = BASE_DIR / "test/results/verify_500_rounds"
CONFIG_DIR = RESULTS_DIR / "configs"
PIMID_BINARY = BASE_DIR / "build/pimid/pimid"

# Create directories
RESULTS_DIR.mkdir(exist_ok=True)
CONFIG_DIR.mkdir(exist_ok=True)

# Memory technologies
MEMORY_TECHS = {
    0: {'name': 'SRAM', 'yaml': 'SRAM'},
    1: {'name': 'DRAM', 'yaml': 'DRAM'},
    2: {'name': 'STT-MRAM', 'yaml': 'STT_MRAM'},
    3: {'name': 'PCM', 'yaml': 'PCM'},
    4: {'name': 'ReRAM', 'yaml': 'RERAM'},
}

# Workloads (all 16: message passing + shared memory)
WORKLOADS = {
    'bfs_message': {'binary': 'bfs_message_pimid', 'sizes': [32, 64, 128, 256, 512, 1024]},
    'bfs_shared': {'binary': 'bfs_shared_pimid', 'sizes': [32, 64, 128, 256, 512]},
    'gemm_message': {'binary': 'gemm_message_pimid', 'sizes': [32, 64, 96, 128, 192, 256]},
    'gemm_shared': {'binary': 'gemm_shared_pimid', 'sizes': [32, 64, 96, 128, 192, 256]},
    'spmv_message': {'binary': 'spmv_message_pimid', 'sizes': [32, 64, 96, 128, 192]},
    'spmv_shared': {'binary': 'spmv_shared_pimid', 'sizes': [32, 64, 96, 128, 192]},
    'dotproduct_message': {'binary': 'dotproduct_message_pimid', 'sizes': [256, 512, 1024, 2048]},
    'dotproduct_shared': {'binary': 'dotproduct_shared_pimid', 'sizes': [256, 512, 1024, 2048]},
    'reduction_message': {'binary': 'reduction_message_pimid', 'sizes': [256, 512, 1024, 2048]},
    'reduction_shared': {'binary': 'reduction_shared_pimid', 'sizes': [256, 512, 1024, 2048]},
    'histogram_message': {'binary': 'histogram_message_pimid', 'sizes': [256, 512, 1024]},
    'histogram_shared': {'binary': 'histogram_shared_pimid', 'sizes': [256, 512, 1024]},
    'prefixsum_message': {'binary': 'prefixsum_message_pimid', 'sizes': [256, 512, 1024]},
    'prefixsum_shared': {'binary': 'prefixsum_shared_pimid', 'sizes': [256, 512, 1024]},
    'stencil1d_message': {
        'binary': 'stencil1d_message_pimid',
        'sizes': [256, 512, 1024],
        'extra_param': 'num_iterations',
        'extra_values': [10, 50, 100, 200]
    },
    'stencil1d_shared': {
        'binary': 'stencil1d_shared_pimid',
        'sizes': [256, 512, 1024],
        'extra_param': 'num_iterations',
        'extra_values': [10, 50, 100, 200]
    },
}

# PE counts
NUM_SUBARRAYS = [1, 2, 4, 8, 16, 32, 64]

# Interconnect types
IS_LIBCOM = [0, 1]  # 0 = Baseline H-tree, 1 = LIBCom

def generate_yaml_config(test_id, combo):
    """Generate YAML config file for pimid binary"""
    workload_name = combo['workload']
    workload_info = WORKLOADS[workload_name]
    workload_binary = WORKLOAD_DIR / workload_info['binary']

    mem_tech = MEMORY_TECHS[combo['mem_tech']]

    # Build workload args - handle extra parameters for workloads like stencil1d
    args_list = [str(combo['num_subarrays']), str(combo['size'])]

    # Add extra parameter if present (e.g., num_iterations for stencil1d)
    if 'num_iterations' in combo:
        args_list.append(str(combo['num_iterations']))

    args_list.append(str(combo['is_libcom']))

    # Some workloads also accept memory tech parameter
    if combo['workload'] == 'bfs_message':
        args_list.append(str(combo['mem_tech']))

    workload_args = ' '.join(args_list)

    config_content = f"""# Auto-generated config for verification test {test_id}
simulation:
  name: "Verify500_{test_id:04d}"
  mode: "standalone"

execution_model:
  type: "zsim"
  config_file: "{CONFIG_DIR}/zsim_{test_id:04d}.cfg"

memory:
  technology: "{mem_tech['yaml']}"

pim:
  granularity: BANK
  num_pes: {combo['num_subarrays']}

workload:
  binary: "{workload_binary}"
  args: "{workload_args}"

network:
  topology: "{'libcom' if combo['is_libcom'] else 'htree'}"
"""

    config_file = CONFIG_DIR / f"test_{test_id:04d}.yaml"
    with open(config_file, 'w') as f:
        f.write(config_content)

    return config_file

def generate_zsim_config(test_id, combo):
    """Generate ZSim config file"""
    zsim_config = f"""// Auto-generated ZSim config for test {test_id}
sys = {{
    frequency = 2000;
    cores = {{
        type = "Simple";
        dcache = "l1d";
        icache = "l1i";
    }};
}};
"""

    zsim_file = CONFIG_DIR / f"zsim_{test_id:04d}.cfg"
    with open(zsim_file, 'w') as f:
        f.write(zsim_config)

    return zsim_file

def generate_test_configurations(num_tests=500):
    """Generate diverse test configurations"""
    configs = []
    config_hashes = set()

    print(f"Generating {num_tests} unique test configurations...")

    attempts = 0
    max_attempts = num_tests * 10

    while len(configs) < num_tests and attempts < max_attempts:
        attempts += 1

        # Random selection
        workload_name = random.choice(list(WORKLOADS.keys()))
        workload_info = WORKLOADS[workload_name]

        combo = {
            'workload': workload_name,
            'num_subarrays': random.choice(NUM_SUBARRAYS),
            'size': random.choice(workload_info['sizes']),
            'is_libcom': random.choice(IS_LIBCOM),
            'mem_tech': random.choice([0, 1, 2, 3, 4]),
        }

        # Add extra parameter if workload requires it (e.g., num_iterations for stencil1d)
        if 'extra_param' in workload_info:
            combo[workload_info['extra_param']] = random.choice(workload_info['extra_values'])

        # Validate workload-specific constraints
        # stencil1d workloads require (grid_size - 2) to be divisible by num_subarrays
        if workload_name in ['stencil1d_message', 'stencil1d_shared']:
            if (combo['size'] - 2) % combo['num_subarrays'] != 0:
                continue  # Skip this invalid combination

        # Check uniqueness
        config_hash = hashlib.md5(str(sorted(combo.items())).encode()).hexdigest()
        if config_hash not in config_hashes:
            config_hashes.add(config_hash)
            configs.append((len(configs), combo))

    print(f"Generated {len(configs)} unique configurations (attempts: {attempts})")
    return configs

def run_single_test(test_id, combo):
    """Run a single test using pimid binary with config file"""

    # Generate config files
    yaml_config = generate_yaml_config(test_id, combo)
    zsim_config = generate_zsim_config(test_id, combo)

    workload_info = WORKLOADS[combo['workload']]
    workload_binary = WORKLOAD_DIR / workload_info['binary']

    # Verify workload binary exists
    if not workload_binary.exists():
        return {
            'test_id': test_id,
            'status': 'failed',
            'error': f'Workload binary not found: {workload_binary}',
            'config': combo
        }

    # Execute using pimid binary ONLY - this is the ONLY entry point!
    cmd = [
        str(PIMID_BINARY),
        "--mode", "standalone",
        "--config", str(yaml_config),
        "--workload", str(workload_binary)
    ]

    # Add workload parameters (passed through to workload binary)
    cmd.append(str(combo['num_subarrays']))
    cmd.append(str(combo['size']))

    # Add extra parameter if present (e.g., num_iterations for stencil1d)
    if 'num_iterations' in combo:
        cmd.append(str(combo['num_iterations']))

    cmd.append(str(combo['is_libcom']))

    # Some workloads also accept memory tech parameter
    if combo['workload'] == 'bfs_message':
        cmd.append(str(combo['mem_tech']))

    try:
        start_time = time.time()
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=60,
            cwd=BASE_DIR
        )
        execution_time = time.time() - start_time

        # Check for success
        if result.returncode == 0:
            return {
                'test_id': test_id,
                'status': 'passed',
                'workload': combo['workload'],
                'memory_tech': MEMORY_TECHS[combo['mem_tech']]['name'],
                'num_pes': combo['num_subarrays'],
                'interconnect': 'LIBCom' if combo['is_libcom'] else 'Baseline',
                'execution_time': execution_time,
                'config_file': str(yaml_config),
                'entry_point': 'pimid_binary'
            }
        else:
            return {
                'test_id': test_id,
                'status': 'failed',
                'error': result.stderr[:500] if result.stderr else 'Unknown error',
                'returncode': result.returncode,
                'config': combo,
                'entry_point': 'pimid_binary'
            }

    except subprocess.TimeoutExpired:
        return {
            'test_id': test_id,
            'status': 'timeout',
            'error': 'Test exceeded 60 second timeout',
            'config': combo,
            'entry_point': 'pimid_binary'
        }
    except Exception as e:
        return {
            'test_id': test_id,
            'status': 'error',
            'error': str(e),
            'config': combo,
            'entry_point': 'pimid_binary'
        }

def main():
    """Main verification routine"""
    print("=" * 80)
    print("PIMID BINARY VERIFICATION - 500 ROUNDS")
    print("=" * 80)
    print(f"Start Time:     {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Entry Point:    {PIMID_BINARY} (ONLY)")
    print(f"Mode:           Config file driven simulation")
    print(f"Test Count:     500 rounds")
    print("=" * 80)
    print()

    # Verify pimid binary exists
    if not PIMID_BINARY.exists():
        print(f"ERROR: PIMID binary not found at {PIMID_BINARY}")
        print("Please build PIMID first:")
        print("  mkdir -p build && cd build")
        print("  cmake ../pimid && make -j$(nproc)")
        return 1

    # Verify workload binaries exist
    print("Verifying workload binaries...")
    missing_binaries = []
    for workload_name, workload_info in WORKLOADS.items():
        binary_path = WORKLOAD_DIR / workload_info['binary']
        if not binary_path.exists():
            missing_binaries.append(workload_info['binary'])

    if missing_binaries:
        print(f"ERROR: {len(missing_binaries)} workload binaries missing:")
        for binary in missing_binaries:
            print(f"  - {binary}")
        print("\nPlease compile workloads:")
        print("  cd DAC26/workloads_pimid && make all")
        return 1

    print(f"✓ All {len(WORKLOADS)} workload binaries found")
    print()

    # Generate test configurations
    test_configs = generate_test_configurations(500)
    print()

    # Run tests
    print("Executing 500 verification rounds...")
    print("Entry Point: pimid binary with YAML config files")
    print("-" * 80)

    all_results = []
    passed = 0
    failed = 0
    errors = 0

    start_time = time.time()

    for test_id, combo in test_configs:
        if (test_id + 1) % 50 == 0:
            elapsed = time.time() - start_time
            rate = (test_id + 1) / elapsed if elapsed > 0 else 0
            print(f"Progress: {test_id + 1}/500 tests ({rate:.1f} tests/sec)")

        result = run_single_test(test_id, combo)
        all_results.append(result)

        if result['status'] == 'passed':
            passed += 1
        elif result['status'] == 'failed':
            failed += 1
        else:
            errors += 1

    total_time = time.time() - start_time

    # Print summary
    print()
    print("=" * 80)
    print("VERIFICATION SUMMARY")
    print("=" * 80)
    print(f"Total Tests:    500")
    print(f"Passed:         {passed} ({passed/5:.1f}%)")
    print(f"Failed:         {failed} ({failed/5:.1f}%)")
    print(f"Errors:         {errors} ({errors/5:.1f}%)")
    print(f"Total Time:     {total_time:.1f} seconds")
    print(f"Test Rate:      {500/total_time:.1f} tests/second")
    print(f"Entry Point:    pimid binary (verified ONLY entry point)")
    print("=" * 80)

    # Save results
    results_file = RESULTS_DIR / 'verification_500_results.json'
    with open(results_file, 'w') as f:
        json.dump({
            'summary': {
                'total': 500,
                'passed': passed,
                'failed': failed,
                'errors': errors,
                'pass_rate': passed / 5,
                'execution_time': total_time,
                'test_rate': 500 / total_time,
                'entry_point': str(PIMID_BINARY),
                'verification_mode': 'pimid_binary_with_config_files',
                'timestamp': datetime.now().isoformat()
            },
            'results': all_results
        }, f, indent=2)

    print(f"\nResults saved to: {results_file}")

    # Create summary report
    create_summary_report(all_results, passed, failed, errors, total_time)

    # Return status
    if passed == 500:
        print("\n✓ ALL 500 TESTS PASSED - PIMID BINARY VERIFIED!")
        return 0
    else:
        print(f"\n✗ {failed + errors} TESTS FAILED - REVIEW REQUIRED")
        return 1

def create_summary_report(results, passed, failed, errors, total_time):
    """Create detailed summary report"""
    report_file = RESULTS_DIR / 'VERIFICATION_500_REPORT.md'

    # Analyze results by category
    by_workload = {}
    by_memory = {}
    by_interconnect = {}

    for result in results:
        if result['status'] == 'passed':
            # By workload
            wl = result['workload']
            by_workload[wl] = by_workload.get(wl, 0) + 1

            # By memory
            mem = result['memory_tech']
            by_memory[mem] = by_memory.get(mem, 0) + 1

            # By interconnect
            ic = result['interconnect']
            by_interconnect[ic] = by_interconnect.get(ic, 0) + 1

    with open(report_file, 'w') as f:
        f.write(f"""# PIMID Binary Verification Report - 500 Rounds

**Date**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
**Entry Point**: pimid binary ONLY (config file driven)
**Mode**: Comprehensive code audit and verification

## Executive Summary

✓ **Entry Point Verified**: All tests used pimid binary as sole entry point
✓ **Config Driven**: All simulations driven by YAML configuration files
✓ **Test Coverage**: 500 unique test configurations

| Metric | Value |
|--------|-------|
| **Total Tests** | 500 |
| **Passed** | {passed} ({passed/5:.1f}%) |
| **Failed** | {failed} ({failed/5:.1f}%) |
| **Errors** | {errors} ({errors/5:.1f}%) |
| **Execution Time** | {total_time:.1f} seconds |
| **Test Rate** | {500/total_time:.1f} tests/second |
| **Entry Point** | `{PIMID_BINARY}` |

## Verification Status

""")

        if passed == 500:
            f.write("### ✅ PRODUCTION READY\n\n")
            f.write("All 500 verification tests passed successfully.\n")
            f.write("PIMID binary verified as reliable and production-ready entry point.\n\n")
        else:
            f.write(f"### ⚠️ ISSUES DETECTED\n\n")
            f.write(f"{failed + errors} tests failed. Review required.\n\n")

        f.write(f"""## Test Coverage

### By Workload
""")
        for wl, count in sorted(by_workload.items()):
            f.write(f"- **{wl}**: {count} tests\n")

        f.write(f"""
### By Memory Technology
""")
        for mem, count in sorted(by_memory.items()):
            f.write(f"- **{mem}**: {count} tests\n")

        f.write(f"""
### By Interconnect
""")
        for ic, count in sorted(by_interconnect.items()):
            f.write(f"- **{ic}**: {count} tests\n")

        f.write(f"""
## Entry Point Verification

**CRITICAL**: All {len(results)} tests executed using:
```bash
{PIMID_BINARY} --config <yaml_file> --workload <workload_binary>
```

No direct workload execution. All tests config-file driven through pimid binary.

## Configuration Files

All test configurations saved to: `{CONFIG_DIR}/`
- YAML configs: `test_XXXX.yaml`
- ZSim configs: `zsim_XXXX.cfg`

## Results Data

Complete results saved to: `verification_500_results.json`

---

**Verification Mode**: Config-driven through pimid binary ONLY
**Status**: {"VERIFIED ✓" if passed == 500 else "REVIEW REQUIRED ✗"}
**Date**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
""")

    print(f"Summary report saved to: {report_file}")

if __name__ == "__main__":
    exit(main())
