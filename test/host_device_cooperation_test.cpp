/**
 * @file host_device_cooperation_test.cpp
 * @brief Test application demonstrating host/device cooperation in PIMID
 *
 * This application demonstrates the cooperation between host CPU and PIM device:
 * 1. Host initializes data arrays
 * 2. Host transfers data to PIM memory
 * 3. PIM device processes data (vector addition)
 * 4. Device transfers results back to host
 * 5. Host verifies results
 *
 * Usage:
 *   ./host_device_cooperation_test [array_size]
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <chrono>

// Simulated host/device communication
namespace HostDevice {

enum class Domain {
    HOST,
    DEVICE
};

struct Transaction {
    void* data;
    size_t size_bytes;
    Domain source;
    Domain destination;
    uint64_t timestamp_ns;
};

class MemoryManager {
public:
    // Allocate memory on host
    void* allocHost(size_t size) {
        void* ptr = malloc(size);
        host_allocations.push_back({ptr, size});
        std::cout << "[Host] Allocated " << size << " bytes at " << ptr << std::endl;
        return ptr;
    }

    // Allocate memory on device
    void* allocDevice(size_t size) {
        void* ptr = malloc(size);  // Simulated device memory
        device_allocations.push_back({ptr, size});
        std::cout << "[Device] Allocated " << size << " bytes at " << ptr << std::endl;
        return ptr;
    }

    // Transfer data from host to device
    void transferToDevice(void* host_ptr, void* device_ptr, size_t size) {
        auto start = std::chrono::high_resolution_clock::now();

        memcpy(device_ptr, host_ptr, size);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        Transaction tx = {host_ptr, size, Domain::HOST, Domain::DEVICE,
                          static_cast<uint64_t>(duration.count() * 1000)};
        transactions.push_back(tx);

        std::cout << "[Transfer] Host -> Device: " << size << " bytes in "
                  << duration.count() << " us" << std::endl;
    }

    // Transfer data from device to host
    void transferToHost(void* device_ptr, void* host_ptr, size_t size) {
        auto start = std::chrono::high_resolution_clock::now();

        memcpy(host_ptr, device_ptr, size);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        Transaction tx = {device_ptr, size, Domain::DEVICE, Domain::HOST,
                          static_cast<uint64_t>(duration.count() * 1000)};
        transactions.push_back(tx);

        std::cout << "[Transfer] Device -> Host: " << size << " bytes in "
                  << duration.count() << " us" << std::endl;
    }

    // Free all allocations
    void freeAll() {
        for (auto& alloc : host_allocations) {
            free(alloc.ptr);
        }
        for (auto& alloc : device_allocations) {
            free(alloc.ptr);
        }
        host_allocations.clear();
        device_allocations.clear();
        std::cout << "[Cleanup] All memory freed" << std::endl;
    }

    // Print statistics
    void printStats() {
        uint64_t total_host_to_device = 0;
        uint64_t total_device_to_host = 0;
        size_t bytes_to_device = 0;
        size_t bytes_to_host = 0;

        for (const auto& tx : transactions) {
            if (tx.source == Domain::HOST && tx.destination == Domain::DEVICE) {
                total_host_to_device += tx.timestamp_ns;
                bytes_to_device += tx.size_bytes;
            } else if (tx.source == Domain::DEVICE && tx.destination == Domain::HOST) {
                total_device_to_host += tx.timestamp_ns;
                bytes_to_host += tx.size_bytes;
            }
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "HOST/DEVICE TRANSFER STATISTICS" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Host -> Device:" << std::endl;
        std::cout << "  Total bytes: " << bytes_to_device << std::endl;
        std::cout << "  Total time: " << total_host_to_device / 1000.0 << " us" << std::endl;
        if (total_host_to_device > 0) {
            double bandwidth = (bytes_to_device / (1024.0 * 1024.0)) /
                              (total_host_to_device / 1e9);
            std::cout << "  Bandwidth: " << bandwidth << " MB/s" << std::endl;
        }

        std::cout << "\nDevice -> Host:" << std::endl;
        std::cout << "  Total bytes: " << bytes_to_host << std::endl;
        std::cout << "  Total time: " << total_device_to_host / 1000.0 << " us" << std::endl;
        if (total_device_to_host > 0) {
            double bandwidth = (bytes_to_host / (1024.0 * 1024.0)) /
                              (total_device_to_host / 1e9);
            std::cout << "  Bandwidth: " << bandwidth << " MB/s" << std::endl;
        }
    }

private:
    struct Allocation {
        void* ptr;
        size_t size;
    };

    std::vector<Allocation> host_allocations;
    std::vector<Allocation> device_allocations;
    std::vector<Transaction> transactions;
};

} // namespace HostDevice

// Simulated PIM kernel
namespace PIMKernel {

void vectorAdd(const float* a, const float* b, float* c, size_t n) {
    std::cout << "[PIM Kernel] Executing vector addition on " << n << " elements" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    // Simulated PIM processing - near-memory computation
    for (size_t i = 0; i < n; i++) {
        c[i] = a[i] + b[i];
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "[PIM Kernel] Computation completed in " << duration.count() << " us" << std::endl;
    std::cout << "[PIM Kernel] Throughput: " << (n / (duration.count() / 1e6)) / 1e6
              << " M ops/sec" << std::endl;
}

} // namespace PIMKernel

// Main test application
int main(int argc, char** argv) {
    size_t array_size = 1024 * 1024;  // Default: 1M elements

    if (argc > 1) {
        array_size = std::atoi(argv[1]);
    }

    std::cout << "========================================" << std::endl;
    std::cout << "HOST/DEVICE COOPERATION TEST" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Array size: " << array_size << " elements" << std::endl;
    std::cout << "Data size: " << (array_size * sizeof(float)) / 1024.0 << " KB per array" << std::endl;
    std::cout << std::endl;

    HostDevice::MemoryManager mem_mgr;

    // Step 1: Allocate host memory
    std::cout << "Step 1: Allocating host memory" << std::endl;
    float* host_a = static_cast<float*>(mem_mgr.allocHost(array_size * sizeof(float)));
    float* host_b = static_cast<float*>(mem_mgr.allocHost(array_size * sizeof(float)));
    float* host_c = static_cast<float*>(mem_mgr.allocHost(array_size * sizeof(float)));

    // Step 2: Initialize data on host
    std::cout << "\nStep 2: Initializing data on host" << std::endl;
    for (size_t i = 0; i < array_size; i++) {
        host_a[i] = static_cast<float>(i);
        host_b[i] = static_cast<float>(i * 2);
    }
    std::cout << "[Host] Initialized arrays A and B" << std::endl;

    // Step 3: Allocate device (PIM) memory
    std::cout << "\nStep 3: Allocating device (PIM) memory" << std::endl;
    float* device_a = static_cast<float*>(mem_mgr.allocDevice(array_size * sizeof(float)));
    float* device_b = static_cast<float*>(mem_mgr.allocDevice(array_size * sizeof(float)));
    float* device_c = static_cast<float*>(mem_mgr.allocDevice(array_size * sizeof(float)));

    // Step 4: Transfer data to device
    std::cout << "\nStep 4: Transferring data to device" << std::endl;
    mem_mgr.transferToDevice(host_a, device_a, array_size * sizeof(float));
    mem_mgr.transferToDevice(host_b, device_b, array_size * sizeof(float));

    // Step 5: Execute computation on device
    std::cout << "\nStep 5: Executing computation on device" << std::endl;
    PIMKernel::vectorAdd(device_a, device_b, device_c, array_size);

    // Step 6: Transfer results back to host
    std::cout << "\nStep 6: Transferring results back to host" << std::endl;
    mem_mgr.transferToHost(device_c, host_c, array_size * sizeof(float));

    // Step 7: Verify results on host
    std::cout << "\nStep 7: Verifying results on host" << std::endl;
    bool success = true;
    size_t errors = 0;
    const size_t max_errors_to_show = 5;

    for (size_t i = 0; i < array_size; i++) {
        float expected = host_a[i] + host_b[i];
        if (std::abs(host_c[i] - expected) > 1e-5) {
            if (errors < max_errors_to_show) {
                std::cout << "[Host] Error at index " << i << ": expected "
                          << expected << ", got " << host_c[i] << std::endl;
            }
            errors++;
            success = false;
        }
    }

    if (success) {
        std::cout << "[Host] ✓ All results verified successfully!" << std::endl;
    } else {
        std::cout << "[Host] ✗ Verification failed with " << errors << " errors" << std::endl;
    }

    // Print transfer statistics
    mem_mgr.printStats();

    // Cleanup
    std::cout << "\nStep 8: Cleanup" << std::endl;
    mem_mgr.freeAll();

    std::cout << "\n========================================" << std::endl;
    std::cout << "TEST " << (success ? "PASSED" : "FAILED") << std::endl;
    std::cout << "========================================" << std::endl;

    return success ? 0 : 1;
}
