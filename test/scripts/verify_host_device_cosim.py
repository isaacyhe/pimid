#!/usr/bin/env python3
"""
Host/Device Co-Simulation Verification Suite
Tests 5 realistic use cases with host (OOO/Simple with cache) and device (ALU without cache)
Both running under ZSim execution model
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
RESULTS_DIR = BASE_DIR / "test/results/verify_host_device_cosim"
CONFIG_DIR = RESULTS_DIR / "configs"
PIMID_BINARY = BASE_DIR / "build/pimid/pimid"

# Create directories
RESULTS_DIR.mkdir(exist_ok=True)
CONFIG_DIR.mkdir(exist_ok=True)

# 5 Host/Device Co-Simulation Use Cases
CO_SIM_USE_CASES = {
    0: {
        'name': 'Data Preparation + PIM Compute',
        'desc': 'Host prepares data, device does matrix multiply, host collects results',
        'workload': 'gemm_message_pimid',
        'host_core': 'OOO',
        'device_core': 'ALU',
        'host_cores': 1,
        'device_cores': 8,
        'params': {'num_subarrays': 8, 'size': 64, 'is_libcom': 1},
        'scenario': 'Host reads input matrices → Device computes GEMM → Host writes results'
    },
    1: {
        'name': 'Iterative Graph Processing',
        'desc': 'Host coordinates BFS iterations, device processes graph levels',
        'workload': 'bfs_message_pimid',
        'host_core': 'Simple',
        'device_core': 'ALU',
        'host_cores': 1,
        'device_cores': 16,
        'params': {'num_subarrays': 16, 'size': 256, 'is_libcom': 1, 'mem_tech': 0},
        'scenario': 'Host manages frontier queue → Device explores neighbors → Host updates levels'
    },
    2: {
        'name': 'Parallel Sparse Matrix Operations',
        'desc': 'Host decomposes sparse matrix, device processes partitions, host aggregates',
        'workload': 'spmv_message_pimid',
        'host_core': 'OOO',
        'device_core': 'ALU',
        'host_cores': 2,
        'device_cores': 32,
        'params': {'num_subarrays': 32, 'size': 128, 'is_libcom': 1},
        'scenario': 'Host partitions matrix → Device computes SpMV on partitions → Host aggregates'
    },
    3: {
        'name': 'Reduction with Host Aggregation',
        'desc': 'Host distributes data, device performs local reductions, host final reduce',
        'workload': 'reduction_message_pimid',
        'host_core': 'OOO',
        'device_core': 'ALU',
        'host_cores': 1,
        'device_cores': 64,
        'params': {'num_subarrays': 64, 'size': 1024, 'is_libcom': 1},
        'scenario': 'Host distributes array → Device local reductions → Host final aggregation'
    },
    4: {
        'name': 'Pipelined Histogram Processing',
        'desc': 'Host streams data, device computes histograms, host merges results',
        'workload': 'histogram_message_pimid',
        'host_core': 'Simple',
        'device_core': 'ALU',
        'host_cores': 1,
        'device_cores': 16,
        'params': {'num_subarrays': 16, 'size': 512, 'is_libcom': 0},
        'scenario': 'Host streams data blocks → Device computes local histograms → Host merges'
    },
}

def generate_zsim_config(test_id, use_case_id):
    """Generate ZSim config with both host and device cores"""
    use_case = CO_SIM_USE_CASES[use_case_id]
    host_core = use_case['host_core']
    device_core = use_case['device_core']
    host_cores = use_case['host_cores']
    device_cores = use_case['device_cores']

    zsim_config = f"""// Auto-generated ZSim config for host/device co-simulation
// Use Case: {use_case['name']}
// Scenario: {use_case['scenario']}

