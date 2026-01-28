/**
 * @file test_host_device_comm.cpp
 * @brief Integration Tests for PIMID Host-Device Communication
 *
 * Tests for message passing, synchronization, memory requests,
 * and data transfer between host and device engines.
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

// Include test utilities
#include "../test_utils.cpp"

// Communication layer
#include "common/types.h"
#include "communication/socket_comm.h"

using namespace pimid;
using namespace pimid::test;

//=============================================================================
// Mock Communication Channel for Testing
//=============================================================================

/**
 * Simple mock communication channel that simulates host-device communication
 * without requiring actual socket connections.
 */
class MockCommChannel {
public:
    MockCommChannel() : closed_(false), latency_ns_(1000) {}

    void setLatency(uint64_t latency_ns) { latency_ns_ = latency_ns; }

    bool send(const CommMessage& msg) {
        if (closed_) return false;

        std::lock_guard<std::mutex> lock(mutex_);

        // Simulate network latency
        std::this_thread::sleep_for(std::chrono::nanoseconds(latency_ns_));

        messages_.push(msg);
        cv_.notify_one();
        return true;
    }

    bool receive(CommMessage& msg, int timeout_ms = 1000) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (!cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                          [this] { return !messages_.empty() || closed_; })) {
            return false;  // Timeout
        }

        if (closed_ && messages_.empty()) {
            return false;
        }

        msg = messages_.front();
        messages_.pop();
        return true;
    }

    bool hasMessages() {
        std::lock_guard<std::mutex> lock(mutex_);
        return !messages_.empty();
    }

    size_t queueSize() {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_.size();
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        cv_.notify_all();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!messages_.empty()) messages_.pop();
        closed_ = false;
    }

private:
    std::queue<CommMessage> messages_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> closed_;
    uint64_t latency_ns_;
};

//=============================================================================
// Basic Communication Tests
//=============================================================================

bool test_message_send_receive() {
    MockCommChannel channel;

    CommMessage sent;
    sent.type = MessageType::MEMORY_REQUEST;
    sent.src_domain = SimulationDomain::HOST;
    sent.dst_domain = SimulationDomain::DEVICE;
    sent.addr = 0x1000;
    sent.size = 64;
    sent.timestamp = 100;
    sent.request_id = 1;

    assertTrue(channel.send(sent), "Send should succeed");

    CommMessage received;
    assertTrue(channel.receive(received, 100), "Receive should succeed");

    assertEqual(received.type, sent.type, "Message type mismatch");
    assertEqual(received.addr, sent.addr, "Address mismatch");
    assertEqual(received.size, sent.size, "Size mismatch");
    assertEqual(received.timestamp, sent.timestamp, "Timestamp mismatch");

    return true;
}

bool test_message_queue_ordering() {
    MockCommChannel channel;
    channel.setLatency(0);  // Disable latency for ordering test

    // Send multiple messages
    for (int i = 0; i < 10; i++) {
        CommMessage msg;
        msg.type = MessageType::MEMORY_REQUEST;
        msg.request_id = i;
        assertTrue(channel.send(msg), "Send should succeed");
    }

    // Verify FIFO ordering
    for (int i = 0; i < 10; i++) {
        CommMessage msg;
        assertTrue(channel.receive(msg, 100), "Receive should succeed");
        assertEqual(msg.request_id, (uint64_t)i, "Message order incorrect");
    }

    return true;
}

bool test_receive_timeout() {
    MockCommChannel channel;

    CommMessage msg;
    Timer timer;
    timer.start();

    // Should timeout after 50ms
    bool received = channel.receive(msg, 50);
    double elapsed = timer.elapsedMs();

    assertFalse(received, "Receive should timeout");
    assertTrue(elapsed >= 45 && elapsed < 100, "Timeout timing incorrect");

    return true;
}

bool test_channel_close() {
    MockCommChannel channel;

    // Send a message
    CommMessage msg;
    msg.type = MessageType::SYNC_REQUEST;
    assertTrue(channel.send(msg), "Send before close should succeed");

    // Close channel
    channel.close();

    // Receive existing message should still work
    CommMessage received;
    assertTrue(channel.receive(received, 100), "Should receive queued message");

    // Send after close should fail
    assertFalse(channel.send(msg), "Send after close should fail");

    return true;
}

//=============================================================================
// Message Type Tests
//=============================================================================

bool test_memory_request_message() {
    MockCommChannel channel;
    channel.setLatency(0);

    CommMessage req;
    req.type = MessageType::MEMORY_REQUEST;
    req.src_domain = SimulationDomain::HOST;
    req.dst_domain = SimulationDomain::DEVICE;
    req.addr = 0xDEADBEEF;
    req.size = 4096;
    req.timestamp = 12345;
    req.request_id = 42;

    assertTrue(channel.send(req), "Send should succeed");

    CommMessage received;
    assertTrue(channel.receive(received, 100), "Receive should succeed");

    assertEqual(received.type, MessageType::MEMORY_REQUEST, "Type mismatch");
    assertEqual(received.addr, (uint64_t)0xDEADBEEF, "Address mismatch");
    assertEqual(received.size, (uint64_t)4096, "Size mismatch");

    return true;
}

