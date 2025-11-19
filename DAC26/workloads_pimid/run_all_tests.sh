#!/bin/bash

echo "=== DAC26 PIMID Comprehensive Evaluation ==="
echo "Technology: 45nm, 1GHz"
echo "Architectures: Baseline H-tree, LIBCom"
echo "Programming Models: Shared Memory, Message Passing"
echo ""

# Shared Memory Workloads
echo "### SHARED MEMORY WORKLOADS ###"
echo ""

for config in 0 1; do
    if [ $config -eq 0 ]; then
        echo "--- BASELINE H-TREE ---"
    else
        echo "--- LIBCOM ---"
    fi
    
    echo "Reduction (8 subarrays, 1024 elements):"
    ./reduction_shared_pimid 8 1024 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo "SpMV (8 subarrays, 512x512 matrix):"
    ./spmv_shared_pimid 8 512 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo "GEMM (8 subarrays, 128x128 matrix):"
    ./gemm_shared_pimid 8 128 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo "BFS (8 subarrays, 512 vertices):"
    ./bfs_shared_pimid 8 512 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo "Histogram (8 subarrays, 1024 elements):"
    ./histogram_shared_pimid 8 1024 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo "Dot Product (8 subarrays, 1024 elements):"
    ./dotproduct_shared_pimid 8 1024 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo "Prefix Sum (8 subarrays, 1024 elements):"
    ./prefixsum_shared_pimid 8 1024 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo "Stencil 1D (8 subarrays, 514 points, 100 iters):"
    ./stencil1d_shared_pimid 8 514 100 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo ""
done

# Message Passing Workloads
echo "### MESSAGE PASSING WORKLOADS ###"
echo ""

for config in 0 1; do
    if [ $config -eq 0 ]; then
        echo "--- BASELINE H-TREE ---"
    else
        echo "--- LIBCOM ---"
    fi
    
    echo "Reduction (8 subarrays, 1024 elements):"
    ./reduction_message_pimid 8 1024 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo "SpMV (8 subarrays, 512x512 matrix):"
    ./spmv_message_pimid 8 512 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo "GEMM (8 subarrays, 128x128 matrix):"
    ./gemm_message_pimid 8 128 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo "BFS (8 subarrays, 512 vertices):"
    ./bfs_message_pimid 8 512 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo "Histogram (8 subarrays, 1024 elements):"
    ./histogram_message_pimid 8 1024 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo "Dot Product (8 subarrays, 1024 elements):"
    ./dotproduct_message_pimid 8 1024 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo "Prefix Sum (8 subarrays, 1024 elements):"
    ./prefixsum_message_pimid 8 1024 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo "Stencil 1D (8 subarrays, 514 points, 100 iters):"
    ./stencil1d_message_pimid 8 514 100 $config 2>&1 | grep -E "(Total energy:|Network energy:|Execution time:)" | head -3
    
    echo ""
done
