#!/usr/bin/env python3
"""
Quick Execution Model Test
Fast verification test with minimal configurations for rapid feedback.
Tests both ZSim and Event-Driven models with small workloads.

Usage:
    ./test_quick_execution_models.py
"""

import os
import sys
import yaml
import subprocess
from pathlib import Path

def create_test_config(name: str, exec_model: str, num_pes: int, workload: str, params: dict) -> Path:
    """Create a test configuration file"""
    config = {
        "simulation": {
            "name": name,
            "mode": "standalone",
            "host_execution_model": "event_driven",
            "device_execution_model": exec_model,
        },
        "memory": {
            "technology": "DRAM",
            "dram_type": "DDR4",
            "channels": 1,
            "ranks_per_channel": 1,
            "banks_per_rank": 16,
        },
        "pim": {
            "enabled": True,
            "num_pes": num_pes,
            "pe_placement": "bank",
        },
        "processing_element": {
            "type": "Simple" if exec_model == "zsim" else "analytical",
            "frequency_mhz": 1000.0,
        },
        "workload": {
            "name": workload,
            "params": params,
        }
    }

    if exec_model == "event_driven":
        config["event_driven"] = {
            "performance_model": "roofline",
            "device": {
                "num_cores": num_pes,
                "frequency_mhz": 1000.0,
                "ipc": 1.0,
            }
        }

    config_file = Path(f"test_configs/{name}.yaml")
    config_file.parent.mkdir(exist_ok=True)

    with open(config_file, 'w') as f:
        yaml.dump(config, f)

    return config_file

def run_quick_tests():
    """Run quick verification tests"""

    print("="*80)
    print("QUICK EXECUTION MODEL VERIFICATION TEST")
    print("="*80)
    print("\nTesting both ZSim and Event-Driven execution models")
    print("with small workloads for rapid feedback...\n")

    # Test configurations: (name, exec_model, num_pes, workload, params)
    # Realistic PE counts: 16 (bank-level), 64, 256, 1024 (subarray-level)
    tests = [
        # 16 PEs (bank-level: 1 PE per bank, 16 banks)
        ("quick_16pe_zsim_bfs", "zsim", 16, "bfs", {"num_vertices": 64, "avg_degree": 4}),
        ("quick_16pe_event_bfs", "event_driven", 16, "bfs", {"num_vertices": 64, "avg_degree": 4}),

        ("quick_16pe_zsim_gemm", "zsim", 16, "gemm", {"matrix_size": 32}),
        ("quick_16pe_event_gemm", "event_driven", 16, "gemm", {"matrix_size": 32}),

        # 256 PEs (subarray-level: 16 banks × 16 subarrays)
        ("quick_256pe_zsim_dotprod", "zsim", 256, "dot_product", {"vector_length": 256}),
        ("quick_256pe_event_dotprod", "event_driven", 256, "dot_product", {"vector_length": 256}),

        # 1024 PEs (subarray-level: 16 banks × 64 subarrays)
        ("quick_1024pe_event_reduction", "event_driven", 1024, "reduction", {"elements_per_subarray": 256}),
    ]

    results = []
    pimid_binary = Path("/home/user/pimid-dev/build/pimid/pimid")

    for test_name, exec_model, num_pes, workload, params in tests:
        print(f"\n{'─'*80}")
        print(f"Test: {test_name}")
        print(f"  Model: {exec_model}, PEs: {num_pes:,}, Workload: {workload}")
        print(f"{'─'*80}")

        # Create config
        config_file = create_test_config(test_name, exec_model, num_pes, workload, params)

        # Run test
        try:
            cmd = [str(pimid_binary), "--config", str(config_file)]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)

            if result.returncode == 0:
                print(f"✓ PASSED")
                results.append((test_name, "PASS"))
            else:
                print(f"✗ FAILED: {result.stderr[:200]}")
                results.append((test_name, "FAIL"))
        except subprocess.TimeoutExpired:
            print(f"✗ TIMEOUT (>60s)")
            results.append((test_name, "TIMEOUT"))
        except Exception as e:
            print(f"✗ ERROR: {str(e)}")
            results.append((test_name, "ERROR"))

    # Summary
    print(f"\n{'='*80}")
    print("QUICK TEST SUMMARY")
    print(f"{'='*80}\n")

    passed = sum(1 for _, status in results if status == "PASS")
    total = len(results)

    print(f"Total: {total}, Passed: {passed}, Failed: {total - passed}")
    print(f"Success Rate: {100*passed/total:.1f}%\n")

    for name, status in results:
        symbol = "✓" if status == "PASS" else "✗"
        print(f"{symbol} {name}: {status}")

    print()
    return passed == total

if __name__ == "__main__":
    success = run_quick_tests()
    sys.exit(0 if success else 1)
