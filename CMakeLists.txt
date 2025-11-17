cmake_minimum_required(VERSION 3.15)
project(PIMID_ROOT VERSION 1.0.0 LANGUAGES CXX)

# C++17 standard required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Main PIMID library and executables
add_subdirectory(pimid)

# Testing suite
enable_testing()
add_subdirectory(test)

# Summary
message(STATUS "")
message(STATUS "PIMID Root Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Source directory: ${CMAKE_SOURCE_DIR}")
message(STATUS "  Binary directory: ${CMAKE_BINARY_DIR}")
message(STATUS "")
message(STATUS "Subdirectories:")
message(STATUS "  - pimid: Main library and executables")
message(STATUS "  - test: Test suite, benchmarks, and workloads")
message(STATUS "  - config: Configuration files")
message(STATUS "  - docs: Documentation")
message(STATUS "")
