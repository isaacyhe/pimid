# PIMID Benchmarks
# Config-driven benchmark system

# Benchmark runner executable
add_executable(benchmark_runner benchmark_runner.cpp)
target_link_libraries(benchmark_runner pimid_lib)

# Install benchmark runner
install(TARGETS benchmark_runner DESTINATION bin)

# Copy benchmark configs to build directory for easy access
file(COPY ${PROJECT_SOURCE_DIR}/configs/benchmarks
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/configs)

message(STATUS "Benchmark runner will be built: benchmark_runner")
