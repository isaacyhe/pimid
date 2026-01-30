#!/usr/bin/env python3
"""
Topology Analysis Script

Analyzes the network properties of each topology by examining
the implementation code and calculating theoretical properties.
"""

import re
import math

def analyze_htree():
    """Analyze HTree topology properties"""
    print("\n" + "="*70)
    print("HTree Topology Analysis")
    print("="*70)

    for num_nodes in [4, 8, 16, 32]:
        tree_depth = int(math.ceil(math.log2(num_nodes)))
        total_routers = 2 ** tree_depth
        diameter = 2 * (tree_depth - 1)  # Root to leaf and back

        # Links in a binary tree: 2 * (internal nodes)
        internal_nodes = total_routers // 2
        num_links_unidirectional = 2 * internal_nodes
        num_links_bidirectional = 2 * num_links_unidirectional

        print(f"\n{num_nodes} nodes:")
        print(f"  Tree depth: {tree_depth}")
        print(f"  Total routers: {total_routers}")
        print(f"  Diameter: {diameter} hops")
        print(f"  Internal links: {num_links_bidirectional} (bidirectional)")
        print(f"  Average hops: ~{tree_depth:.1f}")

def analyze_bus():
    """Analyze Bus topology properties"""
    print("\n" + "="*70)
    print("Bus Topology Analysis")
    print("="*70)

    for num_nodes in [4, 8, 16, 32]:
        routers = 1
        diameter = 2  # Node -> Bus -> Node
        num_ext_links = num_nodes
        num_int_links = 0

        print(f"\n{num_nodes} nodes:")
        print(f"  Routers: {routers}")
        print(f"  Diameter: {diameter} hops (through central router)")
        print(f"  External links: {num_ext_links}")
        print(f"  Internal links: {num_int_links}")
        print(f"  Average hops: 2")

def analyze_ringbus():
    """Analyze RingBus topology properties"""
    print("\n" + "="*70)
    print("RingBus Topology Analysis")
    print("="*70)

    for num_nodes in [4, 8, 16, 32]:
        routers = num_nodes
        diameter = num_nodes // 2

        # Each router connects to 2 neighbors, bidirectional
        num_int_links = num_nodes * 2  # Each link counted twice (bidirectional)

        print(f"\n{num_nodes} nodes:")
        print(f"  Routers: {routers}")
        print(f"  Diameter: {diameter} hops")
        print(f"  Internal links: {num_int_links} (bidirectional ring)")
        print(f"  Average hops: ~{diameter/2:.1f}")
        print(f"  Bisection bandwidth: 2 links")

def analyze_htreebus():
    """Analyze HTreeBus topology properties"""
    print("\n" + "="*70)
    print("HTreeBus Topology Analysis")
    print("="*70)

    for num_nodes in [4, 8, 16, 32]:
        tree_depth = int(math.ceil(math.log2(num_nodes)))
        total_routers = 2 ** tree_depth

        # Tree links (same as HTree)
        internal_nodes = total_routers // 2
        tree_links = 2 * internal_nodes * 2  # Bidirectional

        # Bus links at each level
        bus_links = 0
        for level in range(tree_depth):
            level_size = 2 ** level
            bus_links += 2 * (level_size - 1)  # Adjacent connections, bidirectional

        total_int_links = tree_links + bus_links

        # Diameter: Can use bus shortcuts
        diameter_tree = 2 * (tree_depth - 1)
        diameter_with_bus = tree_depth  # Approximate

        print(f"\n{num_nodes} nodes:")
        print(f"  Tree depth: {tree_depth}")
        print(f"  Total routers: {total_routers}")
        print(f"  Diameter: ~{diameter_with_bus} hops (with bus shortcuts)")
        print(f"  Tree links: {tree_links}")
        print(f"  Bus links: {bus_links}")
        print(f"  Total internal links: {total_int_links}")
        print(f"  Average hops: ~{diameter_with_bus/2:.1f}")

def compare_topologies():
    """Generate comparison table"""
    print("\n" + "="*70)
    print("Topology Comparison (16 nodes)")
    print("="*70)

    comparisons = [
        ("Topology", "Routers", "Diameter", "Links", "Complexity"),
        ("-" * 15, "-" * 7, "-" * 8, "-" * 5, "-" * 10),
        ("HTree", "16", "6", "30", "O(n)"),
        ("Bus", "1", "2", "0", "O(1)"),
        ("RingBus", "16", "8", "32", "O(n)"),
        ("HTreeBus", "16", "~4", "54", "O(n log n)"),
    ]

    for row in comparisons:
        print(f"  {row[0]:15} {row[1]:7} {row[2]:8} {row[3]:5} {row[4]:10}")

def routing_efficiency():
    """Analyze routing efficiency"""
    print("\n" + "="*70)
    print("Routing Efficiency Analysis")
    print("="*70)

    print("\nTraffic Pattern Suitability:")
    print("  Uniform Random:")
    print("    HTree:     Good     (balanced tree structure)")
    print("    Bus:       Poor     (central bottleneck)")
    print("    RingBus:   Fair     (limited bandwidth)")
    print("    HTreeBus:  Excellent (multiple paths)")

    print("\n  Neighbor Communication:")
    print("    HTree:     Poor     (must go through parent)")
    print("    Bus:       Good     (direct through center)")
    print("    RingBus:   Excellent (adjacent routers)")
    print("    HTreeBus:  Excellent (bus shortcuts)")

    print("\n  Broadcast:")
    print("    HTree:     Good     (tree propagation)")
    print("    Bus:       Excellent (single hop from center)")
    print("    RingBus:   Fair     (sequential propagation)")
    print("    HTreeBus:  Good     (tree + bus propagation)")

def power_analysis():
    """Analyze power consumption"""
    print("\n" + "="*70)
    print("Relative Power Consumption (16 nodes)")
    print("="*70)

    print("\n  Topology       Router Power   Link Power    Total")
    print("  " + "-" * 60)
    print("  HTree          16 routers     30 links      Moderate")
    print("  Bus            1 router       16 ext links  Low")
    print("  RingBus        16 routers     32 links      Moderate")
    print("  HTreeBus       16 routers     54 links      Higher")

    print("\n  Notes:")
    print("  - More routers = higher static power")
    print("  - More links = higher dynamic power")
    print("  - Bus has lowest power but worst performance")
    print("  - HTreeBus trades power for performance")

def main():
    """Run all analyses"""
    print("="*70)
    print("GARNET TOPOLOGY ANALYSIS SUITE")
    print("="*70)

    analyze_htree()
    analyze_bus()
    analyze_ringbus()
    analyze_htreebus()
    compare_topologies()
    routing_efficiency()
    power_analysis()

    print("\n" + "="*70)
    print("Analysis Complete")
    print("="*70)
    print("\nRecommendations:")
    print("  • Use Bus for: Testing, very small systems (<4 nodes)")
    print("  • Use HTree for: Hierarchical applications, balanced workloads")
    print("  • Use RingBus for: Sequential processing, fault tolerance")
    print("  • Use HTreeBus for: General-purpose, high-performance NoC")
    print()

if __name__ == '__main__':
    main()
