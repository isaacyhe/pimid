#!/bin/bash
# Master runner: execute all PIMID benchmark suites
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export TIER="${TIER:-tiny}"
export METHOD="${METHOD:-exec}"
export PIMID_BIN="${PIMID_BIN:-pimid}"

TOTAL_PASS=0; TOTAL_FAIL=0; TOTAL_SKIP=0

run_suite() {
    local name="$1"
    local script="$2"
    echo ""
    echo "================================================================"
    echo "  $name"
    echo "================================================================"
    if [ -x "$script" ]; then
        bash "$script" || true
    else
        echo "SKIP $name (script not found: $script)"
    fi
}

run_suite "PIM Kernels" "$SCRIPT_DIR/pim_kernels/run_pim.sh"
run_suite "BabelStream" "$SCRIPT_DIR/babelstream/run_babelstream.sh"
run_suite "Rodinia"     "$SCRIPT_DIR/rodinia/run_rodinia.sh"
run_suite "Classic"     "$SCRIPT_DIR/classic/run_classic.sh"
run_suite "NPB"         "$SCRIPT_DIR/npb/run_npb.sh"
run_suite "SPLASH-3"    "$SCRIPT_DIR/splash3/run_splash3.sh"
run_suite "PARSEC"      "$SCRIPT_DIR/parsec/run_parsec.sh"

echo ""
echo "================================================================"
echo "  All suites complete."
echo "================================================================"
