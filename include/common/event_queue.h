#ifndef PIMID_EVENT_QUEUE_H
#define PIMID_EVENT_QUEUE_H

#include "common/types.h"
#include <queue>
#include <functional>
#include <memory>

namespace pimid {

/**
 * Event types
 */
enum class EventType {
    MEMORY_RESPONSE,
    NETWORK_ARRIVAL,
    OFFLOAD_COMPLETE,
    SYNCHRONIZATION,
    CUSTOM
};

/**
 * Simulation event
 */
struct Event {
    EventType type;
    Cycle scheduled_cycle;
    uint32_t priority;
    std::function<void()> callback;

    // Event data
    std::shared_ptr<void> data;

    Event(EventType t, Cycle c, uint32_t p, std::function<void()> cb)
        : type(t), scheduled_cycle(c), priority(p), callback(cb), data(nullptr) {}

    // Comparison for priority queue (earlier cycles have higher priority)
    bool operator<(const Event& other) const {
        if (scheduled_cycle == other.scheduled_cycle) {
            return priority < other.priority;
        }
        return scheduled_cycle > other.scheduled_cycle;
    }
};

/**
 * Event queue for discrete event simulation
 * Manages timing and event ordering across the simulation
 */
class EventQueue {
public:
    EventQueue();

    // Event management
    void scheduleEvent(EventType type, Cycle cycle, uint32_t priority,
                       std::function<void()> callback);
    void scheduleEvent(const Event& event);

    // Process events
    void processEvents(Cycle until_cycle);
    void processNextEvent();
    bool hasEvents() const;

    // Timing queries
    Cycle getNextEventCycle() const;
    Cycle getCurrentCycle() const { return current_cycle_; }
    void setCurrentCycle(Cycle cycle) { current_cycle_ = cycle; }

    // Statistics
    uint64_t getTotalEventsProcessed() const { return total_events_; }
    uint64_t getPendingEventCount() const { return events_.size(); }

    // Clear all events
    void clear();

private:
    std::priority_queue<Event> events_;
    Cycle current_cycle_;
    uint64_t total_events_;
};

} // namespace pimid

#endif // PIMID_EVENT_QUEUE_H
