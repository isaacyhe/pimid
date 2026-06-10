# CACTI 7.0 - SRAM/Cache Modeling Tool
# Standalone CACTI for PIMID's subarray characterization

cmake_minimum_required(VERSION 3.15)
project(CACTI VERSION 7.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "-Wall -Wno-unknown-pragmas -Wno-unused-variable")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -msse2 -mfpmath=sse")

# CACTI 7.0 source files
set(CACTI_SOURCES
    area.cc
    bank.cc
    mat.cc
    Ucache.cc
    io.cc
    technology.cc
    basic_circuit.cc
    parameter.cc
    decoder.cc
    component.cc
    uca.cc
    subarray.cc
    wire.cc
    htree2.cc
    extio.cc
    extio_technology.cc
    cacti_interface.cc
    router.cc
    nuca.cc
    crossbar.cc
    arbiter.cc
    powergating.cc
    TSV.cc
    memorybus.cc
    memcad.cc
    memcad_parameters.cc
)

# Create CACTI library
add_library(cacti7 STATIC ${CACTI_SOURCES})

target_include_directories(cacti7 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Required definitions
target_compile_definitions(cacti7 PRIVATE
    NTHREADS=8
)

# Suppress warnings for third-party code
target_compile_options(cacti7 PRIVATE
    -w
)

# Thread support
find_package(Threads REQUIRED)
target_link_libraries(cacti7 PUBLIC Threads::Threads m)

# Optional: Build standalone executable
option(BUILD_CACTI_EXECUTABLE "Build standalone CACTI executable" OFF)
if(BUILD_CACTI_EXECUTABLE)
    add_executable(cacti_exe main.cc)
    target_link_libraries(cacti_exe PRIVATE cacti7)
    set_target_properties(cacti_exe PROPERTIES OUTPUT_NAME cacti)
endif()

message(STATUS "CACTI 7.0 configured for subarray characterization")
