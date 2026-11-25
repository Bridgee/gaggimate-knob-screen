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
    ControllerTarget target;
    uint32_t validatedAt = 0;
};
class Discovery {
  public:
    explicit Discovery(bool previews) : previews(previews) {}
    void setTarget(ControllerTarget target) {
        std::lock_guard<std::mutex> lock(mutex);
        liveTarget = std::move(target);
    }
    bool request();
    bool takeChanged() { return changed.exchange(false); }
    Snapshot snapshot() const {
        std::lock_guard<std::mutex> lock(mutex);
        if (state.target == liveTarget) return state;
        Snapshot pending;
        pending.target = liveTarget;
        if (liveTarget.protocol && version(liveTarget.version)) {
            pending.status = "checking";
        } else {
            pending.status = "waiting-controller";
            pending.message = "Waiting for the connected controller to report its version and protocol.";
        }
        return pending;
    }
  private:
    Snapshot check(const ControllerTarget &target);
    void publish(const Snapshot &value) { std::lock_guard<std::mutex> lock(mutex); state = value; changed = true; }
    bool previews;
    mutable std::mutex mutex;
    Snapshot state;
    ControllerTarget liveTarget;
    std::atomic<bool> busy{false}, changed{false};
};
} // namespace matched_release
#endif
