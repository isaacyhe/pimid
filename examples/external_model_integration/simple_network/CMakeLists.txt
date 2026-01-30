cmake_minimum_required(VERSION 3.10)
project(SimpleNetworkIntegration)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include PIMID headers
include_directories(${CMAKE_SOURCE_DIR})

# Build the SimpleNetwork library (user's existing code)
add_library(simple_network STATIC
    SimpleNetwork.cpp
    SimpleNetwork.h
)

# Build the adapter as a shared library (can be dynamically loaded)
add_library(simple_network_adapter SHARED
    simple_network_adapter.cpp
)

target_link_libraries(simple_network_adapter
    simple_network
    pimid_external_models  # Links to the PIMID registry
)

# Build the test executable
add_executable(test_simple_network_integration
    test_integration.cpp
)

target_link_libraries(test_simple_network_integration
    simple_network_adapter
    simple_network
    pimid_external_models
)

# Installation
install(TARGETS simple_network simple_network_adapter
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(TARGETS test_simple_network_integration
    RUNTIME DESTINATION bin
)
