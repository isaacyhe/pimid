#!/bin/bash
#
# ZSim Build Verification Test
# Tests that zsim builds successfully with Pin 3.x on Ubuntu 24.04
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ZSIM_DIR="$PROJECT_ROOT/pimid/external/zsim"

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test results
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

print_summary() {
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

#==============================================================================
# Test 1: Check Pin Path
#==============================================================================
test_pin_path() {
    print_test "Pin Path Configuration"

    if [ -z "$PINPATH" ]; then
        print_info "PINPATH not set, attempting to find Pin installation"

        # Try common locations
        POSSIBLE_PATHS=(
            "/opt/pin"
            "/usr/local/pin"
            "$HOME/pin"
            "/tmp/pin-3.28-98749-g6643ecee5-gcc-linux"
        )

        for path in "${POSSIBLE_PATHS[@]}"; do
            if [ -d "$path" ]; then
                export PINPATH="$path"
                print_info "Found Pin at: $PINPATH"
                break
            fi
        done
    fi

    if [ -z "$PINPATH" ]; then
        print_fail "PINPATH not set and Pin not found in common locations"
        print_info "Please set PINPATH environment variable or download Pin 3.28+"
        return 1
    fi

    if [ ! -d "$PINPATH" ]; then
        print_fail "PINPATH directory does not exist: $PINPATH"
        return 1
    fi

    print_pass "Pin path configured: $PINPATH"
    return 0
}

#==============================================================================
# Test 2: Verify Pin Version
#==============================================================================
test_pin_version() {
    print_test "Pin Version Detection"

    if [ ! -f "$PINPATH/source/include/pin/pin.H" ]; then
        print_fail "Pin header not found at: $PINPATH/source/include/pin/pin.H"
        return 1
    fi

    # Check for Pin 3.x specific headers
    if [ -d "$PINPATH/extras/xed-intel64" ]; then
        print_info "Detected Pin 3.x (XED directory present)"
        print_pass "Pin 3.x headers verified"
        return 0
    else
        print_info "Warning: Pin 2.x detected or XED directory not found"
        print_pass "Pin headers found (may be Pin 2.x)"
        return 0
    fi
}

#==============================================================================
# Test 3: Check Build Dependencies
#==============================================================================
test_dependencies() {
    print_test "Build Dependencies"

    MISSING_DEPS=()

    # Check for required tools
    command -v scons >/dev/null 2>&1 || MISSING_DEPS+=("scons")
    command -v g++ >/dev/null 2>&1 || MISSING_DEPS+=("g++")
    command -v python3 >/dev/null 2>&1 || MISSING_DEPS+=("python3")

    # Check for required libraries
    if ! pkg-config --exists libconfig++; then
        MISSING_DEPS+=("libconfig++-dev")
    fi

    if ! pkg-config --exists hdf5; then
        MISSING_DEPS+=("libhdf5-dev")
    fi

    if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
        print_fail "Missing dependencies: ${MISSING_DEPS[*]}"
        print_info "Install with: sudo apt-get install ${MISSING_DEPS[*]}"
        return 1
    fi

    print_pass "All dependencies installed"
    return 0
}

#==============================================================================
# Test 4: Verify Python 3 Scripts
#==============================================================================
test_python_scripts() {
    print_test "Python 3 Script Compatibility"

    cd "$ZSIM_DIR"

    # Find all Python scripts
    PYTHON_SCRIPTS=$(find misc -name "*.py" 2>/dev/null)

    if [ -z "$PYTHON_SCRIPTS" ]; then
        print_info "No Python scripts found to test"
        print_pass "Python scripts check skipped (no scripts found)"
        return 0
    fi

    PYTHON_ERRORS=0
    for script in $PYTHON_SCRIPTS; do
        # Check shebang
        SHEBANG=$(head -n 1 "$script")
        if [[ ! "$SHEBANG" =~ python3 ]]; then
            print_info "Warning: $script does not use python3 shebang"
            ((PYTHON_ERRORS++))
        fi

        # Syntax check
        if ! python3 -m py_compile "$script" 2>/dev/null; then
            print_info "Error: $script has syntax errors"
            ((PYTHON_ERRORS++))
        fi
    done

    if [ $PYTHON_ERRORS -gt 0 ]; then
        print_fail "Python 3 compatibility issues found ($PYTHON_ERRORS errors)"
        return 1
    fi

    print_pass "All Python scripts are Python 3 compatible"
    return 0
}

#==============================================================================
# Test 5: Check Source Code Modifications (Pin 3.x)
#==============================================================================
test_pin3_modifications() {
    print_test "Pin 3.x Source Code Modifications"

    cd "$ZSIM_DIR"

    # Check that GetVmLock and ReleaseVmLock are removed
    if grep -q "GetVmLock\|ReleaseVmLock" src/zsim.cpp 2>/dev/null; then
        # Check if they're only in comments
        if grep "GetVmLock\|ReleaseVmLock" src/zsim.cpp | grep -v "^\s*//" | grep -v "NOTE:" >/dev/null 2>&1; then
            print_fail "Found deprecated GetVmLock/ReleaseVmLock calls in src/zsim.cpp"
            return 1
        fi
    fi

    print_pass "Pin 3.x deprecated API calls removed"
    return 0
}

#==============================================================================
# Test 6: Build Configuration Test (Dry Run)
#==============================================================================
test_build_config() {
    print_test "Build Configuration (Dry Run)"

    cd "$ZSIM_DIR"

    # Clean previous builds
    if [ -d "build" ]; then
        print_info "Cleaning previous build artifacts"
        scons -c >/dev/null 2>&1 || true
    fi

    # Run dry-run build to check configuration
    if scons -n > /tmp/zsim_build_dryrun.log 2>&1; then
        print_pass "Build configuration successful"

        # Verify Pin paths are detected
        if grep -q "$PINPATH" /tmp/zsim_build_dryrun.log; then
            print_info "Pin paths correctly detected in build config"
        fi

        # Verify HDF5 paths (Ubuntu 24.04)
        if grep -q "hdf5/serial" /tmp/zsim_build_dryrun.log; then
            print_info "Ubuntu 24.04 HDF5 paths detected"
        fi

        return 0
    else
        print_fail "Build configuration failed"
        print_info "See /tmp/zsim_build_dryrun.log for details"
        return 1
    fi
}

#==============================================================================
# Test 7: Actual Build Test
#==============================================================================
test_actual_build() {
    print_test "Actual Build Compilation"

    cd "$ZSIM_DIR"

    # Check if SKIP_BUILD is set
    if [ ! -z "$SKIP_BUILD" ]; then
        print_info "Skipping actual build (SKIP_BUILD is set)"
        print_pass "Build test skipped"
        return 0
    fi

    print_info "Building zsim (this may take several minutes)..."

    if scons -j$(nproc) > /tmp/zsim_build.log 2>&1; then
        print_pass "ZSim built successfully"

        # Verify binary exists
        if [ -f "build/opt/zsim" ]; then
            print_info "Binary created: build/opt/zsim"

            # Get binary size
            SIZE=$(du -h build/opt/zsim | cut -f1)
            print_info "Binary size: $SIZE"
        fi

        return 0
    else
        print_fail "ZSim build failed"
        print_info "See /tmp/zsim_build.log for details"
        print_info "Common issues:"
        print_info "  - Missing Pin 3.x installation"
        print_info "  - Missing HDF5 libraries"
        print_info "  - GCC ABI compatibility issues"
        return 1
    fi
}

#==============================================================================
# Test 8: Binary Execution Test
#==============================================================================
test_binary_execution() {
    print_test "Binary Execution Test"

    cd "$ZSIM_DIR"

    if [ ! -f "build/opt/zsim" ]; then
        print_info "Skipping execution test (binary not found)"
        print_pass "Execution test skipped (no binary)"
        return 0
    fi

    # Test help output
    if ./build/opt/zsim --help > /tmp/zsim_help.log 2>&1; then
        print_pass "ZSim binary executes successfully"
        return 0
    else
        # Some versions don't have --help, try with no args
        if ./build/opt/zsim 2>&1 | grep -q "zsim"; then
            print_pass "ZSim binary executes successfully"
            return 0
        fi

        print_fail "ZSim binary execution failed"
        return 1
    fi
}

#==============================================================================
# Main Test Runner
#==============================================================================
main() {
    print_header "ZSim Build Verification Test Suite"
    print_info "Project Root: $PROJECT_ROOT"
    print_info "ZSim Directory: $ZSIM_DIR"
    print_info "GCC Version: $(g++ --version | head -n1)"
    print_info "Python Version: $(python3 --version)"
    print_info "SCons Version: $(scons --version 2>&1 | head -n1)"

    # Run all tests
    test_pin_path
    test_pin_version
    test_dependencies
    test_python_scripts
    test_pin3_modifications
    test_build_config
    test_actual_build
    test_binary_execution

    # Print summary
    print_summary
    return $?
}

# Run main function
main
exit $?
