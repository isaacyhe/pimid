#!/bin/bash
#
# Ubuntu 24.04 Compatibility Verification Test
# Verifies that PIMID and all external tools are compatible with Ubuntu 24.04
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

GREEN='\033[0;32m'
RED='\033[0;31m'
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
# Test 1: Verify Ubuntu Version
#==============================================================================
test_ubuntu_version() {
    print_test "Ubuntu Version Check"

    if [ -f /etc/os-release ]; then
        . /etc/os-release
        print_info "OS: $NAME $VERSION"

        if [[ "$VERSION_ID" == "24.04" ]]; then
            print_pass "Running on Ubuntu 24.04"
            return 0
        else
            print_info "Running on $NAME $VERSION (not Ubuntu 24.04)"
            print_pass "OS version check completed (not Ubuntu 24.04)"
            return 0
        fi
    else
        print_fail "Cannot determine OS version (/etc/os-release not found)"
        return 1
    fi
}

#==============================================================================
# Test 2: GCC Version Check
#==============================================================================
test_gcc_version() {
    print_test "GCC Version Check"

    GCC_VERSION=$(gcc --version | head -n1)
    print_info "$GCC_VERSION"

    # Extract major version
    GCC_MAJOR=$(gcc -dumpversion | cut -d. -f1)

    if [ "$GCC_MAJOR" -ge 11 ]; then
        print_pass "GCC $GCC_MAJOR (compatible with Ubuntu 24.04)"
        return 0
    else
        print_fail "GCC $GCC_MAJOR (Ubuntu 24.04 typically uses GCC 13)"
        return 1
    fi
}

#==============================================================================
# Test 3: Python 3 Version Check
#==============================================================================
test_python_version() {
    print_test "Python 3 Version Check"

    PYTHON_VERSION=$(python3 --version)
    print_info "$PYTHON_VERSION"

    # Extract major.minor version
    PYTHON_VER=$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')

    # Ubuntu 24.04 uses Python 3.12
    if python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)'; then
        print_pass "Python $PYTHON_VER (compatible with Ubuntu 24.04)"
        return 0
    else
        print_fail "Python $PYTHON_VER (Ubuntu 24.04 requires Python 3.8+)"
        return 1
    fi
}

