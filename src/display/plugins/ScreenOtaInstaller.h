#pragma once
#ifdef GAGGIMATE_KNOB
#include "ScreenOtaPolicy.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace screen_ota {
struct InstallJob { Plan plan; bool verifyOnly = false; };
struct InstallStatus {
    std::string phase = "idle", error, tag, verifiedSha;
    unsigned progress = 0;
    bool busy = false, verified = false;
};
class Installer {
  public:
    explicit Installer(bool qualificationPermission) : qualificationPermission(qualificationPermission) {}
    bool queue(const Preflight &, const Request &);
    bool takeQueued(InstallJob &);
    void start(const InstallJob &, std::function<bool()> guard);
    void fail(const char *, bool holdForRecovery = false);
    bool takeChanged() { return changed.exchange(false); }
    InstallStatus snapshot() const { std::lock_guard<std::mutex> lock(mutex); return status; }
  private:
    void run(const InstallJob &, const std::function<bool()> &);
    void report(const char *, unsigned);
    const bool qualificationPermission;
    mutable std::mutex mutex;
    InstallStatus status;
    InstallJob job;
    bool queued = false;
    std::atomic<bool> changed{false};
};
} // namespace screen_ota
#endif