bool test_memory_response_message() {
    MockCommChannel channel;
    channel.setLatency(0);

    CommMessage resp;
    resp.type = MessageType::MEMORY_RESPONSE;
    resp.src_domain = SimulationDomain::DEVICE;
    resp.dst_domain = SimulationDomain::HOST;
    resp.timestamp = 100;  // Latency
    resp.request_id = 42;
    resp.data = {0x01, 0x02, 0x03, 0x04};

    assertTrue(channel.send(resp), "Send should succeed");

    CommMessage received;
    assertTrue(channel.receive(received, 100), "Receive should succeed");

    assertEqual(received.type, MessageType::MEMORY_RESPONSE, "Type mismatch");
    assertEqual(received.data.size(), (size_t)4, "Data size mismatch");
    assertEqual(received.data[0], (uint8_t)0x01, "Data[0] mismatch");

    return true;
}

bool test_sync_message() {
    MockCommChannel channel;
    channel.setLatency(0);

    CommMessage sync;
    sync.type = MessageType::SYNC_REQUEST;
    sync.timestamp = 1000000;  // Cycle count

    assertTrue(channel.send(sync), "Send should succeed");

    CommMessage received;
    assertTrue(channel.receive(received, 100), "Receive should succeed");

    assertEqual(received.type, MessageType::SYNC_REQUEST, "Type mismatch");
    assertEqual(received.timestamp, (uint64_t)1000000, "Timestamp mismatch");

    return true;
}

bool test_offload_message() {
    MockCommChannel channel;
    channel.setLatency(0);

    CommMessage offload;
    offload.type = MessageType::OFFLOAD_REQUEST;
    offload.src_domain = SimulationDomain::HOST;
    offload.dst_domain = SimulationDomain::DEVICE;
    offload.addr = 0x2000;  // Kernel address
    offload.size = 1024;    // Data size

    assertTrue(channel.send(offload), "Send should succeed");

    CommMessage received;
    assertTrue(channel.receive(received, 100), "Receive should succeed");

    assertEqual(received.type, MessageType::OFFLOAD_REQUEST, "Type mismatch");

    return true;
}

//=============================================================================
// Synchronization Tests
//=============================================================================

bool test_bidirectional_communication() {
    MockCommChannel host_to_device;
    MockCommChannel device_to_host;

    host_to_device.setLatency(0);
    device_to_host.setLatency(0);

    // Simulate host sending request
    CommMessage request;
    request.type = MessageType::MEMORY_REQUEST;
    request.request_id = 1;
    assertTrue(host_to_device.send(request), "H->D send should succeed");

    // Device receives and responds
    CommMessage received_req;
    assertTrue(host_to_device.receive(received_req, 100), "D receive should succeed");

    CommMessage response;
    response.type = MessageType::MEMORY_RESPONSE;
    response.request_id = received_req.request_id;
    response.timestamp = 50;  // Simulated latency
    assertTrue(device_to_host.send(response), "D->H send should succeed");

    // Host receives response
    CommMessage received_resp;
    assertTrue(device_to_host.receive(received_resp, 100), "H receive should succeed");
    assertEqual(received_resp.request_id, request.request_id, "Request ID mismatch");

    return true;
}

bool test_concurrent_messages() {
    MockCommChannel channel;
    channel.setLatency(0);

    std::atomic<int> messages_sent(0);
    std::atomic<int> messages_received(0);
    const int num_messages = 100;

    // Sender thread
    std::thread sender([&]() {
        for (int i = 0; i < num_messages; i++) {
            CommMessage msg;
            msg.request_id = i;
            if (channel.send(msg)) {
                messages_sent++;
            }
        }
    });

    // Receiver thread
    std::thread receiver([&]() {
        for (int i = 0; i < num_messages; i++) {
            CommMessage msg;
            if (channel.receive(msg, 1000)) {
                messages_received++;
            }
        }
    });

    sender.join();
    receiver.join();

    assertEqual(messages_sent.load(), num_messages, "Not all messages sent");
    assertEqual(messages_received.load(), num_messages, "Not all messages received");

    return true;
}

bool test_message_throughput() {
    MockCommChannel channel;
    channel.setLatency(0);  // No artificial latency

    const int num_messages = 10000;

    Timer timer;
    timer.start();

    // Send all messages
    for (int i = 0; i < num_messages; i++) {
        CommMessage msg;
        msg.request_id = i;
        channel.send(msg);
    }

    // Receive all messages
    for (int i = 0; i < num_messages; i++) {
        CommMessage msg;
        channel.receive(msg, 100);
    }

    double elapsed_ms = timer.elapsedMs();
    double throughput = (num_messages * 2) / (elapsed_ms / 1000.0);  // Round-trip

    std::cout << "    Throughput: " << std::fixed << std::setprecision(0)
              << throughput << " msg/s";

    assertGreaterThan(throughput, 10000.0, "Throughput too low");

    return true;
}

