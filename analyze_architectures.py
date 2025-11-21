#!/usr/bin/env python3
"""
Analyze test results comparing message passing vs shared memory architectures.
"""

import json
import sys
from collections import defaultdict
from statistics import mean, median, stdev
import os

def load_json_file(filepath):
    """Load and parse a JSON file."""
    print(f"Loading {filepath}...", file=sys.stderr)
    try:
        with open(filepath, 'r') as f:
            data = json.load(f)
        print(f"  Loaded {len(data)} test results", file=sys.stderr)
        return data
    except Exception as e:
        print(f"  Error loading {filepath}: {e}", file=sys.stderr)
        return []

def classify_architecture(test_result):
    """Classify test as message_passing or shared_memory based on workload name."""
    workload = test_result.get('workload', '')
    if workload.endswith('_message'):
        return 'message_passing'
    elif workload.endswith('_shared'):
        return 'shared_memory'
    else:
        return 'unknown'

def extract_metrics(test_result):
    """Extract key performance metrics from a test result."""
    metrics = {}

    # Get core metrics from the metrics field
    if 'metrics' in test_result:
        m = test_result['metrics']
        # Convert to consistent units
        metrics['execution_time_ns'] = m.get('execution_time_ns')
        metrics['execution_time_us'] = m.get('execution_time_ns') / 1000 if m.get('execution_time_ns') else None
        metrics['total_energy_pj'] = m.get('total_energy_pj')
        metrics['network_energy_pj'] = m.get('network_energy_pj')
        metrics['total_cycles'] = m.get('total_cycles')

        # Calculate derived metrics
        if metrics['total_energy_pj'] and metrics['network_energy_pj']:
            metrics['compute_memory_energy_pj'] = metrics['total_energy_pj'] - metrics['network_energy_pj']

        # Extract additional metrics if available
        metrics['memory_accesses'] = m.get('memory_accesses')
        metrics['cache_hits'] = m.get('cache_hits')
        metrics['cache_misses'] = m.get('cache_misses')

    return metrics

def analyze_by_category(results, category_key):
    """Analyze results grouped by a specific category (e.g., memory_tech, pim_level)."""
    grouped = defaultdict(lambda: {'message_passing': [], 'shared_memory': []})

    for result in results:
        config = result.get('config', {})
        arch = classify_architecture(result)
        if arch == 'unknown':
            continue

        category = config.get(category_key, 'unknown')
        metrics = extract_metrics(result)

        if metrics:
            grouped[category][arch].append(metrics)

    return grouped

def calculate_stats(metrics_list, metric_name):
    """Calculate statistics for a specific metric."""
    values = [m[metric_name] for m in metrics_list if m.get(metric_name) is not None]
    if not values:
        return None

    return {
        'count': len(values),
        'mean': mean(values),
        'median': median(values),
        'min': min(values),
        'max': max(values),
        'stdev': stdev(values) if len(values) > 1 else 0
    }

def compare_architectures(mp_metrics, sm_metrics, metric_name):
    """Compare a specific metric between message passing and shared memory."""
    mp_stats = calculate_stats(mp_metrics, metric_name)
    sm_stats = calculate_stats(sm_metrics, metric_name)

    if not mp_stats or not sm_stats:
        return None

    # Calculate improvement (negative means shared memory is better/lower)
    improvement_pct = ((sm_stats['mean'] - mp_stats['mean']) / mp_stats['mean']) * 100

    return {
        'message_passing': mp_stats,
        'shared_memory': sm_stats,
        'improvement_pct': improvement_pct,
        'winner': 'message_passing' if improvement_pct > 0 else 'shared_memory'
    }

