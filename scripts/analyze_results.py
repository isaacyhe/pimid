#!/usr/bin/env python3
"""
Analyze comprehensive benchmark results and generate insights.
Creates comparison tables, identifies bottlenecks, and generates recommendations.
"""

import json
import csv
import sys
from pathlib import Path
from collections import defaultdict
import argparse


class ResultsAnalyzer:
    """Analyzes benchmark results and generates insights."""

    def __init__(self, results_file):
        self.results = self.load_results(results_file)
        self.parse_metadata()

    def load_results(self, filepath):
        """Load results from JSON or CSV file."""
        filepath = Path(filepath)

        if filepath.suffix == '.json':
            with open(filepath) as f:
                return json.load(f)
        elif filepath.suffix == '.csv':
            with open(filepath) as f:
                return list(csv.DictReader(f))
        else:
            raise ValueError(f"Unsupported file format: {filepath.suffix}")

    def parse_metadata(self):
        """Extract metadata from config names."""
        for result in self.results:
            config = result['config']
            parts = config.split('_')

            # Parse config name: bfs_<placement>_<pe_type>_<memory>_<size>_deg<degree>
            result['placement'] = 'unknown'
            result['pe_type'] = 'unknown'
            result['memory_tech'] = 'unknown'
            result['workload_size'] = 'unknown'
            result['degree'] = 0

            # Placement
            for p in ['subarray', 'bank', 'rank']:
                if p in config:
                    result['placement'] = p
                    break

            # PE type
            if 'inorder' in config:
                result['pe_type'] = 'inorder'
            elif 'simple' in config or 'alu' in config:
                result['pe_type'] = 'simple_alu'

            # Memory tech
            for tech in ['dram', 'sram', 'sttmram', 'pcm', 'reram']:
                if tech in config:
                    result['memory_tech'] = tech
                    break

            # Workload size
            for size in ['1k', '4k', '16k', '64k']:
                if size in config:
                    result['workload_size'] = size
                    break

            # Degree
            if 'deg' in config:
                try:
                    deg_part = [p for p in parts if 'deg' in p][0]
                    result['degree'] = int(deg_part.replace('deg', ''))
                except:
                    pass

    def group_by(self, dimension):
        """Group results by a dimension."""
        groups = defaultdict(list)
        for result in self.results:
            key = result.get(dimension, 'unknown')
            groups[key].append(result)
        return groups

    def calculate_statistics(self, group):
        """Calculate statistics for a group of results."""
        latencies = [float(r.get('latency_ms', 0)) for r in group]
        throughputs = [float(r.get('throughput_mvps', 0)) for r in group]

        if not latencies:
            return {}

        return {
            'count': len(latencies),
            'avg_latency_ms': sum(latencies) / len(latencies),
            'min_latency_ms': min(latencies),
            'max_latency_ms': max(latencies),
            'avg_throughput_mvps': sum(throughputs) / len(throughputs) if throughputs else 0
        }

    def print_comparison_table(self, dimension, title):
        """Print comparison table for a dimension."""
        groups = self.group_by(dimension)

        print(f"\n{'='*80}")
        print(f"{title}")
        print(f"{'='*80}")

        print(f"\n{dimension.upper():15s} {'Count':>8s} {'Avg Lat (ms)':>15s} {'Min':>10s} {'Max':>10s} {'Throughput':>12s}")
        print("-"*80)

        for key in sorted(groups.keys()):
            stats = self.calculate_statistics(groups[key])
            if stats:
                print(f"{key:15s} {stats['count']:8d} "
                      f"{stats['avg_latency_ms']:15.2f} "
                      f"{stats['min_latency_ms']:10.2f} "
                      f"{stats['max_latency_ms']:10.2f} "
                      f"{stats['avg_throughput_mvps']:12.2f}")

    def find_best_configs(self, top_n=10):
        """Find top N best performing configurations."""
        sorted_results = sorted(
            self.results,
            key=lambda r: float(r.get('latency_ms', float('inf')))
        )

        print(f"\n{'='*80}")
        print(f"TOP {top_n} FASTEST CONFIGURATIONS")
        print(f"{'='*80}\n")

        print(f"{'Rank':>4s}  {'Config':50s} {'Latency (ms)':>15s} {'Throughput':>12s}")
        print("-"*85)

        for i, result in enumerate(sorted_results[:top_n]):
            config = result['config'][:50]  # Truncate if too long
            latency = float(result.get('latency_ms', 0))
            throughput = float(result.get('throughput_mvps', 0))

            print(f"{i+1:4d}  {config:50s} {latency:15.2f} {throughput:12.2f}")

    def find_worst_configs(self, top_n=10):
        """Find top N worst performing configurations."""
        sorted_results = sorted(
            self.results,
            key=lambda r: float(r.get('latency_ms', 0)),
            reverse=True
        )

        print(f"\n{'='*80}")
        print(f"TOP {top_n} SLOWEST CONFIGURATIONS")
        print(f"{'='*80}\n")

        print(f"{'Rank':>4s}  {'Config':50s} {'Latency (ms)':>15s} {'Bottleneck':>15s}")
        print("-"*85)

        for i, result in enumerate(sorted_results[:top_n]):
            config = result['config'][:50]
            latency = float(result.get('latency_ms', 0))

            # Identify bottleneck
            bottleneck = "unknown"
            if 'pcm' in result['memory_tech']:
                bottleneck = "PCM write"
            elif 'rank' in result['placement']:
                bottleneck = "Single PE"
            elif 'inorder' in result['pe_type']:
                bottleneck = "Branch overhead"

            print(f"{i+1:4d}  {config:50s} {latency:15.2f} {bottleneck:>15s}")

    def analyze_pe_effectiveness(self):
        """Analyze PE type effectiveness across different scenarios."""
        print(f"\n{'='*80}")
        print("PE TYPE EFFECTIVENESS ANALYSIS")
        print(f"{'='*80}\n")

        # Compare Simple ALU vs In-Order Core
        simple_alu = [r for r in self.results if r['pe_type'] == 'simple_alu']
        inorder = [r for r in self.results if r['pe_type'] == 'inorder']

        simple_stats = self.calculate_statistics(simple_alu)
        inorder_stats = self.calculate_statistics(inorder)

        print(f"Simple ALU PE:")
        print(f"  Average latency: {simple_stats.get('avg_latency_ms', 0):.2f} ms")
        print(f"  Configurations: {simple_stats.get('count', 0)}")

        print(f"\nIn-Order Core PE:")
        print(f"  Average latency: {inorder_stats.get('avg_latency_ms', 0):.2f} ms")
        print(f"  Configurations: {inorder_stats.get('count', 0)}")

        if simple_stats.get('avg_latency_ms', 0) > 0:
            overhead = ((inorder_stats.get('avg_latency_ms', 0) /
                        simple_stats.get('avg_latency_ms', 1)) - 1) * 100
            print(f"\nIn-Order Core overhead: {overhead:+.1f}%")
            if overhead > 10:
                print("  ⚠️  Branch overhead is significant - Simple ALU may be better for BFS")
            elif overhead < -10:
                print("  ✓ In-Order Core provides speedup through conditional execution")

    def analyze_memory_tech_ranking(self):
        """Rank memory technologies by performance."""
        print(f"\n{'='*80}")
        print("MEMORY TECHNOLOGY PERFORMANCE RANKING")
        print(f"{'='*80}\n")

        groups = self.group_by('memory_tech')
        rankings = []

        for tech, results in groups.items():
            stats = self.calculate_statistics(results)
            if stats:
                rankings.append((tech, stats))

        # Sort by average latency (lower is better)
        rankings.sort(key=lambda x: x[1]['avg_latency_ms'])

        print(f"{'Rank':>4s}  {'Technology':12s} {'Avg Latency':>15s} {'vs SRAM':>10s} {'Configs':>8s}")
        print("-"*55)

        sram_latency = next((s[1]['avg_latency_ms'] for s in rankings if s[0] == 'sram'), 1.0)

        for i, (tech, stats) in enumerate(rankings):
            speedup = sram_latency / stats['avg_latency_ms'] if stats['avg_latency_ms'] > 0 else 0
            print(f"{i+1:4d}  {tech:12s} {stats['avg_latency_ms']:15.2f} {speedup:10.2f}x {stats['count']:8d}")

    def analyze_placement_efficiency(self):
        """Analyze PE placement level efficiency."""
        print(f"\n{'='*80}")
        print("PE PLACEMENT LEVEL EFFICIENCY")
        print(f"{'='*80}\n")

        groups = self.group_by('placement')

        # Expected: subarray (most PEs) > bank > rank (fewest PEs)
        for placement in ['subarray', 'bank', 'rank']:
            if placement in groups:
                stats = self.calculate_statistics(groups[placement])
                num_pes = {'subarray': 16, 'bank': 4, 'rank': 1}[placement]

                print(f"{placement.upper()} Level ({num_pes} PEs):")
                print(f"  Average latency: {stats.get('avg_latency_ms', 0):.2f} ms")
                print(f"  Configurations tested: {stats.get('count', 0)}")
                print()

    def generate_recommendations(self):
        """Generate design recommendations based on results."""
        print(f"\n{'='*80}")
        print("DESIGN RECOMMENDATIONS")
        print(f"{'='*80}\n")

        # Find best overall config
        best = min(self.results, key=lambda r: float(r.get('latency_ms', float('inf'))))

        print("Best Overall Configuration:")
        print(f"  Config: {best['config']}")
        print(f"  Latency: {best.get('latency_ms', 'N/A')} ms")
        print(f"  Memory: {best['memory_tech'].upper()}")
        print(f"  PE Type: {best['pe_type']}")
        print(f"  Placement: {best['placement']}")

        # Specific recommendations
        print("\nRecommendations:")

        # Memory technology
        mem_groups = self.group_by('memory_tech')
        pcm_avg = self.calculate_statistics(mem_groups.get('pcm', [])).get('avg_latency_ms', 0)
        sram_avg = self.calculate_statistics(mem_groups.get('sram', [])).get('avg_latency_ms', 0)

        if pcm_avg > sram_avg * 5:
            print("  ⚠️  Avoid PCM for write-heavy workloads like BFS")

        # PE type
        if 'inorder' in best['pe_type']:
            print("  ✓ In-Order Core recommended when branch prediction is effective")
        else:
            print("  ✓ Simple ALU recommended for minimal overhead on predictable workloads")

        # Placement
        if best['placement'] == 'subarray':
            print("  ✓ Subarray-level PIM provides maximum parallelism")
        elif best['placement'] == 'bank':
            print("  ✓ Bank-level PIM offers good balance of parallelism and complexity")


