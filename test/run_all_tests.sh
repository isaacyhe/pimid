#!/bin/bash
#
# PIMID Master Test Runner
# Runs all test suites for the PIMID project
#
# Usage:
#   ./run_all_tests.sh [OPTIONS]
#
# Options:
#   --quick       Run quick tests only (no actual builds)
#   --build       Run build tests (may take a long time)
#   --compat      Run compatibility tests only
#   --integration Run integration tests only
#   --all         Run all tests (default)
#   --help        Show this help message
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test suite tracking
TOTAL_SUITES=0
PASSED_SUITES=0
FAILED_SUITES=()

# Configuration
RUN_BUILD_TESTS=false
RUN_COMPAT_TESTS=false
RUN_INTEGRATION_TESTS=false
SKIP_BUILD=1  # By default, skip actual building (use 0 to enable)

#==============================================================================
# Helper Functions
#==============================================================================

print_banner() {
    echo ""
    echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║              PIMID COMPREHENSIVE TEST SUITE                ║${NC}"
    echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

print_section() {
    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
}

print_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

run_test_suite() {
    local name="$1"
    local script="$2"
    local description="$3"

    echo ""
    echo -e "${MAGENTA}▶ Running:${NC} $name"
    echo -e "${CYAN}  Description:${NC} $description"
    echo -e "${CYAN}  Script:${NC} $script"
    echo ""

    ((TOTAL_SUITES++))

    if bash "$script"; then
        ((PASSED_SUITES++))
        print_success "$name completed successfully"
        return 0
    else
        FAILED_SUITES+=("$name")
        print_error "$name failed"
        return 1
    fi
}

run_python_test() {
    local name="$1"
    local script="$2"
    local description="$3"

    echo ""
    echo -e "${MAGENTA}▶ Running:${NC} $name"
    echo -e "${CYAN}  Description:${NC} $description"
    echo -e "${CYAN}  Script:${NC} $script"
    echo ""

    ((TOTAL_SUITES++))

    if python3 "$script"; then
        ((PASSED_SUITES++))
        print_success "$name completed successfully"
        return 0
    else
        FAILED_SUITES+=("$name")
        print_error "$name failed"
        return 1
    fi
}

#==============================================================================
# Test Suites
#==============================================================================

run_compatibility_tests() {
    print_section "COMPATIBILITY TESTS"

    # Ubuntu 24.04 compatibility
    run_test_suite \
        "Ubuntu 24.04 Compatibility" \
        "$SCRIPT_DIR/compatibility/test_ubuntu24_compatibility.sh" \
        "Verify Ubuntu 24.04 system compatibility"

    # Python 3 compatibility
    run_python_test \
        "Python 3 Compatibility" \
        "$SCRIPT_DIR/compatibility/test_python3_compatibility.py" \
        "Verify all Python scripts use Python 3"

    # Pin 3.x compatibility
    run_test_suite \
        "Pin 3.x Compatibility" \
        "$SCRIPT_DIR/compatibility/test_pin3_compatibility.sh" \
        "Verify zsim Pin 3.x upgrade and compatibility"
}

run_build_tests() {
    print_section "BUILD VERIFICATION TESTS"

    print_info "Build tests verify that external tools can be compiled"
    print_info "SKIP_BUILD=${SKIP_BUILD} (set to 0 to enable actual compilation)"
    echo ""

    export SKIP_BUILD

    # ZSim
    run_test_suite \
        "ZSim Build" \
        "$SCRIPT_DIR/build/test_zsim_build.sh" \
        "Build zsim with Pin 3.x support"

    # Ramulator2
    run_test_suite \
        "Ramulator2 Build" \
        "$SCRIPT_DIR/build/test_ramulator2_build.sh" \
        "Build ramulator2 DRAM simulator"

    # CACTI
    run_test_suite \
        "CACTI Build" \
        "$SCRIPT_DIR/build/test_cacti_build.sh" \
        "Build CACTI SRAM modeling tool"

    # NVSim
    run_test_suite \
        "NVSim Build" \
        "$SCRIPT_DIR/build/test_nvsim_build.sh" \
        "Build NVSim NVM simulator"

    # McPAT
    run_test_suite \
        "McPAT Build" \
        "$SCRIPT_DIR/build/test_mcpat_build.sh" \
        "Build McPAT power modeling tool"

    # gem5 (configuration only, actual build skipped by default)
    run_test_suite \
        "gem5 Build Configuration" \
        "$SCRIPT_DIR/build/test_gem5_build.sh" \
        "Verify gem5 build configuration"
}

run_integration_tests() {
    print_section "INTEGRATION TESTS"

    # Check if CMake build exists
    if [ -d "$PROJECT_ROOT/build" ]; then
        print_info "CMake build directory found"

        # Run CTest if available
        if command -v ctest >/dev/null 2>&1; then
            echo ""
            echo -e "${MAGENTA}▶ Running:${NC} PIMID Integration Tests (CTest)"
            echo ""
            cd "$PROJECT_ROOT/build"
            ((TOTAL_SUITES++))
            if ctest --output-on-failure; then
                ((PASSED_SUITES++))
                print_success "CTest integration tests passed"
            else
                FAILED_SUITES+=("CTest Integration Tests")
                print_error "CTest integration tests failed"
            fi
            cd "$SCRIPT_DIR"
        else
            print_info "CTest not available, skipping CMake integration tests"
        fi
    else
        print_info "No CMake build directory found"
        print_info "Run 'mkdir build && cd build && cmake ..' to enable integration tests"
    fi

    # Run Python topology tests
    if [ -f "$PROJECT_ROOT/test_topologies.py" ]; then
        run_python_test \
            "Topology Verification" \
            "$PROJECT_ROOT/test_topologies.py" \
            "Verify gem5 network topologies"
    fi
}

#==============================================================================
# Summary
#==============================================================================

print_summary() {
    echo ""
    echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║                     TEST SUMMARY                           ║${NC}"
    echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo "Total Test Suites: $TOTAL_SUITES"
    echo "Passed:            $PASSED_SUITES"
    echo "Failed:            $((TOTAL_SUITES - PASSED_SUITES))"
    echo ""

    if [ ${#FAILED_SUITES[@]} -gt 0 ]; then
        echo -e "${RED}Failed Test Suites:${NC}"
        for suite in "${FAILED_SUITES[@]}"; do
            echo -e "${RED}  ✗ $suite${NC}"
        done
        echo ""
    fi

    if [ $PASSED_SUITES -eq $TOTAL_SUITES ]; then
        echo -e "${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${GREEN}║            ✓ ALL TESTS PASSED! ✓                           ║${NC}"
        echo -e "${GREEN}╚════════════════════════════════════════════════════════════╝${NC}"
        echo ""
        echo -e "${GREEN}Your PIMID installation is fully verified!${NC}"
        echo ""
        return 0
    else
        echo -e "${RED}╔════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${RED}║            ✗ SOME TESTS FAILED ✗                           ║${NC}"
        echo -e "${RED}╚════════════════════════════════════════════════════════════╝${NC}"
        echo ""
        echo -e "${YELLOW}Review the failed tests above and check:${NC}"
        echo "  - Are all dependencies installed?"
        echo "  - Is PINPATH set correctly?"
        echo "  - Are you running on Ubuntu 24.04?"
        echo ""
        return 1
    fi
}

#==============================================================================
# Argument Parsing
#==============================================================================

show_help() {
    echo "PIMID Test Suite Runner"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --quick         Run quick tests only (no actual builds)"
    echo "  --build         Run build tests with actual compilation"
    echo "  --compat        Run compatibility tests only"
    echo "  --integration   Run integration tests only"
    echo "  --all           Run all tests (default)"
    echo "  --help          Show this help message"
    echo ""
    echo "Environment Variables:"
    echo "  PINPATH         Path to Intel Pin installation (required for zsim)"
    echo "  SKIP_BUILD      Set to 0 to enable actual builds (default: 1)"
    echo ""
    echo "Examples:"
    echo "  # Run all quick tests (no building)"
    echo "  $0 --quick"
    echo ""
    echo "  # Run compatibility tests only"
    echo "  $0 --compat"
    echo ""
    echo "  # Run all tests with actual builds"
    echo "  SKIP_BUILD=0 $0 --all"
    echo ""
    exit 0
}

#==============================================================================
# Main
#==============================================================================

main() {
    # Parse arguments
    if [ $# -eq 0 ]; then
        # Default: run all tests
        RUN_BUILD_TESTS=true
        RUN_COMPAT_TESTS=true
        RUN_INTEGRATION_TESTS=true
    fi

    while [ $# -gt 0 ]; do
        case "$1" in
            --quick)
                RUN_COMPAT_TESTS=true
                RUN_BUILD_TESTS=true
                SKIP_BUILD=1
                shift
                ;;
            --build)
                RUN_BUILD_TESTS=true
                SKIP_BUILD=0
                shift
                ;;
            --compat)
                RUN_COMPAT_TESTS=true
                shift
                ;;
            --integration)
                RUN_INTEGRATION_TESTS=true
                shift
                ;;
            --all)
                RUN_BUILD_TESTS=true
                RUN_COMPAT_TESTS=true
                RUN_INTEGRATION_TESTS=true
                shift
                ;;
            --help)
                show_help
                ;;
            *)
                echo "Unknown option: $1"
                echo "Use --help for usage information"
                exit 1
                ;;
        esac
    done

    # Print banner
    print_banner

    # System information
    print_info "Project Root: $PROJECT_ROOT"
    print_info "Test Directory: $SCRIPT_DIR"
    print_info "OS: $(cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d= -f2 | tr -d '\"' || echo 'Unknown')"
    print_info "GCC: $(gcc --version 2>/dev/null | head -n1 || echo 'Not installed')"
    print_info "Python: $(python3 --version 2>/dev/null || echo 'Not installed')"
    print_info "PINPATH: ${PINPATH:-Not set}"
    echo ""

    # Run test suites
    if [ "$RUN_COMPAT_TESTS" = true ]; then
        run_compatibility_tests
    fi

    if [ "$RUN_BUILD_TESTS" = true ]; then
        run_build_tests
    fi

    if [ "$RUN_INTEGRATION_TESTS" = true ]; then
        run_integration_tests
    fi

    # Print summary
    print_summary
    return $?
}

# Make all test scripts executable
chmod +x "$SCRIPT_DIR"/build/*.sh 2>/dev/null || true
chmod +x "$SCRIPT_DIR"/compatibility/*.sh 2>/dev/null || true
chmod +x "$SCRIPT_DIR"/compatibility/*.py 2>/dev/null || true

# Run main
main "$@"
exit $?