//=============================================================================
// Error Handling Tests
//=============================================================================

bool test_empty_queue_receive() {
    MockCommChannel channel;

    CommMessage msg;
    bool received = channel.receive(msg, 10);

    assertFalse(received, "Should not receive from empty queue");
    return true;
}

bool test_channel_reset() {
    MockCommChannel channel;
    channel.setLatency(0);

    // Add some messages
    for (int i = 0; i < 5; i++) {
        CommMessage msg;
        msg.request_id = i;
        channel.send(msg);
    }

    assertEqual(channel.queueSize(), (size_t)5, "Queue should have 5 messages");

    // Reset channel
    channel.reset();

    assertEqual(channel.queueSize(), (size_t)0, "Queue should be empty after reset");
    assertFalse(channel.hasMessages(), "Should have no messages after reset");

    // Should be able to use channel again
    CommMessage msg;
    msg.request_id = 100;
    assertTrue(channel.send(msg), "Send after reset should work");

    return true;
}

//=============================================================================
// Data Integrity Tests
//=============================================================================

bool test_large_data_transfer() {
    MockCommChannel channel;
    channel.setLatency(0);

    // Create large data payload
    const size_t data_size = 4096;
    std::vector<uint8_t> large_data(data_size);
    for (size_t i = 0; i < data_size; i++) {
        large_data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    CommMessage msg;
    msg.type = MessageType::MEMORY_RESPONSE;
    msg.data = large_data;

    assertTrue(channel.send(msg), "Send large message should succeed");

    CommMessage received;
    assertTrue(channel.receive(received, 100), "Receive should succeed");

    assertEqual(received.data.size(), data_size, "Data size mismatch");

    // Verify data integrity
    for (size_t i = 0; i < data_size; i++) {
        if (received.data[i] != large_data[i]) {
            assertTrue(false, "Data corruption detected");
        }
    }

    return true;
}

bool test_message_with_all_fields() {
    MockCommChannel channel;
    channel.setLatency(0);

    CommMessage msg;
    msg.type = MessageType::MEMORY_REQUEST;
    msg.src_domain = SimulationDomain::HOST;
    msg.dst_domain = SimulationDomain::DEVICE;
    msg.addr = 0x123456789ABCDEF0;
    msg.size = 0xFEDCBA9876543210;
    msg.timestamp = 999999999;
    msg.request_id = 0xDEADBEEF;
    msg.data = {0x11, 0x22, 0x33};

    assertTrue(channel.send(msg), "Send should succeed");

    CommMessage received;
    assertTrue(channel.receive(received, 100), "Receive should succeed");

    assertEqual(received.addr, msg.addr, "Address field mismatch");
    assertEqual(received.size, msg.size, "Size field mismatch");
    assertEqual(received.timestamp, msg.timestamp, "Timestamp field mismatch");
    assertEqual(received.request_id, msg.request_id, "Request ID field mismatch");
    assertEqual(received.data.size(), msg.data.size(), "Data size mismatch");

    return true;
}

//=============================================================================
// Main
//=============================================================================

int main() {
    printTestHeader("PIMID Host-Device Communication Tests");

    int total_failures = 0;

    // Basic Communication Tests
    {
        TestSuite suite("Basic Communication");
        suite.run("Send/Receive", test_message_send_receive);
        suite.run("Queue Ordering", test_message_queue_ordering);
        suite.run("Receive Timeout", test_receive_timeout);
        suite.run("Channel Close", test_channel_close);
        total_failures += suite.summarize();
    }

    // Message Type Tests
    {
        TestSuite suite("Message Types");
        suite.run("Memory Request", test_memory_request_message);
        suite.run("Memory Response", test_memory_response_message);
        suite.run("Sync Message", test_sync_message);
        suite.run("Offload Message", test_offload_message);
        total_failures += suite.summarize();
    }

    // Synchronization Tests
    {
        TestSuite suite("Synchronization");
        suite.run("Bidirectional", test_bidirectional_communication);
        suite.run("Concurrent Messages", test_concurrent_messages);
        suite.run("Message Throughput", test_message_throughput);
        total_failures += suite.summarize();
    }

    // Error Handling Tests
    {
        TestSuite suite("Error Handling");
        suite.run("Empty Queue Receive", test_empty_queue_receive);
        suite.run("Channel Reset", test_channel_reset);
        total_failures += suite.summarize();
    }

    // Data Integrity Tests
    {
        TestSuite suite("Data Integrity");
        suite.run("Large Data Transfer", test_large_data_transfer);
        suite.run("All Message Fields", test_message_with_all_fields);
        total_failures += suite.summarize();
    }

    // Final Summary
    printSeparator();
    if (total_failures == 0) {
        std::cout << "\n\033[32m✓ All tests passed!\033[0m\n" << std::endl;
    } else {
        std::cout << "\n\033[31m✗ " << total_failures << " test(s) failed\033[0m\n" << std::endl;
    }

    return total_failures > 0 ? 1 : 0;
}
