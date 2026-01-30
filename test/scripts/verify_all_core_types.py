#!/usr/bin/env python3
"""
ZSim Core Types Verification Suite
Tests ALL 5 ZSim core types: Simple, Timing, OOO, Null, ALU
Entry Point: pimid binary with config files ONLY
"""

import os
import subprocess
import json
import time
from datetime import datetime
from pathlib import Path

# Base directories
BASE_DIR = Path("/home/user/pimid-dev")
WORKLOAD_DIR = BASE_DIR / "DAC26/workloads_pimid"
RESULTS_DIR = BASE_DIR / "test/results/verify_all_core_types"
CONFIG_DIR = RESULTS_DIR / "configs"
PIMID_BINARY = BASE_DIR / "build/pimid/pimid"

# Create directories
RESULTS_DIR.mkdir(exist_ok=True)
CONFIG_DIR.mkdir(exist_ok=True)

# All 5 ZSim core types
CORE_TYPES = {
    0: {'name': 'Simple', 'type': 'Simple', 'has_cache': True, 'desc': 'Simple in-order core'},
    1: {'name': 'OOO', 'type': 'OOO', 'has_cache': True, 'desc': 'Out-of-order superscalar core'},
    2: {'name': 'Timing', 'type': 'Timing', 'has_cache': True, 'desc': 'Timing model core'},
    3: {'name': 'ALU', 'type': 'ALU', 'has_cache': False, 'desc': 'Cacheless ALU-only core (PIM-optimized)'},
    4: {'name': 'Null', 'type': 'Null', 'has_cache': False, 'desc': 'Null core (minimal)'},
}

# Simple test workload
TEST_WORKLOAD = {
    'name': 'gemm_message',
    'binary': 'gemm_message_pimid',
    'params': {
        'num_subarrays': 4,
        'size': 32,
        'is_libcom': 0,
    }
}

# Memory technology for test
MEMORY_TECH = {'name': 'SRAM', 'yaml': 'SRAM'}

# Network topology for test
NETWORK = {'name': 'H-tree', 'yaml': 'H_TREE'}

def generate_zsim_config(test_id, core_type_id):
    """Generate ZSim config file with specified core type"""
    core_info = CORE_TYPES[core_type_id]
    core_type = core_info['type']
    has_cache = core_info['has_cache']

    zsim_config = f"""// Auto-generated ZSim config for core type test {test_id}
// Core type: {core_type} - {core_info['desc']}

sys = {{
    frequency = 2000;  // 2 GHz

    cores = {{
        type = "{core_type}";
"""

    if core_type == "ALU":
        # ALU core has configurable aluLatency but NO caches
        zsim_config += """        aluLatency = 1;  // 1 cycle per ALU operation
        // NOTE: ALU core has NO icache/dcache - it's cacheless!
"""
    elif has_cache:
        # Cores with caches need icache/dcache
        zsim_config += """        dcache = "l1d";
        icache = "l1i";
"""
    # Null core needs nothing

    zsim_config += """    };
"""

    if has_cache:
        # Add cache hierarchy for cores that need it
        zsim_config += """
    caches = {
        l1d = {
            size = 32768;  // 32 KB
            array = {
                type = "SetAssoc";
                ways = 8;
            };
            latency = 2;
        };

        l1i = {
            size = 32768;  // 32 KB
            array = {
                type = "SetAssoc";
                ways = 8;
            };
            latency = 2;
        };
    };
"""

    zsim_config += """};
"""

    zsim_file = CONFIG_DIR / f"zsim_{core_type}_{test_id:04d}.cfg"
    with open(zsim_file, 'w') as f:
        f.write(zsim_config)

    return zsim_file

