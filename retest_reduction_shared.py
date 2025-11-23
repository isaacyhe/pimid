#!/usr/bin/env python3
"""
Re-run failed reduction_shared tests to verify the fix
"""

import os
import subprocess
import json
import time
from datetime import datetime
from pathlib import Path

BASE_DIR = Path("/home/user/pimid-dev")
PIMID_BINARY = BASE_DIR / "build/pimid/pimid"
RESULTS_DIR = BASE_DIR / "test_results_zsim_1000"

# Load original results
with open(RESULTS_DIR / 'all_results_zsim_1000.json', 'r') as f:
    all_results = json.load(f)

# Find all failed reduction_shared tests
failed_tests = [r for r in all_results if r['workload'] == 'reduction_shared' and r['status'] == 'failed']

print("=" * 80)
print(f"RE-RUNNING {len(failed_tests)} FAILED reduction_shared TESTS")
print("=" * 80)
print()

passed = 0
failed = 0
start_time = time.time()

for i, test_result in enumerate(failed_tests):
    test_id = test_result['test_id']
    cmd = test_result['command'].split()

    print(f"[{i+1:3d}/{len(failed_tests)}] Test {test_id:04d}: {test_result['workload']}")
    print(f"  Command: {' '.join(cmd)}")

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=300,
            cwd=str(BASE_DIR)
        )

        if result.returncode == 0:
            passed += 1
            print(f"  ✓ NOW PASSED")
        else:
            failed += 1
            print(f"  ✗ STILL FAILED (code {result.returncode})")
            if result.stderr:
                print(f"  Error: {result.stderr[:200]}")
    except Exception as e:
        failed += 1
        print(f"  ✗ ERROR: {str(e)[:100]}")

print()
print("=" * 80)
print("RE-TEST SUMMARY")
print("=" * 80)
print(f"Total re-tested: {len(failed_tests)}")
print(f"Now passing:     {passed} ({passed/len(failed_tests)*100:.1f}%)")
print(f"Still failing:   {failed} ({failed/len(failed_tests)*100:.1f}%)")
print(f"Duration:        {time.time() - start_time:.1f} seconds")
print("=" * 80)

if passed == len(failed_tests):
    print("\n✅ SUCCESS! All previously failed tests now pass!")
    exit(0)
else:
    print(f"\n⚠️ {failed} tests still failing")
    exit(1)
