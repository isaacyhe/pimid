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

# TODO: Link with actual PIMID libraries when available
# target_link_libraries(test_pim_full_system
#     pimid_lib
#     pimid_cacti_wrapper
#     pimid_ramulator_wrapper
#     pimid_nvsim_wrapper
#     pimid_mcpat_wrapper
#     pimid_garnet_wrapper
# )
