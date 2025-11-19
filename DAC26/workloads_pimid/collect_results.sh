#!/bin/bash

# Function to extract metrics from output
extract_metrics() {
    local output="$1"
    local total_energy=$(echo "$output" | grep "Total energy:" | awk '{print $3}')
    local compute_energy=$(echo "$output" | grep "Compute energy:" | awk '{print $3}')
    local memory_energy=$(echo "$output" | grep "Memory energy:" | awk '{print $3}')
    local network_energy=$(echo "$output" | grep "Network energy:" | awk '{print $3}')
    local exec_time=$(echo "$output" | grep "Execution time:" | tail -1 | awk '{print $3}')
    local total_cycles=$(echo "$output" | grep "Total cycles:" | head -1 | awk '{print $3}')
    
    echo "$total_energy|$compute_energy|$memory_energy|$network_energy|$exec_time|$total_cycles"
}

echo "Workload,Model,Arch,TotalEnergy(pJ),ComputeEnergy(pJ),MemoryEnergy(pJ),NetworkEnergy(pJ),ExecTime(us),TotalCycles"

# Shared Memory Workloads
workloads=("reduction" "spmv" "gemm" "bfs" "histogram" "dotproduct" "prefixsum" "stencil1d")
params=("8 1024" "8 512" "8 128" "8 512" "8 1024" "8 1024" "8 1024" "8 514 100")

for i in "${!workloads[@]}"; do
    workload="${workloads[$i]}"
    param="${params[$i]}"
    
    # Baseline
    output=$(./$(echo $workload)_shared_pimid $param 0 2>&1)
    metrics=$(extract_metrics "$output")
    echo "$workload,Shared,Baseline,$metrics"
    
    # LIBCom
    output=$(./$(echo $workload)_shared_pimid $param 1 2>&1)
    metrics=$(extract_metrics "$output")
    echo "$workload,Shared,LIBCom,$metrics"
done

# Message Passing Workloads
for i in "${!workloads[@]}"; do
    workload="${workloads[$i]}"
    param="${params[$i]}"
    
    # Baseline
    output=$(./$(echo $workload)_message_pimid $param 0 2>&1)
    metrics=$(extract_metrics "$output")
    echo "$workload,Message,Baseline,$metrics"
    
    # LIBCom
    output=$(./$(echo $workload)_message_pimid $param 1 2>&1)
    metrics=$(extract_metrics "$output")
    echo "$workload,Message,LIBCom,$metrics"
done
