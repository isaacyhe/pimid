#!/bin/bash
# Run Classic single-thread benchmarks
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PIMID_BIN="${PIMID_BIN:-pimid}"
TIER="${TIER:-tiny}"
METHOD="${METHOD:-exec}"
PASS=0; FAIL=0; SKIP=0

BENCHMARKS="dhrystone whetstone binary_search quicksort sha256 naive_matmul"

echo "=== Classic Single-Thread Benchmarks (tier=$TIER) ==="

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
    if "$bin" --iters 100 --loops 100 --size 64 --lookups 10 2>&1 | grep -q "BENCH_DONE"; then
        echo "PASS"
        PASS=$((PASS + 1))
    else
        echo "FAIL"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "Classic: $PASS pass, $FAIL fail, $SKIP skip"
[ "$FAIL" -eq 0 ] || exit 1
