#!/usr/bin/env bash
# Emerging-memory technology sweep (the "mem_sweep" figure): SRAM + 3 NVM
# technologies x 5 kernels x {shared-memory (OpenMP), message-passing (MPI)}
# on 16 in-memory PEs at BANK placement, cycle-accurate network model.
#
# Identical harness to dram_sweep.sh with the technology list swapped.
# Run from the repository root:   bash examples/figures/mem_sweep.sh
# Output: results_mem_sweep/mem_sweep.csv
set -uo pipefail
export TECHS="${TECHS:-SRAM STT_MRAM ReRAM PCM}"
export OUT="${OUT:-results_mem_sweep}"
bash "$(dirname "$0")/dram_sweep.sh"
mv -f "$OUT/dram_sweep.csv" "$OUT/mem_sweep.csv" 2>/dev/null || true
echo "DONE -> $OUT/mem_sweep.csv"
