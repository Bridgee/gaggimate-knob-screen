#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
namespace official_release {
struct Request { std::string channel, controllerVersion; uint64_t generation = 0; };
struct Snapshot {
    std::string status = "not-checked", version, message;
    bool displayUpdate = false, controllerUpdate = false;
};
// Network work must run after take() releases this copy-only lock.
class Mailbox {
  public:
    uint64_t enqueue(const std::string &channel, const std::string &controller) {
        std::lock_guard<std::mutex> lock(mutex);
        request = {channel == "nightly" ? "nightly" : "latest", controller, ++generation};
        pending = true; state = {}; state.status = "checking"; changed = true; return generation;
    }
    bool take(Request &value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!pending) return false;
        value = request; pending = false; return true;
    }
    bool publish(uint64_t current, const Snapshot &value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (current != generation) return false;
        state = value; changed = true; return true;
    }
    Snapshot snapshot() const { std::lock_guard<std::mutex> lock(mutex); return state; }
    bool takeChanged() { return changed.exchange(false); }
  private:
    mutable std::mutex mutex;
    Request request; Snapshot state;
    uint64_t generation = 0;
    bool pending = false;
    std::atomic<bool> changed{false};
};
}
