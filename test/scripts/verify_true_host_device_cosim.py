#!/usr/bin/env python3
"""
TRUE Host/Device Co-Simulation Verification Suite
Tests 5 workloads where BOTH host and device do meaningful work
Entry Point: pimid binary with heterogeneous ZSim configs
"""

import os
import subprocess
import json
import time
from datetime import datetime
from pathlib import Path

# Base directories
BASE_DIR = Path("/home/user/pimid-dev")
COSIM_WORKLOAD_DIR = BASE_DIR / "test/benchmarks/host_device_cosim"
RESULTS_DIR = BASE_DIR / "test/results/verify_true_host_device_cosim"
CONFIG_DIR = RESULTS_DIR / "configs"
PIMID_BINARY = BASE_DIR / "build/pimid/pimid"

# Create directories
RESULTS_DIR.mkdir(exist_ok=True)
CONFIG_DIR.mkdir(exist_ok=True)

# TRUE Host/Device Co-Simulation Workloads
COSIM_WORKLOADS = {
    'vector_add': {
        'name': 'Vector Addition Co-Simulation',
        'binary': 'vector_add_cosim',
        'params': [1000, 8],  # <array_size> <num_device_pes>
        'host_work': 'Data allocation/initialization, coordination, verification',
        'device_work': 'Parallel vector addition computation',
        'communication': 'Shared memory arrays A, B, C',
    },
    'matmul_tiled': {
        'name': 'Tiled Matrix Multiplication Co-Simulation',
        'binary': 'matmul_tiled_cosim',
        'params': [64, 8, 8],  # <matrix_size> <tile_size> <num_device_pes>
        'host_work': 'Matrix tiling, tile distribution, result assembly, verification',
        'device_work': 'Parallel tile-by-tile matrix multiplication',
        'communication': 'Shared memory matrices A, B, C and tile metadata',
    },
    'histogram_merge': {
        'name': 'Histogram with Merging Co-Simulation',
        'binary': 'histogram_merge_cosim',
        'params': [2000, 8],  # <data_size> <num_device_pes>
        'host_work': 'Data generation, local histogram merging, statistics',
        'device_work': 'Parallel local histogram computation per PE',
        'communication': 'Shared input data and local histogram arrays',
    },
    'reduction_tree': {
        'name': 'Hierarchical Reduction Co-Simulation',
        'binary': 'reduction_tree_cosim',
        'params': [1024, 8, 0],  # <array_size> <num_device_pes> <operation> (0=SUM)
        'host_work': 'Data generation, final reduction, correctness verification',
        'device_work': 'Parallel local reduction per PE',
        'communication': 'Shared input array and partial results',
    },
    'bfs_iterative': {
        'name': 'Iterative BFS Co-Simulation',
        'binary': 'bfs_iterative_cosim',
        'params': [100, 0, 8],  # <num_vertices> <source_vertex> <num_device_pes>
        'host_work': 'Graph generation, frontier management, convergence checking, results display',
        'device_work': 'Parallel neighbor exploration for frontier nodes',
        'communication': 'Shared graph structure, frontier queues, BFS state',
    },
}

# Heterogeneous core configurations
# Host: OOO/Simple WITH cache (general-purpose processing)
# Device: ALU WITHOUT cache (PIM compute-intensive processing)
HETERO_CONFIGS = [
    {
        'id': 0,
        'name': 'OOO_host_ALU_device',
        'host_core': {'type': 'OOO', 'cores': 1, 'has_cache': True},
        'device_core': {'type': 'ALU', 'cores': 8, 'has_cache': False},
        'desc': 'OOO host (with cache) + ALU device (cacheless)',
    },
    {
        'id': 1,
        'name': 'Simple_host_ALU_device',
        'host_core': {'type': 'Simple', 'cores': 1, 'has_cache': True},
        'device_core': {'type': 'ALU', 'cores': 8, 'has_cache': False},
        'desc': 'Simple host (with cache) + ALU device (cacheless)',
    },
]

