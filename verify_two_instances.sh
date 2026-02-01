#!/bin/bash

# Verification script for testing socket-based host-device communication
# with two separate zsim instances

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PORT=${PORT:-9999}
CYCLES=${CYCLES:-10000}
BUILD_DIR="./build"
HOST_BIN="${BUILD_DIR}/pimid_host"
DEVICE_BIN="${BUILD_DIR}/pimid_device"
LOG_DIR="./logs"
HOST_LOG="${LOG_DIR}/host.log"
DEVICE_LOG="${LOG_DIR}/device.log"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}PIMID Two-Instance Verification${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Create logs directory
mkdir -p "${LOG_DIR}"

# Check if binaries exist
if [ ! -f "${HOST_BIN}" ]; then
    echo -e "${RED}Error: Host binary not found at ${HOST_BIN}${NC}"
    echo "Please build the project first with: cd build && cmake .. && make"
    exit 1
fi

if [ ! -f "${DEVICE_BIN}" ]; then
    echo -e "${RED}Error: Device binary not found at ${DEVICE_BIN}${NC}"
    echo "Please build the project first with: cd build && cmake .. && make"
    exit 1
fi

echo -e "${GREEN}Found binaries:${NC}"
echo "  Host:   ${HOST_BIN}"
echo "  Device: ${DEVICE_BIN}"
echo ""

# Clean up previous logs
rm -f "${HOST_LOG}" "${DEVICE_LOG}"

# Clean up any processes using the port
echo -e "${YELLOW}Checking for processes on port ${PORT}...${NC}"
if lsof -i :${PORT} >/dev/null 2>&1; then
    echo -e "${YELLOW}Killing existing processes on port ${PORT}${NC}"
    lsof -ti :${PORT} | xargs kill -9 2>/dev/null || true
    sleep 1
fi

# Function to cleanup background processes
cleanup() {
    echo ""
    echo -e "${YELLOW}Cleaning up processes...${NC}"
    if [ ! -z "$HOST_PID" ]; then
        kill $HOST_PID 2>/dev/null || true
    fi
    if [ ! -z "$DEVICE_PID" ]; then
        kill $DEVICE_PID 2>/dev/null || true
    fi
    # Kill any remaining processes
    pkill -f pimid_host 2>/dev/null || true
    pkill -f pimid_device 2>/dev/null || true
}

trap cleanup EXIT INT TERM

echo -e "${BLUE}Starting verification test...${NC}"
echo "  Port: ${PORT}"
echo "  Cycles: ${CYCLES}"
echo ""

# Start host instance in background
echo -e "${GREEN}[1/3] Starting host instance...${NC}"
"${HOST_BIN}" --port ${PORT} --cycles ${CYCLES} > "${HOST_LOG}" 2>&1 &
HOST_PID=$!
echo "  Host PID: ${HOST_PID}"
echo "  Log: ${HOST_LOG}"

# Wait a bit for host to start listening
sleep 2

# Check if host is still running
if ! ps -p ${HOST_PID} > /dev/null; then
    echo -e "${RED}Error: Host instance failed to start${NC}"
    echo -e "${RED}Host log:${NC}"
    cat "${HOST_LOG}"
    exit 1
fi

# Start device instance in background
echo -e "${GREEN}[2/3] Starting device instance...${NC}"
"${DEVICE_BIN}" --host 127.0.0.1 --port ${PORT} --cycles ${CYCLES} --delay 1 > "${DEVICE_LOG}" 2>&1 &
DEVICE_PID=$!
echo "  Device PID: ${DEVICE_PID}"
echo "  Log: ${DEVICE_LOG}"
echo ""

# Monitor both processes
echo -e "${BLUE}[3/3] Monitoring communication...${NC}"
echo ""

# Function to show real-time logs
tail -f "${HOST_LOG}" &
TAIL_HOST_PID=$!
tail -f "${DEVICE_LOG}" &
TAIL_DEVICE_PID=$!

# Wait for both processes to complete
wait ${HOST_PID} 2>/dev/null
HOST_EXIT=$?

wait ${DEVICE_PID} 2>/dev/null
DEVICE_EXIT=$?

# Stop tail processes
kill ${TAIL_HOST_PID} 2>/dev/null || true
kill ${TAIL_DEVICE_PID} 2>/dev/null || true

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Verification Results${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check results
if [ ${HOST_EXIT} -eq 0 ] && [ ${DEVICE_EXIT} -eq 0 ]; then
    echo -e "${GREEN}SUCCESS!${NC}"
    echo ""
    echo -e "${GREEN}Both instances completed successfully:${NC}"
    echo "  - Host exited with code: ${HOST_EXIT}"
    echo "  - Device exited with code: ${DEVICE_EXIT}"
    echo ""
    echo -e "${GREEN}Socket communication verified!${NC}"
    exit 0
else
    echo -e "${RED}FAILURE!${NC}"
    echo ""
    echo "Exit codes:"
    echo "  - Host: ${HOST_EXIT}"
    echo "  - Device: ${DEVICE_EXIT}"
    echo ""

    if [ ${HOST_EXIT} -ne 0 ]; then
        echo -e "${RED}Host log:${NC}"
        cat "${HOST_LOG}"
        echo ""
    fi

    if [ ${DEVICE_EXIT} -ne 0 ]; then
        echo -e "${RED}Device log:${NC}"
        cat "${DEVICE_LOG}"
        echo ""
    fi

    exit 1
fi
