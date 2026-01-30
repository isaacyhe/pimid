/**
 * @file test_utils.cpp
 * @brief PIMID Test Utilities
 *
 * Common utilities for unit and integration tests.
 */

#include "common/types.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
#include <functional>
#include <cmath>
#include <cstring>

namespace pimid {
namespace test {

//=============================================================================
// Test Result Tracking
//=============================================================================

struct TestResult {
    std::string name;
    bool passed;
    std::string message;
    double duration_ms;
};

class TestSuite {
public:
    TestSuite(const std::string& name) : suite_name_(name) {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test Suite: " << name << std::endl;
        std::cout << "========================================" << std::endl;
    }

    void run(const std::string& test_name, std::function<bool()> test_fn) {
        std::cout << "  Running: " << test_name << "... ";
        std::cout.flush();

        auto start = std::chrono::high_resolution_clock::now();
        bool passed = false;
        std::string message;

        try {
            passed = test_fn();
            message = passed ? "OK" : "FAILED";
        } catch (const std::exception& e) {
            passed = false;
            message = std::string("EXCEPTION: ") + e.what();
        } catch (...) {
            passed = false;
            message = "UNKNOWN EXCEPTION";
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double duration_ms = duration.count() / 1000.0;

        results_.push_back({test_name, passed, message, duration_ms});

        if (passed) {
            std::cout << "\033[32mPASSED\033[0m";
        } else {
            std::cout << "\033[31mFAILED\033[0m";
        }
        std::cout << " (" << std::fixed << std::setprecision(2) << duration_ms << " ms)";

        if (!passed && !message.empty() && message != "FAILED") {
            std::cout << " - " << message;
        }
        std::cout << std::endl;
    }

    int summarize() {
        int passed = 0, failed = 0;
        double total_time = 0;

        for (const auto& result : results_) {
            if (result.passed) passed++;
            else failed++;
            total_time += result.duration_ms;
        }

        std::cout << "\n----------------------------------------" << std::endl;
        std::cout << "Suite: " << suite_name_ << std::endl;
        std::cout << "  Total:  " << results_.size() << std::endl;
        std::cout << "  Passed: \033[32m" << passed << "\033[0m" << std::endl;
        std::cout << "  Failed: " << (failed > 0 ? "\033[31m" : "") << failed
                  << (failed > 0 ? "\033[0m" : "") << std::endl;
        std::cout << "  Time:   " << std::fixed << std::setprecision(2) << total_time << " ms" << std::endl;
        std::cout << "----------------------------------------" << std::endl;

        return failed;
    }

    const std::vector<TestResult>& getResults() const { return results_; }

private:
    std::string suite_name_;
    std::vector<TestResult> results_;
};

//=============================================================================
// Assertion Utilities
//=============================================================================

class AssertionError : public std::runtime_error {
public:
    AssertionError(const std::string& msg) : std::runtime_error(msg) {}
};

inline void assertTrue(bool condition, const std::string& message = "") {
    if (!condition) {
        throw AssertionError(message.empty() ? "Assertion failed" : message);
    }
}

inline void assertFalse(bool condition, const std::string& message = "") {
    assertTrue(!condition, message);
}

template<typename T>
void assertEqual(const T& expected, const T& actual, const std::string& message = "") {
    if (expected != actual) {
        std::ostringstream oss;
        oss << (message.empty() ? "Values not equal" : message)
            << ": expected " << expected << ", got " << actual;
        throw AssertionError(oss.str());
    }
}

template<typename T>
void assertNotEqual(const T& a, const T& b, const std::string& message = "") {
    if (a == b) {
        std::ostringstream oss;
        oss << (message.empty() ? "Values should not be equal" : message)
            << ": both are " << a;
        throw AssertionError(oss.str());
    }
}

inline void assertNear(double expected, double actual, double tolerance = 1e-6,
                       const std::string& message = "") {
    if (std::abs(expected - actual) > tolerance) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(9)
            << (message.empty() ? "Values not within tolerance" : message)
            << ": expected " << expected << ", got " << actual
            << " (tolerance: " << tolerance << ")";
        throw AssertionError(oss.str());
    }
}

template<typename T>
void assertGreaterThan(const T& value, const T& threshold, const std::string& message = "") {
    if (!(value > threshold)) {
        std::ostringstream oss;
        oss << (message.empty() ? "Value not greater than threshold" : message)
            << ": " << value << " <= " << threshold;
        throw AssertionError(oss.str());
    }
}

template<typename T>
void assertLessThan(const T& value, const T& threshold, const std::string& message = "") {
    if (!(value < threshold)) {
        std::ostringstream oss;
        oss << (message.empty() ? "Value not less than threshold" : message)
            << ": " << value << " >= " << threshold;
        throw AssertionError(oss.str());
    }
}

inline void assertNotNull(void* ptr, const std::string& message = "") {
    if (ptr == nullptr) {
        throw AssertionError(message.empty() ? "Pointer is null" : message);
    }
}

//=============================================================================
// Memory Test Utilities
//=============================================================================

/**
 * Create a test memory request
 */
inline MemoryRequest makeTestRequest(
    Address addr = 0x1000,
    MemoryRequestType type = MemoryRequestType::READ,
    uint64_t size = 64,
    uint32_t src_id = 0) {
    return MemoryRequest(addr, type, size, 0, SimulationDomain::HOST, src_id);
}

/**
 * Generate random memory requests for stress testing
 */
inline std::vector<MemoryRequest> generateRandomRequests(
    size_t count,
    Address base_addr = 0,
    Address max_addr = 0x100000,
    uint64_t max_size = 4096) {

    std::vector<MemoryRequest> requests;
    requests.reserve(count);

    for (size_t i = 0; i < count; i++) {
        Address addr = base_addr + (rand() % (max_addr - base_addr));
        uint64_t size = 1 + (rand() % max_size);
        MemoryRequestType type = (rand() % 2 == 0) ? MemoryRequestType::READ
                                                    : MemoryRequestType::WRITE;
        requests.push_back(MemoryRequest(addr, type, size, i, SimulationDomain::HOST, 0));
    }

    return requests;
}

/**
 * Generate sequential access pattern
 */
inline std::vector<MemoryRequest> generateSequentialRequests(
    size_t count,
    Address start_addr = 0,
    uint64_t stride = 64,
    MemoryRequestType type = MemoryRequestType::READ) {

    std::vector<MemoryRequest> requests;
    requests.reserve(count);

    for (size_t i = 0; i < count; i++) {
        Address addr = start_addr + (i * stride);
        requests.push_back(MemoryRequest(addr, type, stride, i, SimulationDomain::HOST, 0));
    }

    return requests;
}

//=============================================================================
// Timing Utilities
//=============================================================================

class Timer {
public:
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    double elapsedMs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now - start_time_).count() / 1000.0;
    }

