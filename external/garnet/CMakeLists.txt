# CMakeLists.txt for standalone Garnet NoC simulator
# Extracted from gem5 with compatibility layer

cmake_minimum_required(VERSION 3.15)

# Garnet source files
set(GARNET_SOURCES
    src/BusArbiter.cc
    src/Credit.cc
    src/CrossbarSwitch.cc
    src/GarnetLink.cc
    src/GarnetNetwork.cc
    src/InputUnit.cc
    src/NetworkBridge.cc
    src/NetworkInterface.cc
    src/NetworkLink.cc
    src/OutVcState.cc
    src/OutputUnit.cc
    src/RingRouter.cc
    src/Router.cc
    src/RoutingUnit.cc
    src/SwitchAllocator.cc
    src/Topology.cc
    src/TopologyBuilders.cc
    src/TreeRouter.cc
    src/VirtualChannel.cc
    src/flit.cc
    src/flitBuffer.cc
)

# Create Garnet library
add_library(garnet STATIC ${GARNET_SOURCES})
set_target_properties(garnet PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Include directories
target_include_directories(garnet PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gem5_compat
)

# Compiler options
target_compile_features(garnet PUBLIC cxx_std_17)
target_compile_options(garnet PRIVATE
    -Wall
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-sign-compare
)

# Optional debug build
option(GARNET_DEBUG "Enable Garnet debug output" OFF)
if(GARNET_DEBUG)
    target_compile_definitions(garnet PUBLIC GARNET_DEBUG)
endif()

# Export for parent project
set(HAVE_GARNET ON PARENT_SCOPE)

message(STATUS "  - Garnet NoC simulator (extracted from gem5)")

# Build test executable
option(BUILD_GARNET_TESTS "Build Garnet test programs" ON)
if(BUILD_GARNET_TESTS)
    add_executable(garnet_test tests/simple_test.cc)
    target_link_libraries(garnet_test garnet)
endif()
