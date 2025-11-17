cmake_minimum_required(VERSION 3.15)
project(PIMID VERSION 1.0.0 LANGUAGES CXX)

# C++17 standard required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/memory_models/include
    ${PROJECT_SOURCE_DIR}/network_models/include
    ${PROJECT_SOURCE_DIR}/power_models/include
    ${PROJECT_SOURCE_DIR}/external
    ${PROJECT_SOURCE_DIR}/external/ramulator/src
    ${PROJECT_SOURCE_DIR}/external/cacti
    ${PROJECT_SOURCE_DIR}/external/nvsim
    ${PROJECT_SOURCE_DIR}/external/mcpat
)

# Find required packages
find_package(Threads REQUIRED)

# Optional: Boost for enhanced functionality
find_package(Boost COMPONENTS system filesystem)
if(Boost_FOUND)
    add_definitions(-DHAVE_BOOST)
endif()

# Optional: yaml-cpp for configuration parsing
find_package(yaml-cpp)
if(yaml-cpp_FOUND)
    add_definitions(-DHAVE_YAML_CPP)
endif()

# Build external models
message(STATUS "Building external models...")

# Ramulator2 for DRAM modeling
if(EXISTS "${PROJECT_SOURCE_DIR}/external/ramulator/CMakeLists.txt")
    message(STATUS "  - Ramulator2 DRAM simulator")
    # Ramulator requires C++20
    set(OLD_CXX_STANDARD ${CMAKE_CXX_STANDARD})
    set(CMAKE_CXX_STANDARD 20)
    add_subdirectory(external/ramulator)
    set(CMAKE_CXX_STANDARD ${OLD_CXX_STANDARD})
    add_definitions(-DHAVE_RAMULATOR)
    set(HAVE_RAMULATOR ON)
endif()

# CACTI for SRAM/cache modeling
if(EXISTS "${PROJECT_SOURCE_DIR}/external/cacti/CMakeLists.txt")
    message(STATUS "  - CACTI SRAM/cache analyzer")
    add_subdirectory(external/cacti)
    add_definitions(-DHAVE_CACTI)
    set(HAVE_CACTI ON)
endif()

# NVSim for NVM modeling
if(EXISTS "${PROJECT_SOURCE_DIR}/external/nvsim/CMakeLists.txt")
    message(STATUS "  - NVSim NVM analyzer")
    add_subdirectory(external/nvsim)
    add_definitions(-DHAVE_NVSIM)
    set(HAVE_NVSIM ON)
endif()

# McPAT for power modeling
if(EXISTS "${PROJECT_SOURCE_DIR}/external/mcpat/CMakeLists.txt")
    message(STATUS "  - McPAT power analyzer")
    add_subdirectory(external/mcpat)
    add_definitions(-DHAVE_MCPAT)
    set(HAVE_MCPAT ON)
endif()

# Options
option(BUILD_PLUGINS "Build plugin system" ON)
option(BUILD_TOOLS "Build configuration tools" ON)
option(BUILD_EXAMPLES "Build example plugins" ON)

# Source files
set(PIMID_SOURCES
    # Common
    src/common/simulation_engine.cpp
    src/common/event_queue.cpp
    src/common/config_parser.cpp

    # Host engine
    src/host_engine/host_engine.cpp
    src/host_engine/host_interconnect.cpp

    # Device engine
    src/device_engine/device_engine.cpp

    # Communication
    src/communication/socket_comm.cpp

    # Memory models
    memory_models/src/memory_model.cpp
    memory_models/src/dram_model.cpp
    memory_models/src/sram_model.cpp
    memory_models/src/nvm_model.cpp
    memory_models/src/sttmram_model.cpp
    memory_models/src/pcm_model.cpp
    memory_models/src/reram_model.cpp
    memory_models/src/ramulator_wrapper.cpp
    memory_models/src/cacti_wrapper.cpp
    memory_models/src/nvsim_wrapper.cpp

    # PIM extensions for Ramulator
    memory_models/src/pim_bandwidth_tracker.cpp
    memory_models/src/internal_dram_network.cpp
    memory_models/src/pim_controller_plugin.cpp

    # DRAM architecture specifications
    memory/dram_architecture.cpp

    # Network (includes GARNET integration)
    network_models/src/network_model.cpp

    # Power
    power_models/src/power_model.cpp
    power_models/src/mcpat_model.cpp
    power_models/src/mcpat_wrapper.cpp

    # Address translation
    src/address_translation/pe_placement.cpp
    src/address_translation/address_translator.cpp

    # Scheduler
    src/scheduler/scheduler.cpp
    src/scheduler/nearest_scheduler.cpp
    src/scheduler/roundrobin_scheduler.cpp
    src/scheduler/loadbalanced_scheduler.cpp

    # Instrumentation (PIMID API for workloads)
    src/instrumentation/pimid_api.cpp

    # Main
    src/pimid.cpp
)

