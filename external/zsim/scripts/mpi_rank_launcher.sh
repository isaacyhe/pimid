#!/bin/bash
# MPI rank launcher for ZSim config files
# Sets up PMI environment and coordinates ranks via shared file
# Usage: mpi_rank_launcher.sh <rank> <size> <binary> [args]

RANK=$1
SIZE=$2
shift 2
BINARY="$@"

# Coordination directory
COORD_DIR=/tmp/zsim_mpi_coord_$$
mkdir -p $COORD_DIR

# Set PMI environment (for MPICH ch3:nemesis)
export PMI_RANK=$RANK
export PMI_SIZE=$SIZE
export PMI_SPAWNED=1

# Simple barrier via files
barrier_wait() {
    local name=$1
    touch ${COORD_DIR}/${name}_${RANK}
    while [ $(ls ${COORD_DIR}/${name}_* 2>/dev/null | wc -l) -lt $SIZE ]; do
        sleep 0.01
    done
}

# Signal ready
barrier_wait "ready"

# Run the binary
exec $BINARY
