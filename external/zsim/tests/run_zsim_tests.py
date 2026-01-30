#!/usr/bin/env python3
"""
Comprehensive ZSim test runner for PIM PE configurations.
Runs multiple tests with varying parameters to validate ZSim/PIN functionality.
"""

import subprocess
import os
import sys
import tempfile
from pathlib import Path
from datetime import datetime

# Test configurations to generate and run
TEST_CONFIGS = [
    # (name, core_type, num_cores, simd_width, alu_latency)
    ("simple_1core", "Simple", 1, 128, None),
    ("simple_4core", "Simple", 4, 128, None),
    ("alu_1pe", "ALU", 1, 128, 1),
    ("alu_4pe", "ALU", 4, 256, 1),
    ("alu_16pe", "ALU", 16, 256, 1),
    ("alu_64pe_bank", "ALU", 64, 256, 1),
    ("alu_256pe_subarray", "ALU", 256, 512, 1),
    ("alu_latency_2", "ALU", 16, 256, 2),
    ("alu_latency_4", "ALU", 16, 256, 4),
    ("ooo_1core", "OOO", 1, 128, None),
    ("ooo_4core", "OOO", 4, 256, None),
]

def generate_config(name, core_type, num_cores, simd_width, alu_latency):
    """Generate a ZSim config file for the given parameters."""

    # Determine if we need cache hierarchy
    needs_cache = core_type in ["Simple", "OOO", "Timing"]

    config = f"""// Auto-generated ZSim test config: {name}
// Core type: {core_type}, Cores: {num_cores}, SIMD: {simd_width}

sys = {{
    lineSize = 64;
    frequency = 2400;
    simdWidth = {simd_width};

    cores = {{
        test_cores = {{
            type = "{core_type}";
            cores = {num_cores};
"""

    if alu_latency is not None:
        config += f"            aluLatency = {alu_latency};\n"

    if needs_cache:
        config += """            icache = "l1i";
            dcache = "l1d";
"""

    config += """        };
    };
"""

    if needs_cache:
        config += """
    caches = {
        l1d = {
            caches = """ + str(num_cores) + """;
            size = 32768;
            latency = 4;
        };
        l1i = {
            caches = """ + str(num_cores) + """;
            size = 32768;
            latency = 3;
        };
    };
"""

    config += """
    mem = {
        type = "Simple";
        latency = 100;
    };
};

sim = {
    phaseLength = 10000;
    maxTotalInstrs = 100000L;
    statsPhaseInterval = 1000;
    printHierarchy = true;
    aslr = true;
};

process0 = {
    command = "/bin/echo 'ZSim Test: """ + name + """'";
};
"""
    return config


def run_test(zsim_path, config_path, name, timeout=60):
    """Run a single ZSim test and return results."""

    # Ensure fake osrelease exists
    fake_proc = Path("/tmp/fake_proc/sys/kernel")
    fake_proc.mkdir(parents=True, exist_ok=True)
    (fake_proc / "osrelease").write_text("5.15.0-generic\n")

    cmd = [
        "proot", "-b", "/tmp/fake_proc/sys/kernel/osrelease:/proc/sys/kernel/osrelease",
        str(zsim_path), str(config_path)
    ]

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=zsim_path.parent
        )

        # Check for successful execution (output should contain the echo message)
        success = f"ZSim Test: {name}" in result.stdout or f"ZSim Test: {name}" in result.stderr

        # Also check for panic messages
        has_panic = "Panic" in result.stdout or "Panic" in result.stderr

        return {
            "name": name,
            "success": success and not has_panic,
            "exit_code": result.returncode,
            "stdout": result.stdout[-500:] if len(result.stdout) > 500 else result.stdout,
            "stderr": result.stderr[-500:] if len(result.stderr) > 500 else result.stderr,
        }
    except subprocess.TimeoutExpired:
        return {
            "name": name,
            "success": False,
            "exit_code": -1,
            "stdout": "",
            "stderr": "TIMEOUT",
        }
    except Exception as e:
        return {
            "name": name,
            "success": False,
            "exit_code": -1,
            "stdout": "",
            "stderr": str(e),
        }


def main():
    # Find ZSim binary
    zsim_path = Path("/home/user/pimid-dev/pimid/external/zsim/build/opt/zsim")
    if not zsim_path.exists():
        print(f"Error: ZSim not found at {zsim_path}")
        sys.exit(1)

    print("=" * 70)
    print(f"ZSim Comprehensive Test Suite")
    print(f"Started: {datetime.now().isoformat()}")
    print("=" * 70)
    print(f"ZSim binary: {zsim_path}")
    print(f"Total tests: {len(TEST_CONFIGS)}")
    print("=" * 70)
    print()

    results = []
    passed = 0
    failed = 0

    with tempfile.TemporaryDirectory() as tmpdir:
        for i, (name, core_type, num_cores, simd_width, alu_latency) in enumerate(TEST_CONFIGS, 1):
            print(f"[{i}/{len(TEST_CONFIGS)}] Testing {name}...", end=" ", flush=True)

            # Generate config
            config_content = generate_config(name, core_type, num_cores, simd_width, alu_latency)
            config_path = Path(tmpdir) / f"{name}.cfg"
            config_path.write_text(config_content)

            # Run test
            result = run_test(zsim_path, config_path, name)
            results.append(result)

            if result["success"]:
                print(f"PASS")
                passed += 1
            else:
                print(f"FAIL (exit={result['exit_code']})")
                if "Panic" in result.get("stderr", ""):
                    # Extract panic message
                    for line in result["stderr"].split("\n"):
                        if "Panic" in line:
                            print(f"    {line.strip()[:80]}")
                failed += 1

    print()
    print("=" * 70)
    print("TEST SUMMARY")
    print("=" * 70)
    print(f"Total:  {len(TEST_CONFIGS)}")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    print(f"Rate:   {100*passed/len(TEST_CONFIGS):.1f}%")
    print()

    # Detailed results table
    print("Detailed Results:")
    print("-" * 70)
    print(f"{'Test Name':<30} {'Type':<10} {'Cores':<8} {'Status':<10}")
    print("-" * 70)

    for i, (name, core_type, num_cores, simd_width, alu_latency) in enumerate(TEST_CONFIGS):
        status = "PASS" if results[i]["success"] else "FAIL"
        print(f"{name:<30} {core_type:<10} {num_cores:<8} {status:<10}")

    print("-" * 70)
    print()

    # Return exit code based on results
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
