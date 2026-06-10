#!/bin/bash
# Wrapper to run MPI processes under PIN/ZSim
# Usage: mpirun -np N pin_mpi_wrapper.sh <binary> [args]
#
# Environment variables (set before mpirun):
#   PINPATH  - Path to PIN installation (required)
#   ZSIM_DIR - Path to ZSim directory (required)
#   OUTDIR_BASE - Base directory for output (default: /tmp/zsim_mpi)

set -e

# Required environment
if [ -z "$PINPATH" ]; then
    echo "ERROR: PINPATH not set" >&2
    exit 1
fi

if [ -z "$ZSIM_DIR" ]; then
    echo "ERROR: ZSIM_DIR not set" >&2
    exit 1
fi

# Get rank from MPICH environment
RANK=${PMI_RANK:-0}

# Output directory per rank
OUTDIR_BASE=${OUTDIR_BASE:-/tmp/zsim_mpi}
OUTDIR=${OUTDIR_BASE}/rank${RANK}
mkdir -p $OUTDIR
cd $OUTDIR

# Create per-rank config
cat > zsim.cfg << EOF
sys = {
    cores = {
        simple = {
            type = "Simple";
            cores = 1;
            icache = "l1i";
            dcache = "l1d";
        };
    };
    lineSize = 64;
    caches = {
        l1d = { caches = 1; size = 32768; };
        l1i = { caches = 1; size = 32768; };
        l2 = { caches = 1; size = 1048576; children = "l1d|l1i"; };
    };
};
sim = {
    phaseLength = 10000;
    maxTotalInstrs = 500000000;
};
process0 = { command = "$@"; };
EOF

export ZSIM_FORCE_POSIX_SHM=1

# Run zsim from output dir so zsim.out goes there
exec ${ZSIM_DIR}/build/opt/zsim zsim.cfg 2>&1 | tee zsim.log
