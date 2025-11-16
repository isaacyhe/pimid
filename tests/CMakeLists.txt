# PIMID Comprehensive Test Suite
#
# This includes all test categories:
# - Build verification (shell scripts)
# - Compatibility tests (shell/Python)
# - Functional tests (C++)
# - Integration tests (C++)
# - Workload tests (C++)

cmake_minimum_required(VERSION 3.20)
project(PIMID_Tests CXX)

# Enable testing
include(CTest)
enable_testing()

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Memory architecture tests
add_subdirectory(memory)

# Functional tests (component-level)
add_subdirectory(functional)

# Integration tests (system-level)
add_subdirectory(integration)

# Workload tests (application-level)
add_subdirectory(workloads)

# Shell script tests (build/compatibility)
# These are run via run_all_tests.sh
message(STATUS "Build verification tests: tests/build/*.sh")
message(STATUS "Compatibility tests: tests/compatibility/*.sh")
message(STATUS "Master test runner: tests/run_all_tests.sh")
message(STATUS "")
message(STATUS "To run C++ tests: make && ctest")
message(STATUS "To run all tests: ./run_all_tests.sh --quick")