def generate_yaml_config(test_id, core_type_id):
    """Generate YAML config file for pimid binary"""
    core_info = CORE_TYPES[core_type_id]
    workload_binary = WORKLOAD_DIR / TEST_WORKLOAD['binary']

    # Build workload args
    params = TEST_WORKLOAD['params']
    workload_args = f"{params['num_subarrays']} {params['size']} {params['is_libcom']}"

    config_content = f"""# Auto-generated config for core type verification test {test_id}
# Core Type: {core_info['type']} - {core_info['desc']}
simulation:
  name: "CoreType_{core_info['name']}_{test_id:04d}"
  mode: "standalone"

execution_model:
  type: "zsim"
  config_file: "{CONFIG_DIR}/zsim_{core_info['type']}_{test_id:04d}.cfg"
  core_type: "{core_info['type']}"
  has_cache: {str(core_info['has_cache']).lower()}

memory:
  technology: "{MEMORY_TECH['yaml']}"

pim:
  granularity: BANK
  num_pes: {params['num_subarrays']}

workload:
  binary: "{workload_binary}"
  args: "{workload_args}"

network:
  topology: "{NETWORK['yaml']}"
  model: "GARNET"
"""

    config_file = CONFIG_DIR / f"test_{core_info['type']}_{test_id:04d}.yaml"
    with open(config_file, 'w') as f:
        f.write(config_content)

    return config_file

def run_single_test(test_id, core_type_id):
    """Run a single test for a specific core type"""
    core_info = CORE_TYPES[core_type_id]

    # Generate config files
    yaml_config = generate_yaml_config(test_id, core_type_id)
    zsim_config = generate_zsim_config(test_id, core_type_id)

    workload_binary = WORKLOAD_DIR / TEST_WORKLOAD['binary']

    # Verify workload binary exists
    if not workload_binary.exists():
        return {
            'test_id': test_id,
            'core_type': core_info['name'],
            'status': 'failed',
            'error': f'Workload binary not found: {workload_binary}',
        }

    # Execute using pimid binary ONLY
    cmd = [
        str(PIMID_BINARY),
        "--mode", "standalone",
        "--config", str(yaml_config),
        "--workload", str(workload_binary)
    ]

    # Add workload parameters
    params = TEST_WORKLOAD['params']
    cmd.extend([
        str(params['num_subarrays']),
        str(params['size']),
        str(params['is_libcom'])
    ])

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
                'core_type': core_info['name'],
                'core_type_full': core_info['type'],
                'description': core_info['desc'],
                'has_cache': core_info['has_cache'],
                'status': 'passed',
                'execution_time': execution_time,
                'config_file': str(yaml_config),
                'entry_point': 'pimid_binary'
            }
        else:
            return {
                'test_id': test_id,
                'core_type': core_info['name'],
                'core_type_full': core_info['type'],
                'description': core_info['desc'],
                'status': 'failed',
                'error': result.stderr[:500] if result.stderr else result.stdout[:500] if result.stdout else 'Unknown error',
                'returncode': result.returncode,
                'entry_point': 'pimid_binary'
            }

    except subprocess.TimeoutExpired:
        return {
            'test_id': test_id,
            'core_type': core_info['name'],
            'status': 'timeout',
            'error': 'Test exceeded 60 second timeout',
        }
    except Exception as e:
        return {
            'test_id': test_id,
            'core_type': core_info['name'],
            'status': 'error',
            'error': str(e),
        }

