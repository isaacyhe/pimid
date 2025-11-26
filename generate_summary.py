#!/usr/bin/env python3
"""
Generate human-readable summary from architecture analysis.
"""

import json
import sys

def format_number(num, decimals=2):
    """Format a number with appropriate precision."""
    if num is None:
        return "N/A"
    if abs(num) >= 1000000:
        return f"{num/1000000:.{decimals}f}M"
    if abs(num) >= 1000:
        return f"{num/1000:.{decimals}f}K"
    return f"{num:.{decimals}f}"

def print_section(title, char='='):
    """Print a formatted section header."""
    print(f"\n{char * 80}")
    print(f"{title}")
    print(f"{char * 80}\n")

def print_comparison(metric_name, comparison, indent=""):
    """Print a comparison for a specific metric."""
    if not comparison:
        return

    mp = comparison['message_passing']
    sm = comparison['shared_memory']
    winner = comparison['winner']
    improvement = comparison['improvement_pct']
    unit = comparison.get('unit', '')

    print(f"{indent}{metric_name}:")
    print(f"{indent}  Message Passing: {format_number(mp['mean'])} {unit} (±{format_number(mp['stdev'])})")
    print(f"{indent}  Shared Memory:   {format_number(sm['mean'])} {unit} (±{format_number(sm['stdev'])})")
    print(f"{indent}  Winner: {winner.upper().replace('_', ' ')} ({format_number(abs(improvement), 1)}% {'better' if improvement > 0 else 'worse'})")
    print()

