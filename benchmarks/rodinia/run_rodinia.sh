#!/bin/bash
# Run Rodinia benchmarks
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PIMID_BIN="${PIMID_BIN:-pimid}"
TIER="${TIER:-tiny}"
METHOD="${METHOD:-exec}"
PASS=0; FAIL=0; SKIP=0

BENCHMARKS="hotspot needle pathfinder srad kmeans lud backprop lavamd particlefilter myocyte"

echo "=== Rodinia Benchmarks (tier=$TIER) ==="

# Build
make -C "$SCRIPT_DIR" all 2>&1 | tail -1

for bench in $BENCHMARKS; do
    bin="$SCRIPT_DIR/$bench/$bench"
    if [ "$TIER" = "small" ]; then
        cfg="$SCRIPT_DIR/$bench/${bench}.yaml"
    else
        cfg="$SCRIPT_DIR/$bench/${bench}_${TIER}.yaml"
    fi
    name="${bench}_${TIER}"

    if [ ! -x "$bin" ]; then
        echo "SKIP $name (binary not built)"
        SKIP=$((SKIP + 1))
        continue
    fi
    if [ ! -f "$cfg" ]; then
        echo "SKIP $name (config not found)"
        SKIP=$((SKIP + 1))
        continue
    fi

    echo -n "RUN  $name... "
    # Standalone test with tiny args
    if OMP_NUM_THREADS=2 "$bin" --size 16 --rows 16 --cols 16 --iters 2 --boxes 2 --points 64 --clusters 2 --dims 4 --particles 32 --frames 2 --cells 1 --steps 10 --input 256 --lookups 10 2>&1 | grep -q "BENCH_DONE"; then
        echo "PASS"
        PASS=$((PASS + 1))
    else
        echo "FAIL"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "Rodinia: $PASS pass, $FAIL fail, $SKIP skip"
[ "$FAIL" -eq 0 ] || exit 1
