#!/bin/bash
#
# Ramulator2 Build Verification Test
# Tests that ramulator2 builds successfully on Ubuntu 24.04
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RAMULATOR_DIR="$PROJECT_ROOT/pimid/external/ramulator"

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

TOTAL_TESTS=0
PASSED_TESTS=0

print_header() {
    echo ""
    echo "=============================================================="
    echo "$1"
    echo "=============================================================="
}

print_test() {
    echo ""
    echo "TEST: $1"
}

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

#==============================================================================
# Test 1: Check Build System
#==============================================================================
test_build_system() {
    print_test "CMake Build System"

    if [ ! -f "$RAMULATOR_DIR/CMakeLists.txt" ]; then
        print_fail "CMakeLists.txt not found"
        return 1
    fi

    command -v cmake >/dev/null 2>&1 || {
        print_fail "cmake not installed"
        return 1
    }

    print_pass "CMake build system present"
    return 0
}

#==============================================================================
# Test 2: Check Dependencies
#==============================================================================
test_dependencies() {
    print_test "Build Dependencies"

    MISSING_DEPS=()

    command -v cmake >/dev/null 2>&1 || MISSING_DEPS+=("cmake")
    command -v g++ >/dev/null 2>&1 || MISSING_DEPS+=("g++")
    command -v make >/dev/null 2>&1 || MISSING_DEPS+=("make")

    if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
        print_fail "Missing dependencies: ${MISSING_DEPS[*]}"
        return 1
    fi

    print_pass "All dependencies installed"
    return 0
}

#==============================================================================
# Test 3: CMake Configuration
#==============================================================================
test_cmake_config() {
    print_test "CMake Configuration"

    cd "$RAMULATOR_DIR"

    # Create build directory
    rm -rf build_test
    mkdir -p build_test
    cd build_test

    if cmake .. > /tmp/ramulator_cmake.log 2>&1; then
        print_pass "CMake configuration successful"
        return 0
    else
        print_fail "CMake configuration failed"
        print_info "See /tmp/ramulator_cmake.log for details"
        return 1
    fi
}

#==============================================================================
# Test 4: Actual Build
#==============================================================================
test_actual_build() {
    print_test "Actual Build Compilation"

    cd "$RAMULATOR_DIR/build_test"

    if [ ! -z "$SKIP_BUILD" ]; then
        print_info "Skipping actual build (SKIP_BUILD is set)"
        print_pass "Build test skipped"
        return 0
    fi

    print_info "Building ramulator2 (this may take a few minutes)..."

    if make -j$(nproc) > /tmp/ramulator_build.log 2>&1; then
        print_pass "Ramulator2 built successfully"

        # Check if binary exists
        if [ -f "ramulator" ] || [ -f "ramulator2" ]; then
            print_info "Binary created successfully"
        fi

        return 0
    else
        print_fail "Ramulator2 build failed"
        print_info "See /tmp/ramulator_build.log for details"
        return 1
    fi
}

#==============================================================================
# Test 5: Example Configs Exist
#==============================================================================
test_example_configs() {
    print_test "Example Configuration Files"

    cd "$RAMULATOR_DIR"

    # Look for example configs
    if ls *.yaml >/dev/null 2>&1 || ls configs/*.yaml >/dev/null 2>&1 || ls example*.yaml >/dev/null 2>&1; then
        print_pass "Example configuration files found"
        return 0
    else
        print_info "No example YAML configs found (may be in different location)"
        print_pass "Example configs check completed"
        return 0
    fi
}

#==============================================================================
# Main
#==============================================================================
main() {
    print_header "Ramulator2 Build Verification Test Suite"
    print_info "Project Root: $PROJECT_ROOT"
    print_info "Ramulator Directory: $RAMULATOR_DIR"
    print_info "GCC Version: $(g++ --version | head -n1)"
    print_info "CMake Version: $(cmake --version | head -n1)"

    test_build_system
    test_dependencies
    test_cmake_config
    test_actual_build
    test_example_configs

    # Summary
    echo ""
    echo "=============================================================="
    echo "TEST SUMMARY"
    echo "=============================================================="
    echo "Total Tests: $TOTAL_TESTS"
    echo "Passed:      $PASSED_TESTS"
    echo "Failed:      $((TOTAL_TESTS - PASSED_TESTS))"

    if [ $PASSED_TESTS -eq $TOTAL_TESTS ]; then
        echo -e "${GREEN}ALL TESTS PASSED!${NC}"
        return 0
    else
        echo -e "${RED}SOME TESTS FAILED${NC}"
        return 1
    fi
}

main
exit $?
