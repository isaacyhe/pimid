# PIMID Benchmarks
# Config-driven benchmark system

# Benchmark runner executable
add_executable(benchmark_runner benchmark_runner.cpp)
target_link_libraries(benchmark_runner pimid_lib)

# Install benchmark runner
install(TARGETS benchmark_runner DESTINATION bin)

# Copy benchmark configs to build directory for easy access (if they exist)
if(EXISTS ${PROJECT_SOURCE_DIR}/config/benchmarks)
    file(COPY ${PROJECT_SOURCE_DIR}/config/benchmarks
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/configs)
endif()

message(STATUS "Benchmark runner will be built: benchmark_runner")