def main():
    parser = argparse.ArgumentParser(description='Analyze PIMID benchmark results')
    parser.add_argument('results_file', help='Results file (JSON or CSV)')
    parser.add_argument('--top', type=int, default=10,
                        help='Number of top configs to show')

    args = parser.parse_args()

    if not Path(args.results_file).exists():
        print(f"Error: Results file not found: {args.results_file}")
        return 1

    analyzer = ResultsAnalyzer(args.results_file)

    # Print all analyses
    analyzer.print_comparison_table('memory_tech', 'COMPARISON BY MEMORY TECHNOLOGY')
    analyzer.print_comparison_table('pe_type', 'COMPARISON BY PE TYPE')
    analyzer.print_comparison_table('placement', 'COMPARISON BY PLACEMENT LEVEL')
    analyzer.print_comparison_table('workload_size', 'COMPARISON BY WORKLOAD SIZE')

    analyzer.find_best_configs(args.top)
    analyzer.find_worst_configs(args.top)

    analyzer.analyze_pe_effectiveness()
    analyzer.analyze_memory_tech_ranking()
    analyzer.analyze_placement_efficiency()
    analyzer.generate_recommendations()

    print(f"\n{'='*80}\n")

    return 0


if __name__ == '__main__':
    sys.exit(main())
