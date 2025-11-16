# PIMID Test Suite

# Enable testing
include(CTest)
enable_testing()

# Test utilities
set(TEST_UTILS
    test_utils.cpp
)

add_library(test_utils STATIC ${TEST_UTILS})
target_link_libraries(test_utils pimid_lib)

# Unit tests
add_subdirectory(unit)

# Integration tests
add_subdirectory(integration)

# Benchmark tests
add_subdirectory(benchmarks)

# Example unit test (if not in subdirectory)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_memory_model.cpp)
    add_executable(test_memory_model unit/test_memory_model.cpp)
    target_link_libraries(test_memory_model pimid_lib test_utils)
    add_test(NAME MemoryModelTest COMMAND test_memory_model)
endif()

# Example integration test (if not in subdirectory)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integration/test_host_device_comm.cpp)
    add_executable(test_host_device_comm integration/test_host_device_comm.cpp)
    target_link_libraries(test_host_device_comm pimid_lib test_utils)
    add_test(NAME HostDeviceCommTest COMMAND test_host_device_comm)
endif()
