#!/usr/bin/env python3
"""
Generate 10,000 comprehensive test configuration files covering:
- All network topologies
- All memory technologies
- All PIM placement levels
- All core models
- All scheduling policies
- Various PE counts and configurations
"""

import os
import yaml
import itertools
import random
from pathlib import Path

# Network topology types
TOPOLOGIES = ["BUS", "CROSSBAR", "MESH_2D", "TORUS_2D", "FAT_TREE", "H_TREE"]

# Memory technologies with realistic configurations
DRAM_CONFIGS = [
    # DDR4 variants
    {"type": "DDR4", "channels": 1, "ranks": 1, "chips": 8, "bgs": 4, "banks": 4, "subarrays": 16},
    {"type": "DDR4", "channels": 2, "ranks": 2, "chips": 8, "bgs": 4, "banks": 4, "subarrays": 16},
    {"type": "DDR4", "channels": 4, "ranks": 2, "chips": 16, "bgs": 4, "banks": 4, "subarrays": 32},

    # DDR5 variants
    {"type": "DDR5", "channels": 2, "ranks": 1, "chips": 8, "bgs": 8, "banks": 4, "subarrays": 16},
    {"type": "DDR5", "channels": 4, "ranks": 2, "chips": 8, "bgs": 8, "banks": 4, "subarrays": 32},
    {"type": "DDR5", "channels": 8, "ranks": 2, "chips": 16, "bgs": 8, "banks": 4, "subarrays": 32},

    # HBM2 variants
    {"type": "HBM2", "channels": 8, "ranks": 2, "chips": 2, "bgs": 4, "banks": 4, "subarrays": 16},
    {"type": "HBM2", "channels": 16, "ranks": 2, "chips": 4, "bgs": 4, "banks": 4, "subarrays": 16},

    # HBM3 variants
    {"type": "HBM3", "channels": 16, "ranks": 2, "chips": 2, "bgs": 8, "banks": 4, "subarrays": 16},
    {"type": "HBM3", "channels": 32, "ranks": 2, "chips": 4, "bgs": 8, "banks": 4, "subarrays": 32},

    # GDDR6 variants
    {"type": "GDDR6", "channels": 2, "ranks": 1, "chips": 4, "bgs": 4, "banks": 4, "subarrays": 16},
    {"type": "GDDR6", "channels": 4, "ranks": 2, "chips": 8, "bgs": 4, "banks": 4, "subarrays": 16},

    # LPDDR5 variants
    {"type": "LPDDR5", "channels": 4, "ranks": 1, "chips": 8, "bgs": 8, "banks": 4, "subarrays": 16},
    {"type": "LPDDR5", "channels": 8, "ranks": 2, "chips": 8, "bgs": 8, "banks": 4, "subarrays": 32},
]

# PIM placement levels
PIM_PLACEMENTS = ["subarray", "bank", "chip", "rank"]

# Core models for PEs
CORE_MODELS = ["in-order", "out-of-order", "simple"]

# Scheduling policies
SCHEDULING_POLICIES = ["nearest", "round_robin", "load_balanced", "priority_based", "data_aware"]

# PE count ranges (will be calculated based on placement and hierarchy)
PE_COUNT_MULTIPLIERS = [1, 2, 4, 8]

# Port count configurations
PORT_CONFIGS = [4, 8, 16, 32, 64]

def calculate_pe_count(dram_config, placement, multiplier=1):
    """Calculate number of PEs based on placement level and DRAM hierarchy"""
    if placement == "subarray":
        count = dram_config["channels"] * dram_config["ranks"] * dram_config["chips"] * \
                dram_config["bgs"] * dram_config["banks"] * dram_config["subarrays"]
    elif placement == "bank":
        count = dram_config["channels"] * dram_config["ranks"] * dram_config["chips"] * \
                dram_config["bgs"] * dram_config["banks"]
    elif placement == "chip":
        count = dram_config["channels"] * dram_config["ranks"] * dram_config["chips"]
    elif placement == "rank":
        count = dram_config["channels"] * dram_config["ranks"]
    else:
        count = dram_config["channels"]

    return max(1, count * multiplier)

