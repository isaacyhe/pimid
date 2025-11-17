#!/usr/bin/env python3
"""
Comprehensive test runner for PIMID benchmarks.
Runs all benchmark configs and aggregates results for analysis.
"""

import os
import sys
import subprocess
import json
import csv
import time
from pathlib import Path
from datetime import datetime
import argparse


class BenchmarkResults:
    """Stores and manages benchmark results."""

    def __init__(self):
        self.results = []

    def add_result(self, config_name, result_data):
        """Add a benchmark result."""
        self.results.append({
            'config': config_name,
            'timestamp': datetime.now().isoformat(),
            **result_data
        })

    def save_json(self, filepath):
        """Save results as JSON."""
        with open(filepath, 'w') as f:
            json.dump(self.results, f, indent=2)

    def save_csv(self, filepath):
        """Save results as CSV."""
        if not self.results:
            return

        keys = self.results[0].keys()
        with open(filepath, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=keys)
            writer.writeheader()
            writer.writerows(self.results)

    def print_summary(self):
        """Print summary statistics."""
        if not self.results:
            print("No results to summarize")
            return

        print("\n" + "="*80)
        print("BENCHMARK RESULTS SUMMARY")
        print("="*80)
        print(f"Total benchmarks run: {len(self.results)}")

        # Group by dimensions
        by_memory = {}
        by_pe = {}
        by_placement = {}
        by_size = {}

        for result in self.results:
            # Extract metadata from config name
            parts = result['config'].split('_')

            # Memory tech
            for tech in ['dram', 'sram', 'sttmram', 'pcm', 'reram']:
                if tech in result['config']:
                    by_memory.setdefault(tech, []).append(result.get('latency_ms', 0))
                    break

            # PE type
            if 'inorder' in result['config']:
                by_pe.setdefault('inorder', []).append(result.get('latency_ms', 0))
            elif 'simple' in result['config'] or 'alu' in result['config']:
                by_pe.setdefault('simple_alu', []).append(result.get('latency_ms', 0))

            # Placement
            if 'subarray_' in result['config']:
                by_placement.setdefault('subarray', []).append(result.get('latency_ms', 0))
            elif 'bank_' in result['config']:
                by_placement.setdefault('bank', []).append(result.get('latency_ms', 0))
            elif 'rank_' in result['config']:
                by_placement.setdefault('rank', []).append(result.get('latency_ms', 0))

        print("\nAverage Latency by Memory Technology:")
        for tech, latencies in sorted(by_memory.items()):
            avg = sum(latencies) / len(latencies) if latencies else 0
            print(f"  {tech:10s}: {avg:8.2f} ms ({len(latencies)} configs)")

        print("\nAverage Latency by PE Type:")
        for pe, latencies in sorted(by_pe.items()):
            avg = sum(latencies) / len(latencies) if latencies else 0
            print(f"  {pe:12s}: {avg:8.2f} ms ({len(latencies)} configs)")

        print("\nAverage Latency by Placement Level:")
        for placement, latencies in sorted(by_placement.items()):
            avg = sum(latencies) / len(latencies) if latencies else 0
            print(f"  {placement:10s}: {avg:8.2f} ms ({len(latencies)} configs)")


def parse_benchmark_output(output):
    """Parse benchmark runner output to extract metrics."""
    metrics = {}

    for line in output.split('\n'):
        line = line.strip()

        if 'Total latency:' in line:
            try:
                # Format: "Total latency: 20.27 ms"
                value = line.split(':')[1].strip().split()[0]
                metrics['latency_ms'] = float(value)
            except (IndexError, ValueError):
                pass

        elif 'Throughput:' in line:
            try:
                # Format: "Throughput: 12.93 M vertices/sec"
                value = line.split(':')[1].strip().split()[0]
                metrics['throughput_mvps'] = float(value)
            except (IndexError, ValueError):
                pass

        elif 'Edges/sec:' in line:
            try:
                # Format: "Edges/sec: 206.89 M edges/sec"
                value = line.split(':')[1].strip().split()[0]
                metrics['edges_per_sec_m'] = float(value)
            except (IndexError, ValueError):
                pass

        elif 'Per-vertex time:' in line:
            try:
                # Format: "Per-vertex time: 309.34 ns"
                value = line.split(':')[1].strip().split()[0]
                metrics['per_vertex_ns'] = float(value)
            except (IndexError, ValueError):
                pass

    return metrics


