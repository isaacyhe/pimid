# NVSim - Local namespaced copy for PIMID
# Original: https://github.com/SEAL-UCSB/NVSim
#
# This is a modified copy with all classes placed in the 'nvsim::' namespace
# to avoid symbol collisions with CACTI (which has similar class names like
# InputParameter, Wire, Mat, Bank, etc.)

cmake_minimum_required(VERSION 3.10)

# Collect all source files (excluding main.cpp which is for standalone)
set(NVSIM_SOURCES
    Bank.cpp
    BankWithHtree.cpp
    BankWithoutHtree.cpp
    BasicDecoder.cpp
    Comparator.cpp
    FunctionUnit.cpp
    InputParameter.cpp
    Mat.cpp
    MemCell.cpp
    Mux.cpp
    OutputDriver.cpp
    Precharger.cpp
    PredecodeBlock.cpp
    Result.cpp
    RowDecoder.cpp
    SenseAmp.cpp
    SubArray.cpp
    Technology.cpp
    Wire.cpp
    formula.cpp
    nvsim_globals.cpp
)

# Create NVSim library (namespaced version)
add_library(nvsim_ns STATIC ${NVSIM_SOURCES})

# Include directory for headers
target_include_directories(nvsim_ns PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Suppress warnings in external code
target_compile_options(nvsim_ns PRIVATE -w)

message(STATUS "  - NVSim (namespaced) for PIMID integration")
