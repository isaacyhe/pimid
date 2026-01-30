cmake_minimum_required(VERSION 3.15)
project(PIMID_External_Models)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/pimid
    ${CMAKE_SOURCE_DIR}/pimid/external_models/include
)

# External model registry library
add_library(pimid_external_models SHARED
    src/model_registry.cpp
)

target_link_libraries(pimid_external_models
    ${CMAKE_DL_LIBS}  # For dynamic loading (dlopen/dlsym)
)

# Install headers
install(DIRECTORY include/
    DESTINATION include/pimid/external_models
    FILES_MATCHING PATTERN "*.h"
)

# Install library
install(TARGETS pimid_external_models
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

message(STATUS "External model integration system enabled")
