cmake_minimum_required(VERSION 3.10)
project(DAC26_LIBCom_Workloads)

# C++ Standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")

# Build all workloads
add_executable(gemm_workload gemm_workload.cpp)
add_executable(bfs_workload bfs_workload.cpp)
add_executable(spmv_workload spmv_workload.cpp)
add_executable(reduction_workload reduction_workload.cpp)

# Install targets (optional)
install(TARGETS gemm_workload bfs_workload spmv_workload reduction_workload
        RUNTIME DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../bin)

# Print build info
message(STATUS "DAC'26 LIBCom Workloads")
message(STATUS "  GEMM: Matrix multiplication benchmark")
message(STATUS "  BFS: Graph traversal benchmark")
message(STATUS "  SpMV: Sparse matrix-vector multiply")
message(STATUS "  Reduction: Tree reduction benchmark")
