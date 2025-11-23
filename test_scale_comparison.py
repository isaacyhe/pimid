#!/usr/bin/env python3
"""
Execution Model Scale Comparison Test
Compares ZSim vs Event-Driven execution models at different scales.
Tests: 10K, 20K, 50K, 100K PEs with small workloads.

This script focuses on comparing the accuracy and performance
characteristics of both execution models across different scales.
"""

import os
import sys
import yaml
import json
import subprocess
import time
from pathlib import Path
from typing import Dict, List, Tuple
from dataclasses import dataclass, asdict

@dataclass
class ScaleTestResult:
    """Result from a scale test"""
    scale: int
    execution_model: str
    workload: str
    memory_tech: str
    success: bool
    execution_time_ms: float
    simulated_cycles: int = 0
    throughput: float = 0.0
    energy_pj: float = 0.0
    error: str = ""

class ScaleComparisonTester:
    """Test execution models at different scales"""

    def __init__(self):
        self.results: List[ScaleTestResult] = []
        self.pimid_binary = Path("/home/user/pimid-dev/build/pimid/pimid")
        self.config_dir = Path("test_configs_scale")
        self.config_dir.mkdir(exist_ok=True)

    def create_config(self, name: str, exec_model: str, num_pes: int,
                     workload: str, params: dict, mem_tech: str = "DRAM") -> Path:
        """Create configuration file"""

        config = {
            "simulation": {
                "name": name,
                "mode": "standalone",
                "host_execution_model": "event_driven",  # Fast host
                "device_execution_model": exec_model,
            },
            "memory": {
                "technology": mem_tech,
                "dram_type": "DDR4" if mem_tech == "DRAM" else None,
                "channels": 1,
                "ranks_per_channel": 1,
                "banks_per_rank": 16,
                "bg_per_rank": 4,
            },
            "pim": {
                "enabled": True,
                "num_pes": num_pes,
                "pe_placement": "bank",
            },
            "processing_element": {
                "type": "Simple" if exec_model == "zsim" else "analytical",
                "frequency_mhz": 1000.0,
            },
            "workload": {
                "name": workload,
                "params": params,
            }
        }

        if exec_model == "event_driven":
            config["event_driven"] = {
                "performance_model": "roofline",
                "device": {
                    "num_cores": num_pes,
                    "frequency_mhz": 1000.0,
                    "ipc": 1.0,
                    "peak_bandwidth_gbps": 100.0 * (num_pes / 10000),  # Scale bandwidth
                    "peak_flops": num_pes * 2.0,
                }
            }

        config_file = self.config_dir / f"{name}.yaml"
        with open(config_file, 'w') as f:
            yaml.dump(config, f, default_flow_style=False)

        return config_file

    def run_test(self, scale: int, exec_model: str, workload: str,
                 params: dict, mem_tech: str = "DRAM") -> ScaleTestResult:
        """Run a single scale test"""

        test_name = f"scale_{scale}_{exec_model}_{workload}_{mem_tech}"

        print(f"\nRunning: {test_name}")
        print(f"  Scale: {scale:,} PEs, Model: {exec_model}, Memory: {mem_tech}")

        config_file = self.create_config(test_name, exec_model, scale,
                                        workload, params, mem_tech)

        start_time = time.time()
        try:
            cmd = [str(self.pimid_binary), "--config", str(config_file)]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=180)

            execution_time = (time.time() - start_time) * 1000

            if result.returncode == 0:
                # Parse metrics from output
                cycles = self._parse_cycles(result.stdout)
                throughput = self._parse_throughput(result.stdout)
                energy = self._parse_energy(result.stdout)

                test_result = ScaleTestResult(
                    scale=scale,
                    execution_model=exec_model,
                    workload=workload,
                    memory_tech=mem_tech,
                    success=True,
                    execution_time_ms=execution_time,
                    simulated_cycles=cycles,
                    throughput=throughput,
                    energy_pj=energy,
                )
                print(f"  ✓ Success - {execution_time:.1f}ms, {cycles:,} cycles")
            else:
                test_result = ScaleTestResult(
                    scale=scale,
                    execution_model=exec_model,
                    workload=workload,
                    memory_tech=mem_tech,
                    success=False,
                    execution_time_ms=execution_time,
                    error=result.stderr[:300],
                )
                print(f"  ✗ Failed: {result.stderr[:100]}")

        except subprocess.TimeoutExpired:
            test_result = ScaleTestResult(
                scale=scale,
                execution_model=exec_model,
                workload=workload,
                memory_tech=mem_tech,
                success=False,
                execution_time_ms=0.0,
                error="Timeout (>180s)",
            )
            print(f"  ✗ Timeout")

        except Exception as e:
            test_result = ScaleTestResult(
                scale=scale,
                execution_model=exec_model,
                workload=workload,
                memory_tech=mem_tech,
                success=False,
                execution_time_ms=0.0,
                error=str(e),
            )
            print(f"  ✗ Error: {str(e)[:100]}")

        self.results.append(test_result)
        return test_result

    def _parse_cycles(self, output: str) -> int:
        """Parse cycle count from output"""
        for line in output.split('\n'):
            if 'cycles' in line.lower() or 'cycle' in line.lower():
                words = line.split()
                for word in words:
                    try:
                        return int(word.replace(',', ''))
                    except:
                        continue
        return 0

    def _parse_throughput(self, output: str) -> float:
        """Parse throughput from output"""
        for line in output.split('\n'):
            if 'throughput' in line.lower() or 'gops' in line.lower():
                words = line.split()
                for word in words:
                    try:
                        return float(word)
                    except:
                        continue
        return 0.0

    def _parse_energy(self, output: str) -> float:
        """Parse energy from output"""
        for line in output.split('\n'):
            if 'energy' in line.lower() and ('pj' in line.lower() or 'nj' in line.lower()):
                words = line.split()
                for word in words:
                    try:
                        return float(word)
                    except:
                        continue
        return 0.0

    def run_scale_comparison(self):
        """Run comprehensive scale comparison"""

        print("="*80)
        print("EXECUTION MODEL SCALE COMPARISON TEST")
        print("="*80)
        print("\nComparing ZSim vs Event-Driven at realistic scales")
        print("16 PEs (bank-level), 64, 256, 1024 PEs (subarray-level)")
        print("Using small workloads for fast testing\n")

        # Scales to test (realistic: 1 PE per bank or per subarray)
        # 16 = bank-level (16 banks)
        # 64 = 4 banks × 16 subarrays or 64 banks
        # 256 = 16 banks × 16 subarrays
        # 1024 = 16 banks × 64 subarrays or 4 ranks × 16 banks × 16 subarrays
        scales = [16, 64, 256, 1024]

        # Execution models
        models = ["zsim", "event_driven"]

        # Workloads with small inputs
        workloads = [
            ("bfs", {"num_vertices": 64, "avg_degree": 4}),
            ("gemm", {"matrix_size": 32}),
            ("dot_product", {"vector_length": 256}),
            ("reduction", {"elements_per_subarray": 256}),
        ]

        # Memory technologies
        mem_techs = ["DRAM", "SRAM"]

        # Run tests
        total_tests = len(scales) * len(models) * len(workloads) * len(mem_techs)
        current_test = 0

        for scale in scales:
            print(f"\n{'='*80}")
            print(f"Testing Scale: {scale:,} PEs")
            print(f"{'='*80}")

            for exec_model in models:
                for workload_name, params in workloads:
                    for mem_tech in mem_techs:
                        current_test += 1
                        print(f"\n[{current_test}/{total_tests}]", end=" ")
                        self.run_test(scale, exec_model, workload_name, params, mem_tech)

        # Generate comparison report
        self.generate_comparison_report()

    def generate_comparison_report(self):
        """Generate detailed comparison report"""

        print(f"\n{'='*80}")
        print("SCALE COMPARISON REPORT")
        print(f"{'='*80}\n")

        # Overall stats
        total = len(self.results)
        passed = sum(1 for r in self.results if r.success)

        print(f"Total Tests: {total}")
        print(f"Passed: {passed} ({100*passed/total:.1f}%)")
        print(f"Failed: {total - passed}")

        # Group by scale
        print(f"\n{'─'*80}")
        print("RESULTS BY SCALE")
        print(f"{'─'*80}\n")

        for scale in [16, 64, 256, 1024]:
            scale_results = [r for r in self.results if r.scale == scale]
            scale_passed = sum(1 for r in scale_results if r.success)

            print(f"\n{scale:,} PEs: {scale_passed}/{len(scale_results)} passed")

            # Compare ZSim vs Event-Driven at this scale
            zsim_results = [r for r in scale_results if r.success and r.execution_model == "zsim"]
            event_results = [r for r in scale_results if r.success and r.execution_model == "event_driven"]

            if zsim_results:
                zsim_avg = sum(r.execution_time_ms for r in zsim_results) / len(zsim_results)
                print(f"  ZSim avg time: {zsim_avg:.1f}ms ({len(zsim_results)} tests)")

            if event_results:
                event_avg = sum(r.execution_time_ms for r in event_results) / len(event_results)
                print(f"  Event-Driven avg time: {event_avg:.1f}ms ({len(event_results)} tests)")

            if zsim_results and event_results:
                speedup = zsim_avg / event_avg if event_avg > 0 else 0
                print(f"  Speedup (Event/ZSim): {speedup:.2f}x")

        # Save detailed results
        results_file = Path("scale_comparison_results.json")
        with open(results_file, 'w') as f:
            json.dump([asdict(r) for r in self.results], f, indent=2)

        print(f"\n\nDetailed results saved to: {results_file}")

        # Generate CSV
        csv_file = Path("scale_comparison.csv")
        with open(csv_file, 'w') as f:
            f.write("Scale,Execution_Model,Workload,Memory,Success,Time_ms,Cycles,Throughput,Energy_pJ\n")
            for r in self.results:
                f.write(f"{r.scale},{r.execution_model},{r.workload},{r.memory_tech},"
                       f"{r.success},{r.execution_time_ms:.2f},{r.simulated_cycles},"
                       f"{r.throughput:.2f},{r.energy_pj:.2f}\n")

        print(f"CSV report saved to: {csv_file}\n")


def main():
    tester = ScaleComparisonTester()
    tester.run_scale_comparison()

    print("\n" + "="*80)
    print("Scale comparison testing complete!")
    print("="*80 + "\n")


if __name__ == "__main__":
    main()