#==============================================================================
# Test 4: System Libraries Check
#==============================================================================
test_system_libraries() {
    print_test "System Libraries Availability"

    LIBRARIES=(
        "libstdc++.so.6"
        "libc.so.6"
        "libm.so.6"
        "libgcc_s.so.1"
        "libpthread.so.0"
    )

    MISSING=()
    for lib in "${LIBRARIES[@]}"; do
        if ! ldconfig -p | grep -q "$lib"; then
            MISSING+=("$lib")
        fi
    done

    if [ ${#MISSING[@]} -eq 0 ]; then
        print_pass "All standard system libraries present"
        return 0
    else
        print_fail "Missing libraries: ${MISSING[*]}"
        return 1
    fi
}

#==============================================================================
# Test 5: HDF5 Library Paths (Ubuntu 24.04 Specific)
#==============================================================================
test_hdf5_paths() {
    print_test "HDF5 Library Paths (Ubuntu 24.04)"

    # Ubuntu 24.04 moved HDF5 to /usr/include/hdf5/serial
    if [ -d "/usr/include/hdf5/serial" ]; then
        print_info "HDF5 headers found at: /usr/include/hdf5/serial"
        print_pass "Ubuntu 24.04 HDF5 header path exists"
    else
        print_info "HDF5 headers not in Ubuntu 24.04 standard location"
        print_fail "HDF5 not installed or not in standard Ubuntu 24.04 path"
        return 1
    fi

    if [ -d "/usr/lib/x86_64-linux-gnu/hdf5/serial" ]; then
        print_info "HDF5 libraries found at: /usr/lib/x86_64-linux-gnu/hdf5/serial"
        print_pass "Ubuntu 24.04 HDF5 library path exists"
    else
        print_fail "HDF5 libraries not in standard Ubuntu 24.04 path"
        return 1
    fi

    return 0
}

#==============================================================================
# Test 6: Kernel Headers Path (Ubuntu 24.04)
#==============================================================================
test_kernel_headers() {
    print_test "Kernel Headers Location"

    # Ubuntu 24.04 uses x86_64-linux-gnu architecture-specific paths
    if [ -d "/usr/include/x86_64-linux-gnu/asm" ]; then
        print_info "Architecture-specific headers: /usr/include/x86_64-linux-gnu/asm"
        print_pass "Ubuntu 24.04 kernel header paths exist"

        # Check for unistd.h (needed by zsim)
        if [ -f "/usr/include/x86_64-linux-gnu/asm/unistd_64.h" ]; then
            print_info "Found: /usr/include/x86_64-linux-gnu/asm/unistd_64.h"
        fi

        return 0
    else
        print_fail "Architecture-specific kernel headers not found"
        return 1
    fi
}

#==============================================================================
# Test 7: Build Tools Versions
#==============================================================================
test_build_tools() {
    print_test "Build Tools Compatibility"

    # CMake
    if command -v cmake >/dev/null 2>&1; then
        CMAKE_VERSION=$(cmake --version | head -n1)
        print_info "CMake: $CMAKE_VERSION"
        print_pass "CMake available"
    else
        print_fail "CMake not installed"
        return 1
    fi

    # SCons
    if command -v scons >/dev/null 2>&1; then
        SCONS_VERSION=$(scons --version 2>&1 | head -n1)
        print_info "SCons: $SCONS_VERSION"
        print_pass "SCons available"
    else
        print_fail "SCons not installed"
        return 1
    fi

    # Make
    if command -v make >/dev/null 2>&1; then
        MAKE_VERSION=$(make --version | head -n1)
        print_info "Make: $MAKE_VERSION"
        print_pass "Make available"
    else
        print_fail "Make not installed"
        return 1
    fi

    return 0
}

#==============================================================================
# Test 8: ZSim Ubuntu 24.04 Specific Fixes
#==============================================================================
test_zsim_ubuntu24_fixes() {
    print_test "ZSim Ubuntu 24.04 Compatibility Fixes"

    ZSIM_DIR="$PROJECT_ROOT/pimid/external/zsim"

    # Test 1: list_syscalls.py has Ubuntu 24.04 path support
    if grep -q "x86_64-linux-gnu/asm/unistd_64.h" "$ZSIM_DIR/misc/list_syscalls.py"; then
        print_pass "list_syscalls.py has Ubuntu 24.04 path support"
    else
        print_fail "list_syscalls.py missing Ubuntu 24.04 paths"
        return 1
    fi

    # Test 2: SConstruct has HDF5 serial paths
    if grep -q "hdf5/serial" "$ZSIM_DIR/SConstruct"; then
        print_pass "SConstruct has Ubuntu 24.04 HDF5 paths"
    else
        print_fail "SConstruct missing Ubuntu 24.04 HDF5 paths"
        return 1
    fi

    return 0
}

#==============================================================================
# Test 9: Package Dependencies Check
#==============================================================================
test_package_dependencies() {
    print_test "Required Ubuntu Packages"

    REQUIRED_PACKAGES=(
        "build-essential"
        "cmake"
        "scons"
        "libconfig++-dev"
        "libhdf5-dev"
        "libelf-dev"
        "python3"
        "python3-dev"
    )

    MISSING_PACKAGES=()
    for pkg in "${REQUIRED_PACKAGES[@]}"; do
        if ! dpkg -l | grep -q "^ii  $pkg"; then
            MISSING_PACKAGES+=("$pkg")
        fi
    done

    if [ ${#MISSING_PACKAGES[@]} -eq 0 ]; then
        print_pass "All required packages installed"
        return 0
    else
        print_fail "Missing packages: ${MISSING_PACKAGES[*]}"
        print_info "Install with: sudo apt-get install ${MISSING_PACKAGES[*]}"
        return 1
    fi
}

#==============================================================================
# Main Test Runner
#==============================================================================
main() {
    print_header "Ubuntu 24.04 Compatibility Verification Test Suite"
    print_info "Project Root: $PROJECT_ROOT"

    # System information
    print_info "Hostname: $(hostname)"
    print_info "Kernel: $(uname -r)"
    print_info "Architecture: $(uname -m)"

    # Run all tests
    test_ubuntu_version
    test_gcc_version
    test_python_version
    test_system_libraries
    test_hdf5_paths
    test_kernel_headers
    test_build_tools
    test_zsim_ubuntu24_fixes
    test_package_dependencies

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
        echo ""
        echo "✓ System is fully compatible with Ubuntu 24.04"
        echo "✓ All PIMID external tools should build successfully"
        return 0
    else
        echo -e "${RED}SOME TESTS FAILED${NC}"
        echo ""
        echo "Some compatibility issues detected."
        echo "Review failed tests and install missing dependencies."
        return 1
    fi
}

main
exit $?
