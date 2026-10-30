#pragma once
#include <cstdint>

namespace guition_knob {
struct BootHealthSample {
    bool setup = false, storage = false, settings = false, panel = false, touch = false, frame = false;
    uint32_t mainAt = 0, uiAt = 0, logicAt = 0;
    uint32_t mainCount = 0, uiCount = 0, logicCount = 0;
};
class BootHealthWindow {
  public:
    static constexpr uint32_t StableMs = 20000, DeadlineMs = 120000, FreshMs = 2500;
    bool observe(uint32_t now, const BootHealthSample &s) {
        const bool ready = s.setup && s.storage && s.settings && s.panel && s.touch && s.frame &&
            s.mainCount >= 3 && s.uiCount >= 3 && s.logicCount >= 3 &&
            now - s.mainAt < FreshMs && now - s.uiAt < FreshMs && now - s.logicAt < FreshMs;
        if (!ready) { tracking = false; return false; }
        if (!tracking) { tracking = true; since = now; }
        return now - since >= StableMs;
    }
  private:
    bool tracking = false;
    uint32_t since = 0;
};
} // namespace guition_knob
