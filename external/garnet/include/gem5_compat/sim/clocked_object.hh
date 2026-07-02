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

// A SINGLE shared gem5 context (clock + event queue) drives the NoC. The
// detailed NoC is one shared Garnet network stepped consistently by all
// threads/ranks; there is no per-thread isolated context.
inline Tick g_sharedTick = 0;
inline Tick& curTickRef() { return g_sharedTick; }
inline Tick curTick() { return curTickRef(); }
inline void setCurTick(Tick t) { curTickRef() = t; }

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
    // Queue entry with an IMMUTABLE snapshot of (when, priority) so the heap key
    // never changes underneath the heap. seq gives FIFO order among equal keys.
    struct QEntry {
        Tick     when;
        int      pri;
        uint64_t seq;
        Event*   e;
    };
    struct QEntryCompare {
        bool operator()(const QEntry& a, const QEntry& b) const {
            if (a.when != b.when) return a.when > b.when;  // earliest first
            if (a.pri  != b.pri)  return a.pri  > b.pri;   // lower pri value first
            return a.seq > b.seq;                          // FIFO tie-break
        }
    };

public:
    static EventQueue& instance() {
        static EventQueue sharedQ;            // one shared event queue for the NoC
        return sharedQ;
    }

    void schedule(Event* e, Tick when) {
        e->setWhen(when);
        e->setScheduled(true);
        // Snapshot (when, priority) INTO the queue entry. The heap must order on
        // an IMMUTABLE key: storing bare Event* and ordering on the live
        // Event::when corrupts the heap whenever a later schedule()/reschedule()
        // mutates when on an Event that still has copies in the heap (a
        // changed-key-in-heap bug) -> buried events that never fire -> the NI
        // credit-return deadlock. With a snapshot key the heap is always valid;
        // superseded entries are detected lazily on pop (when != live when).
        events_.push(QEntry{when, (int)e->getPriority(), seq_++, e});
    }

    void deschedule(Event* e) {
        e->setScheduled(false);
        // Lazy: the stale heap entry is skipped on pop (isScheduled()==false).
    }

    void reschedule(Event* e, Tick when) {
        e->setScheduled(false);
        schedule(e, when);
    }

    // A heap entry is LIVE iff its Event is still scheduled AND its snapshot
    // tick still matches the Event's current when (i.e. it was not superseded by
    // a later reschedule to a different tick).
    static bool entryLive(const QEntry& q) {
        return q.e->isScheduled() && q.e->when_scheduled() == q.when;
    }

    bool empty() const {
        auto& q = const_cast<EventQueue*>(this)->events_;
        while (!q.empty() && !entryLive(q.top())) q.pop();
        return q.empty();
    }

    Tick nextEventTime() const {
        auto& q = const_cast<EventQueue*>(this)->events_;
        while (!q.empty()) {
            if (entryLive(q.top())) return q.top().when;
            q.pop();
        }
        return MaxTick;
    }

    void processEvents(Tick until) {
        while (!events_.empty()) {
            QEntry top = events_.top();
            if (!entryLive(top)) { events_.pop(); continue; }
            if (top.when > until) break;
            events_.pop();
            curTickRef() = top.when;
            top.e->setScheduled(false);
            top.e->process();
        }
    }

    // Process one event
    bool processOneEvent() {
        while (!events_.empty()) {
            QEntry top = events_.top();
            events_.pop();
            if (!entryLive(top)) continue;   // descheduled or superseded
            curTickRef() = top.when;
            top.e->setScheduled(false);
            top.e->process();
            return true;
        }
        return false;
    }

    void clear() {
        while (!events_.empty()) events_.pop();
    }

private:
    EventQueue() = default;
    std::priority_queue<QEntry, std::vector<QEntry>, QEntryCompare> events_;
    uint64_t seq_ = 0;
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
        Tick clock_period = 1; // 1 tick = 1 cycle for standalone Garnet
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

    // Clock edge after n cycles from current cycle
    Tick clockEdge(Cycles c = Cycles(0)) const {
        Tick t = curTick();
        Tick aligned = (t / clockPeriod_) * clockPeriod_;
        return aligned + static_cast<Tick>(c) * clockPeriod_;
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
