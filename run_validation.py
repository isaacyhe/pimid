#!/usr/bin/env python3
"""Simple validation script for PIMID analytical model."""

import os
import subprocess
import sys
from pathlib import Path
import re

def main():
    pimid_sim = Path("/home/user/pimid-dev/pimid/build/pimid_sim")

    # Multiple config directories to test
    config_dirs = [
        Path("/home/user/pimid-dev/pimid/configs/benchmarks"),
        Path("/home/user/pimid-dev/pimid/configs/comprehensive_tests/quick"),
        Path("/home/user/pimid-dev/pimid/configs/memory"),
        Path("/home/user/pimid-dev/pimid/configs/dram"),
    ]

    all_configs = []
    for configs_dir in config_dirs:
        if configs_dir.exists():
            all_configs.extend(list(configs_dir.glob("*.yaml")))

    configs = sorted(set(all_configs))

    if not pimid_sim.exists():
        print(f"Error: pimid_sim not found at {pimid_sim}")
        sys.exit(1)
    print(f"Found {len(configs)} benchmark configs")
    print("=" * 60)

    passed = 0
    failed = 0
    results = []

    for i, config in enumerate(sorted(configs), 1):
        config_name = config.name
        print(f"[{i}/{len(configs)}] {config_name}...", end=" ")
        sys.stdout.flush()

        try:
            result = subprocess.run(
                [str(pimid_sim), str(config)],
                capture_output=True,
                text=True,
                timeout=60
            )

            if result.returncode == 0:
                # Parse output for latency
                output = result.stdout
                match = re.search(r'TOTAL:\s+([\d.]+)\s+us', output)
                if match:
                    latency = float(match.group(1))
                    print(f"PASS (latency: {latency:.2f} us)")
                    results.append((config_name, latency, "PASS"))
                    passed += 1
                else:
                    print("PASS (no latency found)")
                    results.append((config_name, 0, "PASS"))
                    passed += 1
            else:
                print(f"FAIL (exit code {result.returncode})")
                if result.stderr:
                    print(f"    Error: {result.stderr.strip()[:100]}")
                results.append((config_name, 0, "FAIL"))
                failed += 1
        except subprocess.TimeoutExpired:
            print("FAIL (timeout)")
            results.append((config_name, 0, "TIMEOUT"))
            failed += 1
        except Exception as e:
            print(f"FAIL ({e})")
            results.append((config_name, 0, str(e)))
            failed += 1

    print()
    print("=" * 60)
    print(f"VALIDATION SUMMARY")
    print("=" * 60)
    print(f"Total tests: {len(configs)}")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    print(f"Pass rate: {100*passed/len(configs):.1f}%")

    if results:
        print()
        print("Latency Results (sorted by latency):")
        print("-" * 60)
        sorted_results = sorted([r for r in results if r[1] > 0], key=lambda x: x[1])
        for name, latency, status in sorted_results[:10]:
            print(f"  {name:45s} {latency:12.2f} us")
        if len(sorted_results) > 10:
            print(f"  ... and {len(sorted_results)-10} more")

    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