def calculate_optimal_switches(num_endpoints, max_ports, topology):
    """Calculate optimal number of switches for given constraints"""
    if topology in ["BUS", "CROSSBAR"]:
        return max(1, (num_endpoints + max_ports - 1) // max_ports)
    elif topology in ["MESH_2D", "TORUS_2D"]:
        import math
        side = int(math.ceil(math.sqrt(num_endpoints)))
        return side * side
    elif topology == "H_TREE":
        return max(1, num_endpoints - 1)
    elif topology == "FAT_TREE":
        return max(1, num_endpoints)
    else:
        return max(1, num_endpoints // max_ports)

def calculate_ports_per_switch(topology, num_endpoints, num_switches):
    """Calculate ports per switch for topology"""
    if topology in ["BUS", "CROSSBAR"]:
        return max(2, num_endpoints // num_switches)
    elif topology in ["MESH_2D", "TORUS_2D"]:
        return 5
    elif topology == "H_TREE":
        return 4
    elif topology == "FAT_TREE":
        return min(16, max(4, num_endpoints // num_switches + 2))
    else:
        return max(2, num_endpoints // num_switches)

def generate_level_config(level, topology, num_endpoints, max_ports=None):
    """Generate configuration for a single network level"""
    if max_ports is None:
        max_ports = num_endpoints

    num_switches = calculate_optimal_switches(num_endpoints, max_ports, topology)
    ports_per_switch = calculate_ports_per_switch(topology, num_endpoints, num_switches)

    return {
        "use_default": False,
        "topology": topology,
        "num_switches": num_switches,
        "ports_per_switch": ports_per_switch,
        "num_endpoints": num_endpoints
    }

def generate_test_config(test_id, test_name, dram_config, pim_config, topo_config=None):
    """Generate a complete test configuration"""

    config = {
        "simulation": {
            "name": test_name,
            "mode": "standalone",
            "duration_cycles": 10000,
            "workload": "build/test/workloads/test_pim_workloads"
        },
        "memory": {
            "dram_type": dram_config["type"],
            "channels": dram_config["channels"],
            "ranks_per_channel": dram_config["ranks"],
            "chips_per_rank": dram_config["chips"],
            "bg_per_chip": dram_config["bgs"],
            "banks_per_bg": dram_config["banks"],
            "subarrays_per_bank": dram_config["subarrays"]
        },
        "pim": {
            "enabled": True,
            "num_pes": pim_config["num_pes"],
            "pe_placement": pim_config["placement"],
            "scheduling_policy": pim_config["scheduling_policy"],
            "core_model": pim_config["core_model"]
        }
    }

    # Add custom topology if specified
    if topo_config:
        config["memory"]["network"] = {
            "custom_topology": {
                "enabled": True
            }
        }

        custom_topo = config["memory"]["network"]["custom_topology"]

        # L0: Banks within BG
        if "l0" in topo_config:
            num_endpoints = dram_config["banks"]
            custom_topo["l0"] = generate_level_config(
                0, topo_config["l0"], num_endpoints, topo_config.get("l0_ports"))
        else:
            custom_topo["l0"] = {"use_default": True}

        # L1: BGs within chip
        if "l1" in topo_config:
            num_endpoints = dram_config["bgs"] * dram_config["banks"]
            custom_topo["l1"] = generate_level_config(
                1, topo_config["l1"], num_endpoints, topo_config.get("l1_ports"))
        else:
            custom_topo["l1"] = {"use_default": True}

        # L2: Chips within rank
        if "l2" in topo_config:
            num_endpoints = dram_config["chips"]
            custom_topo["l2"] = generate_level_config(
                2, topo_config["l2"], num_endpoints, topo_config.get("l2_ports"))
        else:
            custom_topo["l2"] = {"use_default": True}

        # L3: Ranks within channel
        if "l3" in topo_config:
            num_endpoints = dram_config["ranks"]
            custom_topo["l3"] = generate_level_config(
                3, topo_config["l3"], num_endpoints, topo_config.get("l3_ports"))
        else:
            custom_topo["l3"] = {"use_default": True}

        # L4: Channels
        if "l4" in topo_config:
            num_endpoints = dram_config["channels"]
            custom_topo["l4"] = generate_level_config(
                4, topo_config["l4"], num_endpoints, topo_config.get("l4_ports"))
        else:
            custom_topo["l4"] = {"use_default": True}

        # L5: System level
        if "l5" in topo_config:
            custom_topo["l5"] = generate_level_config(
                5, topo_config["l5"], dram_config["channels"], topo_config.get("l5_ports"))
        else:
            custom_topo["l5"] = {"use_default": True}

    return config

def generate_test_suite():
    """Generate 10,000 comprehensive test configurations"""

    output_dir = Path("test/configs/comprehensive_10k_tests")
    output_dir.mkdir(parents=True, exist_ok=True)

    test_cases = []
    test_id = 0

    print("Generating 10,000 comprehensive test configurations...")
    print("Covering: topologies × memory techs × PIM levels × core models × scheduling policies")
    print("=" * 80)

    # Category 1: All memory technologies with default topology (1400 tests)
    # 14 DRAM configs × 4 placements × 5 scheduling × 2 core models = 560
    # Add variations with different PE multipliers = 1400
    print("\n[1/10] Generating memory technology sweep (1400 tests)...")
    for i in range(1400):
        dram = random.choice(DRAM_CONFIGS)
        placement = random.choice(PIM_PLACEMENTS)
        scheduling = random.choice(SCHEDULING_POLICIES)
        core_model = random.choice(CORE_MODELS)
        pe_multiplier = random.choice(PE_COUNT_MULTIPLIERS)

        num_pes = calculate_pe_count(dram, placement, pe_multiplier)

        test_cases.append({
            "name": f"memtech_{test_id:05d}",
            "dram": dram,
            "pim": {
                "placement": placement,
                "scheduling_policy": scheduling,
                "core_model": core_model,
                "num_pes": num_pes
            },
            "topology": None
        })
        test_id += 1

    # Category 2: Single-level topology variations across all techs (1500 tests)
    print("[2/10] Generating single-level topology tests (1500 tests)...")
    for i in range(1500):
        dram = random.choice(DRAM_CONFIGS)
        placement = random.choice(PIM_PLACEMENTS)
        scheduling = random.choice(SCHEDULING_POLICIES)
        core_model = random.choice(CORE_MODELS)
        topology = random.choice(TOPOLOGIES)
        level = random.choice(["l0", "l1", "l2", "l3", "l4", "l5"])
        max_ports = random.choice(PORT_CONFIGS)

        num_pes = calculate_pe_count(dram, placement)

        test_cases.append({
            "name": f"single_topo_{test_id:05d}",
            "dram": dram,
            "pim": {
                "placement": placement,
                "scheduling_policy": scheduling,
                "core_model": core_model,
                "num_pes": num_pes
            },
            "topology": {
                level: topology,
                f"{level}_ports": max_ports
            }
        })
        test_id += 1

    # Category 3: Multi-level topology combinations (1200 tests)
    print("[3/10] Generating multi-level topology tests (1200 tests)...")
    for i in range(1200):
        dram = random.choice(DRAM_CONFIGS)
        placement = random.choice(PIM_PLACEMENTS)
        scheduling = random.choice(SCHEDULING_POLICIES)
        core_model = random.choice(CORE_MODELS)

        # Select 2-4 levels to customize
        num_levels = random.randint(2, 4)
        levels = random.sample(["l0", "l1", "l2", "l3", "l4", "l5"], k=num_levels)

        topo_config = {}
        for level in levels:
            topo_config[level] = random.choice(TOPOLOGIES)
            topo_config[f"{level}_ports"] = random.choice(PORT_CONFIGS)

        num_pes = calculate_pe_count(dram, placement)

        test_cases.append({
            "name": f"multi_topo_{test_id:05d}",
            "dram": dram,
            "pim": {
                "placement": placement,
                "scheduling_policy": scheduling,
                "core_model": core_model,
                "num_pes": num_pes
            },
            "topology": topo_config
        })
        test_id += 1

    # Category 4: All core models with various configurations (1000 tests)
    print("[4/10] Generating core model variations (1000 tests)...")
    for i in range(1000):
        dram = random.choice(DRAM_CONFIGS)
        placement = random.choice(PIM_PLACEMENTS)
        scheduling = random.choice(SCHEDULING_POLICIES)
        # Cycle through all core models
        core_model = CORE_MODELS[i % len(CORE_MODELS)]

        num_pes = calculate_pe_count(dram, placement)

        # Half with topology, half without
        if i % 2 == 0:
            topo_config = {
                random.choice(["l1", "l2", "l3"]): random.choice(TOPOLOGIES)
            }
        else:
            topo_config = None

        test_cases.append({
            "name": f"coremodel_{test_id:05d}",
            "dram": dram,
            "pim": {
                "placement": placement,
                "scheduling_policy": scheduling,
                "core_model": core_model,
                "num_pes": num_pes
            },
            "topology": topo_config
        })
        test_id += 1

    # Category 5: All scheduling policies (1000 tests)
    print("[5/10] Generating scheduling policy variations (1000 tests)...")
    for i in range(1000):
        dram = random.choice(DRAM_CONFIGS)
        placement = random.choice(PIM_PLACEMENTS)
        # Cycle through all scheduling policies
        scheduling = SCHEDULING_POLICIES[i % len(SCHEDULING_POLICIES)]
        core_model = random.choice(CORE_MODELS)

        num_pes = calculate_pe_count(dram, placement)

        # Add some topology variation
        if i % 3 == 0:
            topo_config = {
                "l0": random.choice(TOPOLOGIES),
                "l2": random.choice(TOPOLOGIES)
            }
        else:
            topo_config = None

        test_cases.append({
            "name": f"scheduling_{test_id:05d}",
            "dram": dram,
            "pim": {
                "placement": placement,
                "scheduling_policy": scheduling,
                "core_model": core_model,
                "num_pes": num_pes
            },
            "topology": topo_config
        })
        test_id += 1

    # Category 6: All PIM placement levels (1000 tests)
    print("[6/10] Generating PIM placement variations (1000 tests)...")
    for i in range(1000):
        dram = random.choice(DRAM_CONFIGS)
        # Cycle through all placements
        placement = PIM_PLACEMENTS[i % len(PIM_PLACEMENTS)]
        scheduling = random.choice(SCHEDULING_POLICIES)
        core_model = random.choice(CORE_MODELS)
        pe_multiplier = random.choice(PE_COUNT_MULTIPLIERS)

        num_pes = calculate_pe_count(dram, placement, pe_multiplier)

        # Vary topology
        num_levels = random.randint(1, 3)
        levels = random.sample(["l0", "l1", "l2", "l3", "l4"], k=num_levels)
        topo_config = {}
        for level in levels:
            topo_config[level] = random.choice(TOPOLOGIES)

        test_cases.append({
            "name": f"placement_{test_id:05d}",
            "dram": dram,
            "pim": {
                "placement": placement,
                "scheduling_policy": scheduling,
                "core_model": core_model,
                "num_pes": num_pes
            },
            "topology": topo_config if topo_config else None
        })
        test_id += 1

    # Category 7: HBM-specific tests (600 tests)
    print("[7/10] Generating HBM-specific tests (600 tests)...")
    hbm_configs = [d for d in DRAM_CONFIGS if "HBM" in d["type"]]
    for i in range(600):
        dram = random.choice(hbm_configs)
        placement = random.choice(PIM_PLACEMENTS)
        scheduling = random.choice(SCHEDULING_POLICIES)
        core_model = random.choice(CORE_MODELS)

        num_pes = calculate_pe_count(dram, placement)

        # HBM often uses mesh/torus for high bandwidth
        topo_choices = ["MESH_2D", "TORUS_2D", "FAT_TREE"]
        num_levels = random.randint(2, 4)
        levels = random.sample(["l1", "l2", "l3", "l4"], k=num_levels)
        topo_config = {}
        for level in levels:
            topo_config[level] = random.choice(topo_choices)

        test_cases.append({
            "name": f"hbm_{test_id:05d}",
            "dram": dram,
            "pim": {
                "placement": placement,
                "scheduling_policy": scheduling,
                "core_model": core_model,
                "num_pes": num_pes
            },
            "topology": topo_config
        })
        test_id += 1

    # Category 8: DDR-specific tests (600 tests)
    print("[8/10] Generating DDR-specific tests (600 tests)...")
    ddr_configs = [d for d in DRAM_CONFIGS if "DDR" in d["type"]]
    for i in range(600):
        dram = random.choice(ddr_configs)
        placement = random.choice(PIM_PLACEMENTS)
        scheduling = random.choice(SCHEDULING_POLICIES)
        core_model = random.choice(CORE_MODELS)

        num_pes = calculate_pe_count(dram, placement)

        # DDR often uses simpler topologies
        topo_choices = ["BUS", "CROSSBAR", "H_TREE"]
        num_levels = random.randint(1, 3)
        levels = random.sample(["l0", "l1", "l2", "l3"], k=num_levels)
        topo_config = {}
        for level in levels:
            topo_config[level] = random.choice(topo_choices)

        test_cases.append({
            "name": f"ddr_{test_id:05d}",
            "dram": dram,
            "pim": {
                "placement": placement,
                "scheduling_policy": scheduling,
                "core_model": core_model,
                "num_pes": num_pes
            },
            "topology": topo_config
        })
        test_id += 1

    # Category 9: Comprehensive combinations (1200 tests)
    print("[9/10] Generating comprehensive combination tests (1200 tests)...")
    for i in range(1200):
        dram = random.choice(DRAM_CONFIGS)
        placement = random.choice(PIM_PLACEMENTS)
        scheduling = random.choice(SCHEDULING_POLICIES)
        core_model = random.choice(CORE_MODELS)
        pe_multiplier = random.choice(PE_COUNT_MULTIPLIERS)

        num_pes = calculate_pe_count(dram, placement, pe_multiplier)

        # Random number of topology levels
        num_levels = random.randint(0, 5)
        if num_levels > 0:
            levels = random.sample(["l0", "l1", "l2", "l3", "l4", "l5"], k=num_levels)
            topo_config = {}
            for level in levels:
                topo_config[level] = random.choice(TOPOLOGIES)
                topo_config[f"{level}_ports"] = random.choice(PORT_CONFIGS)
        else:
            topo_config = None

        test_cases.append({
            "name": f"comprehensive_{test_id:05d}",
            "dram": dram,
            "pim": {
                "placement": placement,
                "scheduling_policy": scheduling,
                "core_model": core_model,
                "num_pes": num_pes
            },
            "topology": topo_config
        })
        test_id += 1

    # Category 10: Stress tests with extreme configurations (500 tests)
    print("[10/10] Generating stress test configurations (500 tests)...")
    for i in range(500):
        # Use high-end memory configs
        high_end_configs = [d for d in DRAM_CONFIGS if d["channels"] >= 4]
        dram = random.choice(high_end_configs if high_end_configs else DRAM_CONFIGS)
        placement = random.choice(PIM_PLACEMENTS)
        scheduling = random.choice(SCHEDULING_POLICIES)
        core_model = random.choice(CORE_MODELS)
        # Higher PE multipliers for stress testing
        pe_multiplier = random.choice([4, 8])

        num_pes = calculate_pe_count(dram, placement, pe_multiplier)

        # All levels with topology
        topo_config = {
            "l0": random.choice(TOPOLOGIES),
            "l1": random.choice(TOPOLOGIES),
            "l2": random.choice(TOPOLOGIES),
            "l3": random.choice(TOPOLOGIES),
            "l4": random.choice(TOPOLOGIES),
            "l5": random.choice(TOPOLOGIES),
            "l0_ports": random.choice([8, 16, 32]),
            "l1_ports": random.choice([8, 16, 32]),
            "l2_ports": random.choice([16, 32, 64]),
            "l3_ports": random.choice([16, 32, 64]),
            "l4_ports": random.choice([32, 64]),
            "l5_ports": random.choice([32, 64])
        }

        test_cases.append({
            "name": f"stress_{test_id:05d}",
            "dram": dram,
            "pim": {
                "placement": placement,
                "scheduling_policy": scheduling,
                "core_model": core_model,
                "num_pes": num_pes
            },
            "topology": topo_config
        })
        test_id += 1

    print(f"\nGenerated {len(test_cases)} test case definitions")
    print("=" * 80)

    # Write config files
    print("\nWriting configuration files...")
    manifest = []

    for i, test_case in enumerate(test_cases):
        config = generate_test_config(
            i,
            test_case["name"],
            test_case["dram"],
            test_case["pim"],
            test_case["topology"]
        )

        filename = f"{test_case['name']}.yaml"
        filepath = output_dir / filename

        with open(filepath, 'w') as f:
            yaml.dump(config, f, default_flow_style=False, sort_keys=False)

        # Add to manifest
        manifest.append({
            "id": i,
            "name": test_case["name"],
            "file": filename,
            "dram_type": test_case["dram"]["type"],
            "pim_placement": test_case["pim"]["placement"],
            "core_model": test_case["pim"]["core_model"],
            "scheduling_policy": test_case["pim"]["scheduling_policy"],
            "category": test_case["name"].split('_')[0]
        })

        if (i + 1) % 1000 == 0:
            print(f"  Written {i + 1}/{len(test_cases)} files...")

    print(f"✓ All {len(test_cases)} configuration files written")

    # Write manifest
    manifest_file = output_dir / "test_manifest.yaml"
    with open(manifest_file, 'w') as f:
        yaml.dump({"tests": manifest, "total_count": len(manifest)}, f)

    print(f"✓ Manifest written to {manifest_file}")

    # Write summary
    summary_file = output_dir / "README.md"
    with open(summary_file, 'w') as f:
        f.write("# 10,000 Comprehensive Test Suite\n\n")
        f.write("## Overview\n\n")
        f.write(f"This directory contains {len(test_cases)} test configuration files for comprehensive\n")
        f.write("testing covering all dimensions:\n")
        f.write("- **Network Topologies**: BUS, CROSSBAR, MESH_2D, TORUS_2D, FAT_TREE, H_TREE\n")
        f.write("- **Memory Technologies**: DDR4, DDR5, HBM2, HBM3, GDDR6, LPDDR5\n")
        f.write("- **PIM Placement Levels**: subarray, bank, chip, rank\n")
        f.write("- **Core Models**: in-order, out-of-order, simple\n")
        f.write("- **Scheduling Policies**: nearest, round_robin, load_balanced, priority_based, data_aware\n\n")
        f.write("## Test Categories\n\n")
        f.write("1. **Memory technology sweep** (1400 tests): All memory techs with various PIM configs\n")
        f.write("2. **Single-level topology** (1500 tests): One topology level varied\n")
        f.write("3. **Multi-level topology** (1200 tests): 2-4 topology levels varied\n")
        f.write("4. **Core model variations** (1000 tests): All core models tested\n")
        f.write("5. **Scheduling policies** (1000 tests): All scheduling policies tested\n")
        f.write("6. **PIM placement levels** (1000 tests): All placement levels tested\n")
        f.write("7. **HBM-specific tests** (600 tests): HBM2/HBM3 focused\n")
        f.write("8. **DDR-specific tests** (600 tests): DDR4/DDR5 focused\n")
        f.write("9. **Comprehensive combinations** (1200 tests): Random comprehensive configs\n")
        f.write("10. **Stress tests** (500 tests): Extreme configurations\n\n")
        f.write("## Usage\n\n")
        f.write("```bash\n")
        f.write("# Run a single test\n")
        f.write("pimid --config test/configs/comprehensive_10k_tests/memtech_00000.yaml\n\n")
        f.write("# Run all tests with 8 parallel workers\n")
        f.write("python3 test/config_generator/run_10k_tests.py \\\n")
        f.write("  --config-dir test/configs/comprehensive_10k_tests \\\n")
        f.write("  --workers 8 \\\n")
        f.write("  --report comprehensive_test_report.json\n")
        f.write("```\n\n")
        f.write("## Files\n\n")
        f.write("- `*.yaml`: Individual test configuration files\n")
        f.write("- `test_manifest.yaml`: Index of all tests with metadata\n")
        f.write("- `README.md`: This file\n")

    print(f"✓ Summary written to {summary_file}")
    print("\n" + "=" * 80)
    print("✓ Comprehensive test suite generation complete!")
    print(f"✓ Output directory: {output_dir}")
    print("=" * 80)
    print("\nTest Suite Statistics:")

    # Calculate statistics
    dram_types = {}
    placements = {}
    core_models = {}
    schedulings = {}
    for test in test_cases:
        dram_type = test["dram"]["type"]
        dram_types[dram_type] = dram_types.get(dram_type, 0) + 1

        placement = test["pim"]["placement"]
        placements[placement] = placements.get(placement, 0) + 1

        core_model = test["pim"]["core_model"]
        core_models[core_model] = core_models.get(core_model, 0) + 1

        scheduling = test["pim"]["scheduling_policy"]
        schedulings[scheduling] = schedulings.get(scheduling, 0) + 1

    print(f"\nDRAM Types:")
    for dtype, count in sorted(dram_types.items()):
        print(f"  {dtype}: {count} tests")

    print(f"\nPIM Placements:")
    for placement, count in sorted(placements.items()):
        print(f"  {placement}: {count} tests")

    print(f"\nCore Models:")
    for model, count in sorted(core_models.items()):
        print(f"  {model}: {count} tests")

    print(f"\nScheduling Policies:")
    for policy, count in sorted(schedulings.items()):
        print(f"  {policy}: {count} tests")

    print("=" * 80)

if __name__ == "__main__":
    generate_test_suite()
