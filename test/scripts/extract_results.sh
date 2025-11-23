#!/bin/bash
echo "Workload,Memory_Tech,Total_Latency_ms,Throughput_Mv/s,Edges_Medges/s,Per_Vertex_Time_ns"
for log in benchmark_results/*.log; do
    workload=$(basename "$log" .log | cut -d'_' -f1)
    mem_tech=$(basename "$log" .log | cut -d'_' -f2)
    
    latency=$(grep "Total latency:" "$log" | awk '{print $3}')
    throughput=$(grep "Throughput:" "$log" | awk '{print $2}')
    edges=$(grep "Edges/sec:" "$log" | awk '{print $2}')
    per_vertex=$(grep "Per-vertex time:" "$log" | awk '{print $3}')
    
    echo "$workload,$mem_tech,$latency,$throughput,$edges,$per_vertex"
done
