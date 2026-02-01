/**
 * @file Consumer.hh
 * @brief Consumer interface for event-driven simulation
 *
 * This is the base class for all objects that can receive wakeup events.
 */

#ifndef __GARNET_COMPAT_MEM_RUBY_COMMON_CONSUMER_HH__
#define __GARNET_COMPAT_MEM_RUBY_COMMON_CONSUMER_HH__

#include <iostream>
#include <set>
#include <functional>

#include "sim/clocked_object.hh"
#include "base/types.hh"

namespace gem5 {
namespace ruby {

/**
 * Consumer - base class for objects that handle scheduled events
 */
class Consumer {
public:
    Consumer(ClockedObject* clocked_obj,
             Event::Priority ev_prio = Event::Default_Pri)
        : em_(clocked_obj),
          wakeupEvent_([this]() { this->processCurrentEvent(); },
                       "ConsumerWakeup", false, ev_prio) {}

    virtual ~Consumer() = default;

    // Pure virtual - called when the consumer should process
    virtual void wakeup() = 0;

    // Print for debugging
    virtual void print(std::ostream& out) const = 0;

    // Store event info (optional)
    virtual void storeEventInfo(int info) {}

    // Check if already scheduled at this time
    bool alreadyScheduled(Tick time) {
        return wakeupTicks_.find(time) != wakeupTicks_.end();
    }

    // Get the associated clocked object
    ClockedObject* getObject() { return em_; }

    // Schedule event at absolute time
    void scheduleEventAbsolute(Tick timeAbs) {
        if (wakeupTicks_.find(timeAbs) != wakeupTicks_.end())
            return; // Already scheduled

        wakeupTicks_.insert(timeAbs);
        scheduleNextWakeup();
    }

    // Schedule event relative to current time in cycles
    void scheduleEvent(Cycles timeDelta) {
        Tick when = em_->clockEdge(timeDelta);
        scheduleEventAbsolute(when);
    }

private:
    void scheduleNextWakeup() {
        if (wakeupTicks_.empty())
            return;

        Tick nextTime = *wakeupTicks_.begin();

        if (!wakeupEvent_.isScheduled()) {
            gem5::schedule(wakeupEvent_, nextTime);
        } else if (nextTime < wakeupEvent_.when_scheduled()) {
            gem5::deschedule(wakeupEvent_);
            gem5::schedule(wakeupEvent_, nextTime);
        }
    }

    void processCurrentEvent() {
        Tick now = curTick();

        // Remove this time from pending wakeups
        wakeupTicks_.erase(now);

        // Call the actual wakeup handler
        wakeup();

        // Schedule next wakeup if needed
        scheduleNextWakeup();
    }

    std::set<Tick> wakeupTicks_;
    EventFunctionWrapper wakeupEvent_;
    ClockedObject* em_;
};

inline std::ostream& operator<<(std::ostream& out, const Consumer& obj) {
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_MEM_RUBY_COMMON_CONSUMER_HH__
