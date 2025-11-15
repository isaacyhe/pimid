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
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external
)

# Find required packages
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)

# Optional: yaml-cpp for configuration parsing
find_package(yaml-cpp)
if(yaml-cpp_FOUND)
    add_definitions(-DHAVE_YAML_CPP)
endif()

# Source files
set(PIMID_SOURCES
    # Common
    src/common/simulation_engine.cpp
    src/common/event_queue.cpp
    src/common/config_parser.cpp

    # Host engine
    src/host_engine/host_engine.cpp

    # Device engine
    src/device_engine/device_engine.cpp

    # Communication
    src/communication/socket_comm.cpp

    # Memory models
    src/memory_models/memory_model.cpp
    src/memory_models/dram_model.cpp
    src/memory_models/sram_model.cpp
    src/memory_models/nvm_model.cpp

    # Network
    src/network/network_model.cpp
    src/network/garnet_model.cpp

    # Power
    src/power/power_model.cpp
    src/power/mcpat_model.cpp

    # Address translation
    src/address_translation/pe_placement.cpp
    src/address_translation/address_translator.cpp

    # Scheduler
    src/scheduler/scheduler.cpp
    src/scheduler/nearest_scheduler.cpp
    src/scheduler/roundrobin_scheduler.cpp
    src/scheduler/loadbalanced_scheduler.cpp

    # Main
    src/pimid.cpp
)

# PIMID library
add_library(pimid_lib STATIC ${PIMID_SOURCES})
target_link_libraries(pimid_lib
    Threads::Threads
    Boost::system
    Boost::filesystem
)

if(yaml-cpp_FOUND)
    target_link_libraries(pimid_lib yaml-cpp)
endif()

# Host simulator executable
add_executable(pimid_host src/host_main.cpp)
target_link_libraries(pimid_host pimid_lib)

# Device simulator executable
add_executable(pimid_device src/device_main.cpp)
target_link_libraries(pimid_device pimid_lib)

# Standalone simulator (single-process mode for debugging)
add_executable(pimid_standalone src/standalone_main.cpp)
target_link_libraries(pimid_standalone pimid_lib)

# Install targets
install(TARGETS pimid_lib pimid_host pimid_device pimid_standalone
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include/pimid)
install(DIRECTORY configs/ DESTINATION share/pimid/configs)

# Testing
enable_testing()
add_subdirectory(tests)

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