def run_single_benchmark(benchmark_runner, config_file, verbose=False):
    """Run a single benchmark and return results."""

    config_name = Path(config_file).stem

    if verbose:
        print(f"\nRunning: {config_name}")

    try:
        result = subprocess.run(
            [benchmark_runner, '--config', config_file],
            capture_output=True,
            text=True,
            timeout=300  # 5 minute timeout
        )

        if result.returncode != 0:
            print(f"  ERROR: Benchmark failed with return code {result.returncode}")
            if verbose:
                print(f"  STDERR: {result.stderr}")
            return None

        # Parse output
        metrics = parse_benchmark_output(result.stdout)

        if not metrics:
            print(f"  WARNING: No metrics extracted from output")
            if verbose:
                print(f"  Output: {result.stdout[:500]}")
            return None

        if verbose:
            print(f"  Latency: {metrics.get('latency_ms', 'N/A')} ms")

        return metrics

    except subprocess.TimeoutExpired:
        print(f"  ERROR: Benchmark timed out after 5 minutes")
        return None
    except Exception as e:
        print(f"  ERROR: {str(e)}")
        return None


def run_batch_tests(benchmark_runner, config_dir, results_dir, verbose=False):
    """Run all benchmarks in a directory."""

    config_dir = Path(config_dir)
    results_dir = Path(results_dir)
    results_dir.mkdir(parents=True, exist_ok=True)

    # Find all YAML configs
    config_files = sorted(config_dir.glob('*.yaml'))

    if not config_files:
        print(f"No YAML configs found in {config_dir}")
        return None

    print(f"\nFound {len(config_files)} benchmark configs")
    print(f"Benchmark runner: {benchmark_runner}")
    print(f"Output directory: {results_dir}")

    results = BenchmarkResults()

    start_time = time.time()
    completed = 0
    failed = 0

    for i, config_file in enumerate(config_files):
        config_name = config_file.stem

        print(f"\n[{i+1}/{len(config_files)}] {config_name}", end='')
        if not verbose:
            print(" ...", end='', flush=True)

        metrics = run_single_benchmark(benchmark_runner, str(config_file), verbose)

        if metrics:
            results.add_result(config_name, metrics)
            completed += 1
            if not verbose:
                print(f" OK ({metrics.get('latency_ms', 'N/A')} ms)")
        else:
            failed += 1
            if not verbose:
                print(" FAILED")

    elapsed_time = time.time() - start_time

    # Save results
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    json_file = results_dir / f'results_{timestamp}.json'
    csv_file = results_dir / f'results_{timestamp}.csv'

    results.save_json(json_file)
    results.save_csv(csv_file)

    print(f"\n{'='*80}")
    print(f"Batch test completed in {elapsed_time:.1f} seconds")
    print(f"  Completed: {completed}")
    print(f"  Failed: {failed}")
    print(f"  Total: {len(config_files)}")
    print(f"\nResults saved to:")
    print(f"  JSON: {json_file}")
    print(f"  CSV: {csv_file}")

    # Print summary
    results.print_summary()

    return results


def main():
    parser = argparse.ArgumentParser(description='Run comprehensive PIMID benchmarks')
    parser.add_argument('--runner', '-r',
                        default='build/benchmarks/benchmark_runner',
                        help='Path to benchmark_runner executable')
    parser.add_argument('--configs', '-c',
                        default='configs/comprehensive_tests/quick',
                        help='Directory containing benchmark configs')
    parser.add_argument('--output', '-o',
                        default='results/comprehensive',
                        help='Output directory for results')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Verbose output')
    parser.add_argument('--suite', '-s',
                        choices=['quick', 'comprehensive', 'custom'],
                        default='quick',
                        help='Test suite to run')

    args = parser.parse_args()

    # Resolve paths
    benchmark_runner = Path(args.runner)
    if not benchmark_runner.exists():
        print(f"Error: Benchmark runner not found: {benchmark_runner}")
        print("Please build it first: cd build && make benchmark_runner")
        return 1

    # Select config directory based on suite
    if args.suite == 'quick':
        config_dir = Path('configs/comprehensive_tests/quick')
    elif args.suite == 'comprehensive':
        config_dir = Path('configs/comprehensive_tests/comprehensive')
    else:
        config_dir = Path(args.configs)

    if not config_dir.exists():
        print(f"Error: Config directory not found: {config_dir}")
        return 1

    print("="*80)
    print("PIMID COMPREHENSIVE TEST RUNNER")
    print("="*80)
    print(f"Suite: {args.suite}")
    print(f"Config directory: {config_dir}")

    # Run tests
    results = run_batch_tests(
        str(benchmark_runner),
        config_dir,
        Path(args.output),
        args.verbose
    )

    if results is None:
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
