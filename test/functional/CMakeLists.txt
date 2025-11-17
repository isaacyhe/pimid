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

# TODO: Add when actual wrappers are available
# target_link_libraries(test_cacti_wrapper pimid_cacti_wrapper)
# target_link_libraries(test_nvsim_wrapper pimid_nvsim_wrapper)
# target_link_libraries(test_mcpat_wrapper pimid_mcpat_wrapper)
