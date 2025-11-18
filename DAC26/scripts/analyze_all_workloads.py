#!/usr/bin/env python3
"""
Comprehensive workload analysis for all 8 benchmarks × 2 programming models
Extracts execution time and energy metrics from benchmark results
"""

import re
import sys
from typing import Dict, List, Tuple
from collections import defaultdict

def parse_benchmark_output(filepath: str) -> Dict:
    """Parse benchmark output file and extract metrics"""
    results = defaultdict(lambda: defaultdict(dict))

    with open(filepath, 'r') as f:
        content = f.read()

    # Split by workload sections
    workload_sections = re.split(r'#{20,}', content)

    current_workload = None
    current_bank = None
    current_model = None
    current_topology = None

    for section in workload_sections:
        # Extract workload name
        workload_match = re.search(r'# ([^:]+):', section)
        if workload_match:
            current_workload = workload_match.group(1).strip()

        # Extract bank configuration
        bank_match = re.search(r'(\w+)-(\d+KB) \((\d+) subarrays\)', section)
        if bank_match:
            current_bank = f"{bank_match.group(1)}-{bank_match.group(2)}"

        # Extract programming model and topology
        if "Message Passing" in section and "Baseline" in section:
            current_model = "Message Passing"
            current_topology = "H-tree"
        elif "Message Passing" in section and "LIBCom" in section:
            current_model = "Message Passing"
            current_topology = "LIBCom"
        elif "Shared Memory" in section and "Baseline" in section:
            current_model = "Shared Memory"
            current_topology = "H-tree"
        elif "Shared Memory" in section and "LIBCom" in section:
            current_model = "Shared Memory"
            current_topology = "LIBCom"

        # Extract metrics
        if current_workload and current_bank and current_model and current_topology:
            key = (current_workload, current_bank, current_model, current_topology)

            # Total cycles
            cycles_match = re.search(r'Total cycles:\s+(\d+)', section)
            if cycles_match:
                results[key]['total_cycles'] = int(cycles_match.group(1))

            # Compute cycles
            compute_match = re.search(r'Compute cycles:\s+(\d+)', section)
            if compute_match:
                results[key]['compute_cycles'] = int(compute_match.group(1))

            # Transfer cycles
            transfer_match = re.search(r'Transfer cycles:\s+(\d+)', section)
            if transfer_match:
                results[key]['transfer_cycles'] = int(transfer_match.group(1))

            # Total energy
            energy_match = re.search(r'Total energy \(relative\):\s+([\d.]+)', section)
            if energy_match:
                results[key]['total_energy'] = float(energy_match.group(1))

            # Transfers
            transfers_match = re.search(r'Inter-subarray (?:block )?transfers:\s+(\d+)', section)
            if transfers_match:
                results[key]['num_transfers'] = int(transfers_match.group(1))

    return dict(results)

def generate_summary_table(results: Dict):
    """Generate summary table for all workloads"""

    print("\n" + "="*120)
    print("COMPREHENSIVE WORKLOAD ANALYSIS: All 8 Benchmarks × 2 Programming Models")
    print("="*120)

    # Group by workload
    workloads = sorted(set(key[0] for key in results.keys()))

    for workload in workloads:
        print(f"\n{'='*120}")
        print(f"{workload}")
        print(f"{'='*120}")

        # Get all configurations for this workload
        workload_results = {k: v for k, v in results.items() if k[0] == workload}

        # Group by bank
        banks = sorted(set(key[1] for key in workload_results.keys()))

        for bank in banks:
            print(f"\n{bank}:")
            print(f"{'Model':<20} {'Topology':<10} {'Total Cycles':<15} {'Transfer Cycles':<18} {'Energy':<12} {'Speedup'}")
            print("-" * 120)

            # Get H-tree baseline for speedup calculation
            baseline_key = None
            for key in workload_results.keys():
                if key[1] == bank and key[3] == "H-tree":
                    baseline_key = key
                    break

            baseline_cycles = workload_results[baseline_key]['total_cycles'] if baseline_key else None

            # Print results
            for model in ["Message Passing", "Shared Memory"]:
                for topo in ["H-tree", "LIBCom"]:
                    key = (workload, bank, model, topo)
                    if key in workload_results:
                        data = workload_results[key]
                        total_cyc = data.get('total_cycles', 0)
                        transfer_cyc = data.get('transfer_cycles', 0)
                        energy = data.get('total_energy', 0.0)

                        speedup = f"{baseline_cycles / total_cyc:.3f}×" if baseline_cycles and total_cyc else "N/A"

                        print(f"{model:<20} {topo:<10} {total_cyc:<15,} {transfer_cyc:<18,} {energy:<12.2f} {speedup}")

