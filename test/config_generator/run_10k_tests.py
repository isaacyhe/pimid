#!/usr/bin/env python3
"""
Test runner for 10,000 configuration-based topology tests

This script runs all generated test configurations and reports:
- Success/failure counts
- Performance statistics
- Error analysis
- Coverage metrics
"""

import os
import sys
import subprocess
import yaml
import time
import json
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor, as_completed
import argparse

class TestRunner:
    def __init__(self, config_dir, pimid_binary, num_workers=4, timeout=30):
        self.config_dir = Path(config_dir)
        self.pimid_binary = pimid_binary
        self.num_workers = num_workers
        self.timeout = timeout
        self.results = []

    def run_single_test(self, test_info):
        """Run a single test configuration"""
        test_id = test_info["id"]
        test_name = test_info["name"]
        config_file = self.config_dir / test_info["file"]

        result = {
            "id": test_id,
            "name": test_name,
            "file": str(config_file),
            "category": test_info.get("category", "unknown"),
            "dram_type": test_info.get("dram_type", "unknown"),
            "status": "unknown",
            "duration": 0.0,
            "error": None
        }

        if not config_file.exists():
            result["status"] = "skip"
            result["error"] = "Config file not found"
            return result

        try:
            start_time = time.time()

            # Run pimid binary with config
            cmd = [self.pimid_binary, "--config", str(config_file)]

            process = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout
            )

            duration = time.time() - start_time
            result["duration"] = duration

            if process.returncode == 0:
                result["status"] = "pass"
            else:
                result["status"] = "fail"
                result["error"] = f"Exit code {process.returncode}"
                result["stderr"] = process.stderr[-500:]  # Last 500 chars

        except subprocess.TimeoutExpired:
            result["status"] = "timeout"
            result["error"] = f"Timeout after {self.timeout}s"

        except Exception as e:
            result["status"] = "error"
            result["error"] = str(e)

        return result

    def run_all_tests(self, manifest_file, start_idx=0, end_idx=None, verbose=False):
        """Run all tests from manifest"""

        # Load manifest
        with open(manifest_file, 'r') as f:
            manifest = yaml.safe_load(f)

        tests = manifest["tests"]
        total_tests = len(tests)

        if end_idx is None:
            end_idx = total_tests

        tests_to_run = tests[start_idx:end_idx]

        print(f"Running tests {start_idx} to {end_idx} ({len(tests_to_run)} tests)")
        print(f"Workers: {self.num_workers}")
        print(f"Timeout: {self.timeout}s per test")
        print("=" * 80)

        # Run tests in parallel
        start_time = time.time()
        completed = 0

        with ProcessPoolExecutor(max_workers=self.num_workers) as executor:
            # Submit all tests
            futures = {executor.submit(self.run_single_test, test): test
                      for test in tests_to_run}

            # Process results as they complete
            for future in as_completed(futures):
                result = future.result()
                self.results.append(result)
                completed += 1

                if verbose or result["status"] != "pass":
                    status_symbol = {
                        "pass": "✓",
                        "fail": "✗",
                        "error": "⚠",
                        "timeout": "⏱",
                        "skip": "⊘"
                    }.get(result["status"], "?")

                    print(f"[{completed:5d}/{len(tests_to_run)}] {status_symbol} {result['name']} "
                          f"({result['status']}) - {result['duration']:.2f}s")

                    if result["error"] and verbose:
                        print(f"           Error: {result['error']}")

                elif completed % 100 == 0:
                    print(f"[{completed:5d}/{len(tests_to_run)}] Progress...")

        total_duration = time.time() - start_time

        print("=" * 80)
        print(f"Completed {len(tests_to_run)} tests in {total_duration:.2f}s")

        return self.results

    def generate_report(self, output_file=None):
        """Generate test report"""

        # Calculate statistics
        total = len(self.results)
        passed = sum(1 for r in self.results if r["status"] == "pass")
        failed = sum(1 for r in self.results if r["status"] == "fail")
        errors = sum(1 for r in self.results if r["status"] == "error")
        timeouts = sum(1 for r in self.results if r["status"] == "timeout")
        skipped = sum(1 for r in self.results if r["status"] == "skip")

        total_duration = sum(r["duration"] for r in self.results)
        avg_duration = total_duration / total if total > 0 else 0

        # Category breakdown
        category_stats = {}
        for result in self.results:
            cat = result["category"]
            if cat not in category_stats:
                category_stats[cat] = {"total": 0, "passed": 0, "failed": 0}
            category_stats[cat]["total"] += 1
            if result["status"] == "pass":
                category_stats[cat]["passed"] += 1
            else:
                category_stats[cat]["failed"] += 1

        # DRAM type breakdown
        dram_stats = {}
        for result in self.results:
            dram = result["dram_type"]
            if dram not in dram_stats:
                dram_stats[dram] = {"total": 0, "passed": 0, "failed": 0}
            dram_stats[dram]["total"] += 1
            if result["status"] == "pass":
                dram_stats[dram]["passed"] += 1
            else:
                dram_stats[dram]["failed"] += 1

        # Print report
        print("\n" + "=" * 80)
        print("TEST REPORT")
        print("=" * 80)
        print(f"\nOverall Results:")
        print(f"  Total tests:    {total}")
        print(f"  Passed:         {passed} ({100*passed/total:.1f}%)")
        print(f"  Failed:         {failed} ({100*failed/total:.1f}%)")
        print(f"  Errors:         {errors} ({100*errors/total:.1f}%)")
        print(f"  Timeouts:       {timeouts} ({100*timeouts/total:.1f}%)")
        print(f"  Skipped:        {skipped} ({100*skipped/total:.1f}%)")
        print(f"\nPerformance:")
        print(f"  Total duration: {total_duration:.2f}s")
        print(f"  Avg per test:   {avg_duration:.3f}s")

        print(f"\nResults by Category:")
        for cat, stats in sorted(category_stats.items()):
            pass_rate = 100 * stats["passed"] / stats["total"] if stats["total"] > 0 else 0
            print(f"  {cat:20s}: {stats['passed']:4d}/{stats['total']:4d} passed ({pass_rate:.1f}%)")

        print(f"\nResults by DRAM Type:")
        for dram, stats in sorted(dram_stats.items()):
            pass_rate = 100 * stats["passed"] / stats["total"] if stats["total"] > 0 else 0
            print(f"  {dram:10s}: {stats['passed']:4d}/{stats['total']:4d} passed ({pass_rate:.1f}%)")

        # Failed tests detail
        failed_tests = [r for r in self.results if r["status"] in ["fail", "error"]]
        if failed_tests:
            print(f"\nFailed Tests ({len(failed_tests)}):")
            for result in failed_tests[:20]:  # Show first 20
                print(f"  - {result['name']}: {result['error']}")
            if len(failed_tests) > 20:
                print(f"  ... and {len(failed_tests) - 20} more")

        print("=" * 80)

        # Write JSON report
        if output_file:
            report_data = {
                "summary": {
                    "total": total,
                    "passed": passed,
                    "failed": failed,
                    "errors": errors,
                    "timeouts": timeouts,
                    "skipped": skipped,
                    "pass_rate": 100 * passed / total if total > 0 else 0,
                    "total_duration": total_duration,
                    "avg_duration": avg_duration
                },
                "by_category": category_stats,
                "by_dram_type": dram_stats,
                "results": self.results
            }

            with open(output_file, 'w') as f:
                json.dump(report_data, f, indent=2)

            print(f"\n✓ Detailed report written to {output_file}")

