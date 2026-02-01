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

# Include directories - NEW STRUCTURE
# Note: CACTI 7.0 headers are added per-target to avoid naming conflicts
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/memory              # DRAM architecture specs
    ${PROJECT_SOURCE_DIR}/external
    ${PROJECT_SOURCE_DIR}/external/ramulator/src
    ${PROJECT_SOURCE_DIR}/external/nvsim      # Namespaced NVSim (avoids CACTI collision)
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

# CACTI 7.0 for SRAM/subarray characterization AND McPAT power modeling
# McPAT now uses CACTI 7.0 instead of bundled CACTI 6.5-P
if(EXISTS "${PROJECT_SOURCE_DIR}/external/cacti/CMakeLists.txt")
    message(STATUS "  - CACTI 7.0 SRAM/subarray analyzer")
    add_subdirectory(external/cacti)
    add_definitions(-DHAVE_CACTI)
    add_definitions(-DHAVE_CACTI7)
    set(HAVE_CACTI ON)
    set(HAVE_CACTI7 ON)
endif()

# NVSim for NVM modeling (namespaced to avoid CACTI symbol collision)
if(EXISTS "${PROJECT_SOURCE_DIR}/external/nvsim/CMakeLists.txt")
    message(STATUS "  - NVSim NVM analyzer (namespaced)")
    add_subdirectory(external/nvsim)
    add_definitions(-DHAVE_NVSIM)
    set(HAVE_NVSIM ON)
endif()

# McPAT for power modeling (uses bundled CACTI 6.5-P)
# Note: McPAT has API dependencies on CACTI 6.5-P internal structures
# (TechnologyParameter::DeviceType, etc.) that differ from CACTI 7.0.
# PIMID uses CACTI 7.0 separately for subarray characterization.
if(EXISTS "${PROJECT_SOURCE_DIR}/external/mcpat/CMakeLists.txt")
    message(STATUS "  - McPAT power analyzer (with bundled CACTI 6.5-P)")
    add_subdirectory(external/mcpat)
    add_definitions(-DHAVE_MCPAT)
    set(HAVE_MCPAT ON)
endif()

# Garnet NoC simulator (extracted from gem5)
if(EXISTS "${PROJECT_SOURCE_DIR}/external/garnet/CMakeLists.txt")
    message(STATUS "  - Garnet NoC simulator (extracted from gem5)")
    add_subdirectory(external/garnet)
    set(HAVE_GARNET ON)
endif()

# Options
option(BUILD_PLUGINS "Build plugin system" ON)
option(BUILD_TOOLS "Build configuration tools" ON)
option(BUILD_EXAMPLES "Build example plugins" ON)

# Source files - NEW PATHS
set(PIMID_SOURCES
    # Common
    src/common/simulation_engine.cpp
    src/common/event_queue.cpp
    src/common/config_parser.cpp
    src/config/config_parser.cpp
    src/config/config_manager.cpp
    src/config/config_schema.cpp
    src/config/config_validator.cpp
    src/config/network_config_parser.cpp

    # Host engine
    src/host_engine/host_engine.cpp
    src/host_engine/host_interconnect.cpp

    # Device engine
    src/device_engine/device_engine.cpp

    # Communication
    src/communication/socket_comm.cpp

    # Memory models - NEW PATHS
    src/memory/memory_model.cpp
    src/memory/dram_model.cpp
    src/memory/sram_model.cpp
    src/memory/nvm_model.cpp
    src/memory/sttmram_model.cpp
    src/memory/pcm_model.cpp
    src/memory/reram_model.cpp
    src/memory/ramulator_wrapper.cpp
    src/memory/cacti_wrapper.cpp
    src/memory/nvsim_wrapper.cpp

    # PIM extensions
    src/memory/pim_bandwidth_tracker.cpp
    src/memory/internal_dram_network.cpp
    src/memory/internal_memory_network.cpp
    src/memory/multi_instance_garnet_network.cpp
    src/memory/pim_controller_plugin.cpp

    # DRAM architecture specifications
    memory/dram_architecture.cpp

    # Network - NEW PATHS
    src/network/network_model.cpp
    src/network/garnet_detailed.cpp
    src/network/garnet_wrapper.cpp

    # Power - NEW PATHS
    src/power/power_model.cpp
    src/power/mcpat_model.cpp
    src/power/mcpat_wrapper.cpp
    src/power/power_model_manager.cpp

    # Model registry - NEW PATH
    src/model_registry.cpp

    # Address translation
    src/address_translation/pe_placement.cpp
    src/address_translation/address_translator.cpp
    src/address_translation/pe_statistics.cpp

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

# PIMID core library
add_library(pimid_lib STATIC ${PIMID_SOURCES})
target_link_libraries(pimid_lib
    Threads::Threads
    ${CMAKE_DL_LIBS}
    yaml-cpp
)

if(Boost_FOUND)
    target_link_libraries(pimid_lib Boost::system Boost::filesystem)
endif()

# Link external models
if(HAVE_RAMULATOR)
    target_link_libraries(pimid_lib ramulator)
endif()

if(HAVE_CACTI7)
    # Link CACTI 7.0 for subarray characterization
    target_link_libraries(pimid_lib cacti7)
    # Add CACTI 7.0 include directories specifically for pimid_lib
    target_include_directories(pimid_lib PRIVATE ${PROJECT_SOURCE_DIR}/external/cacti)
endif()

if(HAVE_NVSIM)
    target_link_libraries(pimid_lib nvsim_ns)  # Namespaced NVSim library
endif()

if(HAVE_MCPAT)
    target_link_libraries(pimid_lib mcpat)
endif()

# Garnet NoC simulator (extracted from gem5)
if(HAVE_GARNET)
    target_link_libraries(pimid_lib garnet)
    target_include_directories(pimid_lib PRIVATE
        ${PROJECT_SOURCE_DIR}/external/garnet/src
        ${PROJECT_SOURCE_DIR}/external/garnet/include
        ${PROJECT_SOURCE_DIR}/external/garnet/include/gem5_compat
    )
    add_definitions(-DHAVE_GARNET)
endif()

# Plugin system library
if(BUILD_PLUGINS)
    add_library(pimid_plugin SHARED ${PIMID_PLUGIN_SOURCES})
    target_link_libraries(pimid_plugin
        pimid_lib
        ${CMAKE_DL_LIBS}
    )
endif()

# Main PIMID binary - unified entry point for all simulation modes
# Supports: standalone, host, device, and host/device co-simulation
add_executable(pimid src/standalone_main_unified.cpp)
target_link_libraries(pimid pimid_lib)
if(BUILD_PLUGINS)
    target_link_libraries(pimid pimid_plugin)
endif()

# Install targets
install(TARGETS pimid_lib pimid
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include/pimid)
install(DIRECTORY configs/ DESTINATION share/pimid/configs)

# Test executables - now at project root level in tests/pimid/
# Tests are built via the root-level CMakeLists.txt
# Summary
message(STATUS "")
message(STATUS "PIMID Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  YAML-CPP found: ${yaml-cpp_FOUND}")
message(STATUS "  External models:")
if(HAVE_RAMULATOR)
    message(STATUS "    - Ramulator2: YES")
endif()
if(HAVE_CACTI7)
    message(STATUS "    - CACTI 7.0: YES")
endif()
if(HAVE_NVSIM)
    message(STATUS "    - NVSim: YES (namespaced)")
endif()
if(HAVE_MCPAT)
    message(STATUS "    - McPAT: YES (with bundled CACTI 6.5-P)")
endif()
if(HAVE_GARNET)
    message(STATUS "    - Garnet: YES")
endif()
message(STATUS "")
