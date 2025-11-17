/**
 * @file test_event_queue.cpp
 * @brief Comprehensive tests for EventQueue implementation
 */

#include "common/event_queue.h"
#include <iostream>
#include <vector>
#include <cassert>

using namespace pimid;

struct TestResult {
    std::string test_name;
    bool passed;
    std::string message;
};

std::vector<TestResult> test_results;

void reportTest(const std::string& name, bool passed, const std::string& msg = "") {
    test_results.push_back({name, passed, msg});
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << name;
    if (!msg.empty()) {
        std::cout << ": " << msg;
    }
    std::cout << std::endl;
}

//=============================================================================
// Event Queue Tests
//=============================================================================

void testEventQueueConstruction() {
    std::cout << "\n=== Testing Event Queue Construction ===" << std::endl;

    EventQueue eq;

    reportTest("EventQueue: Construction", true,
               "Event queue created successfully");

    reportTest("EventQueue: Initial State Empty", !eq.hasEvents(),
               "Queue should be empty initially");

    reportTest("EventQueue: Initial Cycle Zero", eq.getCurrentCycle() == 0,
               "Current cycle should be 0");
}

void testEventScheduling() {
    std::cout << "\n=== Testing Event Scheduling ===" << std::endl;

    EventQueue eq;
    int callback_count = 0;

    // Schedule event at cycle 10
    eq.scheduleEvent(EventType::MEMORY_RESPONSE, 10, 0, [&callback_count]() {
        callback_count++;
    });

    reportTest("EventQueue: Has Events After Schedule", eq.hasEvents(),
               "Queue should have events after scheduling");

    reportTest("EventQueue: Next Event Cycle", eq.getNextEventCycle() == 10,
               "Next event should be at cycle 10");

    // Process events
    eq.processEvents(10);

    reportTest("EventQueue: Callback Executed", callback_count == 1,
               "Callback should execute once");

    reportTest("EventQueue: Current Cycle Advanced", eq.getCurrentCycle() == 10,
               "Current cycle should be 10");
}

void testChronologicalOrdering() {
    std::cout << "\n=== Testing Chronological Event Ordering ===" << std::endl;

    EventQueue eq;
    std::vector<Cycle> execution_order;

    // Schedule events out of order
    eq.scheduleEvent(EventType::MEMORY_RESPONSE, 30, 0, [&execution_order]() {
        execution_order.push_back(30);
    });

    eq.scheduleEvent(EventType::NETWORK_ARRIVAL, 10, 0, [&execution_order]() {
        execution_order.push_back(10);
    });

    eq.scheduleEvent(EventType::OFFLOAD_COMPLETE, 20, 0, [&execution_order]() {
        execution_order.push_back(20);
    });

    // Process all events
    eq.processEvents(100);

    bool correct_order = (execution_order.size() == 3) &&
                         (execution_order[0] == 10) &&
                         (execution_order[1] == 20) &&
                         (execution_order[2] == 30);

    reportTest("EventQueue: Chronological Ordering", correct_order,
               "Events should execute in order: 10, 20, 30");
}

void testPriorityHandling() {
    std::cout << "\n=== Testing Priority Handling ===" << std::endl;

    EventQueue eq;
    std::vector<uint32_t> priority_order;

    // Schedule events at same cycle with different priorities
    eq.scheduleEvent(EventType::MEMORY_RESPONSE, 10, 1, [&priority_order]() {
        priority_order.push_back(1);
    });

    eq.scheduleEvent(EventType::NETWORK_ARRIVAL, 10, 3, [&priority_order]() {
        priority_order.push_back(3);
    });

    eq.scheduleEvent(EventType::OFFLOAD_COMPLETE, 10, 2, [&priority_order]() {
        priority_order.push_back(2);
    });

    // Process events
    eq.processEvents(10);

    // Higher priority (3) should execute first
    bool correct_priority = (priority_order.size() == 3) &&
                            (priority_order[0] == 3) &&
                            (priority_order[1] == 2) &&
                            (priority_order[2] == 1);

    reportTest("EventQueue: Priority Ordering", correct_priority,
               "Higher priority events should execute first");
}

void testPartialProcessing() {
    std::cout << "\n=== Testing Partial Event Processing ===" << std::endl;

    EventQueue eq;
    int executed = 0;

    // Schedule events at different cycles
    eq.scheduleEvent(EventType::MEMORY_RESPONSE, 10, 0, [&executed]() {
        executed++;
    });

    eq.scheduleEvent(EventType::NETWORK_ARRIVAL, 20, 0, [&executed]() {
        executed++;
    });

    eq.scheduleEvent(EventType::OFFLOAD_COMPLETE, 30, 0, [&executed]() {
        executed++;
    });

    // Process only until cycle 20
    eq.processEvents(20);

    reportTest("EventQueue: Partial Processing", executed == 2,
               "Should execute 2 events by cycle 20 (executed " + std::to_string(executed) + ")");

    reportTest("EventQueue: Remaining Events", eq.hasEvents(),
               "Should have 1 event remaining");

    reportTest("EventQueue: Next Event at 30", eq.getNextEventCycle() == 30,
               "Next event should be at cycle 30");
}