def main():
    """Main verification routine"""
    print("=" * 80)
    print("ZSIM CORE TYPES VERIFICATION - ALL 5 CORE TYPES")
    print("=" * 80)
    print(f"Start Time:     {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Entry Point:    {PIMID_BINARY} (ONLY)")
    print(f"Test Workload:  {TEST_WORKLOAD['name']}")
    print("=" * 80)
    print()

    print("Testing all 5 ZSim core types:")
    for core_id, core_info in CORE_TYPES.items():
        cache_status = "with cache" if core_info['has_cache'] else "NO CACHE (cacheless)"
        print(f"  {core_id + 1}. {core_info['name']:8} - {core_info['desc']} [{cache_status}]")
    print()

    # Verify pimid binary exists
    if not PIMID_BINARY.exists():
        print(f"ERROR: PIMID binary not found at {PIMID_BINARY}")
        return 1

    # Verify test workload binary exists
    workload_binary = WORKLOAD_DIR / TEST_WORKLOAD['binary']
    if not workload_binary.exists():
        print(f"ERROR: Test workload binary not found: {workload_binary}")
        return 1

    print("✓ PIMID binary found")
    print("✓ Test workload binary found")
    print()

    # Run tests for all core types
    print("Executing core type verification tests...")
    print("-" * 80)

    all_results = []
    passed = 0
    failed = 0

    start_time = time.time()

    for core_id in CORE_TYPES.keys():
        core_info = CORE_TYPES[core_id]
        print(f"Testing {core_info['name']:8} core... ", end='', flush=True)

        # Run 3 tests per core type for reliability
        core_results = []
        core_passed = 0
        for test_num in range(3):
            result = run_single_test(test_num, core_id)
            core_results.append(result)
            if result['status'] == 'passed':
                core_passed += 1

        all_results.extend(core_results)

        if core_passed == 3:
            print(f"✓ PASSED (3/3 tests)")
            passed += 3
        else:
            print(f"✗ FAILED ({core_passed}/3 tests)")
            failed += (3 - core_passed)
            # Show first error
            for r in core_results:
                if r['status'] != 'passed':
                    print(f"  Error: {r.get('error', 'Unknown')[:100]}")
                    break

    total_time = time.time() - start_time
    total_tests = len(all_results)

    # Print summary
    print()
    print("=" * 80)
    print("CORE TYPES VERIFICATION SUMMARY")
    print("=" * 80)
    print(f"Core Types Tested:  {len(CORE_TYPES)}")
    print(f"Total Tests:        {total_tests}")
    print(f"Passed:             {passed} ({passed*100/total_tests:.1f}%)")
    print(f"Failed:             {failed} ({failed*100/total_tests:.1f}%)")
    print(f"Total Time:         {total_time:.1f} seconds")
    print(f"Entry Point:        pimid binary (verified)")
    print("=" * 80)

    # Core type breakdown
    print()
    print("Core Type Results:")
    for core_id, core_info in CORE_TYPES.items():
        core_name = core_info['name']
        core_tests = [r for r in all_results if r['core_type'] == core_name]
        core_pass = sum(1 for r in core_tests if r['status'] == 'passed')
        status_icon = "✓" if core_pass == len(core_tests) else "✗"
        cache_tag = "[CACHELESS]" if not core_info['has_cache'] else "[with cache]"
        print(f"  {status_icon} {core_name:8} {cache_tag:15} - {core_pass}/{len(core_tests)} passed")

    # Save results
    results_file = RESULTS_DIR / 'all_core_types_results.json'
    with open(results_file, 'w') as f:
        json.dump({
            'summary': {
                'core_types': len(CORE_TYPES),
                'total_tests': total_tests,
                'passed': passed,
                'failed': failed,
                'pass_rate': passed * 100 / total_tests,
                'execution_time': total_time,
                'entry_point': str(PIMID_BINARY),
                'timestamp': datetime.now().isoformat()
            },
            'core_types': {
                core_info['name']: {
                    'type': core_info['type'],
                    'description': core_info['desc'],
                    'has_cache': core_info['has_cache'],
                    'tests': [r for r in all_results if r['core_type'] == core_info['name']]
                }
                for core_info in CORE_TYPES.values()
            }
        }, f, indent=2)

    print(f"\nResults saved to: {results_file}")

    # Return status
    if passed == total_tests:
        print(f"\n✓ ALL {len(CORE_TYPES)} CORE TYPES VERIFIED - ALL TESTS PASSED!")
        return 0
    else:
        print(f"\n✗ {failed} TESTS FAILED - SOME CORE TYPES HAVE ISSUES")
        return 1

if __name__ == "__main__":
    exit(main())
