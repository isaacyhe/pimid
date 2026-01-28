#!/usr/bin/env python3
"""
PIM Memory Technology Comparison Tests using Ramulator2.
Compares DDR4, DDR5, HBM2, HBM3 for PIM-relevant workloads.
"""

import subprocess
import os
import sys
from pathlib import Path
import yaml
import json

# Memory configurations to test
MEMORY_CONFIGS = [
    {
        "name": "DDR4_2400R",
        "impl": "DDR4",
        "org_preset": "DDR4_8Gb_x8",
        "timing_preset": "DDR4_2400R",
        "description": "DDR4 2400MHz - Baseline"
    },
    {
        "name": "DDR4_3200AA",
        "impl": "DDR4",
        "org_preset": "DDR4_8Gb_x8",
        "timing_preset": "DDR4_3200AA",
        "description": "DDR4 3200MHz - High Speed DDR4"
    },
    {
        "name": "DDR5_3200AN",
        "impl": "DDR5",
        "org_preset": "DDR5_8Gb_x8",
        "timing_preset": "DDR5_3200AN",
        "description": "DDR5 3200MHz - Next Gen"
    },
    {
        "name": "HBM2_2Gbps",
        "impl": "HBM2",
        "org_preset": "HBM2_4Gb",
        "timing_preset": "HBM2_2Gbps",
        "description": "HBM2 2Gbps - PIM Standard"
    },
    {
        "name": "HBM3_2Gbps",
        "impl": "HBM3",
        "org_preset": "HBM3_4Gb",
        "timing_preset": "HBM3_2Gbps",
        "description": "HBM3 2Gbps - Latest HBM"
    },
]

def generate_config(mem_config, trace_file="pim_workload.trace"):
    """Generate Ramulator config for a memory type."""
    dram_config = {
        "impl": mem_config["impl"],
        "org": {"preset": mem_config["org_preset"]},
        "timing": {"preset": mem_config["timing_preset"]}
    }

    # DDR5 requires RFM (Row Failure Mitigation) configuration
    if mem_config["impl"] == "DDR5":
        dram_config["RFM"] = {"BRC": 2}

    config = {
        "Frontend": {
            "impl": "SimpleO3",
            "clock_ratio": 4,
            "num_expected_insts": 100000,
            "traces": [trace_file],
            "Translation": {
                "impl": "RandomTranslation",
                "max_addr": 2147483648
            }
        },
        "MemorySystem": {
            "impl": "GenericDRAM",
            "clock_ratio": 1,
            "DRAM": dram_config,
            "Controller": {
                "impl": "Generic",
                "Scheduler": {"impl": "FRFCFS"},
                "RefreshManager": {"impl": "AllBank"},
                "RowPolicy": {"impl": "OpenRowPolicy", "cap": 16}
            },
            "AddrMapper": {"impl": "RoBaRaCoCh"}
        }
    }
    return config


def run_simulation(ramulator_path, config, config_name):
    """Run a single Ramulator simulation."""
    import tempfile

    # Use absolute path for trace file
    base_dir = ramulator_path.parent.parent
    trace_file = str(base_dir / "pim_workload.trace")
    config["Frontend"]["traces"] = [trace_file]

    with tempfile.NamedTemporaryFile(mode='w', suffix='.yaml', delete=False) as f:
        yaml.dump(config, f)
        config_path = f.name

    try:
        result = subprocess.run(
            [str(ramulator_path), "-f", config_path],
            capture_output=True,
            text=True,
            timeout=120,
            cwd=base_dir
        )

        # Parse output for key metrics
        output = result.stdout
        metrics = {}

        for line in output.split('\n'):
            line = line.strip()
            if ':' in line:
                # Parse key: value pairs
                parts = line.split(':')
                if len(parts) == 2:
                    key = parts[0].strip()
                    value = parts[1].strip()
                    try:
                        # Try to parse as number
                        if '.' in value:
                            metrics[key] = float(value)
                        else:
                            metrics[key] = int(value)
                    except ValueError:
                        pass

        return {
            "success": result.returncode == 0,
            "metrics": metrics,
            "raw_output": output
        }
    except subprocess.TimeoutExpired:
        return {"success": False, "error": "TIMEOUT"}
    except Exception as e:
        return {"success": False, "error": str(e)}
    finally:
        os.unlink(config_path)


