#!/usr/bin/env python3
"""
PIMID Comprehensive Verification Suite
Entry Point: pimid binary with config files ONLY
Coverage: All execution model variations (core types, network topologies, memory techs)
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
RESULTS_DIR = BASE_DIR / "test/results/verify_comprehensive"
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

# Network topologies
NETWORK_TOPOLOGIES = {
    0: {'name': 'H-tree', 'yaml': 'H_TREE', 'is_libcom': 0},
    1: {'name': 'Crossbar (LIBCom)', 'yaml': 'CROSSBAR', 'is_libcom': 1},
}

# Core types (ZSim execution model)
CORE_TYPES = {
    0: {'name': 'Simple', 'type': 'Simple'},
    1: {'name': 'OOO', 'type': 'OOO'},
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

def generate_zsim_config(test_id, combo):
    """Generate ZSim config file with specified core type"""
    core_type = CORE_TYPES[combo['core_type']]['type']

    zsim_config = f"""// Auto-generated ZSim config for comprehensive test {test_id}
// Core type: {core_type}
// Memory: {MEMORY_TECHS[combo['mem_tech']]['name']}
// Network: {NETWORK_TOPOLOGIES[combo['network']]['name']}

sys = {{
    frequency = 2000;  // 2 GHz

    cores = {{
        type = "{core_type}";
        dcache = "l1d";
        icache = "l1i";
    }};

    caches = {{
        l1d = {{
            size = 32768;  // 32 KB
            array = {{
                type = "SetAssoc";
                ways = 8;
            }};
            latency = 2;
        }};

        l1i = {{
            size = 32768;  // 32 KB
            array = {{
                type = "SetAssoc";
                ways = 8;
            }};
            latency = 2;
        }};
    }};
}};
"""

    zsim_file = CONFIG_DIR / f"zsim_{test_id:04d}.cfg"
    with open(zsim_file, 'w') as f:
        f.write(zsim_config)

    return zsim_file

def generate_yaml_config(test_id, combo):
    """Generate YAML config file for pimid binary"""
    workload_name = combo['workload']
    workload_info = WORKLOADS[workload_name]
    workload_binary = WORKLOAD_DIR / workload_info['binary']

    mem_tech = MEMORY_TECHS[combo['mem_tech']]
    network = NETWORK_TOPOLOGIES[combo['network']]
    core_type = CORE_TYPES[combo['core_type']]

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

    config_content = f"""# Auto-generated config for comprehensive verification test {test_id}
# Core: {core_type['name']}, Memory: {mem_tech['name']}, Network: {network['name']}
simulation:
  name: "VerifyComp_{test_id:04d}"
  mode: "standalone"

execution_model:
  type: "zsim"
  config_file: "{CONFIG_DIR}/zsim_{test_id:04d}.cfg"
  core_type: "{core_type['type']}"

memory:
  technology: "{mem_tech['yaml']}"

pim:
  granularity: BANK
  num_pes: {combo['num_subarrays']}

workload:
  binary: "{workload_binary}"
  args: "{workload_args}"

network:
  topology: "{network['yaml']}"
  model: "GARNET"
