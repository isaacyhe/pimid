#!/usr/bin/env python3
"""
Analyze Virtual Channel evaluation results
Extract key metrics and create summary tables
"""

import re
import sys
from typing import Dict, List, Tuple

def parse_result_file(filepath: str) -> List[Dict]:
    """Parse the VC evaluation result file"""
    results = []
    current_config = {}

    with open(filepath, 'r') as f:
        lines = f.readlines()

    i = 0
    while i < len(lines):
        line = lines[i].strip()

        # Detect configuration header
        if "Bank:" in line and "Topology:" in line and "VCs:" in line:
            match = re.search(r'Bank:\s+(\S+)\s+\|\s+Topology:\s+(\S+)\s+\|\s+VCs:\s+(\d+)', line)
            if match:
                current_config = {
                    'bank': match.group(1),
                    'topology': match.group(2),
                    'vcs': int(match.group(3))
                }

        # Extract subarrays
        if "Subarrays:" in line:
            match = re.search(r'Subarrays:\s+(\d+)', line)
            if match:
                current_config['subarrays'] = int(match.group(1))

        # Extract switches info
        if "H-tree switches:" in line:
            match = re.search(r'H-tree switches:\s+(\d+)', line)
            if match:
                current_config['switches'] = int(match.group(1))

        # Extract timing metrics
        if "Total cycles:" in line:
            match = re.search(r'Total cycles:\s+(\d+)', line)
            if match:
                current_config['total_cycles'] = int(match.group(1))

        if "Transfer cycles (actual):" in line:
            match = re.search(r'Transfer cycles \(actual\):\s+(\d+)', line)
            if match:
                current_config['transfer_cycles'] = int(match.group(1))

        if "Transfer cycles (base):" in line:
            match = re.search(r'Transfer cycles \(base\):\s+(\d+)', line)
            if match:
                current_config['base_transfer_cycles'] = int(match.group(1))

        if "Contention overhead:" in line:
            match = re.search(r'Contention overhead:\s+(\d+)\s+cycles', line)
            if match:
                current_config['contention_cycles'] = int(match.group(1))

        if "Blocked cycles (VC):" in line:
            match = re.search(r'Blocked cycles \(VC\):\s+(\d+)', line)
            if match:
                current_config['blocked_cycles'] = int(match.group(1))

        if "Avg contention level:" in line:
            match = re.search(r'Avg contention level:\s+([\d.]+)', line)
            if match:
                current_config['avg_contention'] = float(match.group(1))

        # Save complete config when we hit validation
        if "✓ Reduction validated" in line and current_config:
            results.append(current_config.copy())
            current_config = {}

        i += 1

    return results

def generate_summary_table(results: List[Dict]):
    """Generate a formatted summary table"""

    print("\n" + "="*100)
    print("VIRTUAL CHANNEL IMPACT SUMMARY")
    print("="*100)

    # Group by bank size
    banks = {}
    for r in results:
        bank = r.get('bank', 'Unknown')
        if bank not in banks:
            banks[bank] = []
        banks[bank].append(r)

    for bank_name in sorted(banks.keys()):
        configs = banks[bank_name]

        # Get bank info
        sa = configs[0].get('subarrays', 0)
        switches = configs[0].get('switches', 0)

        print(f"\n{bank_name}: {sa} Subarrays")
        if switches > 0:
            print(f"  H-tree switches: {switches} ({sa}-1)")
            print(f"  Control overhead: {switches * 2} bits ({switches} switches × 2-bit control)")
        print()

        # Table header
        print(f"{'Topology':<12} {'VCs':<5} {'Total Cycles':<15} {'Transfer Cycles':<18} {'Contention':<15} {'Blocked (VC)':<15} {'Avg Contention':<15}")
        print("-" * 100)

        # Sort: LIBCom first, then H-tree by VCs
        libcom = [c for c in configs if c.get('topology') == 'LIBCom']
        htree = sorted([c for c in configs if c.get('topology') == 'H-tree'],
                      key=lambda x: x.get('vcs', 0))

        baseline_cycles = None
        for c in libcom + htree:
            topo = c.get('topology', 'Unknown')
            vcs = c.get('vcs', 0)
            total = c.get('total_cycles', 0)
            transfer = c.get('transfer_cycles', 0)
            base_transfer = c.get('base_transfer_cycles', 0)
            contention = c.get('contention_cycles', 0)
            blocked = c.get('blocked_cycles', 0)
            avg_cont = c.get('avg_contention', 0.0)

            # Track LIBCom as baseline
            if topo == 'LIBCom' and baseline_cycles is None:
                baseline_cycles = total

            # Format with improvement vs baseline
            if baseline_cycles and topo == 'H-tree':
                overhead = total - baseline_cycles
                overhead_pct = (overhead / baseline_cycles) * 100
                total_str = f"{total:,} (+{overhead_pct:.1f}%)"
            else:
                total_str = f"{total:,}"

            transfer_str = f"{transfer} (base: {base_transfer})"

            print(f"{topo:<12} {vcs:<5} {total_str:<15} {transfer_str:<18} {contention:<15} {blocked:<15} {avg_cont:<15.2f}")

        # VC improvement analysis for H-tree
        htree_configs = {c['vcs']: c for c in htree}
        if 1 in htree_configs and len(htree_configs) > 1:
            print()
            print("  VC Improvement (vs 1 VC):")
            base_1vc = htree_configs[1]
            for vcs in sorted([v for v in htree_configs.keys() if v > 1]):
                config = htree_configs[vcs]

                # Calculate improvements
                contention_reduction = base_1vc.get('contention_cycles', 0) - config.get('contention_cycles', 0)
                blocked_reduction = base_1vc.get('blocked_cycles', 0) - config.get('blocked_cycles', 0)

                contention_pct = (contention_reduction / base_1vc.get('contention_cycles', 1)) * 100
                blocked_pct = (blocked_reduction / base_1vc.get('blocked_cycles', 1)) * 100

                print(f"    {vcs} VCs: {contention_reduction} fewer contention cycles ({contention_pct:.1f}% reduction)")
                print(f"           {blocked_reduction} fewer blocked cycles ({blocked_pct:.1f}% reduction)")

