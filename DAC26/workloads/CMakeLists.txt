cmake_minimum_required(VERSION 3.10)
project(DAC26_LIBCom_Workloads)

# C++ Standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")

# Build all workloads

# Original workloads (baseline)
add_executable(gemm_workload gemm_workload.cpp)
add_executable(bfs_workload bfs_workload.cpp)
add_executable(spmv_workload spmv_workload.cpp)
add_executable(reduction_workload reduction_workload.cpp)

# GEMM - Message Passing and Shared Memory
add_executable(gemm_message gemm_message.cpp)
add_executable(gemm_shared gemm_shared.cpp)

# BFS - Message Passing and Shared Memory
add_executable(bfs_message bfs_message.cpp)
add_executable(bfs_shared bfs_shared.cpp)

# SpMV - Message Passing and Shared Memory
add_executable(spmv_message spmv_message.cpp)
add_executable(spmv_shared spmv_shared.cpp)

# Reduction - Message Passing and Shared Memory
add_executable(reduction_message reduction_message.cpp)
add_executable(reduction_shared reduction_shared.cpp)

# Reduction with Virtual Channel support (VC-aware)
add_executable(reduction_message_vc reduction_message_vc.cpp)

# Histogram - Message Passing and Shared Memory
add_executable(histogram_message histogram_message.cpp)
add_executable(histogram_shared histogram_shared.cpp)

# Dot Product - Message Passing and Shared Memory
add_executable(dotproduct_message dotproduct_message.cpp)
add_executable(dotproduct_shared dotproduct_shared.cpp)

# Prefix Sum - Message Passing and Shared Memory
add_executable(prefixsum_message prefixsum_message.cpp)
add_executable(prefixsum_shared prefixsum_shared.cpp)

# Stencil 1D - Message Passing and Shared Memory
add_executable(stencil1d_message stencil1d_message.cpp)
add_executable(stencil1d_shared stencil1d_shared.cpp)

# Install targets (optional)
install(TARGETS
        gemm_workload bfs_workload spmv_workload reduction_workload
        gemm_message gemm_shared
        bfs_message bfs_shared
        spmv_message spmv_shared
        reduction_message reduction_shared
        histogram_message histogram_shared
        dotproduct_message dotproduct_shared
        prefixsum_message prefixsum_shared
        stencil1d_message stencil1d_shared
        RUNTIME DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../bin)

# Print build info
message(STATUS "DAC'26 LIBCom Workloads (16 total implementations)")
message(STATUS "  4 Core Workloads × 2 Programming Models:")
message(STATUS "    - GEMM: Matrix multiplication (message + shared)")
message(STATUS "    - BFS: Graph traversal (message + shared)")
message(STATUS "    - SpMV: Sparse matrix-vector multiply (message + shared)")
message(STATUS "    - Reduction: Tree reduction (message + shared)")
message(STATUS "  4 Additional Workloads × 2 Programming Models:")
message(STATUS "    - Histogram: Parallel histogram (message + shared)")
message(STATUS "    - Dot Product: Vector dot product (message + shared)")
message(STATUS "    - Prefix Sum: Parallel prefix scan (message + shared)")
message(STATUS "    - Stencil 1D: 1D stencil computation (message + shared)")
