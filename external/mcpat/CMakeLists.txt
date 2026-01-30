# McPAT CMake build configuration
# McPAT - An Integrated Power, Area, and Timing Modeling Framework for Multicore Architectures

cmake_minimum_required(VERSION 3.10)
project(mcpat CXX)

# CACTI sources (McPAT depends on CACTI)
set(CACTI_SOURCES
    cacti/arbiter.cc
    cacti/area.cc
    cacti/bank.cc
    cacti/basic_circuit.cc
    cacti/cacti_interface.cc
    cacti/component.cc
    cacti/crossbar.cc
    cacti/decoder.cc
    cacti/htree2.cc
    cacti/io.cc
    cacti/mat.cc
    cacti/nuca.cc
    cacti/parameter.cc
    cacti/powergating.cc
    cacti/router.cc
    cacti/subarray.cc
    cacti/technology.cc
    cacti/uca.cc
    cacti/Ucache.cc
    cacti/wire.cc
)

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
add_library(mcpat STATIC ${CACTI_SOURCES} ${MCPAT_SOURCES})

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
target_include_directories(mcpat PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cacti
)
