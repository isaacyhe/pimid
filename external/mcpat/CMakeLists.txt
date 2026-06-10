# McPAT CMake build configuration
# McPAT - An Integrated Power, Area, and Timing Modeling Framework for Multicore Architectures
#
# McPAT uses standalone CACTI 7.0 for cache/memory modeling (replacing bundled CACTI 6.5-P).

cmake_minimum_required(VERSION 3.10)
project(mcpat CXX)

# McPAT core sources (exclude main.cc for library build)
set(MCPAT_SOURCES
    array.cc
    basic_components.cc
    core.cc
    interconnect.cc
    iocontrollers.cc
    logic.cc
    memoryctrl.cc
    noc.cc
    processor.cc
    sharedcache.cc
    XML_Parse.cc
    xmlParser.cc
)

# Build as static library for integration with pimid
add_library(mcpat STATIC ${MCPAT_SOURCES})

# C++17 standard
target_compile_features(mcpat PUBLIC cxx_std_17)

# Required definitions for McPAT/CACTI
target_compile_definitions(mcpat PRIVATE
    NTHREADS=4  # Number of threads for parallel exploration
)

# Suppress warnings for third-party code
target_compile_options(mcpat PRIVATE
    -w  # Suppress all warnings for external code
)

# Include directories
target_include_directories(mcpat
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../cacti  # CACTI 7.0 headers
)

# Link CACTI 7.0 library
target_link_libraries(mcpat PUBLIC cacti7)

message(STATUS "McPAT configured with CACTI 7.0")
