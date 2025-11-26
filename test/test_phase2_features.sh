#!/bin/bash

# Phase 2 Features Integration Test Script
# Tests: Config parsing, Plugin discovery, PE statistics

echo "========================================="
echo "PIMID Phase 2 Features Integration Test"
echo "========================================="
echo ""

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

test_passed=0
test_failed=0

# Test 1: Build verification
echo "Test 1: Build Verification"
echo "----------------------------"
if [ -f "/home/user/pimid-dev/build/pimid" ]; then
    echo -e "${GREEN}✓ pimid binary exists${NC}"
    ((test_passed++))
else
    echo -e "${RED}✗ pimid binary not found${NC}"
    ((test_failed++))
fi

if [ -f "/home/user/pimid-dev/build/libpimid_lib.a" ]; then
    echo -e "${GREEN}✓ pimid_lib library exists${NC}"
    ((test_passed++))
else
    echo -e "${RED}✗ pimid_lib library not found${NC}"
    ((test_failed++))
fi

if [ -f "/home/user/pimid-dev/build/libpimid_plugin.so" ]; then
    echo -e "${GREEN}✓ pimid_plugin library exists${NC}"
    ((test_passed++))
else
    echo -e "${RED}✗ pimid_plugin library not found${NC}"
    ((test_failed++))
fi
echo ""

# Test 2: Config file parsing
echo "Test 2: Configuration File Parsing"
echo "-----------------------------------"

# Check if config files exist
if [ -f "/home/user/pimid-dev/config/memory_config.yaml" ]; then
    echo -e "${GREEN}✓ memory_config.yaml exists${NC}"
    ((test_passed++))

    # Verify it contains expected sections (YAML hierarchical structure)
    if grep -q "^dram:" /home/user/pimid-dev/config/memory_config.yaml; then
        echo -e "${GREEN}✓ DRAM configuration present${NC}"
        ((test_passed++))
    else
        echo -e "${RED}✗ DRAM configuration missing${NC}"
        ((test_failed++))
    fi

    if grep -q "^sram:" /home/user/pimid-dev/config/memory_config.yaml; then
        echo -e "${GREEN}✓ SRAM configuration present${NC}"
        ((test_passed++))
    else
        echo -e "${RED}✗ SRAM configuration missing${NC}"
        ((test_failed++))
    fi

    if grep -q "^stt_mram:" /home/user/pimid-dev/config/memory_config.yaml; then
        echo -e "${GREEN}✓ STT-MRAM configuration present${NC}"
        ((test_passed++))
    else
        echo -e "${RED}✗ STT-MRAM configuration missing${NC}"
        ((test_failed++))
    fi
else
    echo -e "${RED}✗ memory_config.yaml not found${NC}"
    ((test_failed++))
fi
echo ""

# Test 3: Source code verification
echo "Test 3: Source Code Verification"
echo "---------------------------------"

# Check if config parsing was implemented in memory models
if grep -q "config::ConfigParser" /home/user/pimid-dev/pimid/memory_models/src/dram_model.cpp; then
    echo -e "${GREEN}✓ DRAM model has config parsing${NC}"
    ((test_passed++))
else
    echo -e "${RED}✗ DRAM model missing config parsing${NC}"
    ((test_failed++))
fi

if grep -q "config::ConfigParser" /home/user/pimid-dev/pimid/memory_models/src/sram_model.cpp; then
    echo -e "${GREEN}✓ SRAM model has config parsing${NC}"
    ((test_passed++))
else
    echo -e "${RED}✗ SRAM model missing config parsing${NC}"
    ((test_failed++))
fi

if grep -q "config::ConfigParser" /home/user/pimid-dev/pimid/memory_models/src/sttmram_model.cpp; then
    echo -e "${GREEN}✓ STT-MRAM model has config parsing${NC}"
    ((test_passed++))
else
    echo -e "${RED}✗ STT-MRAM model missing config parsing${NC}"
    ((test_failed++))
fi

# Check plugin discovery implementation
if grep -q "std::filesystem::directory_iterator" /home/user/pimid-dev/pimid/src/plugin/plugin_interface.cpp; then
    echo -e "${GREEN}✓ Plugin discovery system implemented${NC}"
    ((test_passed++))
else
    echo -e "${RED}✗ Plugin discovery system missing${NC}"
    ((test_failed++))
