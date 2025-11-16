# PIM Workload Tests

# Realistic workload simulations
add_executable(test_pim_workloads test_pim_workloads.cpp)
target_compile_options(test_pim_workloads PRIVATE -std=c++17)
add_test(NAME PIMWorkloads COMMAND test_pim_workloads)

# PIM granularity comparison (DDR4 architecture)
add_executable(test_pim_granularity test_pim_granularity.cpp)
target_compile_options(test_pim_granularity PRIVATE -std=c++17)
add_test(NAME PIMGranularity COMMAND test_pim_granularity)
