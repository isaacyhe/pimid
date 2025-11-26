#!/usr/bin/env python3
"""
Generate 10,000 test configuration files for custom topology and external model testing

This script generates a comprehensive test suite covering:
- All topology types at different levels
- Various port counts and switch counts
- External model configurations
- Mixed configurations
- Corner cases
"""

import os
import yaml
import itertools
import random
from pathlib import Path

# Topology types
TOPOLOGIES = ["BUS", "CROSSBAR", "MESH_2D", "TORUS_2D", "FAT_TREE", "H_TREE"]

# DRAM configurations
DRAM_CONFIGS = [
    {"type": "DDR4", "channels": 1, "ranks": 1, "chips": 8, "bgs": 4, "banks": 4, "subarrays": 16},
    {"type": "DDR4", "channels": 2, "ranks": 2, "chips": 8, "bgs": 4, "banks": 4, "subarrays": 16},
    {"type": "DDR5", "channels": 2, "ranks": 1, "chips": 8, "bgs": 8, "banks": 4, "subarrays": 16},
    {"type": "HBM2", "channels": 8, "ranks": 2, "chips": 2, "bgs": 4, "banks": 4, "subarrays": 16},
    {"type": "HBM3", "channels": 16, "ranks": 2, "chips": 2, "bgs": 8, "banks": 4, "subarrays": 16},
    {"type": "GDDR6", "channels": 2, "ranks": 1, "chips": 4, "bgs": 4, "banks": 4, "subarrays": 16},
    {"type": "LPDDR5", "channels": 4, "ranks": 1, "chips": 8, "bgs": 8, "banks": 4, "subarrays": 16},
]

# Port count configurations (for testing port reduction)
PORT_CONFIGS = [4, 8, 16, 32, 64]

