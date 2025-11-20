#!/bin/bash
# Script to update all workload files to support memory technology parameter

WORKLOADS_DIR="/home/user/pimid-dev/DAC26/workloads_pimid"

# List of workloads to update (excluding bfs_message which is already done)
WORKLOADS=(
    "bfs_shared_pimid"
    "gemm_message_pimid"
    "gemm_shared_pimid"
    "spmv_message_pimid"
    "spmv_shared_pimid"
    "dotproduct_message_pimid"
    "dotproduct_shared_pimid"
    "reduction_message_pimid"
    "reduction_shared_pimid"
    "histogram_message_pimid"
    "histogram_shared_pimid"
    "prefixsum_message_pimid"
    "prefixsum_shared_pimid"
    "stencil1d_message_pimid"
    "stencil1d_shared_pimid"
)

echo "Updating workload files to support memory technology parameter..."

for workload in "${WORKLOADS[@]}"; do
    file="${WORKLOADS_DIR}/${workload}.cpp"

    if [ ! -f "$file" ]; then
        echo "  Warning: $file not found, skipping..."
        continue
    fi

    echo "  Processing $workload..."

    # Create backup
    cp "$file" "${file}.bak"

    # For stencil workloads (have different config struct)
    if [[ $workload == stencil* ]]; then
        # Add memory_tech to Stencil1DConfig
        sed -i 's/\(struct Stencil1DConfig {[^}]*\)/\1    MemoryTech memory_tech = MemoryTech::SRAM;\n/g' "$file"
        # Add memory_tech to PIMConfig initialization
        sed -i 's/pim_config\.topology = config\.topology;/pim_config.topology = config.topology;\n        pim_config.memory_tech = config.memory_tech;/g' "$file"
    else
        # For other workloads, find the config struct name
        config_name=$(grep -oP 'struct \K\w+Config' "$file" | head -1)
        if [ -n "$config_name" ]; then
            # Add memory_tech to config struct (before closing brace)
            perl -i -pe 's/(struct '"$config_name"' \{[^}]*)(};)/\1    MemoryTech memory_tech = MemoryTech::SRAM;\n\2/s' "$file"
            # Add memory_tech to PIMConfig initialization
            sed -i 's/pim_config\.topology = config\.topology;/pim_config.topology = config.topology;\n        pim_config.memory_tech = config.memory_tech;/g' "$file"
        fi
    fi

    # Update main() to accept optional memory_tech parameter
    # This is more complex, so let's do it carefully
    # Update the usage message
    sed -i 's/argc < 4/argc < 4 \&\& argc < 5/g' "$file"
    sed -i 's/Usage: .* <num_subarrays>/Usage: " << argv[0] << " <num_subarrays>/g' "$file"

    # Add memory_tech parameter handling before config.avg_degree or config.num_iters
    if [[ $workload == stencil* ]]; then
        # For stencil, insert before config.num_iters
        perl -i -pe 's/(config\.topology = .*;\s*\n)(\s*config\.num_iters)/\1\n    \/\/ Optional memory technology parameter (default: SRAM)\n    if (argc >= 6) {\n        int mem_tech = std::atoi(argv[5]);\n        if (mem_tech >= 0 && mem_tech <= 4) {\n            config.memory_tech = static_cast<MemoryTech>(mem_tech);\n        }\n    }\n\n\2/s' "$file"
    else
        # For others, insert after topology assignment
        perl -i -pe 's/(config\.topology = .*HTREE_BASELINE;\s*\n)/\1\n    \/\/ Optional memory technology parameter (default: SRAM)\n    if (argc >= 5) {\n        int mem_tech = std::atoi(argv[4]);\n        if (mem_tech >= 0 && mem_tech <= 4) {\n            config.memory_tech = static_cast<MemoryTech>(mem_tech);\n        }\n    }\n/s' "$file"
    fi

    echo "    ✓ Updated $workload"
done

echo ""
echo "All workload files updated!"
echo "Backup files created with .bak extension"
echo ""
echo "To revert changes: for f in ${WORKLOADS_DIR}/*.cpp.bak; do mv \"\$f\" \"\${f%.bak}\"; done"