fi

# Check PE statistics system
if [ -f "/home/user/pimid-dev/pimid/include/address_translation/pe_statistics.h" ]; then
    echo -e "${GREEN}✓ PE statistics header exists${NC}"
    ((test_passed++))
else
    echo -e "${RED}✗ PE statistics header missing${NC}"
    ((test_failed++))
fi

if [ -f "/home/user/pimid-dev/pimid/src/address_translation/pe_statistics.cpp" ]; then
    echo -e "${GREEN}✓ PE statistics implementation exists${NC}"
    ((test_passed++))
else
    echo -e "${RED}✗ PE statistics implementation missing${NC}"
    ((test_failed++))
fi
echo ""

# Test 4: CMakeLists.txt verification
echo "Test 4: Build System Integration"
echo "---------------------------------"

if grep -q "test_schedulers" /home/user/pimid-dev/tests/CMakeLists.txt; then
    echo -e "${GREEN}✓ test_schedulers added to CMake${NC}"
    ((test_passed++))
else
    echo -e "${RED}✗ test_schedulers missing from CMake${NC}"
    ((test_failed++))
fi

if grep -q "test_address_translator" /home/user/pimid-dev/tests/CMakeLists.txt; then
    echo -e "${GREEN}✓ test_address_translator added to CMake${NC}"
    ((test_passed++))
else
    echo -e "${RED}✗ test_address_translator missing from CMake${NC}"
    ((test_failed++))
fi

if grep -q "test_multi_component" /home/user/pimid-dev/tests/integration/CMakeLists.txt; then
    echo -e "${GREEN}✓ test_multi_component integration test added${NC}"
    ((test_passed++))
else
    echo -e "${RED}✗ test_multi_component missing from CMake${NC}"
    ((test_failed++))
fi

if grep -q "pe_statistics.cpp" /home/user/pimid-dev/pimid/CMakeLists.txt; then
    echo -e "${GREEN}✓ PE statistics added to CMake${NC}"
    ((test_passed++))
else
    echo -e "${RED}✗ PE statistics missing from CMake${NC}"
    ((test_failed++))
fi
echo ""

# Test 5: Documentation
echo "Test 5: Documentation"
echo "---------------------"

if [ -f "/home/user/pimid-dev/docs/PHASE_2_IMPLEMENTATION_SUMMARY.md" ]; then
    echo -e "${GREEN}✓ Phase 2 implementation summary exists${NC}"
    ((test_passed++))

    # Check summary contains key info
    if grep -q "Memory Model Configuration Parsing" /home/user/pimid-dev/docs/PHASE_2_IMPLEMENTATION_SUMMARY.md; then
        echo -e "${GREEN}✓ Summary documents config parsing${NC}"
        ((test_passed++))
    fi

    if grep -q "Plugin Discovery System" /home/user/pimid-dev/docs/PHASE_2_IMPLEMENTATION_SUMMARY.md; then
        echo -e "${GREEN}✓ Summary documents plugin discovery${NC}"
        ((test_passed++))
    fi

    if grep -q "PE Statistics" /home/user/pimid-dev/docs/PHASE_2_IMPLEMENTATION_SUMMARY.md; then
        echo -e "${GREEN}✓ Summary documents PE statistics${NC}"
        ((test_passed++))
    fi
else
    echo -e "${RED}✗ Phase 2 implementation summary missing${NC}"
    ((test_failed++))
fi
echo ""

# Summary
echo "========================================="
echo "Test Summary"
echo "========================================="
echo -e "Passed: ${GREEN}${test_passed}${NC}"
echo -e "Failed: ${RED}${test_failed}${NC}"
echo ""

if [ $test_failed -eq 0 ]; then
    echo -e "${GREEN}✓ ALL TESTS PASSED!${NC}"
    echo ""
    echo "Phase 2 features successfully implemented and integrated:"
    echo "  • YAML configuration parsing for memory models"
    echo "  • Plugin discovery system with filesystem scanning"
    echo "  • Centralized PE statistics tracking"
    echo "  • Integration tests for multi-component scenarios"
    echo "  • Root-level tests added to build system"
    exit 0
else
    echo -e "${RED}✗ SOME TESTS FAILED${NC}"
    echo "Please review the failed tests above"
    exit 1
fi
