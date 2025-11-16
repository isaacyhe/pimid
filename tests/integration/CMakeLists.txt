# Integration Tests for PIMID

# Existing integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_external_tools.cpp)
    add_executable(test_external_tools test_external_tools.cpp)
    target_link_libraries(test_external_tools pimid_lib test_utils)
    add_test(NAME ExternalToolsIntegration COMMAND test_external_tools)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_garnet.cpp)
    add_executable(test_garnet test_garnet.cpp)
    target_link_libraries(test_garnet pimid_lib test_utils)
    add_test(NAME GarnetIntegration COMMAND test_garnet)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_host_device_comm.cpp)
    add_executable(test_host_device_comm test_host_device_comm.cpp)
    target_link_libraries(test_host_device_comm pimid_lib test_utils)
    add_test(NAME HostDeviceCommIntegration COMMAND test_host_device_comm)
endif()

# New full system integration test
add_executable(test_pim_full_system test_pim_full_system.cpp)
target_compile_options(test_pim_full_system PRIVATE -std=c++17)
add_test(NAME PIMFullSystemIntegration COMMAND test_pim_full_system)

# PIM-Ramulator integration verification test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_pim_ramulator_integration.cpp)
    add_executable(test_pim_ramulator_integration test_pim_ramulator_integration.cpp)
    target_compile_options(test_pim_ramulator_integration PRIVATE -std=c++17)
    # Note: This test needs pimid_lib which includes Ramulator wrapper + PIM extensions
    # target_link_libraries(test_pim_ramulator_integration pimid_lib ramulator)
    add_test(NAME PIMRamulatorIntegration COMMAND test_pim_ramulator_integration)
endif()

# PIM components simple validation (standalone - no Ramulator required)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_pim_simple_validation.cpp)
    add_executable(test_pim_simple_validation test_pim_simple_validation.cpp)
    target_compile_options(test_pim_simple_validation PRIVATE -std=c++17)
    # Note: This test needs only PIM components, not full Ramulator
    # target_link_libraries(test_pim_simple_validation pimid_lib)
    add_test(NAME PIMSimpleValidation COMMAND test_pim_simple_validation)
endif()

# Host vs MC-PIM vs Bank-PIM compute comparison (with equal compute power)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_pim_compute_comparison.cpp)
    add_executable(test_pim_compute_comparison test_pim_compute_comparison.cpp)
    target_compile_options(test_pim_compute_comparison PRIVATE -std=c++17)
    # Note: This test demonstrates the new PIM bandwidth modeling features
    # target_link_libraries(test_pim_compute_comparison pimid_lib)
    add_test(NAME PIMComputeComparison COMMAND test_pim_compute_comparison)
endif()

# Host vs MC-PIM vs Bank-PIM with latency modeling
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_pim_with_latency.cpp)
    add_executable(test_pim_with_latency test_pim_with_latency.cpp)
    target_compile_options(test_pim_with_latency PRIVATE -std=c++17)
    # Note: This test adds DRAM latency, row buffer hits, and network latency
    # target_link_libraries(test_pim_with_latency pimid_lib)
    add_test(NAME PIMWithLatency COMMAND test_pim_with_latency)
endif()

# Comprehensive test of data locality and reach for different PIM granularities
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_pim_data_locality.cpp)
    add_executable(test_pim_data_locality test_pim_data_locality.cpp)
    target_compile_options(test_pim_data_locality PRIVATE -std=c++17)
    # Note: This test demonstrates the critical importance of data locality in PIM
    # target_link_libraries(test_pim_data_locality pimid_lib)
    add_test(NAME PIMDataLocality COMMAND test_pim_data_locality)
endif()

# TODO: Link with actual PIMID libraries when available
# target_link_libraries(test_pim_full_system
#     pimid_lib
#     pimid_cacti_wrapper
#     pimid_ramulator_wrapper
#     pimid_nvsim_wrapper
#     pimid_mcpat_wrapper
#     pimid_garnet_wrapper
# )
