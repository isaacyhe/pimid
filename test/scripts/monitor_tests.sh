#!/bin/bash
# Monitor test progress

while true; do
    clear
    echo "===================================================================="
    echo "  PIMID Test Suite - Live Progress Monitor"
    echo "===================================================================="
    echo ""

    if [ -f test_run_v2.log ]; then
        # Extract current test number
        CURRENT=$(grep -oP '\[\s*\K\d+(?=/1000\])' test_run_v2.log | tail -1)

        # Count passes and failures
        PASSED=$(grep -c "✓ PASSED" test_run_v2.log)
        FAILED=$(grep -c "✗ FAILED" test_run_v2.log)
        TIMEOUT=$(grep -c "⏱ TIMEOUT" test_run_v2.log)

        if [ -n "$CURRENT" ]; then
            PCT=$((CURRENT * 100 / 1000))
            echo "  Progress: $CURRENT / 1000 ($PCT%)"
            echo ""
            echo "  Results:"
            echo "    ✓ Passed:  $PASSED"
            echo "    ✗ Failed:  $FAILED"
            echo "    ⏱ Timeout: $TIMEOUT"
            echo ""

            if [ $PASSED -gt 0 ]; then
                PASS_RATE=$((PASSED * 100 / (PASSED + FAILED + TIMEOUT)))
                echo "  Pass Rate: $PASS_RATE%"
            fi
            echo ""
            echo "  Latest tests:"
            tail -10 test_run_v2.log | grep -E "Test \d+:" | tail -5
        else
            echo "  Initializing..."
        fi
    else
        echo "  Waiting for log file..."
    fi

    echo ""
    echo "===================================================================="
    echo "  Press Ctrl+C to stop monitoring (tests will continue running)"
    echo "===================================================================="

    sleep 5
done
