#!/usr/bin/env python3
"""
Comprehensive analysis of message passing vs shared memory architectures.
"""

import json
import sys
import re
from collections import defaultdict
from statistics import mean, median, stdev

# Memory technology mapping
MEM_TECH_MAP = {
    0: 'SRAM',
    1: 'DRAM',
    2: 'STT-MRAM',
    3: 'PCM',
    4: 'ReRAM'
}

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
    """Classify test as message_passing or shared_memory."""
    workload = test_result.get('workload', '')
    if workload.endswith('_message'):
        return 'message_passing'
    elif workload.endswith('_shared'):
        return 'shared_memory'
    else:
        return 'unknown'

def extract_memory_tech(test_result):
    """Extract memory technology from config or stdout."""
    config = test_result.get('config', {})

    # Check for dram_type (all_dram file)
    if 'dram_type' in config:
        return config['dram_type']

    # Check for mem_tech code
    mem_tech_code = config.get('mem_tech')
    if mem_tech_code is not None:
        return MEM_TECH_MAP.get(mem_tech_code, f'UNKNOWN_{mem_tech_code}')

    # Parse from stdout
    stdout = test_result.get('stdout', '')
    for line in stdout.split('\n'):
        if 'Memory Tech:' in line:
            return line.split('Memory Tech:')[1].strip()
        if 'Memory:' in line and 'PIMID' not in line:
            return line.split('Memory:')[1].strip()

    return 'UNKNOWN'

def extract_pim_level(test_result):
    """Extract PIM level/granularity from config or stdout."""
    config = test_result.get('config', {})

    # Check for pim_granularity (all_dram file)
    if 'pim_granularity' in config:
        return config['pim_granularity']

    # Parse from stdout
    stdout = test_result.get('stdout', '')
    for line in stdout.split('\n'):
        if 'Placement:' in line:
            return line.split('Placement:')[1].strip()
        if 'PIM Level:' in line:
            return line.split('PIM Level:')[1].strip()

    # Use num_subarrays as proxy
    num_subarrays = config.get('num_subarrays')
    if num_subarrays == 1:
        return 'SINGLE_SUBARRAY'
    elif num_subarrays:
        return f'MULTI_SUBARRAY_{num_subarrays}'

    return 'UNKNOWN'

def extract_workload_type(test_result):
    """Extract the base workload type (without _message or _shared suffix)."""
    workload = test_result.get('workload', '')
    return workload.replace('_message', '').replace('_shared', '')

def extract_metrics(test_result):
    """Extract key performance metrics from a test result."""
    metrics = {}

    if 'metrics' in test_result:
        m = test_result['metrics']
        metrics['execution_time_ns'] = m.get('execution_time_ns')
        metrics['execution_time_us'] = m.get('execution_time_ns') / 1000 if m.get('execution_time_ns') else None
        metrics['total_energy_pj'] = m.get('total_energy_pj')
        metrics['network_energy_pj'] = m.get('network_energy_pj')
        metrics['total_cycles'] = m.get('total_cycles')

        # Calculate derived metrics
        if metrics['total_energy_pj'] is not None and metrics['network_energy_pj'] is not None:
            metrics['compute_memory_energy_pj'] = metrics['total_energy_pj'] - metrics['network_energy_pj']

        # Energy efficiency
        if metrics['total_energy_pj'] and metrics['execution_time_ns']:
            metrics['power_mw'] = (metrics['total_energy_pj'] / metrics['execution_time_ns']) * 1000

    return metrics

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

def compare_architectures(mp_metrics, sm_metrics, metric_name, lower_is_better=True):
    """Compare a specific metric between message passing and shared memory."""
    mp_stats = calculate_stats(mp_metrics, metric_name)
    sm_stats = calculate_stats(sm_metrics, metric_name)

    if not mp_stats or not sm_stats:
        return None

    # Skip if either mean is zero (can't calculate meaningful percentage)
    if mp_stats['mean'] == 0 or sm_stats['mean'] == 0:
        return None

    # Calculate improvement percentage
    diff = sm_stats['mean'] - mp_stats['mean']
    improvement_pct = (diff / mp_stats['mean']) * 100

    # Determine winner (for metrics like time/energy, lower is better)
    if lower_is_better:
        winner = 'shared_memory' if diff < 0 else 'message_passing'
        improvement_pct = -improvement_pct if diff < 0 else improvement_pct
    else:
        winner = 'shared_memory' if diff > 0 else 'message_passing'

    return {
        'message_passing': mp_stats,
        'shared_memory': sm_stats,
        'improvement_pct': improvement_pct,
        'winner': winner
    }