    double elapsedUs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            now - start_time_).count() / 1000.0;
    }

private:
    std::chrono::high_resolution_clock::time_point start_time_;
};

/**
 * Benchmark a function and return average time in microseconds
 */
template<typename Func>
double benchmark(Func fn, size_t iterations = 1000) {
    // Warmup
    for (size_t i = 0; i < 10; i++) {
        fn();
    }

    Timer timer;
    timer.start();

    for (size_t i = 0; i < iterations; i++) {
        fn();
    }

    return timer.elapsedUs() / iterations;
}

//=============================================================================
// Network/Packet Test Utilities
//=============================================================================

inline NetworkPacket makeTestPacket(
    uint32_t src = 0,
    uint32_t dst = 1,
    PacketType type = PacketType::DATA,
    uint64_t size = 64) {
    NetworkPacket pkt;
    pkt.src_node = src;
    pkt.dst_node = dst;
    pkt.type = type;
    pkt.size = size;
    pkt.addr = 0;
    pkt.inject_cycle = 0;
    return pkt;
}

//=============================================================================
// Output Utilities
//=============================================================================

inline void printTestHeader(const std::string& category) {
    std::cout << "\n╔══════════════════════════════════════════╗" << std::endl;
    std::cout << "║ " << std::left << std::setw(40) << category << " ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════╝" << std::endl;
}

inline void printSeparator() {
    std::cout << "──────────────────────────────────────────" << std::endl;
}

} // namespace test
} // namespace pimid