def calculate_optimal_switches(num_endpoints, max_ports, topology):
    """Calculate optimal number of switches for given constraints"""
    if topology in ["BUS", "CROSSBAR"]:
        # Need enough switches so each has <= max_ports
        return max(1, (num_endpoints + max_ports - 1) // max_ports)
    elif topology in ["MESH_2D", "TORUS_2D"]:
        # Mesh requires sqrt(N) x sqrt(N)
        import math
        side = int(math.ceil(math.sqrt(num_endpoints)))
        return side * side
    elif topology == "H_TREE":
        # Binary tree: N-1 switches for N endpoints
        return max(1, num_endpoints - 1)
    elif topology == "FAT_TREE":
        # Fat tree: approximately 2*N switches
        return max(1, num_endpoints)
    else:
        return max(1, num_endpoints // max_ports)

def calculate_ports_per_switch(topology, num_endpoints, num_switches):
    """Calculate ports per switch for topology"""
    if topology in ["BUS", "CROSSBAR"]:
        return max(2, num_endpoints // num_switches)
    elif topology in ["MESH_2D", "TORUS_2D"]:
        return 5  # N, S, E, W, Local
    elif topology == "H_TREE":
        return 4  # Parent, 2 children, local
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

def generate_custom_topology_config(dram_config, test_params):
    """Generate custom topology configuration"""
    config = {
        "simulation": {
            "name": test_params["name"],
            "mode": "standalone",
            "duration_cycles": 10000
        },
        "memory": {
            "dram_type": dram_config["type"],
            "channels": dram_config["channels"],
            "ranks_per_channel": dram_config["ranks"],
            "chips_per_rank": dram_config["chips"],
            "bg_per_chip": dram_config["bgs"],
            "banks_per_bg": dram_config["banks"],
            "subarrays_per_bank": dram_config["subarrays"]
        }
    }

    if test_params.get("custom_topology"):
        topo_params = test_params["custom_topology"]

        config["memory"]["network"] = {
            "custom_topology": {
                "enabled": True
            }
        }

        custom_topo = config["memory"]["network"]["custom_topology"]

        # L0: Banks within BG
        if "l0" in topo_params:
            num_endpoints = dram_config["banks"]
            custom_topo["l0"] = generate_level_config(
                0, topo_params["l0"], num_endpoints, topo_params.get("l0_ports"))
        else:
            custom_topo["l0"] = {"use_default": True}

        # L1: BGs within chip
        if "l1" in topo_params:
            num_endpoints = dram_config["bgs"] * dram_config["banks"]
            custom_topo["l1"] = generate_level_config(
                1, topo_params["l1"], num_endpoints, topo_params.get("l1_ports"))
        else:
            custom_topo["l1"] = {"use_default": True}

        # L2: Chips within rank
        if "l2" in topo_params:
            num_endpoints = dram_config["chips"]
            custom_topo["l2"] = generate_level_config(
                2, topo_params["l2"], num_endpoints, topo_params.get("l2_ports"))
        else:
            custom_topo["l2"] = {"use_default": True}

        # L3: Ranks within channel
        if "l3" in topo_params:
            num_endpoints = dram_config["ranks"]
            custom_topo["l3"] = generate_level_config(
                3, topo_params["l3"], num_endpoints, topo_params.get("l3_ports"))
        else:
            custom_topo["l3"] = {"use_default": True}

        # L4: Channels
        if "l4" in topo_params:
            num_endpoints = dram_config["channels"]
            custom_topo["l4"] = generate_level_config(
                4, topo_params["l4"], num_endpoints, topo_params.get("l4_ports"))
        else:
            custom_topo["l4"] = {"use_default": True}

        # L5: System level
        if "l5" in topo_params:
            custom_topo["l5"] = generate_level_config(
                5, topo_params["l5"], dram_config["channels"], topo_params.get("l5_ports"))
        else:
            custom_topo["l5"] = {"use_default": True}

    # Add external models if specified
    if test_params.get("external_models"):
        config["external_models"] = test_params["external_models"]
    else:
        config["external_models"] = {
            "network": {"enabled": False},
            "memory": {"enabled": False}
        }

    # Add PIM configuration
    config["pim"] = {
        "enabled": True,
        "num_pes": 128,
        "pe_placement": "bank",
        "scheduling_policy": "nearest"
    }

    return config

def generate_test_suite():
    """Generate 10,000 test configurations"""

    output_dir = Path("test/configs/10k_topology_tests")
    output_dir.mkdir(parents=True, exist_ok=True)

    test_cases = []
    test_id = 0

    print("Generating 10,000 test configurations...")
    print("=" * 80)

    # Category 1: Single-level topology variations (2000 tests)
    print("\n[1/10] Generating single-level topology tests (2000)...")
    for i in range(2000):
        dram = random.choice(DRAM_CONFIGS)
        topology = random.choice(TOPOLOGIES)
        level = random.choice(["l0", "l1", "l2", "l3", "l4", "l5"])
        max_ports = random.choice(PORT_CONFIGS)

        test_cases.append({
            "name": f"single_level_{test_id:05d}",
            "dram": dram,
            "custom_topology": {
                level: topology,
                f"{level}_ports": max_ports
            }
        })
        test_id += 1

    # Category 2: Two-level topology combinations (1500 tests)
    print("[2/10] Generating two-level topology tests (1500)...")
    for i in range(1500):
        dram = random.choice(DRAM_CONFIGS)
        level1 = random.choice(["l0", "l1", "l2"])
        level2 = random.choice(["l3", "l4", "l5"])
        topo1 = random.choice(TOPOLOGIES)
        topo2 = random.choice(TOPOLOGIES)

        test_cases.append({
            "name": f"two_level_{test_id:05d}",
            "dram": dram,
            "custom_topology": {
                level1: topo1,
                level2: topo2,
                f"{level1}_ports": random.choice(PORT_CONFIGS),
                f"{level2}_ports": random.choice(PORT_CONFIGS)
            }
        })
        test_id += 1

    # Category 3: All-levels custom topology (1000 tests)
    print("[3/10] Generating all-levels custom topology tests (1000)...")
    for i in range(1000):
        dram = random.choice(DRAM_CONFIGS)

        test_cases.append({
            "name": f"all_levels_{test_id:05d}",
            "dram": dram,
            "custom_topology": {
                "l0": random.choice(TOPOLOGIES),
                "l1": random.choice(TOPOLOGIES),
                "l2": random.choice(TOPOLOGIES),
                "l3": random.choice(TOPOLOGIES),
                "l4": random.choice(TOPOLOGIES),
                "l5": random.choice(TOPOLOGIES),
                "l0_ports": random.choice(PORT_CONFIGS),
                "l1_ports": random.choice(PORT_CONFIGS),
                "l2_ports": random.choice(PORT_CONFIGS),
                "l3_ports": random.choice(PORT_CONFIGS),
                "l4_ports": random.choice(PORT_CONFIGS),
                "l5_ports": random.choice(PORT_CONFIGS)
            }
        })
        test_id += 1

    # Category 4: Port count optimization tests (1500 tests)
    print("[4/10] Generating port count optimization tests (1500)...")
    for i in range(1500):
        dram = random.choice(DRAM_CONFIGS)
        max_ports = random.choice([4, 8, 16])  # Strict limits
        levels = random.sample(["l0", "l1", "l2", "l3", "l4", "l5"], k=random.randint(2, 4))

        topo_config = {}
        for level in levels:
            topo_config[level] = "CROSSBAR"  # Use crossbar for port reduction testing
            topo_config[f"{level}_ports"] = max_ports

        test_cases.append({
            "name": f"port_opt_{test_id:05d}",
            "dram": dram,
            "custom_topology": topo_config
        })
        test_id += 1

    # Category 5: Specific topology patterns (1000 tests)
    print("[5/10] Generating specific topology pattern tests (1000)...")
    patterns = [
        {"l0": "H_TREE", "l1": "BUS", "l2": "MESH_2D"},
        {"l0": "CROSSBAR", "l2": "TORUS_2D", "l4": "FAT_TREE"},
        {"l1": "H_TREE", "l3": "MESH_2D", "l5": "CROSSBAR"},
        {"l0": "BUS", "l1": "BUS", "l2": "CROSSBAR"},
        {"l3": "FAT_TREE", "l4": "FAT_TREE", "l5": "CROSSBAR"},
    ]

    for i in range(1000):
        dram = random.choice(DRAM_CONFIGS)
        pattern = random.choice(patterns)

        # Add random port limits
        topo_config = pattern.copy()
        for level in pattern.keys():
            topo_config[f"{level}_ports"] = random.choice(PORT_CONFIGS)

        test_cases.append({
            "name": f"pattern_{test_id:05d}",
            "dram": dram,
            "custom_topology": topo_config
        })
        test_id += 1

    # Category 6: Mesh and Torus focused tests (500 tests)
    print("[6/10] Generating mesh/torus tests (500)...")
    for i in range(500):
        dram = random.choice(DRAM_CONFIGS)
        mesh_type = random.choice(["MESH_2D", "TORUS_2D"])
        levels = random.sample(["l1", "l2", "l3", "l4"], k=random.randint(1, 3))

        topo_config = {}
        for level in levels:
            topo_config[level] = mesh_type

        test_cases.append({
            "name": f"mesh_torus_{test_id:05d}",
            "dram": dram,
            "custom_topology": topo_config
        })
        test_id += 1

    # Category 7: H-TREE focused tests (500 tests)
    print("[7/10] Generating H-TREE tests (500)...")
    for i in range(500):
        dram = random.choice(DRAM_CONFIGS)
        levels = random.sample(["l0", "l1", "l2"], k=random.randint(1, 2))

        topo_config = {}
        for level in levels:
            topo_config[level] = "H_TREE"

        test_cases.append({
            "name": f"htree_{test_id:05d}",
            "dram": dram,
            "custom_topology": topo_config
        })
        test_id += 1

    # Category 8: FAT_TREE focused tests (500 tests)
    print("[8/10] Generating FAT_TREE tests (500)...")
    for i in range(500):
        dram = random.choice(DRAM_CONFIGS)
        # Fat tree typically at upper levels
        levels = random.sample(["l3", "l4", "l5"], k=random.randint(1, 2))

        topo_config = {}
        for level in levels:
            topo_config[level] = "FAT_TREE"

        test_cases.append({
            "name": f"fattree_{test_id:05d}",
            "dram": dram,
            "custom_topology": topo_config
        })
        test_id += 1

    # Category 9: Mixed DRAM types with various topologies (1000 tests)
    print("[9/10] Generating mixed DRAM type tests (1000)...")
    for i in range(1000):
        dram = random.choice(DRAM_CONFIGS)
        num_levels = random.randint(2, 5)
        levels = random.sample(["l0", "l1", "l2", "l3", "l4", "l5"], k=num_levels)

        topo_config = {}
        for level in levels:
            topo_config[level] = random.choice(TOPOLOGIES)
            topo_config[f"{level}_ports"] = random.choice(PORT_CONFIGS)

        test_cases.append({
            "name": f"mixed_{test_id:05d}",
            "dram": dram,
            "custom_topology": topo_config
        })
        test_id += 1

    # Category 10: External model integration tests (500 tests)
    print("[10/10] Generating external model integration tests (500)...")
    for i in range(500):
        dram = random.choice(DRAM_CONFIGS)

        # Some with topology, some without
        if i % 2 == 0:
            topo_config = {
                random.choice(["l0", "l1", "l2"]): random.choice(TOPOLOGIES)
            }
        else:
            topo_config = None

        ext_models = {
            "network": {
                "enabled": random.choice([True, False]),
                "library_path": "./lib/libsimple_network_adapter.so",
                "model_name": "SimpleNetwork",
                "config_file": "./config/network_config.txt"
            },
            "memory": {
                "enabled": False
            }
        }

        test_cases.append({
            "name": f"external_{test_id:05d}",
            "dram": dram,
            "custom_topology": topo_config,
            "external_models": ext_models
        })
        test_id += 1

    print(f"\nGenerated {len(test_cases)} test case definitions")
    print("=" * 80)

    # Write config files
    print("\nWriting configuration files...")
    manifest = []

    for i, test_case in enumerate(test_cases):
        config = generate_custom_topology_config(test_case["dram"], test_case)

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
        f.write("# 10,000 Custom Topology Test Suite\n\n")
        f.write("## Overview\n\n")
        f.write(f"This directory contains {len(test_cases)} test configuration files for comprehensive\n")
        f.write("testing of custom network topology and external model integration features.\n\n")
        f.write("## Test Categories\n\n")
        f.write("1. **Single-level topology** (2000 tests): One level customized\n")
        f.write("2. **Two-level topology** (1500 tests): Two levels customized\n")
        f.write("3. **All-levels topology** (1000 tests): All six levels customized\n")
        f.write("4. **Port count optimization** (1500 tests): Testing port reduction\n")
        f.write("5. **Specific patterns** (1000 tests): Common topology combinations\n")
        f.write("6. **Mesh/Torus focused** (500 tests): Mesh and torus topologies\n")
        f.write("7. **H-TREE focused** (500 tests): H-tree topologies\n")
        f.write("8. **FAT_TREE focused** (500 tests): Fat-tree topologies\n")
        f.write("9. **Mixed DRAM types** (1000 tests): Various DRAM configurations\n")
        f.write("10. **External models** (500 tests): External model integration\n\n")
        f.write("## Usage\n\n")
        f.write("```bash\n")
        f.write("# Run a single test\n")
        f.write("pimid --config test/configs/10k_topology_tests/single_level_00000.yaml\n\n")
        f.write("# Run all tests\n")
        f.write("python3 test/config_generator/run_10k_tests.py\n")
        f.write("```\n\n")
        f.write("## Files\n\n")
        f.write("- `*.yaml`: Individual test configuration files\n")
        f.write("- `test_manifest.yaml`: Index of all tests with metadata\n")
        f.write("- `README.md`: This file\n")

    print(f"✓ Summary written to {summary_file}")
    print("\n" + "=" * 80)
    print("✓ Test suite generation complete!")
    print(f"✓ Output directory: {output_dir}")
    print("=" * 80)

if __name__ == "__main__":
    generate_test_suite()
