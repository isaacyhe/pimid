# Functional Tests for PIMID Components

# Test executables (with mock implementations for now)
# Replace with actual PIMID library when integrated

# CACTI wrapper tests
add_executable(test_cacti_wrapper test_cacti_wrapper.cpp)
target_compile_options(test_cacti_wrapper PRIVATE -std=c++17)
add_test(NAME CACTIWrapperFunctional COMMAND test_cacti_wrapper)

# NVSim wrapper tests
add_executable(test_nvsim_wrapper test_nvsim_wrapper.cpp)
target_compile_options(test_nvsim_wrapper PRIVATE -std=c++17)
add_test(NAME NVSimWrapperFunctional COMMAND test_nvsim_wrapper)

# McPAT wrapper tests
add_executable(test_mcpat_wrapper test_mcpat_wrapper.cpp)
target_compile_options(test_mcpat_wrapper PRIVATE -std=c++17)
add_test(NAME McPATWrapperFunctional COMMAND test_mcpat_wrapper)

# Hierarchical power model manager tests
add_executable(test_hierarchical_power test_hierarchical_power.cpp)
target_link_libraries(test_hierarchical_power pimid_lib)
target_compile_options(test_hierarchical_power PRIVATE -std=c++17)
add_test(NAME HierarchicalPowerModelManager COMMAND test_hierarchical_power)

# Port width scale configuration tests
add_executable(test_port_width_scale test_port_width_scale.cpp)
target_link_libraries(test_port_width_scale pimid_lib)
target_compile_options(test_port_width_scale PRIVATE -std=c++17)
add_test(NAME PortWidthScaleConfig COMMAND test_port_width_scale)

# TODO: Add when actual wrappers are available
# target_link_libraries(test_cacti_wrapper pimid_cacti_wrapper)
# target_link_libraries(test_nvsim_wrapper pimid_nvsim_wrapper)
# target_link_libraries(test_mcpat_wrapper pimid_mcpat_wrapper)

# Comprehensive integration test
add_executable(test_comprehensive_integration
    ${CMAKE_SOURCE_DIR}/test/integration/test_comprehensive_integration.cpp
)
target_link_libraries(test_comprehensive_integration pimid_lib)
target_compile_options(test_comprehensive_integration PRIVATE -std=c++17)
add_test(NAME ComprehensiveIntegration COMMAND test_comprehensive_integration)