def main():
    files = [
        '/home/user/pimid-dev/test_results_5000/all_results_5000.json',
        '/home/user/pimid-dev/test_results_5000_pimid/all_results_5000.json',
        '/home/user/pimid-dev/test_results_all_dram_5000/all_dram_results_5000.json'
    ]

    # Load all results
    all_results = []
    for filepath in files:
        results = load_json_file(filepath)
        all_results.extend(results)

    print(f"\n{'='*80}", file=sys.stderr)
    print(f"Total test results loaded: {len(all_results)}", file=sys.stderr)
    print(f"{'='*80}\n", file=sys.stderr)

    # Classify and enrich results
    mp_results = []
    sm_results = []

    for result in all_results:
        arch = classify_architecture(result)
        if arch == 'message_passing':
            mp_results.append(result)
        elif arch == 'shared_memory':
            sm_results.append(result)

    print(f"Architecture distribution:", file=sys.stderr)
    print(f"  Message Passing: {len(mp_results)}", file=sys.stderr)
    print(f"  Shared Memory: {len(sm_results)}", file=sys.stderr)
    print(file=sys.stderr)

    # Extract metrics
    mp_metrics = [extract_metrics(r) for r in mp_results]
    sm_metrics = [extract_metrics(r) for r in sm_results]

    # Metrics to compare
    metrics_to_compare = {
        'execution_time_ns': {'lower_is_better': True, 'unit': 'ns'},
        'execution_time_us': {'lower_is_better': True, 'unit': 'μs'},
        'total_energy_pj': {'lower_is_better': True, 'unit': 'pJ'},
        'network_energy_pj': {'lower_is_better': True, 'unit': 'pJ'},
        'compute_memory_energy_pj': {'lower_is_better': True, 'unit': 'pJ'},
        'total_cycles': {'lower_is_better': True, 'unit': 'cycles'},
        'power_mw': {'lower_is_better': True, 'unit': 'mW'}
    }

    report = {
        'summary': {
            'total_tests': len(all_results),
            'message_passing_tests': len(mp_results),
            'shared_memory_tests': len(sm_results)
        },
        'overall_comparison': {},
        'by_memory_technology': {},
        'by_pim_level': {},
        'by_workload_type': {}
    }

    # Overall comparison
    print("Calculating overall comparisons...", file=sys.stderr)
    for metric, config in metrics_to_compare.items():
        comparison = compare_architectures(
            mp_metrics, sm_metrics, metric,
            lower_is_better=config['lower_is_better']
        )
        if comparison:
            comparison['unit'] = config['unit']
            report['overall_comparison'][metric] = comparison

    # Comparison by memory technology
    print("Analyzing by memory technology...", file=sys.stderr)
    mp_by_mem = defaultdict(list)
    sm_by_mem = defaultdict(list)

    for result in mp_results:
        mem_tech = extract_memory_tech(result)
        metrics = extract_metrics(result)
        if metrics:
            mp_by_mem[mem_tech].append(metrics)

    for result in sm_results:
        mem_tech = extract_memory_tech(result)
        metrics = extract_metrics(result)
        if metrics:
            sm_by_mem[mem_tech].append(metrics)

    all_mem_techs = set(mp_by_mem.keys()) | set(sm_by_mem.keys())
    for mem_tech in all_mem_techs:
        if mem_tech in mp_by_mem and mem_tech in sm_by_mem:
            report['by_memory_technology'][mem_tech] = {}
            for metric, config in metrics_to_compare.items():
                comparison = compare_architectures(
                    mp_by_mem[mem_tech],
                    sm_by_mem[mem_tech],
                    metric,
                    lower_is_better=config['lower_is_better']
                )
                if comparison:
                    comparison['unit'] = config['unit']
                    report['by_memory_technology'][mem_tech][metric] = comparison

    # Comparison by PIM level
    print("Analyzing by PIM level...", file=sys.stderr)
    mp_by_pim = defaultdict(list)
    sm_by_pim = defaultdict(list)

    for result in mp_results:
        pim_level = extract_pim_level(result)
        metrics = extract_metrics(result)
        if metrics:
            mp_by_pim[pim_level].append(metrics)

    for result in sm_results:
        pim_level = extract_pim_level(result)
        metrics = extract_metrics(result)
        if metrics:
            sm_by_pim[pim_level].append(metrics)

    all_pim_levels = set(mp_by_pim.keys()) | set(sm_by_pim.keys())
    for pim_level in all_pim_levels:
        if pim_level in mp_by_pim and pim_level in sm_by_pim:
            report['by_pim_level'][pim_level] = {}
            for metric, config in metrics_to_compare.items():
                comparison = compare_architectures(
                    mp_by_pim[pim_level],
                    sm_by_pim[pim_level],
                    metric,
                    lower_is_better=config['lower_is_better']
                )
                if comparison:
                    comparison['unit'] = config['unit']
                    report['by_pim_level'][pim_level][metric] = comparison

    # Comparison by workload type
    print("Analyzing by workload type...", file=sys.stderr)
    mp_by_workload = defaultdict(list)
    sm_by_workload = defaultdict(list)

    for result in mp_results:
        workload = extract_workload_type(result)
        metrics = extract_metrics(result)
        if metrics:
            mp_by_workload[workload].append(metrics)

    for result in sm_results:
        workload = extract_workload_type(result)
        metrics = extract_metrics(result)
        if metrics:
            sm_by_workload[workload].append(metrics)

    all_workloads = set(mp_by_workload.keys()) | set(sm_by_workload.keys())
    for workload in all_workloads:
        if workload in mp_by_workload and workload in sm_by_workload:
            report['by_workload_type'][workload] = {}
            for metric, config in metrics_to_compare.items():
                comparison = compare_architectures(
                    mp_by_workload[workload],
                    sm_by_workload[workload],
                    metric,
                    lower_is_better=config['lower_is_better']
                )
                if comparison:
                    comparison['unit'] = config['unit']
                    report['by_workload_type'][workload][metric] = comparison

    # Output JSON report
    print(json.dumps(report, indent=2))
    print("\nAnalysis complete!", file=sys.stderr)

if __name__ == '__main__':
    main()
