#!/bin/bash
# Master Test Runner for Execution Model Testing
# Orchestrates all execution model test suites
# Usage: ./run_execution_model_tests.sh [quick|scale|comprehensive|all]

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print banner
print_banner() {
    echo ""
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║                                                                ║"
    echo "║          PIMID EXECUTION MODEL TEST SUITE                     ║"
    echo "║          Testing ZSim vs Event-Driven Models                  ║"
    echo "║                                                                ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""
}

# Print usage
print_usage() {
    echo "Usage: $0 [TEST_TYPE]"
    echo ""
    echo "TEST_TYPE:"
    echo "  quick         - Quick verification tests (~5 minutes)"
    echo "  scale         - Scale comparison tests (~30-60 minutes)"
    echo "  comprehensive - Comprehensive test suite (~2-4 hours)"
    echo "  all           - Run all test suites (full validation)"
    echo ""
    echo "Examples:"
    echo "  $0 quick              # Fast smoke test"
    echo "  $0 scale              # Compare execution models at different scales"
    echo "  $0 comprehensive 100  # Run 100 comprehensive tests"
    echo ""
}

# Check if PIMID is built
check_build() {
    echo -e "${BLUE}[1/4] Checking PIMID build...${NC}"
    if [ ! -f "build/pimid/pimid" ]; then
        echo -e "${RED}ERROR: PIMID binary not found at build/pimid/pimid${NC}"
        echo "Please build PIMID first:"
        echo "  mkdir -p build && cd build"
        echo "  cmake .."
        echo "  make -j\$(nproc)"
        exit 1
    fi
    echo -e "${GREEN}✓ PIMID binary found${NC}"
}

# Make scripts executable
setup_scripts() {
    echo -e "${BLUE}[2/4] Setting up test scripts...${NC}"
    chmod +x test_quick_execution_models.py
    chmod +x test_scale_comparison.py
    chmod +x test_execution_models_comprehensive.py
    echo -e "${GREEN}✓ Scripts ready${NC}"
}

# Check Python dependencies
check_dependencies() {
    echo -e "${BLUE}[3/4] Checking Python dependencies...${NC}"
    python3 -c "import yaml" 2>/dev/null || {
        echo -e "${YELLOW}Installing pyyaml...${NC}"
        pip3 install pyyaml
    }
    echo -e "${GREEN}✓ Dependencies satisfied${NC}"
}

# Create output directories
setup_directories() {
    echo -e "${BLUE}[4/4] Creating output directories...${NC}"
    mkdir -p test_results_execution_models
    mkdir -p test_configs
    mkdir -p test_configs_scale
    echo -e "${GREEN}✓ Directories created${NC}"
    echo ""
}

# Run quick tests
run_quick_tests() {
    echo ""
    echo "════════════════════════════════════════════════════════════════"
    echo "  RUNNING QUICK VERIFICATION TESTS"
    echo "════════════════════════════════════════════════════════════════"
    echo ""

    python3 test_quick_execution_models.py
    local exit_code=$?

    echo ""
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ Quick tests PASSED${NC}"
    else
        echo -e "${RED}✗ Quick tests FAILED${NC}"
    fi

    return $exit_code
}

# Run scale comparison tests
run_scale_tests() {
    echo ""
    echo "════════════════════════════════════════════════════════════════"
    echo "  RUNNING SCALE COMPARISON TESTS"
    echo "════════════════════════════════════════════════════════════════"
    echo ""

    python3 test_scale_comparison.py
    local exit_code=$?

    echo ""
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ Scale comparison tests PASSED${NC}"
    else
        echo -e "${RED}✗ Scale comparison tests FAILED (or incomplete)${NC}"
    fi

    return $exit_code
}

# Run comprehensive tests
run_comprehensive_tests() {
    local max_tests=${1:-100}

    echo ""
    echo "════════════════════════════════════════════════════════════════"
    echo "  RUNNING COMPREHENSIVE TESTS (max: $max_tests)"
    echo "════════════════════════════════════════════════════════════════"
    echo ""

    python3 test_execution_models_comprehensive.py --max-tests $max_tests
    local exit_code=$?

    echo ""
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ Comprehensive tests PASSED${NC}"
    else
        echo -e "${RED}✗ Comprehensive tests FAILED (or incomplete)${NC}"
    fi

    return $exit_code
}

# Run all tests
run_all_tests() {
    echo -e "${YELLOW}Running ALL test suites...${NC}"
    echo ""

    local all_passed=0

    # Quick tests
    run_quick_tests
    if [ $? -ne 0 ]; then
        all_passed=1
        echo -e "${YELLOW}Warning: Quick tests had failures${NC}"
    fi

    # Scale tests
    run_scale_tests
    if [ $? -ne 0 ]; then
        all_passed=1
        echo -e "${YELLOW}Warning: Scale tests had failures${NC}"
    fi

    # Comprehensive (limited)
    run_comprehensive_tests 50
    if [ $? -ne 0 ]; then
        all_passed=1
        echo -e "${YELLOW}Warning: Comprehensive tests had failures${NC}"
    fi

    echo ""
    echo "════════════════════════════════════════════════════════════════"
    if [ $all_passed -eq 0 ]; then
        echo -e "${GREEN}  ALL TEST SUITES COMPLETED SUCCESSFULLY${NC}"
    else
        echo -e "${YELLOW}  SOME TESTS HAD FAILURES - CHECK LOGS${NC}"
    fi
    echo "════════════════════════════════════════════════════════════════"
    echo ""

    return $all_passed
}

# Print summary
print_summary() {
    echo ""
    echo "════════════════════════════════════════════════════════════════"
    echo "  TEST OUTPUT LOCATIONS"
    echo "════════════════════════════════════════════════════════════════"
    echo ""
    echo "Quick Tests:"
    echo "  - Console output only"
    echo ""
    echo "Scale Comparison:"
    echo "  - scale_comparison_results.json (detailed results)"
    echo "  - scale_comparison.csv (CSV format)"
    echo ""
    echo "Comprehensive:"
    echo "  - test_results_execution_models/test_results.json"
    echo "  - test_results_execution_models/execution_model_comparison.csv"
    echo ""
    echo "Documentation:"
    echo "  - EXECUTION_MODEL_TESTING.md"
    echo ""
}

# Main execution
main() {
    print_banner

    # Parse arguments
    TEST_TYPE=${1:-"quick"}

    # Setup
    check_build
    setup_scripts
    check_dependencies
    setup_directories

    # Run requested tests
    case $TEST_TYPE in
        quick)
            run_quick_tests
            ;;
        scale)
            run_scale_tests
            ;;
        comprehensive)
            MAX_TESTS=${2:-100}
            run_comprehensive_tests $MAX_TESTS
            ;;
        all)
            run_all_tests
            ;;
        help|--help|-h)
            print_usage
            exit 0
            ;;
        *)
            echo -e "${RED}ERROR: Unknown test type '$TEST_TYPE'${NC}"
            echo ""
            print_usage
            exit 1
            ;;
    esac

    local exit_code=$?

    # Print summary
    print_summary

    echo ""
    echo "════════════════════════════════════════════════════════════════"
    echo "  Testing complete!"
    echo "  See EXECUTION_MODEL_TESTING.md for detailed documentation"
    echo "════════════════════════════════════════════════════════════════"
    echo ""

    exit $exit_code
}

# Run main
main "$@"