def generate_energy_comparison(results: Dict):
    """Generate energy comparison table"""

    print("\n" + "="*120)
    print("ENERGY COMPARISON: H-tree vs LIBCom")
    print("="*120)
    print(f"\n{'Workload':<20} {'Bank':<15} {'Model':<20} {'H-tree Energy':<15} {'LIBCom Energy':<15} {'Savings'}")
    print("-" * 120)

    # Group by workload and bank
    for workload in sorted(set(key[0] for key in results.keys())):
        for bank in sorted(set(key[1] for key in results.keys() if key[0] == workload)):
            for model in ["Message Passing", "Shared Memory"]:
                htree_key = (workload, bank, model, "H-tree")
                libcom_key = (workload, bank, model, "LIBCom")

                if htree_key in results and libcom_key in results:
                    htree_energy = results[htree_key].get('total_energy', 0.0)
                    libcom_energy = results[libcom_key].get('total_energy', 0.0)

                    if htree_energy > 0:
                        savings = (htree_energy - libcom_energy) / htree_energy * 100
                        print(f"{workload:<20} {bank:<15} {model:<20} {htree_energy:<15.2f} {libcom_energy:<15.2f} {savings:.1f}%")

def generate_performance_summary(results: Dict):
    """Generate overall performance summary"""

    print("\n" + "="*120)
    print("PERFORMANCE SUMMARY: LIBCom Speedup Across All Workloads")
    print("="*120)

    speedups = []

    print(f"\n{'Workload':<20} {'Bank':<15} {'Model':<20} {'H-tree Cycles':<15} {'LIBCom Cycles':<15} {'Speedup'}")
    print("-" * 120)

    for workload in sorted(set(key[0] for key in results.keys())):
        for bank in sorted(set(key[1] for key in results.keys() if key[0] == workload)):
            for model in ["Message Passing", "Shared Memory"]:
                htree_key = (workload, bank, model, "H-tree")
                libcom_key = (workload, bank, model, "LIBCom")

                if htree_key in results and libcom_key in results:
                    htree_cycles = results[htree_key].get('total_cycles', 0)
                    libcom_cycles = results[libcom_key].get('total_cycles', 0)

                    if htree_cycles > 0 and libcom_cycles > 0:
                        speedup = htree_cycles / libcom_cycles
                        speedups.append(speedup)
                        print(f"{workload:<20} {bank:<15} {model:<20} {htree_cycles:<15,} {libcom_cycles:<15,} {speedup:.4f}×")

    if speedups:
        print("\n" + "-" * 120)
        print(f"Average Speedup: {sum(speedups) / len(speedups):.4f}×")
        print(f"Min Speedup: {min(speedups):.4f}×")
        print(f"Max Speedup: {max(speedups):.4f}×")

def main():
    if len(sys.argv) < 2:
        print("Usage: analyze_all_workloads.py <results_file>")
        sys.exit(1)

    filepath = sys.argv[1]

    print(f"Analyzing benchmark results from: {filepath}")

    results = parse_benchmark_output(filepath)

    if not results:
        print("No results found in file")
        sys.exit(1)

    print(f"\nParsed {len(results)} configurations")

    generate_summary_table(results)
    generate_energy_comparison(results)
    generate_performance_summary(results)

if __name__ == "__main__":
    main()
