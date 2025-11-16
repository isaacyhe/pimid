# DRAM Architecture Tests

# Test: DRAM architecture specifications
add_executable(test_dram_architectures test_dram_architectures.cpp
    ../../pimid/memory/dram_architecture.cpp)
target_include_directories(test_dram_architectures PRIVATE ${CMAKE_SOURCE_DIR})

# Add test to CTest
add_test(NAME DRAMArchitectures COMMAND test_dram_architectures)

# Test: DRAM architecture verification (v2 - rigorously verified specs)
add_executable(test_verification test_verification.cpp
    ../../pimid/memory/dram_architecture.cpp)
target_include_directories(test_verification PRIVATE ${CMAKE_SOURCE_DIR})

# Add test to CTest
add_test(NAME DRAMVerification COMMAND test_verification)
