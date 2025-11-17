#!/usr/bin/env python3
"""
Run focused tests on extended PIMID features:
- OOO (Out-of-Order) core PE type
- New placement levels: BG (bank group), chip, MC (memory controller)
"""

import subprocess
import sys
from pathlib import Path

# Test configs to run - focused on new features
TEST_CONFIGS = [
    # OOO core with different memory techs
    "bfs_bank_ooo_sram_1k_deg16.yaml",
    "bfs_bank_ooo_dram_1k_deg16.yaml",
    "bfs_bank_ooo_reram_1k_deg16.yaml",

    # BG-level placement (bank group)
    "bfs_bg_inorder_sram_1k_deg16.yaml",
    "bfs_bg_simple_alu_dram_1k_deg16.yaml",
    "bfs_bg_ooo_reram_1k_deg16.yaml",

    # Chip-level placement
    "bfs_chip_inorder_sram_1k_deg16.yaml",
    "bfs_chip_simple_alu_dram_1k_deg16.yaml",
    "bfs_chip_ooo_reram_1k_deg16.yaml",

    # MC-level placement (memory controller)
    "bfs_mc_inorder_sram_1k_deg16.yaml",
    "bfs_mc_simple_alu_dram_1k_deg16.yaml",
    "bfs_mc_ooo_reram_1k_deg16.yaml",
]

def run_test(config_path, benchmark_runner):
    """Run a single benchmark test."""
    print(f"\n{'='*80}")
    print(f"Testing: {config_path.name}")
    print(f"{'='*80}")

    try:
        result = subprocess.run(
            [benchmark_runner, '--config', str(config_path)],
            capture_output=True,
            text=True,
            timeout=60
        )

        if result.returncode == 0:
            # Extract latency from output
            for line in result.stdout.split('\n'):
                if 'Total latency:' in line:
                    print(f"✓ SUCCESS: {line.strip()}")
                    return True
            print("✓ Test completed (no latency reported)")
            return True
        else:
            print(f"✗ FAILED: {result.stderr}")
            return False

    except subprocess.TimeoutExpired:
        print("✗ TIMEOUT")
        return False
    except Exception as e:
        print(f"✗ ERROR: {e}")
        return False

def main():
    # Setup paths
    pimid_root = Path(__file__).parent.parent
    config_dir = pimid_root / "configs" / "comprehensive_tests_extended" / "comprehensive"
    benchmark_runner = pimid_root / "build" / "benchmarks" / "benchmark_runner"

    if not benchmark_runner.exists():
        print(f"Error: benchmark_runner not found at {benchmark_runner}")
        print("Please build it first: cd build && make benchmark_runner")
        return 1

    if not config_dir.exists():
        print(f"Error: config directory not found at {config_dir}")
        return 1

    print("="*80)
    print("PIMID EXTENDED FEATURES TEST")
    print("="*80)
    print(f"Testing {len(TEST_CONFIGS)} configurations")
    print(f"Config directory: {config_dir}")
    print(f"Benchmark runner: {benchmark_runner}")

    passed = 0
    failed = 0

    for config_name in TEST_CONFIGS:
        config_path = config_dir / config_name
        if not config_path.exists():
            print(f"\n✗ Config not found: {config_name}")
            failed += 1
            continue

        if run_test(config_path, benchmark_runner):
            passed += 1
        else:
            failed += 1

    # Summary
    print(f"\n{'='*80}")
    print("TEST SUMMARY")
    print(f"{'='*80}")
    print(f"Total tests: {len(TEST_CONFIGS)}")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    print(f"Success rate: {100*passed/len(TEST_CONFIGS):.1f}%")

    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
