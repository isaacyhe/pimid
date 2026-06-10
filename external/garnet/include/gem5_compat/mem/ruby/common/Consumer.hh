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

    // Diagnostic: scheduling state of this Consumer.
    bool diagHasWakeup() const { return wakeupEvent_.isScheduled(); }
    Tick diagNextWakeup() const {
        return wakeupEvent_.isScheduled() ? wakeupEvent_.when_scheduled() : 0;
    }
    size_t diagPendingTicks() const { return wakeupTicks_.size(); }

    // Get the associated clocked object
    ClockedObject* getObject() { return em_; }

    // Clear all pending wakeup schedules.
    // Must be called when resetting network state to prevent stale
    // wakeupTicks_ entries from causing event dedup in scheduleEventAbsolute.
    void clearSchedule() {
        wakeupTicks_.clear();
        if (wakeupEvent_.isScheduled()) {
            gem5::deschedule(wakeupEvent_);
        }
    }

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

        // Remove this time -- AND any stale past entries -- from pending wakeups.
        // The synthetic-traffic harness drives a NON-MONOTONIC clock (it
        // hand-increments curTick when the event queue empties, and processOneEvent
        // can then snap curTick backward to a clock-aligned event a tick or two
        // earlier). Under that clock a wakeup scheduled for tick T may fire when
        // curTick() != T, so erasing only the exact `now` leaves T orphaned in
        // wakeupTicks_ forever; the dedup guard in scheduleEventAbsolute() then
        // silently refuses to ever re-arm this Consumer for T -- a LOST WAKEUP.
        // On a credit link that strands the credit return: the destination NI's
        // ext-link credit sticks at 0 and the up/down DRAM tree HARD DEADLOCKS
        // (dump: packets frozen at "out Local, freeVCinCls=1 credit=0"). Purging
        // every entry <= now guarantees no past/present tick stays stranded, so a
        // later scheduleEventAbsolute() is always honored.
        wakeupTicks_.erase(wakeupTicks_.begin(), wakeupTicks_.upper_bound(now));

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
