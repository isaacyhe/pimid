#include "common/event_queue.h"
#include <iostream>
#include <limits>

namespace pimid {

//=============================================================================
// EventQueue Implementation (Discrete Event Simulation)
//=============================================================================

EventQueue::EventQueue()
    : current_cycle_(0), total_events_(0) {
    std::cout << "EventQueue initialized for discrete event simulation" << std::endl;
}

void EventQueue::scheduleEvent(EventType type, Cycle cycle, uint32_t priority,
                               std::function<void()> callback) {
    if (!callback) {
        std::cerr << "ERROR: Cannot schedule event with null callback" << std::endl;
        return;
    }

    if (cycle < current_cycle_) {
        std::cerr << "WARNING: Scheduling event in the past (cycle " << cycle
                  << ", current " << current_cycle_ << ")" << std::endl;
        // Reschedule for next cycle instead
        cycle = current_cycle_ + 1;
    }

    Event event(type, cycle, priority, callback);
    events_.push(event);
}

void EventQueue::scheduleEvent(const Event& event) {
    if (!event.callback) {
        std::cerr << "ERROR: Cannot schedule event with null callback" << std::endl;
        return;
    }

    Cycle cycle = event.scheduled_cycle;
    if (cycle < current_cycle_) {
        std::cerr << "WARNING: Scheduling event in the past (cycle " << cycle
                  << ", current " << current_cycle_ << ")" << std::endl;
        // Create modified event for next cycle
        Event modified_event = event;
        modified_event.scheduled_cycle = current_cycle_ + 1;
        events_.push(modified_event);
    } else {
        events_.push(event);
    }
}

void EventQueue::processEvents(Cycle until_cycle) {
    while (!events_.empty()) {
        const Event& next_event = events_.top();

        // Stop if next event is beyond until_cycle
        if (next_event.scheduled_cycle > until_cycle) {
            break;
        }

        // Advance simulation time to event's cycle
        current_cycle_ = next_event.scheduled_cycle;

        // Execute event callback
        try {
            if (next_event.callback) {
                next_event.callback();
            }
        } catch (const std::exception& e) {
            std::cerr << "ERROR: Exception during event processing at cycle "
                      << current_cycle_ << ": " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "ERROR: Unknown exception during event processing at cycle "
                      << current_cycle_ << std::endl;
        }

        // Remove processed event
        events_.pop();
        total_events_++;
    }

    // Advance to until_cycle even if no events
    if (current_cycle_ < until_cycle) {
        current_cycle_ = until_cycle;
    }
}

void EventQueue::processNextEvent() {
    if (events_.empty()) {
        std::cerr << "WARNING: processNextEvent called with empty queue" << std::endl;
        return;
    }

    const Event& next_event = events_.top();

    // Advance simulation time to event's cycle
    current_cycle_ = next_event.scheduled_cycle;

    // Execute event callback
    try {
        if (next_event.callback) {
            next_event.callback();
        }
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Exception during event processing at cycle "
                  << current_cycle_ << ": " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "ERROR: Unknown exception during event processing at cycle "
                  << current_cycle_ << std::endl;
    }

    // Remove processed event
    events_.pop();
    total_events_++;
}

bool EventQueue::hasEvents() const {
    return !events_.empty();
}

Cycle EventQueue::getNextEventCycle() const {
    if (events_.empty()) {
        return std::numeric_limits<Cycle>::max();
    }
    return events_.top().scheduled_cycle;
}

void EventQueue::clear() {
    // Clear the priority queue by swapping with empty queue
    std::priority_queue<Event> empty_queue;
    events_.swap(empty_queue);

    // Reset cycle counter (but preserve total_events_ for statistics)
    current_cycle_ = 0;

    std::cout << "EventQueue cleared (total events processed: "
              << total_events_ << ")" << std::endl;
}

} // namespace pimid