def main():
    ramulator_path = Path(__file__).parent / "build" / "ramulator2"
    if not ramulator_path.exists():
        print(f"Error: Ramulator not found at {ramulator_path}")
        sys.exit(1)

    print("=" * 70)
    print("PIM Memory Technology Comparison Test Suite")
    print("=" * 70)
    print(f"Ramulator: {ramulator_path}")
    print(f"Tests: {len(MEMORY_CONFIGS)} memory configurations")
    print("=" * 70)
    print()

    results = []

    for mem_config in MEMORY_CONFIGS:
        name = mem_config["name"]
        desc = mem_config["description"]
        print(f"Testing {name} - {desc}...", end=" ", flush=True)

        config = generate_config(mem_config)
        result = run_simulation(ramulator_path, config, name)

        if result["success"]:
            metrics = result["metrics"]
            avg_latency = metrics.get("avg_read_latency_0", "N/A")
            row_hits = metrics.get("row_hits_0", 0)
            row_misses = metrics.get("row_misses_0", 0)
            total_reads = metrics.get("num_read_reqs_0", 0)
            mem_cycles = metrics.get("memory_system_cycles", 0)

            print(f"PASS")
            results.append({
                "name": name,
                "description": desc,
                "avg_read_latency": avg_latency,
                "row_hits": row_hits,
                "row_misses": row_misses,
                "total_reads": total_reads,
                "memory_cycles": mem_cycles,
                "success": True
            })
        else:
            print(f"FAIL - {result.get('error', 'Unknown error')}")
            results.append({
                "name": name,
                "description": desc,
                "success": False,
                "error": result.get("error", "Unknown")
            })

    print()
    print("=" * 70)
    print("RESULTS SUMMARY")
    print("=" * 70)
    print()
    print(f"{'Memory Type':<20} {'Avg Latency':<15} {'Row Hits':<12} {'Row Misses':<12} {'Hit Rate':<10}")
    print("-" * 70)

    for r in results:
        if r["success"]:
            name = r["name"]
            lat = r["avg_read_latency"]
            hits = r["row_hits"]
            misses = r["row_misses"]
            total = hits + misses
            hit_rate = f"{100*hits/total:.1f}%" if total > 0 else "N/A"
            lat_str = f"{lat:.2f}" if isinstance(lat, float) else str(lat)
            print(f"{name:<20} {lat_str:<15} {hits:<12} {misses:<12} {hit_rate:<10}")
        else:
            print(f"{r['name']:<20} FAILED: {r.get('error', 'Unknown')}")

    print("-" * 70)
    print()

    # Summary for PIM analysis
    passed = sum(1 for r in results if r["success"])
    print(f"Passed: {passed}/{len(MEMORY_CONFIGS)}")

    if passed > 0:
        print()
        print("PIM INSIGHTS:")
        print("-" * 70)

        successful = [r for r in results if r["success"] and isinstance(r["avg_read_latency"], (int, float))]
        if successful:
            best_latency = min(successful, key=lambda x: x["avg_read_latency"])
            print(f"  Lowest latency: {best_latency['name']} @ {best_latency['avg_read_latency']:.2f} cycles")

            best_hit_rate = max(successful, key=lambda x: x["row_hits"] / max(x["row_hits"] + x["row_misses"], 1))
            total = best_hit_rate["row_hits"] + best_hit_rate["row_misses"]
            hit_pct = 100 * best_hit_rate["row_hits"] / total if total > 0 else 0
            print(f"  Best row hit rate: {best_hit_rate['name']} @ {hit_pct:.1f}%")

    return 0 if passed == len(MEMORY_CONFIGS) else 1


if __name__ == "__main__":
    sys.exit(main())
