cmake_minimum_required(VERSION 3.15)
project(PIMID_External_Model_Examples)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include PIMID headers
include_directories(${CMAKE_SOURCE_DIR})

# Simple network integration example
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/simple_network/CMakeLists.txt")
    message(STATUS "  - Simple network integration example")
    add_subdirectory(simple_network)
endif()

message(STATUS "External model integration examples configured")