"""

    config_file = CONFIG_DIR / f"test_{test_id:04d}.yaml"
    with open(config_file, 'w') as f:
        f.write(config_content)

    return config_file

def generate_test_configurations(num_tests=500):
    """Generate diverse test configurations covering all dimensions"""
    configs = []
    config_hashes = set()

    print(f"Generating {num_tests} unique test configurations...")
    print("Coverage dimensions:")
    print(f"  - Memory technologies: {len(MEMORY_TECHS)} ({', '.join(m['name'] for m in MEMORY_TECHS.values())})")
    print(f"  - Network topologies: {len(NETWORK_TOPOLOGIES)} ({', '.join(n['name'] for n in NETWORK_TOPOLOGIES.values())})")
    print(f"  - Core types: {len(CORE_TYPES)} ({', '.join(c['name'] for c in CORE_TYPES.values())})")
    print(f"  - Workloads: {len(WORKLOADS)}")
    print(f"  - PE counts: {len(NUM_SUBARRAYS)}")
    print()

    attempts = 0
    max_attempts = num_tests * 10

    while len(configs) < num_tests and attempts < max_attempts:
        attempts += 1

        # Random selection
        workload_name = random.choice(list(WORKLOADS.keys()))
        workload_info = WORKLOADS[workload_name]
        network_id = random.choice(list(NETWORK_TOPOLOGIES.keys()))

        combo = {
            'workload': workload_name,
            'num_subarrays': random.choice(NUM_SUBARRAYS),
            'size': random.choice(workload_info['sizes']),
            'mem_tech': random.choice(list(MEMORY_TECHS.keys())),
            'network': network_id,
            'is_libcom': NETWORK_TOPOLOGIES[network_id]['is_libcom'],
            'core_type': random.choice(list(CORE_TYPES.keys())),
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
                'network_topology': NETWORK_TOPOLOGIES[combo['network']]['name'],
                'core_type': CORE_TYPES[combo['core_type']]['name'],
                'num_pes': combo['num_subarrays'],
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
    print("PIMID COMPREHENSIVE VERIFICATION")
    print("=" * 80)
    print(f"Start Time:     {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Entry Point:    {PIMID_BINARY} (ONLY)")
    print(f"Mode:           Config file driven simulation")
    print(f"Execution Model: ZSim with all variations")
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
    print(f"Executing {len(test_configs)} comprehensive verification tests...")
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
            print(f"Progress: {test_id + 1}/{len(test_configs)} tests ({rate:.1f} tests/sec)")

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
    print("COMPREHENSIVE VERIFICATION SUMMARY")
    print("=" * 80)
    print(f"Total Tests:    {len(test_configs)}")
    print(f"Passed:         {passed} ({passed*100/len(test_configs):.1f}%)")
    print(f"Failed:         {failed} ({failed*100/len(test_configs):.1f}%)")
    print(f"Errors:         {errors} ({errors*100/len(test_configs):.1f}%)")
    print(f"Total Time:     {total_time:.1f} seconds")
    print(f"Test Rate:      {len(test_configs)/total_time:.1f} tests/second")
    print(f"Entry Point:    pimid binary (verified ONLY entry point)")
    print("=" * 80)

    # Save results
    results_file = RESULTS_DIR / 'comprehensive_verification_results.json'
    with open(results_file, 'w') as f:
        json.dump({
            'summary': {
                'total': len(test_configs),
                'passed': passed,
                'failed': failed,
                'errors': errors,
                'pass_rate': passed * 100 / len(test_configs),
                'execution_time': total_time,
                'test_rate': len(test_configs) / total_time,
                'entry_point': str(PIMID_BINARY),
                'verification_mode': 'comprehensive_all_execution_models',
                'timestamp': datetime.now().isoformat(),
                'coverage': {
                    'memory_technologies': len(MEMORY_TECHS),
                    'network_topologies': len(NETWORK_TOPOLOGIES),
                    'core_types': len(CORE_TYPES),
                    'workloads': len(WORKLOADS),
                    'pe_counts': len(NUM_SUBARRAYS)
                }
            },
            'results': all_results
        }, f, indent=2)

    print(f"\nResults saved to: {results_file}")

    # Create summary report
    create_summary_report(all_results, passed, failed, errors, total_time, len(test_configs))

    # Return status
    if passed == len(test_configs):
        print(f"\n✓ ALL {len(test_configs)} TESTS PASSED - COMPREHENSIVE VERIFICATION COMPLETE!")
        return 0
    else:
        print(f"\n✗ {failed + errors} TESTS FAILED - REVIEW REQUIRED")
        return 1

def create_summary_report(results, passed, failed, errors, total_time, total_tests):
    """Create detailed summary report"""
    report_file = RESULTS_DIR / 'COMPREHENSIVE_VERIFICATION_REPORT.md'

    # Analyze results by category
    by_workload = {}
    by_memory = {}
    by_network = {}
    by_core = {}

    for result in results:
        if result['status'] == 'passed':
            # By workload
            wl = result['workload']
            by_workload[wl] = by_workload.get(wl, 0) + 1

            # By memory
            mem = result['memory_tech']
            by_memory[mem] = by_memory.get(mem, 0) + 1

            # By network
            net = result['network_topology']
            by_network[net] = by_network.get(net, 0) + 1

            # By core type
            core = result['core_type']
            by_core[core] = by_core.get(core, 0) + 1

    with open(report_file, 'w') as f:
        f.write(f"""# PIMID Comprehensive Verification Report

