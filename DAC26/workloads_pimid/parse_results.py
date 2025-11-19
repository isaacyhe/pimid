#!/usr/bin/env python3

import subprocess
import re

def run_workload(workload, model, params, arch):
    """Run a workload and extract key metrics"""
    cmd = f"./{workload}_{model}_pimid {params} {arch}"
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=30)
        output = result.stdout + result.stderr
        
        metrics = {}
        
        # Extract metrics using regex
        total_energy = re.search(r'Total energy:\s+([\d.e+]+)\s+pJ', output)
        compute_energy = re.search(r'Compute energy:\s+([\d.e+]+)\s+pJ', output)
        memory_energy = re.search(r'Memory energy:\s+([\d.e+]+)\s+pJ', output)
        network_energy = re.search(r'Network energy:\s+([\d.e+]+)\s+pJ', output)
        exec_time = re.search(r'Execution time:\s+([\d.e+]+)\s+us', output)
        total_cycles = re.search(r'Total cycles:\s+(\d+)', output)
        
        metrics['total_energy'] = float(total_energy.group(1)) if total_energy else 0
        metrics['compute_energy'] = float(compute_energy.group(1)) if compute_energy else 0
        metrics['memory_energy'] = float(memory_energy.group(1)) if memory_energy else 0
        metrics['network_energy'] = float(network_energy.group(1)) if network_energy else 0
        metrics['exec_time'] = float(exec_time.group(1)) if exec_time else 0
        metrics['total_cycles'] = int(total_cycles.group(1)) if total_cycles else 0
        
        return metrics
    except Exception as e:
        print(f"Error running {workload}_{model}_pimid: {e}")
        return None

# Configuration
workloads = [
    ("reduction", "8 1024"),
    ("spmv", "8 512"),
    ("gemm", "8 128"),
    ("bfs", "8 512"),
    ("histogram", "8 1024"),
    ("dotproduct", "8 1024"),
    ("prefixsum", "8 1024"),
    ("stencil1d", "8 514 100")
]

models = ["shared", "message"]
archs = [("Baseline", 0), ("LIBCom", 1)]

print("=" * 150)
print("DAC'26 PIMID Comprehensive Evaluation Results")
print("Technology: 45nm, Frequency: 1GHz")
print("=" * 150)
print()

for model in models:
    print(f"\n{'='*150}")
    print(f"PROGRAMMING MODEL: {model.upper()}")
    print(f"{'='*150}\n")
    
    for workload_name, params in workloads:
        print(f"\n--- {workload_name.upper()} ---")
        print(f"Configuration: {params}")
        print(f"{'-'*150}")
        print(f"{'Architecture':<12} {'Total Energy (pJ)':<18} {'Compute (pJ)':<15} {'Memory (pJ)':<15} {'Network (pJ)':<15} {'Exec Time (μs)':<18} {'Cycles':<12}")
        print(f"{'-'*150}")
        
        baseline_metrics = None
        libcom_metrics = None
        
        for arch_name, arch_val in archs:
            metrics = run_workload(workload_name, model, params, arch_val)
            if metrics:
                print(f"{arch_name:<12} {metrics['total_energy']:<18.2f} {metrics['compute_energy']:<15.2f} {metrics['memory_energy']:<15.2f} {metrics['network_energy']:<15.2f} {metrics['exec_time']:<18.2f} {metrics['total_cycles']:<12}")
                
                if arch_name == "Baseline":
                    baseline_metrics = metrics
                else:
                    libcom_metrics = metrics
        
        # Calculate reductions
        if baseline_metrics and libcom_metrics and baseline_metrics['total_energy'] > 0:
            energy_reduction = ((baseline_metrics['total_energy'] - libcom_metrics['total_energy']) / baseline_metrics['total_energy']) * 100
            
            if baseline_metrics['network_energy'] > 0:
                network_reduction = ((baseline_metrics['network_energy'] - libcom_metrics['network_energy']) / baseline_metrics['network_energy']) * 100
            else:
                network_reduction = 0
                
            print(f"{'-'*150}")
            print(f"Energy Reduction (LIBCom): {energy_reduction:.1f}%   |   Network Energy Reduction: {network_reduction:.1f}%")

print(f"\n{'='*150}")
print("Evaluation Complete!")
print(f"{'='*150}")