def main():
    # File paths
    files = [
        '/home/user/pimid-dev/test_results_5000/all_results_5000.json',
        '/home/user/pimid-dev/test_results_5000_pimid/all_results_5000.json',
        '/home/user/pimid-dev/test_results_all_dram_5000/all_dram_results_5000.json'
    ]

    # Load all results
    all_results = []
    for filepath in files:
        if os.path.exists(filepath):
            results = load_json_file(filepath)
            all_results.extend(results)

    print(f"\n{'='*80}", file=sys.stderr)
    print(f"Total test results loaded: {len(all_results)}", file=sys.stderr)
    print(f"{'='*80}\n", file=sys.stderr)

    # Classify all results by architecture
    mp_results = []
    sm_results = []
    unknown_results = []

    for result in all_results:
        arch = classify_architecture(result)
        if arch == 'message_passing':
            mp_results.append(result)
        elif arch == 'shared_memory':
            sm_results.append(result)
        else:
            unknown_results.append(result)

    print(f"Architecture distribution:", file=sys.stderr)
    print(f"  Message Passing: {len(mp_results)}", file=sys.stderr)
    print(f"  Shared Memory: {len(sm_results)}", file=sys.stderr)
    print(f"  Unknown: {len(unknown_results)}", file=sys.stderr)
    print(file=sys.stderr)

    # Extract metrics for overall comparison
    mp_metrics = [extract_metrics(r) for r in mp_results]
    sm_metrics = [extract_metrics(r) for r in sm_results]

    # Key metrics to compare
    metrics_to_compare = [
        'execution_time_ns',
        'execution_time_us',
        'total_energy_pj',
        'network_energy_pj',
        'compute_memory_energy_pj',
        'total_cycles'
    ]

    # Generate comprehensive report
    report = {
        'summary': {
            'total_tests': len(all_results),
            'message_passing_tests': len(mp_results),
            'shared_memory_tests': len(sm_results),
            'unknown_tests': len(unknown_results)
        },
        'overall_comparison': {},
        'by_memory_technology': {},
        'by_pim_level': {},
        'by_topology': {}
    }

    # Overall comparison
    print("Calculating overall comparisons...", file=sys.stderr)
    for metric in metrics_to_compare:
        comparison = compare_architectures(mp_metrics, sm_metrics, metric)
        if comparison:
            report['overall_comparison'][metric] = comparison

    # Comparison by memory technology
    print("Analyzing by memory technology...", file=sys.stderr)
    by_memory = analyze_by_category(all_results, 'memory_tech')
    for mem_tech, archs in by_memory.items():
        report['by_memory_technology'][mem_tech] = {}
        for metric in metrics_to_compare:
            comparison = compare_architectures(
                archs['message_passing'],
                archs['shared_memory'],
                metric
            )
            if comparison:
                report['by_memory_technology'][mem_tech][metric] = comparison

    # Comparison by PIM level
    print("Analyzing by PIM level...", file=sys.stderr)
    by_pim = analyze_by_category(all_results, 'pim_level')
    for pim_level, archs in by_pim.items():
        report['by_pim_level'][pim_level] = {}
        for metric in metrics_to_compare:
            comparison = compare_architectures(
                archs['message_passing'],
                archs['shared_memory'],
                metric
            )
            if comparison:
                report['by_pim_level'][pim_level][metric] = comparison

    # Comparison by topology (additional breakdown)
    print("Analyzing by topology type...", file=sys.stderr)
    topology_breakdown = defaultdict(list)
    for result in all_results:
        config = result.get('config', {})
        topology = config.get('topology_type', 'unknown')
        metrics = extract_metrics(result)
        if metrics.get('execution_time') is not None:
            topology_breakdown[topology].append(metrics)

    for topology, metrics_list in topology_breakdown.items():
        if metrics_list:
            report['by_topology'][topology] = {}
            for metric in metrics_to_compare:
                stats = calculate_stats(metrics_list, metric)
                if stats:
                    report['by_topology'][topology][metric] = stats

    # Output JSON report
    print(json.dumps(report, indent=2))

    print("\nAnalysis complete!", file=sys.stderr)

if __name__ == '__main__':
    main()
