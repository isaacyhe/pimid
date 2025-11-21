#!/usr/bin/env python3
"""
Generate detailed insights and additional analysis.
"""

import json

def load_report():
    """Load the analysis report."""
    with open('/home/user/pimid-dev/architecture_analysis_v2.json', 'r') as f:
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

        return json.loads(''.join(json_lines))

def main():
    report = load_report()

    print("=" * 80)
    print("DETAILED INSIGHTS: MESSAGE PASSING vs SHARED MEMORY")
    print("=" * 80)

    # 1. When does shared memory win?
    print("\n1. SCENARIOS WHERE SHARED MEMORY OUTPERFORMS MESSAGE PASSING:")
    print("-" * 80)

    by_mem = report['by_memory_technology']
    by_workload = report['by_workload_type']

    print("\n   Memory Technologies (by execution time):")
    sm_wins_mem = []
    for mem_tech, comparisons in by_mem.items():
        if 'execution_time_ns' in comparisons:
            if comparisons['execution_time_ns']['winner'] == 'shared_memory':
                improvement = comparisons['execution_time_ns']['improvement_pct']
                sm_wins_mem.append((mem_tech, improvement))

    if sm_wins_mem:
        for mem_tech, improvement in sorted(sm_wins_mem, key=lambda x: x[1], reverse=True):
            print(f"     • {mem_tech}: {improvement:.1f}% faster")
    else:
        print("     • None")

    print("\n   Workload Types (by execution time):")
    sm_wins_workload = []
    for workload, comparisons in by_workload.items():
        if 'execution_time_ns' in comparisons:
            if comparisons['execution_time_ns']['winner'] == 'shared_memory':
                improvement = comparisons['execution_time_ns']['improvement_pct']
                sm_wins_workload.append((workload, improvement))

    if sm_wins_workload:
        for workload, improvement in sorted(sm_wins_workload, key=lambda x: x[1], reverse=True):
            print(f"     • {workload.upper()}: {improvement:.1f}% faster")
    else:
        print("     • None")

    # 2. Energy vs Performance Trade-offs
    print("\n\n2. ENERGY vs PERFORMANCE TRADE-OFFS:")
    print("-" * 80)
    print("\n   Memory technologies where winners differ:")

    tradeoffs = []
    for mem_tech, comparisons in by_mem.items():
        if 'execution_time_ns' in comparisons and 'total_energy_pj' in comparisons:
            time_winner = comparisons['execution_time_ns']['winner']
            energy_winner = comparisons['total_energy_pj']['winner']
            if time_winner != energy_winner:
                time_imp = comparisons['execution_time_ns']['improvement_pct']
                energy_imp = comparisons['total_energy_pj']['improvement_pct']
                tradeoffs.append((mem_tech, time_winner, time_imp, energy_winner, energy_imp))

    if tradeoffs:
        for mem_tech, time_winner, time_imp, energy_winner, energy_imp in tradeoffs:
            print(f"\n     {mem_tech}:")
            print(f"       Performance: {time_winner.replace('_', ' ').title()} ({time_imp:.1f}% better)")
            print(f"       Energy:      {energy_winner.replace('_', ' ').title()} ({energy_imp:.1f}% better)")
    else:
        print("     • No significant trade-offs found")

    # 3. Best and worst case scenarios
    print("\n\n3. BEST AND WORST CASE SCENARIOS:")
    print("-" * 80)

    # Find best improvements for message passing
    mp_best_perf = None
    mp_best_perf_cat = None
    mp_best_energy = None
    mp_best_energy_cat = None

    for category_name, category_data in [
        ('Memory Tech', by_mem),
        ('Workload', by_workload)
    ]:
        for key, comparisons in category_data.items():
            if 'execution_time_ns' in comparisons:
                if comparisons['execution_time_ns']['winner'] == 'message_passing':
                    imp = comparisons['execution_time_ns']['improvement_pct']
                    if mp_best_perf is None or imp > mp_best_perf:
                        mp_best_perf = imp
                        mp_best_perf_cat = f"{category_name}: {key}"

            if 'total_energy_pj' in comparisons:
                if comparisons['total_energy_pj']['winner'] == 'message_passing':
                    imp = comparisons['total_energy_pj']['improvement_pct']
                    if mp_best_energy is None or imp > mp_best_energy:
                        mp_best_energy = imp
                        mp_best_energy_cat = f"{category_name}: {key}"

    # Find worst case (where message passing loses most)
    mp_worst_perf = None
    mp_worst_perf_cat = None
    mp_worst_energy = None
    mp_worst_energy_cat = None

    for category_name, category_data in [
        ('Memory Tech', by_mem),
        ('Workload', by_workload)
    ]:
        for key, comparisons in category_data.items():
            if 'execution_time_ns' in comparisons:
                if comparisons['execution_time_ns']['winner'] == 'shared_memory':
                    imp = comparisons['execution_time_ns']['improvement_pct']
                    if mp_worst_perf is None or imp > mp_worst_perf:
                        mp_worst_perf = imp
                        mp_worst_perf_cat = f"{category_name}: {key}"

            if 'total_energy_pj' in comparisons:
                if comparisons['total_energy_pj']['winner'] == 'shared_memory':
                    imp = comparisons['total_energy_pj']['improvement_pct']
                    if mp_worst_energy is None or imp > mp_worst_energy:
                        mp_worst_energy = imp
                        mp_worst_energy_cat = f"{category_name}: {key}"

    print("\n   MESSAGE PASSING - Best Cases:")
    if mp_best_perf:
        print(f"     • Performance: {mp_best_perf:.1f}% faster ({mp_best_perf_cat})")
    if mp_best_energy:
        print(f"     • Energy:      {mp_best_energy:.1f}% better ({mp_best_energy_cat})")

    print("\n   MESSAGE PASSING - Worst Cases (loses to shared memory):")
    if mp_worst_perf:
        print(f"     • Performance: {mp_worst_perf:.1f}% slower ({mp_worst_perf_cat})")
    else:
        print(f"     • Performance: No significant losses")
    if mp_worst_energy:
        print(f"     • Energy:      {mp_worst_energy:.1f}% worse ({mp_worst_energy_cat})")
    else:
        print(f"     • Energy:      No significant losses")

    # 4. Network energy analysis
    print("\n\n4. NETWORK ENERGY ANALYSIS:")
    print("-" * 80)

    overall = report['overall_comparison']
    if 'network_energy_pj' in overall:
        mp_net = overall['network_energy_pj']['message_passing']['mean']
        sm_net = overall['network_energy_pj']['shared_memory']['mean']
        mp_total = overall['total_energy_pj']['message_passing']['mean']
        sm_total = overall['total_energy_pj']['shared_memory']['mean']

        mp_net_pct = (mp_net / mp_total) * 100 if mp_total > 0 else 0
        sm_net_pct = (sm_net / sm_total) * 100 if sm_total > 0 else 0

        print(f"\n   Network Energy as % of Total Energy:")
        print(f"     • Message Passing: {mp_net_pct:.1f}%")
        print(f"     • Shared Memory:   {sm_net_pct:.1f}%")
        print(f"\n   Shared memory has {sm_net_pct - mp_net_pct:.1f}% MORE network overhead")

    # 5. Recommendations
    print("\n\n5. ARCHITECTURE SELECTION RECOMMENDATIONS:")
    print("-" * 80)

    print("\n   USE MESSAGE PASSING when:")
    print("     • Working with SRAM-based systems (145.9% energy advantage)")
    print("     • Running BFS workloads (1,872% performance advantage)")
    print("     • Running histogram operations (151.9% performance advantage)")
    print("     • Running prefix sum operations (255% performance advantage)")
    print("     • Energy efficiency is critical (consistently 55% better overall)")
    print("     • Working with DDR4/DDR5 memory (26-82% performance advantage)")
    print("     • Using GDDR6 memory (64.3% performance advantage)")

    print("\n   USE SHARED MEMORY when:")
    if sm_wins_workload:
        for workload, improvement in sorted(sm_wins_workload, key=lambda x: x[1], reverse=True):
            print(f"     • Running {workload.upper()} workloads ({improvement:.1f}% performance advantage)")
    if sm_wins_mem:
        for mem_tech, improvement in sorted(sm_wins_mem, key=lambda x: x[1], reverse=True):
            print(f"     • Working with {mem_tech} ({improvement:.1f}% performance advantage)")

    print("\n   CONSIDER TRADE-OFFS for:")
    print("     • HBM memory (shared memory is 32% faster but message passing uses 2% less energy)")
    print("     • HBM3 memory (shared memory is 12% faster but message passing uses 5% less energy)")
    print("     • GEMM workloads (shared memory is 3% faster but message passing uses 68% less energy)")

    print("\n" + "=" * 80 + "\n")

if __name__ == '__main__':
    main()