def generate_heterogeneous_zsim_config(workload_key, config_id, test_id):
    """Generate heterogeneous ZSim config with host + device cores"""
    config = HETERO_CONFIGS[config_id]
    workload = COSIM_WORKLOADS[workload_key]

    host_core = config['host_core']
    device_core = config['device_core']

    zsim_config = f"""// Auto-generated heterogeneous ZSim config for TRUE host/device co-simulation
// Workload: {workload['name']}
// Configuration: {config['desc']}

sys = {{
    frequency = 2000;  // 2 GHz

    // HETEROGENEOUS CORES: Host + Device under same ZSim instance
    cores = {{
        // HOST CORE(S): {host_core['type']} with cache (general-purpose processing)
        host = {{
            type = "{host_core['type']}";
            cores = {host_core['cores']};
"""

    if host_core['has_cache']:
        zsim_config += """            icache = "l1i_host";
            dcache = "l1d_host";
"""

    zsim_config += """        };

        // DEVICE CORE(S): ALU WITHOUT cache (PIM processing elements)
        device = {{
            type = "ALU";
            cores = {device_core['cores']};
            aluLatency = 1;  // 1 cycle per ALU operation
            // NOTE: ALU cores are CACHELESS - no icache/dcache needed!
        }};
    }};
"""

    # Add cache hierarchy only for host cores
    if host_core['has_cache']:
        zsim_config += """
    // CACHE HIERARCHY (for host cores only, device cores are cacheless)
    caches = {
        l1d_host = {
            caches = 1;  // One per host core
            size = 32768;  // 32 KB
            array = {
                type = "SetAssoc";
                ways = 8;
            };
            latency = 2;
        };

        l1i_host = {
            caches = 1;  // One per host core
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
"""

    zsim_config += """
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

    zsim_file = CONFIG_DIR / f"zsim_cosim_{config['name']}_{workload_key}_{test_id:04d}.cfg"
    with open(zsim_file, 'w') as f:
        f.write(zsim_config)

    return zsim_file

def generate_yaml_config(workload_key, config_id, test_id):
    """Generate YAML config for pimid binary"""
    config = HETERO_CONFIGS[config_id]
    workload = COSIM_WORKLOADS[workload_key]
    workload_binary = COSIM_WORKLOAD_DIR / workload['binary']

    # Build workload args
    params = workload['params']
    workload_args = ' '.join(str(v) for v in params)

    config_content = f"""# Auto-generated config for TRUE host/device co-simulation test {test_id}
# Workload: {workload['name']}
# Config: {config['desc']}
simulation:
  name: "TrueCosim_{workload_key}_{config['name']}_{test_id:04d}"
  mode: "standalone"

execution_model:
  type: "zsim"
  config_file: "{CONFIG_DIR}/zsim_cosim_{config['name']}_{workload_key}_{test_id:04d}.cfg"
  heterogeneous: true
  host_core_type: "{config['host_core']['type']}"
  device_core_type: "{config['device_core']['type']}"

memory:
  technology: "SRAM"

pim:
  granularity: BANK
  num_pes: {params[-1] if isinstance(params, list) else params.get('num_device_pes', 8)}

workload:
  binary: "{workload_binary}"
  args: "{workload_args}"
  type: "host_device_cosim"
  host_work: "{workload['host_work']}"
  device_work: "{workload['device_work']}"

network:
  topology: "H_TREE"
  model: "GARNET"
