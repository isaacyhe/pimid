#!/bin/bash
#
# CACTI Build Verification Test
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CACTI_DIR="$PROJECT_ROOT/pimid/external/cacti"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

TOTAL_TESTS=0
PASSED_TESTS=0

print_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((PASSED_TESTS++))
    ((TOTAL_TESTS++))
}

print_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((TOTAL_TESTS++))
}

print_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

echo "=============================================================="
echo "CACTI Build Verification Test"
echo "=============================================================="
echo ""

# Test 1: Makefile exists
if [ -f "$CACTI_DIR/Makefile" ]; then
    print_pass "Makefile found"
else
    print_fail "Makefile not found"
fi

# Test 2: Dependencies
command -v g++ >/dev/null 2>&1 && print_pass "g++ installed" || print_fail "g++ not installed"
command -v make >/dev/null 2>&1 && print_pass "make installed" || print_fail "make not installed"

# Test 3: Build
cd "$CACTI_DIR"
make clean >/dev/null 2>&1 || true

if [ -z "$SKIP_BUILD" ]; then
    print_info "Building CACTI..."
    if make -j$(nproc) > /tmp/cacti_build.log 2>&1; then
        print_pass "CACTI built successfully"

        if [ -f "cacti" ]; then
            print_info "Binary: $(du -h cacti | cut -f1)"
        fi
    else
        print_fail "CACTI build failed (see /tmp/cacti_build.log)"
    fi
else
    print_pass "Build test skipped (SKIP_BUILD is set)"
fi

# Summary
echo ""
echo "Summary: $PASSED_TESTS/$TOTAL_TESTS tests passed"
[ $PASSED_TESTS -eq $TOTAL_TESTS ] && exit 0 || exit 1