def generate_paper_table(results: List[Dict]):
    """Generate LaTeX-style table for paper"""

    print("\n" + "="*100)
    print("TABLE FOR PAPER (LaTeX format)")
    print("="*100)
    print()

    print("% Virtual Channel Impact on H-tree Performance")
    print("\\begin{table}[h]")
    print("\\centering")
    print("\\begin{tabular}{|l|c|c|c|c|c|}")
    print("\\hline")
    print("\\textbf{Bank} & \\textbf{Subarrays} & \\textbf{Switches} & \\textbf{VCs} & \\textbf{Blocked Cycles} & \\textbf{Overhead vs LIBCom} \\\\")
    print("\\hline")

    # Group and organize
    banks_data = {}
    for r in results:
        if r.get('topology') == 'H-tree':
            bank = r.get('bank', 'Unknown')
            if bank not in banks_data:
                banks_data[bank] = {'configs': [], 'libcom_cycles': None}
            banks_data[bank]['configs'].append(r)
        elif r.get('topology') == 'LIBCom':
            bank = r.get('bank', 'Unknown')
            if bank not in banks_data:
                banks_data[bank] = {'configs': [], 'libcom_cycles': None}
            banks_data[bank]['libcom_cycles'] = r.get('total_cycles', 0)

    for bank_name in sorted(banks_data.keys()):
        data = banks_data[bank_name]
        configs = sorted(data['configs'], key=lambda x: x.get('vcs', 0))
        libcom = data['libcom_cycles']

        if not configs:
            continue

        sa = configs[0].get('subarrays', 0)
        switches = configs[0].get('switches', 0)

        first = True
        for c in configs:
            vcs = c.get('vcs', 0)
            blocked = c.get('blocked_cycles', 0)
            total = c.get('total_cycles', 0)

            if libcom:
                overhead = ((total - libcom) / libcom) * 100
                overhead_str = f"{overhead:.1f}\\%"
            else:
                overhead_str = "N/A"

            if first:
                print(f"{bank_name} & {sa} & {switches} & {vcs} & {blocked} & {overhead_str} \\\\")
                first = False
            else:
                print(f" & & & {vcs} & {blocked} & {overhead_str} \\\\")

        print("\\hline")

    print("\\end{tabular}")
    print("\\caption{Impact of Virtual Channels on H-tree network performance for different bank sizes}")
    print("\\label{tab:vc_impact}")
    print("\\end{table}")

def main():
    if len(sys.argv) < 2:
        print("Usage: analyze_vc_results.py <results_file>")
        sys.exit(1)

    filepath = sys.argv[1]

    try:
        results = parse_result_file(filepath)

        if not results:
            print("No results found in file")
            sys.exit(1)

        print(f"Parsed {len(results)} configurations")

        generate_summary_table(results)
        generate_paper_table(results)

    except FileNotFoundError:
        print(f"Error: File '{filepath}' not found")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()