"""

    config_file = CONFIG_DIR / f"test_cosim_{config['name']}_{workload_key}_{test_id:04d}.yaml"
    with open(config_file, 'w') as f:
        f.write(config_content)

    return config_file

def run_cosim_test(workload_key, config_id, test_id):
    """Run a single TRUE co-simulation test"""
    workload = COSIM_WORKLOADS[workload_key]
    config = HETERO_CONFIGS[config_id]

    # Generate config files
    yaml_config = generate_yaml_config(workload_key, config_id, test_id)
    zsim_config = generate_heterogeneous_zsim_config(workload_key, config_id, test_id)

    workload_binary = COSIM_WORKLOAD_DIR / workload['binary']

    # Verify workload binary exists
    if not workload_binary.exists():
        return {
            'test_id': test_id,
            'workload': workload['name'],
            'config': config['name'],
            'status': 'failed',
            'error': f'Workload binary not found: {workload_binary}',
        }

    # Execute using pimid binary
    cmd = [
        str(PIMID_BINARY),
        "--mode", "standalone",
        "--config", str(yaml_config),
        "--workload", str(workload_binary)
    ]

    # Add workload parameters
    params = workload['params']
    cmd.extend([str(v) for v in params])

    try:
        start_time = time.time()
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,  # 2 minutes timeout for co-simulation
            cwd=BASE_DIR
        )
        execution_time = time.time() - start_time

        # Check for success
        if result.returncode == 0:
            # Analyze output for host/device activity
            stdout = result.stdout
            has_host_activity = '[HOST]' in stdout
            has_device_activity = '[DEVICE' in stdout

            return {
                'test_id': test_id,
                'workload': workload['name'],
                'workload_key': workload_key,
                'config': config['name'],
                'config_desc': config['desc'],
                'status': 'passed',
                'execution_time': execution_time,
                'host_work': workload['host_work'],
                'device_work': workload['device_work'],
                'communication': workload['communication'],
                'has_host_activity': has_host_activity,
                'has_device_activity': has_device_activity,
                'true_cosim': has_host_activity and has_device_activity,
                'config_file': str(yaml_config),
                'entry_point': 'pimid_binary'
            }
        else:
            return {
                'test_id': test_id,
                'workload': workload['name'],
                'config': config['name'],
                'status': 'failed',
                'error': result.stderr[:500] if result.stderr else result.stdout[:500] if result.stdout else 'Unknown error',
                'returncode': result.returncode,
                'entry_point': 'pimid_binary'
            }

    except subprocess.TimeoutExpired:
        return {
            'test_id': test_id,
            'workload': workload['name'],
            'config': config['name'],
            'status': 'timeout',
            'error': 'Test exceeded 120 second timeout',
        }
    except Exception as e:
        return {
            'test_id': test_id,
            'workload': workload['name'],
            'config': config['name'],
            'status': 'error',
            'error': str(e),
        }

def main():
    """Main verification routine"""
    print("=" * 80)
    print("TRUE HOST/DEVICE CO-SIMULATION VERIFICATION")
    print("=" * 80)
    print(f"Start Time:     {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Entry Point:    {PIMID_BINARY}")
    print(f"Workloads:      {len(COSIM_WORKLOADS)} TRUE co-simulation workloads")
    print(f"Configurations: {len(HETERO_CONFIGS)} heterogeneous configs")
    print("=" * 80)
    print()

    print("Co-Simulation Workloads:")
    for key, workload in COSIM_WORKLOADS.items():
        print(f"  • {workload['name']}")
        print(f"    Host:   {workload['host_work']}")
        print(f"    Device: {workload['device_work']}")
        print()

    print("Heterogeneous Configurations:")
    for config in HETERO_CONFIGS:
        print(f"  {config['id'] + 1}. {config['desc']}")
    print()

    # Verify pimid binary exists
    if not PIMID_BINARY.exists():
        print(f"ERROR: PIMID binary not found at {PIMID_BINARY}")
        return 1

    print("✓ PIMID binary found")

    # Verify all workload binaries exist
    missing_binaries = []
    for key, workload in COSIM_WORKLOADS.items():
        workload_binary = COSIM_WORKLOAD_DIR / workload['binary']
        if not workload_binary.exists():
            missing_binaries.append(workload_binary)
        else:
            print(f"✓ {workload['binary']}")

    if missing_binaries:
        print("\nERROR: Missing workload binaries:")
        for binary in missing_binaries:
            print(f"  ✗ {binary}")
        return 1

    print()
    print("Executing TRUE co-simulation tests...")
    print("-" * 80)

    all_results = []
    passed = 0
    failed = 0

    start_time = time.time()
    test_id = 0

    # Run tests for each workload × configuration
    for workload_key, workload in COSIM_WORKLOADS.items():
        print(f"\nTesting: {workload['name']}")

        for config in HETERO_CONFIGS:
            print(f"  Config: {config['name']:25} ", end='', flush=True)

            result = run_cosim_test(workload_key, config['id'], test_id)
            all_results.append(result)
            test_id += 1

            if result['status'] == 'passed':
                true_cosim_marker = "✓✓" if result.get('true_cosim', False) else "✓"
                print(f"{true_cosim_marker} PASSED ({result['execution_time']:.2f}s)")
                passed += 1
            else:
                print(f"✗ FAILED")
                failed += 1
                if 'error' in result:
                    print(f"    Error: {result['error'][:100]}")

    total_time = time.time() - start_time
    total_tests = len(all_results)

    # Count TRUE co-simulations
    true_cosims = sum(1 for r in all_results if r.get('true_cosim', False))

    # Print summary
    print()
    print("=" * 80)
    print("TRUE HOST/DEVICE CO-SIMULATION SUMMARY")
    print("=" * 80)
    print(f"Workloads:          {len(COSIM_WORKLOADS)}")
    print(f"Configurations:     {len(HETERO_CONFIGS)}")
    print(f"Total Tests:        {total_tests}")
    print(f"Passed:             {passed} ({passed*100/total_tests:.1f}%)")
    print(f"Failed:             {failed} ({failed*100/total_tests:.1f}%)")
    print(f"TRUE Co-Simulations: {true_cosims}/{total_tests} ({true_cosims*100/total_tests:.1f}%)")
    print(f"Total Time:         {total_time:.1f} seconds")
    print(f"Entry Point:        pimid binary (verified)")
    print("=" * 80)

    # Workload breakdown
    print()
    print("Workload Results:")
    for workload_key, workload in COSIM_WORKLOADS.items():
        workload_tests = [r for r in all_results if r.get('workload_key') == workload_key]
        workload_pass = sum(1 for r in workload_tests if r['status'] == 'passed')
        status_icon = "✓" if workload_pass == len(workload_tests) else "✗"
        print(f"  {status_icon} {workload['name']:45} - {workload_pass}/{len(workload_tests)} passed")

    # Save results
    results_file = RESULTS_DIR / 'true_cosim_results.json'
    with open(results_file, 'w') as f:
        json.dump({
            'summary': {
                'workloads': len(COSIM_WORKLOADS),
                'configurations': len(HETERO_CONFIGS),
                'total_tests': total_tests,
                'passed': passed,
                'failed': failed,
                'true_cosimulations': true_cosims,
                'pass_rate': passed * 100 / total_tests,
                'true_cosim_rate': true_cosims * 100 / total_tests,
                'execution_time': total_time,
                'entry_point': str(PIMID_BINARY),
                'timestamp': datetime.now().isoformat()
            },
            'workloads': {
                key: {
                    'name': info['name'],
                    'host_work': info['host_work'],
                    'device_work': info['device_work'],
                    'communication': info['communication'],
                    'tests': [r for r in all_results if r.get('workload_key') == key]
                }
                for key, info in COSIM_WORKLOADS.items()
            },
            'configurations': HETERO_CONFIGS,
        }, f, indent=2)

    print(f"\nResults saved to: {results_file}")

    # Return status
    if passed == total_tests and true_cosims == total_tests:
        print(f"\n✓✓ ALL {total_tests} TESTS PASSED - TRUE HOST/DEVICE CO-SIMULATION VERIFIED!")
        return 0
    elif passed == total_tests:
        print(f"\n⚠ All tests passed but some may not have TRUE co-simulation")
        return 0
    else:
        print(f"\n✗ {failed} TESTS FAILED")
        return 1

if __name__ == "__main__":
    exit(main())
