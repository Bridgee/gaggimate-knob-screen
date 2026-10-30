#pragma once
#include <array>
#include <functional>
#include <mutex>
#include <utility>

namespace network_work {
enum class Kind : unsigned { Screen, Matched, Official, Count };
// Coalesce repeated discovery requests, retaining a separate pending screen job.
// Callbacks execute after take() releases the lock; HTTPS never holds this mutex.
class Pending {
  public:
    bool put(Kind kind, std::function<void()> work) {
        const auto i = static_cast<unsigned>(kind);
        if (i >= jobs.size() || !work) return false;
        std::lock_guard<std::mutex> lock(mutex); jobs[i] = std::move(work); return true;
    }
    std::function<void()> take() {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto &work : jobs) if (work) return std::exchange(work, {});
        return {};
    }
  private:
    std::mutex mutex;
    std::array<std::function<void()>, static_cast<unsigned>(Kind::Count)> jobs;
};
} // namespace network_work
