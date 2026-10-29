#pragma once
#include "LedGaugePolicy.h"
#include <algorithm>
#include <cstring>

namespace guition_knob {
struct LedPreferences {
    bool enabled = false;
    int brightness = 45; // User-facing percent; independent of screen brightness.
};
inline bool operator==(const LedPreferences &a, const LedPreferences &b) {
    return a.enabled == b.enabled && a.brightness == b.brightness;
}
inline bool validLedPercent(const char *text) {
    if (!text || !*text || std::strlen(text) > 3) return false;
    int value = 0;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        value = value * 10 + (*p - '0');
    }
    return value <= 100;
}
inline bool validLedEnabled(const char *text) {
    return text && (!std::strcmp(text, "0") || !std::strcmp(text, "1"));
}
inline LedConfig savedLedConfig(const LedPreferences &preferences) {
    LedConfig config;
    config.mode = preferences.enabled ? LedMode::Ui : LedMode::Off;
    config.brightness = (std::clamp(preferences.brightness, 0, 100) * 255 + 50) / 100;
    return config;
}
// Apply on first readiness, preference changes, or an explicit web save.
// Temporary diagnostic overrides survive ordinary refreshes but not a save/reboot.
class LedPreferenceApplier {
  public:
    template <typename Apply> bool update(const LedPreferences &value, Apply apply, bool force = false) {
        if (force) pending = true;
        if (!pending && value == applied) return false;
        if (!apply(savedLedConfig(value))) { pending = true; return false; }
        applied = value; pending = false; return true;
    }
  private:
    bool pending = true;
    LedPreferences applied;
};
} // namespace guition_knob
