#!/bin/bash
#
# PIMID Build Script
# Automates the build process with various options
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
BUILD_DIR="build"
CLEAN_BUILD=false
RUN_TESTS=false
INSTALL=false
JOBS=$(nproc)

# Print usage
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Build PIMID simulator

OPTIONS:
    -h, --help          Show this help message
    -d, --debug         Build in debug mode
    -r, --release       Build in release mode (default)
    -c, --clean         Clean build (remove build directory first)
    -t, --test          Run tests after building
    -i, --install       Install after building
    -j, --jobs N        Number of parallel build jobs (default: $(nproc))
    --build-dir DIR     Specify build directory (default: build)

EXAMPLES:
    $0                  # Release build
    $0 --debug          # Debug build
    $0 --clean --test   # Clean build and run tests
    $0 -d -c -t         # Debug, clean, and test

EOF
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -i|--install)
            INSTALL=true
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            usage
            ;;
    esac
done

# Print configuration
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}     PIMID Build Configuration${NC}"
echo -e "${GREEN}========================================${NC}"
echo "Build Type:      $BUILD_TYPE"
echo "Build Directory: $BUILD_DIR"
echo "Clean Build:     $CLEAN_BUILD"
echo "Run Tests:       $RUN_TESTS"
echo "Install:         $INSTALL"
echo "Parallel Jobs:   $JOBS"
echo -e "${GREEN}========================================${NC}\n"

# Clean build if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo -e "${GREEN}Configuring with CMake...${NC}"
cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" ..

# Build
echo -e "${GREEN}Building PIMID...${NC}"
make -j"$JOBS"

# Run tests if requested
if [ "$RUN_TESTS" = true ]; then
    echo -e "${GREEN}Running tests...${NC}"
    make test || {
        echo -e "${RED}Tests failed!${NC}"
        exit 1
    }
    echo -e "${GREEN}All tests passed!${NC}"
fi

# Install if requested
if [ "$INSTALL" = true ]; then
    echo -e "${GREEN}Installing PIMID...${NC}"
    sudo make install
fi

# Print success message
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}     Build Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo "Executables:"
echo "  - ${BUILD_DIR}/pimid_host"
echo "  - ${BUILD_DIR}/pimid_device"
echo "  - ${BUILD_DIR}/pimid_standalone"
echo ""
echo "To run a simulation:"
echo "  cd ${BUILD_DIR}"
echo "  ./pimid_standalone ../configs/examples/pimid_config.yaml <workload>"
echo -e "${GREEN}========================================${NC}"
