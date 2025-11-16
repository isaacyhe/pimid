# Integration tests

# External tools integration test
add_executable(test_external_tools test_external_tools.cpp)
target_link_libraries(test_external_tools pimid_lib)

if(HAVE_CACTI)
    target_link_libraries(test_external_tools cacti)
endif()

if(HAVE_RAMULATOR)
    target_link_libraries(test_external_tools ramulator)
endif()

# Add to CTest
add_test(NAME ExternalToolsTest COMMAND test_external_tools)