def main():
    # Load analysis report
    with open('/home/user/pimid-dev/architecture_analysis_v2.json', 'r') as f:
        # Skip stderr output, find JSON
        lines = f.readlines()
        json_lines = []
        in_json = False
        brace_count = 0

        for line in lines:
            if line.strip().startswith('{'):
                in_json = True
                brace_count = line.count('{') - line.count('}')
                json_lines.append(line)
            elif in_json:
                json_lines.append(line)
                brace_count += line.count('{') - line.count('}')
                if brace_count == 0:
                    break

        if not json_lines:
            print("Error: No JSON found in analysis file", file=sys.stderr)
            return

        report = json.loads(''.join(json_lines))

    print_section("MESSAGE PASSING vs SHARED MEMORY ARCHITECTURE COMPARISON", '=')

    # Summary
    summary = report['summary']
    print(f"Total Tests Analyzed: {summary['total_tests']:,}")
    print(f"  Message Passing Tests: {summary['message_passing_tests']:,}")
    print(f"  Shared Memory Tests:   {summary['shared_memory_tests']:,}")

    # Overall comparison
    print_section("OVERALL PERFORMANCE COMPARISON", '=')

    overall = report['overall_comparison']

    # Key metrics in order of importance
    key_metrics = [
        'execution_time_ns',
        'total_energy_pj',
        'total_cycles',
        'network_energy_pj',
        'power_mw'
    ]

    for metric in key_metrics:
        if metric in overall:
            print_comparison(metric.replace('_', ' ').title(), overall[metric])

    # Summary of overall winner
    print("\nOVERALL WINNER:")
    time_winner = overall.get('execution_time_ns', {}).get('winner', 'unknown')
    energy_winner = overall.get('total_energy_pj', {}).get('winner', 'unknown')

    if time_winner == energy_winner:
        print(f"  {time_winner.upper().replace('_', ' ')} is consistently better for both performance and energy")
    else:
        print(f"  Performance: {time_winner.upper().replace('_', ' ')}")
        print(f"  Energy: {energy_winner.upper().replace('_', ' ')}")

    # By memory technology
    print_section("PERFORMANCE BY MEMORY TECHNOLOGY", '=')

    by_mem = report['by_memory_technology']
    for mem_tech in sorted(by_mem.keys()):
        print(f"\n{mem_tech}:")
        print("-" * 60)

        comparisons = by_mem[mem_tech]

        # Show key metrics
        for metric in ['execution_time_ns', 'total_energy_pj']:
            if metric in comparisons:
                print_comparison(metric.replace('_', ' ').title(), comparisons[metric], indent="  ")

    # By PIM level
    print_section("PERFORMANCE BY PIM LEVEL", '=')

    by_pim = report['by_pim_level']
    for pim_level in sorted(by_pim.keys()):
        print(f"\n{pim_level}:")
        print("-" * 60)

        comparisons = by_pim[pim_level]

        # Show key metrics
        for metric in ['execution_time_ns', 'total_energy_pj']:
            if metric in comparisons:
                print_comparison(metric.replace('_', ' ').title(), comparisons[metric], indent="  ")

    # By workload type
    print_section("PERFORMANCE BY WORKLOAD TYPE", '=')

    by_workload = report['by_workload_type']
    for workload in sorted(by_workload.keys()):
        print(f"\n{workload.upper()}:")
        print("-" * 60)

        comparisons = by_workload[workload]

        # Show key metrics
        for metric in ['execution_time_ns', 'total_energy_pj']:
            if metric in comparisons:
                print_comparison(metric.replace('_', ' ').title(), comparisons[metric], indent="  ")

    # Key insights
    print_section("KEY INSIGHTS AND PATTERNS", '=')

    # Analyze patterns
    print("1. CONSISTENCY ACROSS MEMORY TECHNOLOGIES:\n")
    time_winners_by_mem = {}
    energy_winners_by_mem = {}

    for mem_tech, comparisons in by_mem.items():
        if 'execution_time_ns' in comparisons:
            time_winners_by_mem[mem_tech] = comparisons['execution_time_ns']['winner']
        if 'total_energy_pj' in comparisons:
            energy_winners_by_mem[mem_tech] = comparisons['total_energy_pj']['winner']

    # Count winners
    from collections import Counter
    time_winner_counts = Counter(time_winners_by_mem.values())
    energy_winner_counts = Counter(energy_winners_by_mem.values())

    print(f"   Performance winner frequency:")
    for winner, count in time_winner_counts.most_common():
        print(f"     {winner.replace('_', ' ').title()}: {count}/{len(time_winners_by_mem)} memory technologies")

    print(f"\n   Energy winner frequency:")
    for winner, count in energy_winner_counts.most_common():
        print(f"     {winner.replace('_', ' ').title()}: {count}/{len(energy_winner_counts)} memory technologies")

    print("\n2. CONSISTENCY ACROSS WORKLOAD TYPES:\n")
    time_winners_by_workload = {}
    energy_winners_by_workload = {}

    for workload, comparisons in by_workload.items():
        if 'execution_time_ns' in comparisons:
            time_winners_by_workload[workload] = comparisons['execution_time_ns']['winner']
        if 'total_energy_pj' in comparisons:
            energy_winners_by_workload[workload] = comparisons['total_energy_pj']['winner']

    time_workload_counts = Counter(time_winners_by_workload.values())
    energy_workload_counts = Counter(energy_winners_by_workload.values())

    print(f"   Performance winner frequency:")
    for winner, count in time_workload_counts.most_common():
        print(f"     {winner.replace('_', ' ').title()}: {count}/{len(time_winners_by_workload)} workload types")

    print(f"\n   Energy winner frequency:")
    for winner, count in energy_workload_counts.most_common():
        print(f"     {winner.replace('_', ' ').title()}: {count}/{len(energy_workload_counts)} workload types")

    print("\n3. SPECIFIC PERFORMANCE ADVANTAGES:\n")

    # Find best improvements
    best_time_improvement = None
    best_time_category = None
    best_energy_improvement = None
    best_energy_category = None

    for category_name, category_data in [
        ('memory_technology', by_mem),
        ('pim_level', by_pim),
        ('workload_type', by_workload)
    ]:
        for key, comparisons in category_data.items():
            if 'execution_time_ns' in comparisons:
                imp = comparisons['execution_time_ns']['improvement_pct']
                if best_time_improvement is None or imp > best_time_improvement:
                    best_time_improvement = imp
                    best_time_category = f"{category_name}: {key}"

            if 'total_energy_pj' in comparisons:
                imp = comparisons['total_energy_pj']['improvement_pct']
                if best_energy_improvement is None or imp > best_energy_improvement:
                    best_energy_improvement = imp
                    best_energy_category = f"{category_name}: {key}"

    if best_time_improvement:
        print(f"   Best performance improvement: {format_number(best_time_improvement, 1)}%")
        print(f"     Category: {best_time_category}")

    if best_energy_improvement:
        print(f"\n   Best energy improvement: {format_number(best_energy_improvement, 1)}%")
        print(f"     Category: {best_energy_category}")

    print("\n" + "=" * 80 + "\n")

if __name__ == '__main__':
    main()
