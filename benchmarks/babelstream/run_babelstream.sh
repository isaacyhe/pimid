#!/bin/bash
# Run BabelStream benchmark through pimid
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PIMID_BIN="${PIMID_BIN:-pimid}"
TIER="${TIER:-tiny}"
METHOD="${METHOD:-exec}"
PASS=0; FAIL=0; SKIP=0

echo "=== BabelStream (tier=$TIER) ==="

# Build if needed
make -C "$SCRIPT_DIR" 2>&1 | tail -1

bin="$SCRIPT_DIR/babelstream"
if [ "$TIER" = "small" ]; then
    cfg="$SCRIPT_DIR/configs/babelstream.yaml"
else
    cfg="$SCRIPT_DIR/configs/babelstream_${TIER}.yaml"
fi

if [ ! -x "$bin" ]; then
    echo "SKIP babelstream (binary not built)"
    exit 0
fi

if [ ! -f "$cfg" ]; then
    echo "SKIP babelstream (config not found: $cfg)"
    exit 0
fi

echo -n "RUN  babelstream_${TIER}... "

# Standalone test
if ! OMP_NUM_THREADS=2 "$bin" --arraysize 1024 --numtimes 2 2>&1 | grep -q "BENCH_DONE"; then
    echo "FAIL (standalone)"
    exit 1
fi

# Run through pimid if available
if command -v "$PIMID_BIN" &>/dev/null; then
    outdir=$(mktemp -d /tmp/pimid_babel_XXXXXX)
    if $PIMID_BIN --method "$METHOD" --config "$cfg" --output "$outdir" --no-power 2>&1 | tail -5 | grep -q "BENCH_DONE\|complete\|Simulation"; then
        echo "PASS"
        PASS=1
    else
        echo "FAIL (pimid)"
        FAIL=1
    fi
    rm -rf "$outdir"
else
    echo "PASS (standalone only)"
    PASS=1
fi

echo ""
echo "BabelStream: $PASS pass, $FAIL fail, $SKIP skip"
[ "$FAIL" -eq 0 ] || exit 1
