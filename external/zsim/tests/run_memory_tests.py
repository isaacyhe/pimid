#!/usr/bin/env python3
"""
ZSim/PIN Memory Technology Test Suite.
Tests various memory configurations with consistent core setup.
"""

import subprocess
import sys
import tempfile
from pathlib import Path
from datetime import datetime

# Memory configurations to test
# (name, mem_type, latency_cycles, bandwidth_comment)
MEMORY_CONFIGS = [
    # DDR generations
    ("ddr3_1600", "DDR", 150, "DDR3-1600 (~100ns)"),
    ("ddr4_2400", "DDR", 100, "DDR4-2400 (~70ns)"),
    ("ddr4_3200", "DDR", 85, "DDR4-3200 (~55ns)"),
    ("ddr5_4800", "DDR", 70, "DDR5-4800 (~45ns)"),
    ("ddr5_6400", "DDR", 55, "DDR5-6400 (~35ns)"),

    # High Bandwidth Memory
    ("hbm2", "HBM", 40, "HBM2 (~25ns, 256GB/s)"),
    ("hbm2e", "HBM", 35, "HBM2E (~22ns, 460GB/s)"),
    ("hbm3", "HBM", 30, "HBM3 (~20ns, 819GB/s)"),

    # Low Power DDR
    ("lpddr4", "LPDDR", 120, "LPDDR4 (~80ns)"),
    ("lpddr5", "LPDDR", 90, "LPDDR5 (~60ns)"),

    # Graphics Memory
    ("gddr6", "GDDR", 60, "GDDR6 (~40ns, 672GB/s)"),
    ("gddr6x", "GDDR", 50, "GDDR6X (~32ns, 1TB/s)"),

    # Non-volatile Memory (NVM)
    ("stt_mram", "NVM", 200, "STT-MRAM (~130ns read)"),
    ("pcm", "NVM", 400, "PCM (~260ns read)"),
    ("reram", "NVM", 300, "ReRAM (~200ns read)"),

    # Ideal/Synthetic
    ("ideal_low", "Ideal", 10, "Ideal low latency"),
    ("ideal_zero", "Ideal", 1, "Near-zero latency"),
]

# Core configurations (fixed across all memory tests)
CORE_CONFIGS = [
    # (name, core_type, num_cores, simd_width, alu_latency)
    ("pim_16pe", "ALU", 16, 256, 1),
    ("pim_64pe", "ALU", 64, 256, 1),
]


def generate_config(core_name, core_type, num_cores, simd_width, alu_latency,
                    mem_name, mem_latency, mem_comment):
    """Generate ZSim config for core + memory combination."""

    test_name = f"{core_name}_{mem_name}"

    config = f"""// Auto-generated ZSim test: {test_name}
// Memory: {mem_comment}
// Cores: {num_cores} {core_type} cores, SIMD {simd_width}-bit

sys = {{
    lineSize = 64;
    frequency = 2400;
    simdWidth = {simd_width};

    cores = {{
        pim_cores = {{
            type = "{core_type}";
            cores = {num_cores};
"""
    if alu_latency is not None:
        config += f"            aluLatency = {alu_latency};\n"

    config += f"""        }};
    }};

    mem = {{
        type = "Simple";
        latency = {mem_latency};  // {mem_comment}
    }};
}};

sim = {{
    phaseLength = 10000;
    maxTotalInstrs = 100000L;
    statsPhaseInterval = 1000;
    printHierarchy = true;
    aslr = true;
}};

process0 = {{
    command = "/bin/echo 'ZSim: {test_name}'";
}};
"""
    return test_name, config


def run_test(zsim_path, config_path, name, timeout=60):
    """Run a single ZSim test."""

    # Ensure fake osrelease
    fake_proc = Path("/tmp/fake_proc/sys/kernel")
    fake_proc.mkdir(parents=True, exist_ok=True)
    (fake_proc / "osrelease").write_text("5.15.0-generic\n")

    cmd = [
        "proot", "-b", "/tmp/fake_proc/sys/kernel/osrelease:/proc/sys/kernel/osrelease",
        str(zsim_path), str(config_path)
    ]

    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True,
            timeout=timeout, cwd=zsim_path.parent
        )

        success = f"ZSim: {name}" in result.stdout or f"ZSim: {name}" in result.stderr
        has_panic = "Panic" in result.stdout or "Panic" in result.stderr

        return {"name": name, "success": success and not has_panic, "exit_code": result.returncode}
    except subprocess.TimeoutExpired:
        return {"name": name, "success": False, "exit_code": -1}
    except Exception as e:
        return {"name": name, "success": False, "exit_code": -1}


def main():
    zsim_path = Path("/home/user/pimid-dev/pimid/external/zsim/build/opt/zsim")
    if not zsim_path.exists():
        print(f"Error: ZSim not found at {zsim_path}")
        sys.exit(1)

    total_tests = len(CORE_CONFIGS) * len(MEMORY_CONFIGS)

    print("=" * 80)
    print("ZSim/PIN Memory Technology Test Suite")
    print(f"Started: {datetime.now().isoformat()}")
    print("=" * 80)
    print(f"Core configurations: {len(CORE_CONFIGS)}")
    print(f"Memory configurations: {len(MEMORY_CONFIGS)}")
    print(f"Total tests: {total_tests}")
    print("=" * 80)
    print()

    results = []
    passed = 0
    failed = 0
    test_num = 0

    with tempfile.TemporaryDirectory() as tmpdir:
        for core_name, core_type, num_cores, simd_width, alu_latency in CORE_CONFIGS:
            print(f"\n--- Core: {core_name} ({num_cores} {core_type} cores) ---")

            for mem_name, mem_type, mem_latency, mem_comment in MEMORY_CONFIGS:
                test_num += 1
                test_name, config_content = generate_config(
                    core_name, core_type, num_cores, simd_width, alu_latency,
                    mem_name, mem_latency, mem_comment
                )

                print(f"[{test_num}/{total_tests}] {test_name}...", end=" ", flush=True)

                config_path = Path(tmpdir) / f"{test_name}.cfg"
                config_path.write_text(config_content)

                result = run_test(zsim_path, config_path, test_name)
                results.append({**result, "core": core_name, "mem": mem_name,
                               "latency": mem_latency, "mem_type": mem_type})

                if result["success"]:
                    print(f"PASS")
                    passed += 1
                else:
                    print(f"FAIL")
                    failed += 1

    print()
    print("=" * 80)
    print("TEST SUMMARY")
    print("=" * 80)
    print(f"Total:  {total_tests}")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    print(f"Rate:   {100*passed/total_tests:.1f}%")
    print()

    # Results by memory type
    print("Results by Memory Type:")
    print("-" * 80)
    for mem_type in ["DDR", "HBM", "LPDDR", "GDDR", "NVM", "Ideal"]:
        type_results = [r for r in results if r["mem_type"] == mem_type]
        type_passed = sum(1 for r in type_results if r["success"])
        print(f"  {mem_type:8s}: {type_passed}/{len(type_results)} passed")
    print()

    # Detailed table
    print("Detailed Results (Latency in cycles):")
    print("-" * 80)
    print(f"{'Test Name':<35} {'Mem Type':<8} {'Lat':<6} {'Status':<8}")
    print("-" * 80)
    for r in results:
        status = "PASS" if r["success"] else "FAIL"
        print(f"{r['name']:<35} {r['mem_type']:<8} {r['latency']:<6} {status:<8}")
    print("-" * 80)

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