void testProcessNextEvent() {
    std::cout << "\n=== Testing Process Next Event ===" << std::endl;

    EventQueue eq;
    int count = 0;

    // Schedule multiple events
    for (int i = 1; i <= 5; i++) {
        eq.scheduleEvent(EventType::MEMORY_RESPONSE, i * 10, 0, [&count]() {
            count++;
        });
    }

    // Process one event at a time
    for (int i = 0; i < 3; i++) {
        eq.processNextEvent();
    }

    reportTest("EventQueue: Process Next Event", count == 3,
               "Should have processed 3 events (processed " + std::to_string(count) + ")");

    reportTest("EventQueue: Still Has Events", eq.hasEvents(),
               "Should still have 2 events remaining");
}

void testEventStatistics() {
    std::cout << "\n=== Testing Event Statistics ===" << std::endl;

    EventQueue eq;

    // Schedule and process multiple events
    for (int i = 1; i <= 10; i++) {
        eq.scheduleEvent(EventType::MEMORY_RESPONSE, i, 0, []() {});
    }

    eq.processEvents(10);

    reportTest("EventQueue: Total Events Processed",
               eq.getTotalEventsProcessed() == 10,
               "Should have processed 10 events (got " +
               std::to_string(eq.getTotalEventsProcessed()) + ")");

    reportTest("EventQueue: Queue Empty", !eq.hasEvents(),
               "Queue should be empty after processing all events");
}

void testPastEventProtection() {
    std::cout << "\n=== Testing Past Event Protection ===" << std::endl;

    EventQueue eq;

    // Advance to cycle 50
    eq.setCurrentCycle(50);

    int executed = 0;

    // Try to schedule event in the past (cycle 40)
    eq.scheduleEvent(EventType::MEMORY_RESPONSE, 40, 0, [&executed]() {
        executed++;
    });

    // Process events
    eq.processEvents(100);

    reportTest("EventQueue: Past Event Handling", executed == 1,
               "Past event should be rescheduled and execute");

    // Event should have been rescheduled to cycle 51 (current + 1)
    reportTest("EventQueue: Past Event Rescheduled", eq.getCurrentCycle() >= 51,
               "Past events should execute at next cycle");
}

void testExceptionHandling() {
    std::cout << "\n=== Testing Exception Handling ===" << std::endl;

    EventQueue eq;
    bool second_event_executed = false;

    // Schedule event that throws exception
    eq.scheduleEvent(EventType::MEMORY_RESPONSE, 10, 0, []() {
        throw std::runtime_error("Test exception");
    });

    // Schedule second event that should still execute
    eq.scheduleEvent(EventType::NETWORK_ARRIVAL, 20, 0, [&second_event_executed]() {
        second_event_executed = true;
    });

    // Process events (exception should be caught)
    eq.processEvents(100);

    reportTest("EventQueue: Exception Handling", second_event_executed,
               "Queue should continue after exception");
}

void testClearQueue() {
    std::cout << "\n=== Testing Queue Clear ===" << std::endl;

    EventQueue eq;

    // Schedule multiple events
    for (int i = 1; i <= 5; i++) {
        eq.scheduleEvent(EventType::MEMORY_RESPONSE, i * 10, 0, []() {});
    }

    uint64_t events_before = eq.getPendingEventCount();

    // Clear queue
    eq.clear();

    reportTest("EventQueue: Clear Removes Events", !eq.hasEvents(),
               "Queue should be empty after clear");

    reportTest("EventQueue: Clear Resets Cycle", eq.getCurrentCycle() == 0,
               "Current cycle should reset to 0");

    reportTest("EventQueue: Events Were Cleared", events_before == 5,
               "Had " + std::to_string(events_before) + " events before clear");
}

//=============================================================================
// Main Test Driver
//=============================================================================

int main() {
    std::cout << "╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  PIMID Event Queue Test Suite                         ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;

    // Run all tests
    testEventQueueConstruction();
    testEventScheduling();
    testChronologicalOrdering();
    testPriorityHandling();
    testPartialProcessing();
    testProcessNextEvent();
    testEventStatistics();
    testPastEventProtection();
    testExceptionHandling();
    testClearQueue();

    // Print summary
    std::cout << "\n╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  Test Summary                                          ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;

    int passed = 0, failed = 0;
    for (const auto& result : test_results) {
        if (result.passed) passed++;
        else failed++;
    }

    std::cout << "Total Tests:  " << test_results.size() << std::endl;
    std::cout << "Passed:       " << passed << " ✓" << std::endl;
    std::cout << "Failed:       " << failed << (failed > 0 ? " ✗" : "") << std::endl;
    std::cout << "Success Rate: " << (100.0 * passed / test_results.size()) << "%" << std::endl;

    if (failed > 0) {
        std::cout << "\nFailed Tests:" << std::endl;
        for (const auto& result : test_results) {
            if (!result.passed) {
                std::cout << "  - " << result.test_name << std::endl;
                if (!result.message.empty()) {
                    std::cout << "    " << result.message << std::endl;
                }
            }
        }
    }

    return (failed == 0) ? 0 : 1;
}