# Plugin system sources
set(PIMID_PLUGIN_SOURCES
    src/plugin/plugin_interface.cpp
    src/plugin/memory_model_plugin.cpp
    src/plugin/scheduler_plugin.cpp
)

# Configuration system sources
set(PIMID_CONFIG_SOURCES
    src/config/config_schema.cpp
    src/config/config_validator.cpp
    src/config/config_manager.cpp
)

# PIMID core library
add_library(pimid_lib STATIC ${PIMID_SOURCES})
target_link_libraries(pimid_lib
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

if(Boost_FOUND)
    target_link_libraries(pimid_lib Boost::system Boost::filesystem)
endif()

if(yaml-cpp_FOUND)
    target_link_libraries(pimid_lib yaml-cpp)
endif()

# Link external models
if(HAVE_RAMULATOR)
    target_link_libraries(pimid_lib ramulator)
endif()

if(HAVE_CACTI)
    target_link_libraries(pimid_lib cacti)
endif()

if(HAVE_NVSIM)
    target_link_libraries(pimid_lib nvsim)
endif()

if(HAVE_MCPAT)
    target_link_libraries(pimid_lib mcpat)
endif()

# Plugin system library
if(BUILD_PLUGINS)
    add_library(pimid_plugin SHARED ${PIMID_PLUGIN_SOURCES})
    target_link_libraries(pimid_plugin
        pimid_lib
        ${CMAKE_DL_LIBS}
    )
endif()

# Configuration system library
if(BUILD_PLUGINS AND yaml-cpp_FOUND)
    add_library(pimid_config SHARED ${PIMID_CONFIG_SOURCES})
    target_link_libraries(pimid_config
        pimid_lib
        pimid_plugin
        yaml-cpp
    )
endif()

# Host simulator executable
add_executable(pimid_host src/host_main.cpp)
target_link_libraries(pimid_host pimid_lib)
if(BUILD_PLUGINS)
    target_link_libraries(pimid_host pimid_plugin)
    if(yaml-cpp_FOUND)
        target_link_libraries(pimid_host pimid_config)
    endif()
endif()

# Device simulator executable
add_executable(pimid_device src/device_main.cpp)
target_link_libraries(pimid_device pimid_lib)
if(BUILD_PLUGINS)
    target_link_libraries(pimid_device pimid_plugin)
    if(yaml-cpp_FOUND)
        target_link_libraries(pimid_device pimid_config)
    endif()
endif()

# Standalone simulator (single-process mode for debugging)
add_executable(pimid_standalone src/standalone_main.cpp)
target_link_libraries(pimid_standalone pimid_lib)
if(BUILD_PLUGINS)
    target_link_libraries(pimid_standalone pimid_plugin)
    if(yaml-cpp_FOUND)
        target_link_libraries(pimid_standalone pimid_config)
    endif()
endif()

# Main PIMID binary - primary entry point for running simulations
# This is the main simulator that executes external workload binaries
add_executable(pimid src/standalone_main_new.cpp)
target_link_libraries(pimid pimid_lib)
if(BUILD_PLUGINS)
    target_link_libraries(pimid pimid_plugin)
    if(yaml-cpp_FOUND)
        target_link_libraries(pimid pimid_config)
    endif()
endif()

# PIM Simulator command-line tool (config-driven simulation)
# NOTE: Standalone tool, doesn't link against pimid_lib to avoid external dependencies
add_executable(pimid_sim
    tools/pimid_sim.cpp
    memory/dram_architecture.cpp
)
target_compile_options(pimid_sim PRIVATE -std=c++17)
target_link_libraries(pimid_sim Threads::Threads)

# Install targets
install(TARGETS pimid_lib pimid_host pimid_device pimid_standalone pimid pimid_sim
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include/pimid)
install(DIRECTORY configs/ DESTINATION share/pimid/configs)

# Testing
enable_testing()
add_subdirectory(tests)

# Benchmarks
add_subdirectory(benchmarks)

# Documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()

# Summary
message(STATUS "")
message(STATUS "PIMID Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  YAML-CPP found: ${yaml-cpp_FOUND}")
message(STATUS "")
