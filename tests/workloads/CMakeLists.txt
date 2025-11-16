# PIM Workload Tests

# Realistic workload simulations
add_executable(test_pim_workloads test_pim_workloads.cpp)
target_compile_options(test_pim_workloads PRIVATE -std=c++17)
add_test(NAME PIMWorkloads COMMAND test_pim_workloads)

# TODO: Add more workload-specific tests
# - Vector operations
# - Matrix operations
# - Graph analytics
# - Database operations
# - ML inference
