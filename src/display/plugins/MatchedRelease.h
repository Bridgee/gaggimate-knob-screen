#pragma once
#ifdef GAGGIMATE_KNOB
#include "MatchedReleasePolicy.h"
#include <atomic>
#include <mutex>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace matched_release {
struct Snapshot {
    std::string status = "not-checked", message;
    Release release;
    Pair pair;
    uint32_t validatedAt = 0;
};
class Discovery {
  public:
    explicit Discovery(bool previews) : previews(previews) {}
    void request();
    bool takeChanged() { return changed.exchange(false); }
    Snapshot snapshot() const { std::lock_guard<std::mutex> lock(mutex); return state; }
  private:
    Snapshot check();
    void publish(const Snapshot &value) { std::lock_guard<std::mutex> lock(mutex); state = value; changed = true; }
    bool previews;
    mutable std::mutex mutex;
    Snapshot state;
    std::atomic<bool> busy{false}, changed{false};
};
} // namespace matched_release
#endif
