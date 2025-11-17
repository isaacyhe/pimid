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

# Memory models with inner-bank timing test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_memory_models_inner_bank_timing.cpp)
    add_executable(test_memory_models_inner_bank_timing test_memory_models_inner_bank_timing.cpp)
    target_link_libraries(test_memory_models_inner_bank_timing pimid_lib)
    add_test(NAME MemoryModelsInnerBankTimingTest COMMAND test_memory_models_inner_bank_timing)
endif()

# Per-subarray PIM comparison test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_subarray_pim_comparison.cpp)
    add_executable(test_subarray_pim_comparison test_subarray_pim_comparison.cpp)
    target_link_libraries(test_subarray_pim_comparison pimid_lib)
    add_test(NAME SubarrayPIMComparisonTest COMMAND test_subarray_pim_comparison)
endif()

# Bank-level PIM comparison test (in-order core vs ALU)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_bank_pim_comparison.cpp)
    add_executable(test_bank_pim_comparison test_bank_pim_comparison.cpp)
    target_link_libraries(test_bank_pim_comparison pimid_lib)
    add_test(NAME BankPIMComparisonTest COMMAND test_bank_pim_comparison)
endif()

# Subarray-level BFS PIM test (ALU PE, 4 subarrays/bank)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_subarray_bfs_pim.cpp)
    add_executable(test_subarray_bfs_pim test_subarray_bfs_pim.cpp)
    target_link_libraries(test_subarray_bfs_pim pimid_lib)
    add_test(NAME SubarrayBFSPIMTest COMMAND test_subarray_bfs_pim)
endif()

# Bank-level BFS PIM test (ALU + In-Order Core, 4 banks)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_bank_bfs_pim.cpp)
    add_executable(test_bank_bfs_pim test_bank_bfs_pim.cpp)
    target_link_libraries(test_bank_bfs_pim pimid_lib)
    add_test(NAME BankBFSPIMTest COMMAND test_bank_bfs_pim)
endif()

# Bank-level BFS PIM test with In-Order Core across all memory technologies
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_bank_inorder_bfs_pim.cpp)
    add_executable(test_bank_inorder_bfs_pim test_bank_inorder_bfs_pim.cpp)
    target_link_libraries(test_bank_inorder_bfs_pim pimid_lib)
    add_test(NAME BankInOrderBFSPIMTest COMMAND test_bank_inorder_bfs_pim)
endif()
