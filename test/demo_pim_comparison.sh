#!/bin/bash

# Demo script to compile and run PIM compute comparison test
# This demonstrates Host vs MC-PIM vs Bank-PIM with EQUAL compute power

echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║ PIM Compute Comparison Demo (Equal Compute Power)               ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""

# Check if we're in the right directory
if [ ! -f "tests/integration/test_pim_compute_comparison.cpp" ]; then
    echo "Error: Must run from pimid-dev root directory"
    exit 1
fi

echo "Step 1: Compiling test..."
echo "────────────────────────────────────────────────────────────────────"

# Create temp build directory
mkdir -p /tmp/pim_test_build
cd /tmp/pim_test_build

# Compile the test
g++ -std=c++17 -I/home/user/pimid-dev/pimid \
    -I/home/user/pimid-dev/pimid/memory_models/include \
    /home/user/pimid-dev/tests/integration/test_pim_compute_comparison.cpp \
    /home/user/pimid-dev/pimid/memory_models/src/pim_bandwidth_tracker.cpp \
    /home/user/pimid-dev/pimid/memory_models/src/internal_dram_network.cpp \
    /home/user/pimid-dev/pimid/memory/dram_architecture.cpp \
    -o test_pim_compute_comparison

if [ $? -ne 0 ]; then
    echo "❌ Compilation failed!"
    exit 1
fi

echo "✅ Compilation successful!"
echo ""

echo "Step 2: Running comparison test..."
echo "────────────────────────────────────────────────────────────────────"
echo ""

# Run the test
./test_pim_compute_comparison

echo ""
echo "════════════════════════════════════════════════════════════════════"
echo "Demo complete! See results above."
echo "════════════════════════════════════════════════════════════════════"