def main():
    parser = argparse.ArgumentParser(description="Run 10,000 topology test configurations")
    parser.add_argument("--config-dir", default="test/configs/10k_topology_tests",
                       help="Directory containing test configs")
    parser.add_argument("--pimid", default="./build/pimid/pimid",
                       help="Path to pimid binary")
    parser.add_argument("--workers", type=int, default=4,
                       help="Number of parallel workers")
    parser.add_argument("--timeout", type=int, default=30,
                       help="Timeout per test (seconds)")
    parser.add_argument("--start", type=int, default=0,
                       help="Start test index")
    parser.add_argument("--end", type=int, default=None,
                       help="End test index (exclusive)")
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="Verbose output")
    parser.add_argument("--report", default="test_report.json",
                       help="Output report file")

    args = parser.parse_args()

    # Check if pimid binary exists
    if not Path(args.pimid).exists():
        print(f"Error: pimid binary not found at {args.pimid}")
        print("Please build pimid first or specify correct path with --pimid")
        return 1

    # Check if config directory exists
    config_dir = Path(args.config_dir)
    if not config_dir.exists():
        print(f"Error: Config directory not found: {config_dir}")
        print("Please run generate_10k_tests.py first")
        return 1

    manifest_file = config_dir / "test_manifest.yaml"
    if not manifest_file.exists():
        print(f"Error: Test manifest not found: {manifest_file}")
        return 1

    # Run tests
    runner = TestRunner(
        config_dir=config_dir,
        pimid_binary=args.pimid,
        num_workers=args.workers,
        timeout=args.timeout
    )

    results = runner.run_all_tests(
        manifest_file=manifest_file,
        start_idx=args.start,
        end_idx=args.end,
        verbose=args.verbose
    )

    # Generate report
    runner.generate_report(output_file=args.report)

    # Return exit code based on results
    failed_count = sum(1 for r in results if r["status"] != "pass")
    return 0 if failed_count == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
