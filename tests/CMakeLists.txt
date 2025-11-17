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

# Core component tests (root level)
# These tests verify the core PIMID components implemented in Phase 1

# Test schedulers (nearest, round-robin, load-balanced)
add_executable(test_schedulers test_schedulers.cpp)
target_compile_options(test_schedulers PRIVATE -std=c++17)
target_include_directories(test_schedulers PRIVATE
    ${CMAKE_SOURCE_DIR}/../pimid/include
    ${CMAKE_SOURCE_DIR}/../pimid/memory/include
)
add_test(NAME SchedulersComponentTest COMMAND test_schedulers)

# Test address translator and PE placement
add_executable(test_address_translator test_address_translator.cpp)
target_compile_options(test_address_translator PRIVATE -std=c++17)
target_include_directories(test_address_translator PRIVATE
    ${CMAKE_SOURCE_DIR}/../pimid/include
    ${CMAKE_SOURCE_DIR}/../pimid/memory/include
)
add_test(NAME AddressTranslatorComponentTest COMMAND test_address_translator)

# Test event queue
add_executable(test_event_queue test_event_queue.cpp)
target_compile_options(test_event_queue PRIVATE -std=c++17)
target_include_directories(test_event_queue PRIVATE
    ${CMAKE_SOURCE_DIR}/../pimid/include
)
add_test(NAME EventQueueComponentTest COMMAND test_event_queue)

# Test configuration validator
add_executable(test_config_validator test_config_validator.cpp)
target_compile_options(test_config_validator PRIVATE -std=c++17)
target_include_directories(test_config_validator PRIVATE
    ${CMAKE_SOURCE_DIR}/../pimid/include
)
add_test(NAME ConfigValidatorComponentTest COMMAND test_config_validator)

# Shell script tests (build/compatibility)
# These are run via run_all_tests.sh
message(STATUS "Build verification tests: tests/build/*.sh")
message(STATUS "Compatibility tests: tests/compatibility/*.sh")
message(STATUS "Master test runner: tests/run_all_tests.sh")
message(STATUS "")
message(STATUS "To run C++ tests: make && ctest")
message(STATUS "To run all tests: ./run_all_tests.sh --quick")
