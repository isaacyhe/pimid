#!/usr/bin/env python3
"""
Comprehensive Test Suite for Execution Models
Tests both ZSim and Event-Driven execution models with various workloads
at different scales (10K to 100K) using small data inputs for speed.

Author: PIMID Testing Framework
Date: 2025-11-23
"""

import os
import sys
import json
import yaml
import subprocess
import time
from pathlib import Path
from typing import Dict, List, Tuple, Any
from dataclasses import dataclass, asdict
import argparse

@dataclass
class TestConfig:
    """Test configuration parameters"""
    name: str
    workload: str
    execution_model: str  # "zsim" or "event_driven"
    num_pes: int
    pe_placement: str  # "bank" or "subarray"
    core_type: str  # ZSim core type
    memory_tech: str
    workload_params: Dict[str, Any]
    expected_speedup_range: Tuple[float, float] = (0.5, 5.0)

@dataclass
class TestResult:
    """Test result data"""
    config_name: str
    execution_model: str
    num_pes: int
    workload: str
    success: bool
    execution_time_ms: float
    throughput: float
    error_message: str = ""

class ExecutionModelTester:
    """Comprehensive execution model testing framework"""

    def __init__(self, output_dir: str = "test_results"):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(exist_ok=True)
        self.results: List[TestResult] = []
        self.pimid_binary = Path("/home/user/pimid-dev/build/test/benchmarks/benchmark_runner")

    def create_config_file(self, config: TestConfig) -> Path:
        """Create YAML configuration file for test"""
        config_data = {
            "simulation": {
                "name": config.name,
                "mode": "standalone",
                "host_execution_model": "event_driven",  # Fast host for testing
                "device_execution_model": config.execution_model,
            },
            "memory": {
                "technology": config.memory_tech,
                "dram_type": "DDR4",
                "channels": 1,
                "ranks_per_channel": 1,
                "banks_per_rank": 16,
                "bg_per_rank": 4,
            },
            "pim": {
                "enabled": True,
                "num_pes": config.num_pes,
                "pe_placement": config.pe_placement,
            },
            "processing_element": {
                "type": config.core_type,
                "frequency_mhz": 1000.0,
            },
            "workload": {
                "name": config.workload,
                "params": config.workload_params,
            }
        }

        # Add ZSim-specific config if needed
        if config.execution_model == "zsim":
            config_data["zsim"] = {
                "config_file": "pimid/external/zsim/tests/pim_pe_bank_level.cfg",
                "phase_length": 1000,
                "stats_phase_interval": 1000,
            }

        # Add event-driven config
        if config.execution_model == "event_driven":
            config_data["event_driven"] = {
                "performance_model": "roofline",
                "device": {
                    "num_cores": config.num_pes,
                    "frequency_mhz": 1000.0,
                    "ipc": 1.0,
                    "peak_bandwidth_gbps": 100.0,
                    "peak_flops": config.num_pes * 2.0,
                }
            }

        config_file = self.output_dir / f"{config.name}.yaml"
        with open(config_file, 'w') as f:
            yaml.dump(config_data, f, default_flow_style=False)

        return config_file

    def run_test(self, config: TestConfig) -> TestResult:
        """Run a single test configuration"""
        print(f"\n{'='*80}")
        print(f"Testing: {config.name}")
        print(f"  Execution Model: {config.execution_model}")
        print(f"  Workload: {config.workload}")
        print(f"  PEs: {config.num_pes}")
        print(f"  Core Type: {config.core_type}")
        print(f"  Memory: {config.memory_tech}")
        print(f"{'='*80}")

        # Create config file
        config_file = self.create_config_file(config)

        # Run simulation
        start_time = time.time()
        try:
            cmd = [str(self.pimid_binary), "--config", str(config_file)]
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=300,  # 5 minute timeout per test
            )

            execution_time = (time.time() - start_time) * 1000  # Convert to ms

            if result.returncode == 0:
                # Parse output for metrics
                throughput = self._parse_throughput(result.stdout)

                test_result = TestResult(
                    config_name=config.name,
                    execution_model=config.execution_model,
                    num_pes=config.num_pes,
                    workload=config.workload,
                    success=True,
                    execution_time_ms=execution_time,
                    throughput=throughput,
                )
                print(f"✓ PASSED - Time: {execution_time:.2f}ms, Throughput: {throughput:.2f}")
            else:
                test_result = TestResult(
                    config_name=config.name,
                    execution_model=config.execution_model,
                    num_pes=config.num_pes,
                    workload=config.workload,
                    success=False,
                    execution_time_ms=execution_time,
                    throughput=0.0,
                    error_message=result.stderr[:500],
                )
                print(f"✗ FAILED - {result.stderr[:200]}")

        except subprocess.TimeoutExpired:
            test_result = TestResult(
                config_name=config.name,
                execution_model=config.execution_model,
                num_pes=config.num_pes,
                workload=config.workload,
                success=False,
                execution_time_ms=0.0,
                throughput=0.0,
                error_message="Timeout after 300 seconds",
            )
            print(f"✗ TIMEOUT")

        except Exception as e:
            test_result = TestResult(
                config_name=config.name,
                execution_model=config.execution_model,
                num_pes=config.num_pes,
                workload=config.workload,
                success=False,
                execution_time_ms=0.0,
                throughput=0.0,
                error_message=str(e),
            )
            print(f"✗ ERROR - {str(e)}")

        self.results.append(test_result)
        return test_result

    def _parse_throughput(self, output: str) -> float:
        """Parse throughput from simulation output"""
        # Look for throughput metrics in output
        for line in output.split('\n'):
            if 'throughput' in line.lower() or 'ops/sec' in line.lower():
                try:
                    # Extract numeric value
                    parts = line.split()
                    for part in parts:
                        try:
                            return float(part)
                        except ValueError:
                            continue
                except:
                    pass
        return 1.0  # Default if not found

    def generate_test_configs(self) -> List[TestConfig]:
        """Generate comprehensive test configurations"""
        configs = []

        # Scales to test (realistic PE counts: 1 PE per bank or per subarray)
        scales = [
            (16, "16pe"),      # Bank-level: 1 PE per bank (16 banks)
            (64, "64pe"),      # 4 banks × 16 subarrays or 64 banks
            (256, "256pe"),    # 16 banks × 16 subarrays
            (1024, "1024pe"),  # 16 banks × 64 subarrays
        ]

        # Execution models to test
        execution_models = [
            ("zsim", "Simple"),  # ZSim with Simple cores
            ("zsim", "ALU"),     # ZSim with ALU cores (no cache)
            ("event_driven", "analytical"),  # Event-driven analytical
        ]

        # Workloads with SMALL inputs for speed
        workloads = [
            {
                "name": "bfs",
                "params": {
                    "num_vertices": 128,      # Small graph
                    "avg_degree": 4,
                },
            },
            {
                "name": "gemm",
                "params": {
                    "matrix_size": 64,        # Small matrix (64x64)
                },
            },
            {
                "name": "spmv",
                "params": {
                    "matrix_size": 128,       # Small sparse matrix
                    "sparsity": 0.9,
                },
            },
            {
                "name": "dot_product",
                "params": {
                    "vector_length": 256,     # Small vectors
                },
            },
            {
                "name": "reduction",
                "params": {
                    "elements_per_subarray": 256,
                },
            },
            {
                "name": "histogram",
                "params": {
                    "num_elements": 512,      # Small dataset
                },
            },
        ]

        # Memory technologies
        memory_techs = ["DRAM", "SRAM", "ReRAM"]

        # PE placements
        pe_placements = ["bank", "subarray"]

        # Generate configurations
        test_id = 0
        for scale, scale_name in scales:
            for exec_model, core_type in execution_models:
                for workload in workloads:
                    for mem_tech in memory_techs:
                        for placement in pe_placements:
                            # Skip invalid combinations
                            if placement == "bank" and scale > 64:
                                # Can't have more than 64 banks typically
                                continue
                            if placement == "subarray" and scale <= 16:
                                # Subarray placement needs more PEs than banks
                                continue

                            test_id += 1
                            config = TestConfig(
                                name=f"test_{test_id:04d}_{scale_name}_{exec_model}_{core_type}_{workload['name']}_{mem_tech}_{placement}",
                                workload=workload['name'],
                                execution_model=exec_model,
                                num_pes=scale,
                                pe_placement=placement,
                                core_type=core_type,
                                memory_tech=mem_tech,
                                workload_params=workload['params'],
                            )
                            configs.append(config)

        return configs

    def run_all_tests(self, max_tests: int = None):
        """Run all test configurations"""
        configs = self.generate_test_configs()

        if max_tests:
            configs = configs[:max_tests]

        print(f"\n{'#'*80}")
        print(f"# COMPREHENSIVE EXECUTION MODEL TEST SUITE")
        print(f"# Total configurations: {len(configs)}")
        print(f"# Testing scales: 16, 64, 256, 1024 PEs (realistic)")
        print(f"# Execution models: ZSim (Simple, ALU), Event-Driven")
        print(f"# Workloads: BFS, GEMM, SpMV, DotProduct, Reduction, Histogram")
        print(f"# Memory technologies: DRAM, SRAM, ReRAM")
        print(f"{'#'*80}\n")

        start_time = time.time()

        for i, config in enumerate(configs, 1):
            print(f"\n[{i}/{len(configs)}] Running test...")
            self.run_test(config)

        total_time = time.time() - start_time

        # Generate summary
        self.generate_summary(total_time)

    def generate_summary(self, total_time: float):
        """Generate test summary and comparison report"""
        print(f"\n{'#'*80}")
        print(f"# TEST SUMMARY")
        print(f"{'#'*80}\n")

        total_tests = len(self.results)
        passed = sum(1 for r in self.results if r.success)
        failed = total_tests - passed

        print(f"Total Tests: {total_tests}")
        print(f"Passed: {passed} ({100*passed/total_tests:.1f}%)")
        print(f"Failed: {failed} ({100*failed/total_tests:.1f}%)")
        print(f"Total Time: {total_time:.2f}s ({total_time/60:.1f} minutes)")

        # Compare execution models
        print(f"\n{'='*80}")
        print(f"EXECUTION MODEL COMPARISON")
        print(f"{'='*80}\n")

        # Group by workload and scale
        zsim_results = [r for r in self.results if r.success and 'zsim' in r.execution_model]
        event_results = [r for r in self.results if r.success and 'event' in r.execution_model]

        print(f"ZSim Tests: {len(zsim_results)} passed")
        print(f"Event-Driven Tests: {len(event_results)} passed")

        if zsim_results and event_results:
            zsim_avg_time = sum(r.execution_time_ms for r in zsim_results) / len(zsim_results)
            event_avg_time = sum(r.execution_time_ms for r in event_results) / len(event_results)
            speedup = zsim_avg_time / event_avg_time if event_avg_time > 0 else 0

            print(f"\nAverage Execution Time:")
            print(f"  ZSim: {zsim_avg_time:.2f}ms")
            print(f"  Event-Driven: {event_avg_time:.2f}ms")
            print(f"  Speedup (Event/ZSim): {speedup:.2f}x")

        # Save detailed results to JSON
        results_file = self.output_dir / "test_results.json"
        with open(results_file, 'w') as f:
            json.dump([asdict(r) for r in self.results], f, indent=2)
        print(f"\nDetailed results saved to: {results_file}")

        # Generate comparison CSV
        self.generate_comparison_csv()

    def generate_comparison_csv(self):
        """Generate CSV comparison report"""
        csv_file = self.output_dir / "execution_model_comparison.csv"

        with open(csv_file, 'w') as f:
            f.write("Config,Execution_Model,Num_PEs,Workload,Success,Time_ms,Throughput\n")
            for result in self.results:
                f.write(f"{result.config_name},{result.execution_model},{result.num_pes},"
                       f"{result.workload},{result.success},{result.execution_time_ms:.2f},"
                       f"{result.throughput:.2f}\n")

        print(f"Comparison CSV saved to: {csv_file}")


def main():
    parser = argparse.ArgumentParser(
        description="Comprehensive Execution Model Testing Suite"
    )
    parser.add_argument(
        "--max-tests",
        type=int,
        default=None,
        help="Maximum number of tests to run (default: all)"
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default="test_results_execution_models",
        help="Output directory for results"
    )

    args = parser.parse_args()

    # Create tester
    tester = ExecutionModelTester(output_dir=args.output_dir)

    # Run tests
    tester.run_all_tests(max_tests=args.max_tests)

    print(f"\n{'#'*80}")
    print(f"# Testing complete! Check {args.output_dir}/ for detailed results")
    print(f"{'#'*80}\n")


if __name__ == "__main__":
    main()
