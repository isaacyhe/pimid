/**
 * @file clocked_object.hh
 * @brief Standalone ClockedObject for Garnet extraction
 *
 * This provides a simplified event-driven simulation infrastructure
 * that replaces gem5's event system.
 */

#ifndef __GARNET_COMPAT_SIM_CLOCKED_OBJECT_HH__
#define __GARNET_COMPAT_SIM_CLOCKED_OBJECT_HH__

#include <algorithm>
#include <functional>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/types.hh"

namespace gem5 {

// Forward declarations
class ClockedObject;
class EventQueue;

// Global current tick
inline Tick g_curTick = 0;
inline Tick curTick() { return g_curTick; }
inline void setCurTick(Tick t) { g_curTick = t; }

/**
 * Event class for scheduling callbacks
 */
class Event {
public:
    enum Priority {
        Minimum_Pri = -128,
        Debug_Enable_Pri = -110,
        Debug_Break_Pri = -100,
        CPU_Switch_Pri = -31,
        Delayed_Writeback_Pri = -1,
        Default_Pri = 0,
        DVFS_Update_Pri = 31,
        Serialize_Pri = 32,
        CPU_Tick_Pri = 50,
        Stat_Event_Pri = 90,
        Progress_Event_Pri = 95,
        Sim_Exit_Pri = 100,
        Maximum_Pri = 127
    };

    Event(Priority p = Default_Pri) : priority(p), when(0), scheduled(false) {}
    virtual ~Event() = default;

    virtual void process() = 0;

    Priority getPriority() const { return priority; }
    Tick when_scheduled() const { return when; }
    bool isScheduled() const { return scheduled; }

    void setWhen(Tick w) { when = w; }
    void setScheduled(bool s) { scheduled = s; }

private:
    Priority priority;
    Tick when;
    bool scheduled;
};

/**
 * Wrapper for function-based events
 */
class EventFunctionWrapper : public Event {
public:
    EventFunctionWrapper(std::function<void()> callback,
                         const std::string& name = "",
                         bool del = false,
                         Priority p = Default_Pri)
        : Event(p), callback_(callback), name_(name), autoDelete_(del) {}

    void process() override {
        if (callback_) callback_();
    }

    const std::string& name() const { return name_; }

private:
    std::function<void()> callback_;
    std::string name_;
    bool autoDelete_;
};

/**
 * Event comparison for priority queue
 */
struct EventCompare {
    bool operator()(const Event* a, const Event* b) const {
        if (a->when_scheduled() != b->when_scheduled())
            return a->when_scheduled() > b->when_scheduled();
        return a->getPriority() > b->getPriority();
    }
};

/**
 * Global event queue
 */
class EventQueue {
public:
    static EventQueue& instance() {
        static EventQueue eq;
        return eq;
    }

    void schedule(Event* e, Tick when) {
        e->setWhen(when);
        e->setScheduled(true);
        events_.push(e);
    }

    void deschedule(Event* e) {
        e->setScheduled(false);
        // Note: actual removal from queue happens during processing
    }

    void reschedule(Event* e, Tick when) {
        e->setScheduled(false);
        schedule(e, when);
    }

    bool empty() const { return events_.empty(); }

    Tick nextEventTime() const {
        while (!events_.empty()) {
            if (events_.top()->isScheduled())
                return events_.top()->when_scheduled();
            // Skip cancelled events
            auto* e = const_cast<EventQueue*>(this)->events_.top();
            const_cast<EventQueue*>(this)->events_.pop();
        }
        return MaxTick;
    }

    void processEvents(Tick until) {
        while (!events_.empty()) {
            Event* e = events_.top();

            // Skip cancelled events
            if (!e->isScheduled()) {
                events_.pop();
                continue;
            }

            if (e->when_scheduled() > until)
                break;

            events_.pop();
            g_curTick = e->when_scheduled();
            e->setScheduled(false);
            e->process();
        }
    }

    // Process one event
    bool processOneEvent() {
        while (!events_.empty()) {
            Event* e = events_.top();
            events_.pop();

            if (!e->isScheduled())
                continue;

            g_curTick = e->when_scheduled();
            e->setScheduled(false);
            e->process();
            return true;
        }
        return false;
    }

    void clear() {
        while (!events_.empty()) events_.pop();
    }

private:
    EventQueue() = default;
    std::priority_queue<Event*, std::vector<Event*>, EventCompare> events_;
};

// Global event queue access
inline EventQueue& mainEventQueue() { return EventQueue::instance(); }

// Schedule/deschedule functions
inline void schedule(Event& e, Tick when) {
    EventQueue::instance().schedule(&e, when);
}

inline void deschedule(Event& e) {
    EventQueue::instance().deschedule(&e);
}

inline void reschedule(Event& e, Tick when) {
    EventQueue::instance().reschedule(&e, when);
}

/**
 * ClockedObject base class
 *
 * Provides timing infrastructure for simulation objects.
 */
class ClockedObject {
public:
    struct Params {
        std::string name;
        Tick clock_period = 1000; // 1ns default (in ps)
    };

    explicit ClockedObject(const Params& p)
        : params_(p), clockPeriod_(p.clock_period), currentCycle_(0) {}

    virtual ~ClockedObject() = default;

    // Clock period in ticks (picoseconds)
    Tick clockPeriod() const { return clockPeriod_; }

    // Set clock period
    void setClockPeriod(Tick period) { clockPeriod_ = period; }

    // Clock frequency in GHz
    double clockFrequency() const {
        return 1e12 / static_cast<double>(clockPeriod_);
    }

    // Set frequency in GHz
    void setClockFrequency(double freq_ghz) {
        clockPeriod_ = static_cast<Tick>(1e12 / freq_ghz);
    }

    // Convert cycles to ticks
    Tick cyclesToTicks(Cycles c) const {
        return static_cast<Tick>(c) * clockPeriod_;
    }

    // Convert ticks to cycles
    Cycles ticksToCycles(Tick t) const {
        return Cycles(t / clockPeriod_);
    }

    // Get current cycle
    Cycles curCycle() const {
        return ticksToCycles(curTick());
    }

    // Get next clock edge
    Tick nextCycle() const {
        Tick t = curTick();
        return t + clockPeriod_ - (t % clockPeriod_);
    }

    // Clock edge after n cycles
    Tick clockEdge(Cycles c = Cycles(0)) const {
        return nextCycle() + cyclesToTicks(c);
    }

    // Name accessor
    const std::string& name() const { return params_.name; }

    // Virtual initialization
    virtual void init() {}
    virtual void startup() {}

protected:
    Params params_;
    Tick clockPeriod_;
    Cycles currentCycle_;
};

// Type alias for compatibility
using SimObject = ClockedObject;

} // namespace gem5

#endif // __GARNET_COMPAT_SIM_CLOCKED_OBJECT_HH__