sys = {{
    frequency = 2000;  // 2 GHz

    // HETEROGENEOUS CORES: Host + Device under same ZSim instance
    cores = {{
        // HOST CORE(S): {host_core} with cache (general-purpose processing)
        host = {{
            type = "{host_core}";
            cores = {host_cores};
            icache = "l1i_host";
            dcache = "l1d_host";
        }};

        // DEVICE CORE(S): {device_core} WITHOUT cache (PIM processing elements)
        device = {{
            type = "{device_core}";
            cores = {device_cores};
"""

    if device_core == "ALU":
        zsim_config += """            aluLatency = 1;  // 1 cycle per ALU operation
            // NOTE: ALU cores are CACHELESS - no icache/dcache needed!
"""

    zsim_config += """        };
    };

    // CACHE HIERARCHY (for host cores only, device cores are cacheless)
    caches = {
        l1d_host = {
            caches = """ + str(host_cores) + """;  // One per host core
            size = 32768;  // 32 KB
            array = {
                type = "SetAssoc";
                ways = 8;
            };
            latency = 2;
        };

        l1i_host = {
            caches = """ + str(host_cores) + """;  // One per host core
            size = 32768;  // 32 KB
            array = {
                type = "SetAssoc";
                ways = 8;
            };
            latency = 2;
        };

        l2 = {
            caches = 1;  // Shared L2 for host cores
            size = 262144;  // 256 KB
            latency = 10;
            children = "l1i_host|l1d_host";
        };
    };

    // Memory system (shared between host and device)
    mem = {
        type = "Simple";
        latency = 100;  // 100 cycles
    };
};

sim = {
    phaseLength = 10000;
    maxTotalInstrs = 10000000L;
    statsPhaseInterval = 1000;
    printHierarchy = true;
};
"""

    zsim_file = CONFIG_DIR / f"zsim_cosim_{use_case_id:02d}_{test_id:04d}.cfg"
    with open(zsim_file, 'w') as f:
        f.write(zsim_config)

    return zsim_file

def generate_yaml_config(test_id, use_case_id):
    """Generate YAML config for host/device co-simulation"""
    use_case = CO_SIM_USE_CASES[use_case_id]
    workload_binary = WORKLOAD_DIR / use_case['workload']
    params = use_case['params']

    # Build workload args
    args_list = [
        str(params['num_subarrays']),
        str(params['size']),
    ]
    if 'mem_tech' in params:
        args_list.append(str(params['mem_tech']))
    args_list.append(str(params['is_libcom']))
    workload_args = ' '.join(args_list)

    config_content = f"""# Auto-generated config for host/device co-simulation test {test_id}
# Use Case {use_case_id}: {use_case['name']}
# {use_case['desc']}

simulation:
  name: "CoSim_{use_case_id:02d}_{test_id:04d}"
  mode: "standalone"
  co_simulation: true
  scenario: "{use_case['scenario']}"

execution_model:
  type: "zsim"
  config_file: "{CONFIG_DIR}/zsim_cosim_{use_case_id:02d}_{test_id:04d}.cfg"

  # HETEROGENEOUS CONFIGURATION
  heterogeneous: true

  host:
    core_type: "{use_case['host_core']}"
    num_cores: {use_case['host_cores']}
    has_cache: true
    role: "Coordinator and data management"

  device:
    core_type: "{use_case['device_core']}"
    num_cores: {use_case['device_cores']}
    has_cache: false
    role: "Compute-intensive processing"

memory:
  technology: "SRAM"
  shared: true  # Shared between host and device

pim:
  granularity: BANK
  num_pes: {params['num_subarrays']}

workload:
  binary: "{workload_binary}"
  args: "{workload_args}"
  execution_model: "host_device_cosim"

network:
  topology: "{'CROSSBAR' if params['is_libcom'] else 'H_TREE'}"
  model: "GARNET"
"""

    config_file = CONFIG_DIR / f"test_cosim_{use_case_id:02d}_{test_id:04d}.yaml"
    with open(config_file, 'w') as f:
        f.write(config_content)

    return config_file

def run_single_test(test_id, use_case_id):
    """Run a single host/device co-simulation test"""
    use_case = CO_SIM_USE_CASES[use_case_id]

    # Generate config files
    yaml_config = generate_yaml_config(test_id, use_case_id)
    zsim_config = generate_zsim_config(test_id, use_case_id)

    workload_binary = WORKLOAD_DIR / use_case['workload']
    params = use_case['params']

    # Verify workload binary exists
    if not workload_binary.exists():
        return {
            'test_id': test_id,
            'use_case': use_case['name'],
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
    cmd.append(str(params['num_subarrays']))
    cmd.append(str(params['size']))
    if 'mem_tech' in params:
        cmd.append(str(params['mem_tech']))
    cmd.append(str(params['is_libcom']))

    try:
        start_time = time.time()
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,
            cwd=BASE_DIR
        )
        execution_time = time.time() - start_time

        # Check for success
        if result.returncode == 0:
            return {
                'test_id': test_id,
                'use_case_id': use_case_id,
                'use_case': use_case['name'],
                'description': use_case['desc'],
                'scenario': use_case['scenario'],
                'host_core': use_case['host_core'],
                'device_core': use_case['device_core'],
                'host_cores': use_case['host_cores'],
                'device_cores': use_case['device_cores'],
                'total_cores': use_case['host_cores'] + use_case['device_cores'],
                'status': 'passed',
                'execution_time': execution_time,
                'config_file': str(yaml_config),
                'entry_point': 'pimid_binary'
            }
        else:
            return {
                'test_id': test_id,
                'use_case': use_case['name'],
                'status': 'failed',
                'error': result.stderr[:500] if result.stderr else result.stdout[:500] if result.stdout else 'Unknown error',
                'returncode': result.returncode,
            }

    except subprocess.TimeoutExpired:
        return {
            'test_id': test_id,
            'use_case': use_case['name'],
            'status': 'timeout',
            'error': 'Test exceeded 120 second timeout',
        }
    except Exception as e:
        return {
            'test_id': test_id,
            'use_case': use_case['name'],
            'status': 'error',
            'error': str(e),
        }

def main():
    """Main verification routine"""
    print("=" * 80)
    print("HOST/DEVICE CO-SIMULATION VERIFICATION")
    print("=" * 80)
    print(f"Start Time:     {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Entry Point:    {PIMID_BINARY} (ONLY)")
    print(f"Execution Model: ZSim (HETEROGENEOUS: Host + Device cores)")
    print("=" * 80)
    print()

    print("Testing 5 Host/Device Co-Simulation Use Cases:")
    print()
    for use_case_id, use_case in CO_SIM_USE_CASES.items():
        print(f"{use_case_id + 1}. {use_case['name']}")
        print(f"   Host:   {use_case['host_cores']} × {use_case['host_core']} core(s) [WITH cache]")
        print(f"   Device: {use_case['device_cores']} × {use_case['device_core']} core(s) [NO cache]")
        print(f"   Total:  {use_case['host_cores'] + use_case['device_cores']} cores (heterogeneous)")
        print(f"   Scenario: {use_case['scenario']}")
        print()

    # Verify pimid binary exists
    if not PIMID_BINARY.exists():
        print(f"ERROR: PIMID binary not found at {PIMID_BINARY}")
        return 1

    print("✓ PIMID binary found")
    print()

    # Run tests for all use cases
    print("Executing host/device co-simulation tests...")
    print("-" * 80)

    all_results = []
    passed = 0
    failed = 0

    start_time = time.time()

    for use_case_id in CO_SIM_USE_CASES.keys():
        use_case = CO_SIM_USE_CASES[use_case_id]
        print(f"Testing: {use_case['name'][:40]:40} ... ", end='', flush=True)

        # Run 2 tests per use case for reliability
        use_case_results = []
        use_case_passed = 0
        for test_num in range(2):
            result = run_single_test(test_num, use_case_id)
            use_case_results.append(result)
            if result['status'] == 'passed':
                use_case_passed += 1

        all_results.extend(use_case_results)

        if use_case_passed == 2:
            print(f"✓ PASSED (2/2)")
            passed += 2
        else:
            print(f"✗ FAILED ({use_case_passed}/2)")
            failed += (2 - use_case_passed)
            # Show first error
            for r in use_case_results:
                if r['status'] != 'passed':
                    print(f"  Error: {r.get('error', 'Unknown')[:80]}")
                    break

    total_time = time.time() - start_time
    total_tests = len(all_results)

    # Print summary
    print()
    print("=" * 80)
    print("HOST/DEVICE CO-SIMULATION SUMMARY")
    print("=" * 80)
    print(f"Use Cases Tested:   {len(CO_SIM_USE_CASES)}")
    print(f"Total Tests:        {total_tests}")
    print(f"Passed:             {passed} ({passed*100/total_tests:.1f}%)")
    print(f"Failed:             {failed} ({failed*100/total_tests:.1f}%)")
    print(f"Total Time:         {total_time:.1f} seconds")
    print(f"Entry Point:        pimid binary (verified)")
    print(f"Execution Model:    ZSim (heterogeneous host+device)")
    print("=" * 80)

    # Use case breakdown
    print()
    print("Use Case Results:")
    for use_case_id, use_case in CO_SIM_USE_CASES.items():
        use_case_name = use_case['name']
        use_case_tests = [r for r in all_results if r.get('use_case_id') == use_case_id]
        use_case_pass = sum(1 for r in use_case_tests if r['status'] == 'passed')
        status_icon = "✓" if use_case_pass == len(use_case_tests) else "✗"
        cores_info = f"{use_case['host_cores']}H+{use_case['device_cores']}D"
        print(f"  {status_icon} [{cores_info:7}] {use_case_name:45} - {use_case_pass}/{len(use_case_tests)} passed")

    # Save results
    results_file = RESULTS_DIR / 'host_device_cosim_results.json'
    with open(results_file, 'w') as f:
        json.dump({
            'summary': {
                'use_cases': len(CO_SIM_USE_CASES),
                'total_tests': total_tests,
                'passed': passed,
                'failed': failed,
                'pass_rate': passed * 100 / total_tests,
                'execution_time': total_time,
                'entry_point': str(PIMID_BINARY),
                'execution_model': 'zsim_heterogeneous_host_device',
                'timestamp': datetime.now().isoformat()
            },
            'use_cases': {
                use_case['name']: {
                    'description': use_case['desc'],
                    'scenario': use_case['scenario'],
                    'host_core': use_case['host_core'],
                    'device_core': use_case['device_core'],
                    'host_cores': use_case['host_cores'],
                    'device_cores': use_case['device_cores'],
                    'tests': [r for r in all_results if r.get('use_case_id') == use_case_id]
                }
                for use_case_id, use_case in CO_SIM_USE_CASES.items()
            }
        }, f, indent=2)

    print(f"\nResults saved to: {results_file}")

    # Return status
    if passed == total_tests:
        print(f"\n✓ ALL {len(CO_SIM_USE_CASES)} HOST/DEVICE CO-SIMULATION USE CASES VERIFIED!")
        return 0
    else:
        print(f"\n✗ {failed} TESTS FAILED - REVIEW REQUIRED")
        return 1

if __name__ == "__main__":
    exit(main())
