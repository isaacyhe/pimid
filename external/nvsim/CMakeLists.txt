# NVSim CMake build configuration
# NVSim - An area, timing and power model for NVM and volatile memory

cmake_minimum_required(VERSION 3.10)
project(nvsim CXX)

# Source files for library (exclude main.cpp for library build)
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
    PredecodeBlock.cpp
    Precharger.cpp
    Result.cpp
    RowDecoder.cpp
    SenseAmp.cpp
    SubArray.cpp
    Technology.cpp
    Wire.cpp
    formula.cpp
    nvsim_globals.cpp  # Global variable definitions for library use
)

# Build as static library for integration with pimid
add_library(nvsim STATIC ${NVSIM_SOURCES})

# C++17 compatibility fix: renamed 'data' enum to 'data_array' to avoid
# conflict with std::data() when using namespace std
target_compile_features(nvsim PUBLIC cxx_std_17)

# Suppress warnings for third-party code
target_compile_options(nvsim PRIVATE
    -w  # Suppress all warnings for external code
)

# Include directory for headers
target_include_directories(nvsim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
