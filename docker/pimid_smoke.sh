#!/bin/bash
# In-image PIMID feature sweep — same coverage as the host regression
# harness, but pointed at the in-image paths.
set -uo pipefail
cd /opt/pimid
PIMID=/opt/pimid/bin/pimid
EXAMPLES=/opt/configs/examples
COSIM_CFG=/opt/pimid/configs/cosim
COSIM_WL=/opt/pimid/test/benchmarks/host_device_cosim
OUT=${1:-/tmp/pimid_smoke}
rm -rf $OUT
mkdir -p $OUT

PASS=0; FAIL=0; TIMEOUT_CT=0
RESULTS=()

run() {
  local label="$1"; shift
  local rc=0
  local tmo=120
  case "$label" in
    tech_STT_MRAM|tech_PCM|tech_ReRAM) tmo=600 ;;   # NVSim characterization
    cosim_*) tmo=180 ;;
    mpi_*) tmo=300 ;;
    trace_gen_*) tmo=300 ;;
  esac
  timeout $tmo "$@" > $OUT/$label.log 2>&1
  rc=$?
  echo "rc=$rc" >> $OUT/$label.log
  if   [ $rc -eq 0 ];   then PASS=$((PASS+1)); RESULTS+=("PASS    $label")
  elif [ $rc -eq 124 ]; then TIMEOUT_CT=$((TIMEOUT_CT+1)); RESULTS+=("TIMEOUT $label")
  else                       FAIL=$((FAIL+1)); RESULTS+=("FAIL    $label (rc=$rc)")
  fi
}

# ── 12 memory techs
for f in $EXAMPLES/tech_*.yaml; do
  run "$(basename $f .yaml)" $PIMID --method exec --config $f --no-power
done

# ── 5 core / PE types
for f in $EXAMPLES/core_*.yaml; do
  run "$(basename $f .yaml)" $PIMID --method exec --config $f --no-power
done

# ── 6 NoC topologies
for f in $EXAMPLES/topo_*.yaml; do
  run "$(basename $f .yaml)" $PIMID --method exec --config $f --no-power
done

# ── 2 network models (simple vs garnet detailed)
for f in $EXAMPLES/netmodel_*.yaml; do
  run "$(basename $f .yaml)" $PIMID --method exec --config $f --no-power
done

# ── 5 hierarchical DRAM placement tiers
for f in $EXAMPLES/placement_*.yaml; do
  run "$(basename $f .yaml)" $PIMID --method exec --config $f --no-power
done

# ── 2 HBM logic-die variants
for f in $EXAMPLES/logicdie_*.yaml; do
  run "$(basename $f .yaml)" $PIMID --method exec --config $f --no-power
done

# ── Power on / off
run "power_off" $PIMID --method exec --config $EXAMPLES/tech_DDR4.yaml --no-power
run "power_on"  $PIMID --method exec --config $EXAMPLES/tech_DDR4.yaml --power

# ── Cosim
for kernel in reduction_tree_cosim matmul_tiled_cosim histogram_merge_cosim; do
  run "cosim_basic_$(echo $kernel | cut -d_ -f1)" \
      $PIMID --method exec --scope system \
      --config $COSIM_CFG/host_device_basic.yaml \
      --workload $COSIM_WL/$kernel 256 4 0
done
run "cosim_multidev_reduction" $PIMID --method exec --scope system \
    --config $COSIM_CFG/multi_device.yaml \
    --workload $COSIM_WL/reduction_tree_cosim 256 4 0

# ── CLI surface
run "cli_help"    $PIMID --help
run "cli_version" $PIMID --version

echo
echo "============================================================="
echo "PIMID feature sweep:  $PASS pass  /  $FAIL fail  /  $TIMEOUT_CT timeout"
echo "============================================================="
for r in "${RESULTS[@]}"; do echo "  $r"; done | tee $OUT/_summary.txt
echo
echo "Per-test logs: $OUT/"
[ $FAIL -eq 0 ] && exit 0 || exit 1
