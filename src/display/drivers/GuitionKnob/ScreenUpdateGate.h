#pragma once
#include <atomic>
#include <mutex>

namespace guition_knob {
// Reserve an entire user action against OTA entry without holding the process
// mutex across plugin callbacks, network relay calls or UI events.
class ActionReservation {
  public:
    ActionReservation(std::recursive_mutex &mutex, unsigned &count, const std::atomic<bool> &updating)
        : mutex(mutex), count(count) {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        entered = !updating.load(); if (entered) ++count;
    }
    ~ActionReservation() {
        if (entered) { std::lock_guard<std::recursive_mutex> lock(mutex); --count; }
    }
    explicit operator bool() const { return entered; }
    ActionReservation(const ActionReservation &) = delete;
    ActionReservation &operator=(const ActionReservation &) = delete;
  private:
    std::recursive_mutex &mutex;
    unsigned &count;
    bool entered = false;
};
} // namespace guition_knob