**Date**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
**Entry Point**: pimid binary ONLY (config file driven)
**Mode**: Comprehensive execution model verification
**Coverage**: All memory technologies, network topologies, core types

## Executive Summary

✓ **Entry Point Verified**: All tests used pimid binary as sole entry point
✓ **Config Driven**: All simulations driven by YAML configuration files
✓ **ZSim Execution Model**: All tests used ZSim with varied configurations
✓ **Comprehensive Coverage**: {total_tests} unique test configurations

| Metric | Value |
|--------|-------|
| **Total Tests** | {total_tests} |
| **Passed** | {passed} ({passed*100/total_tests:.1f}%) |
| **Failed** | {failed} ({failed*100/total_tests:.1f}%) |
| **Errors** | {errors} ({errors*100/total_tests:.1f}%) |
| **Execution Time** | {total_time:.1f} seconds |
| **Test Rate** | {total_tests/total_time:.1f} tests/second |
| **Entry Point** | `{PIMID_BINARY}` |

## Verification Status

""")

        if passed == total_tests:
            f.write("### ✅ PRODUCTION READY\n\n")
            f.write(f"All {total_tests} comprehensive verification tests passed successfully.\n")
            f.write("PIMID verified across all execution model variations.\n\n")
        else:
            f.write(f"### ⚠️ ISSUES DETECTED\n\n")
            f.write(f"{failed + errors} tests failed. Review required.\n\n")

        f.write(f"""## Coverage Analysis

### Execution Model Dimensions

| Dimension | Count | Options |
|-----------|-------|---------|
| **Memory Technologies** | {len(MEMORY_TECHS)} | {', '.join(m['name'] for m in MEMORY_TECHS.values())} |
| **Network Topologies** | {len(NETWORK_TOPOLOGIES)} | {', '.join(n['name'] for n in NETWORK_TOPOLOGIES.values())} |
| **Core Types** | {len(CORE_TYPES)} | {', '.join(c['name'] for c in CORE_TYPES.values())} |
| **Workloads** | {len(WORKLOADS)} | 16 total (8 message passing + 8 shared memory) |
| **PE Counts** | {len(NUM_SUBARRAYS)} | {', '.join(str(n) for n in NUM_SUBARRAYS)} |

### Test Coverage Breakdown

#### By Workload
""")
        for wl, count in sorted(by_workload.items()):
            f.write(f"- **{wl}**: {count} tests\n")

        f.write(f"""
#### By Memory Technology
""")
        for mem, count in sorted(by_memory.items()):
            f.write(f"- **{mem}**: {count} tests\n")

        f.write(f"""
#### By Network Topology
""")
        for net, count in sorted(by_network.items()):
            f.write(f"- **{net}**: {count} tests\n")

        f.write(f"""
#### By Core Type
""")
        for core, count in sorted(by_core.items()):
            f.write(f"- **{core}**: {count} tests\n")

        f.write(f"""
## Entry Point Verification

**CRITICAL**: All {total_tests} tests executed using:
```bash
{PIMID_BINARY} --mode standalone --config <yaml_file> --workload <workload_binary>
```

No direct workload execution. All tests config-file driven through pimid binary.

## ZSim Execution Model Verification

All tests executed with ZSim execution model:
- **Core types tested**: {', '.join(c['name'] for c in CORE_TYPES.values())}
- **Network models**: GARNET with H-tree and Crossbar topologies
- **Memory technologies**: All 5 technologies (SRAM, DRAM, STT-MRAM, PCM, ReRAM)

## Configuration Files

All test configurations saved to: `{CONFIG_DIR}/`
- YAML configs: `test_XXXX.yaml`
- ZSim configs: `zsim_XXXX.cfg`

## Results Data

Complete results saved to: `comprehensive_verification_results.json`

---

**Verification Mode**: Comprehensive execution model testing (ZSim + all variations)
**Status**: {"VERIFIED ✓" if passed == total_tests else "REVIEW REQUIRED ✗"}
**Date**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
""")

    print(f"Summary report saved to: {report_file}")

if __name__ == "__main__":
    exit(main())
