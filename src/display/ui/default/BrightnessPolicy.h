#pragma once
#include <stdint.h>

namespace gaggimate_ui {
constexpr int boundedBrightness(int value) { return value < 0 ? 0 : value > 16 ? 16 : value; }

// Evaluate on the UI task each loop, independently of telemetry redraws.
// Saving a setting updates the brightness of the CURRENT screen state; the
// standby delay still counts from standby entry, not from the settings save.
constexpr int brightnessForState(bool standby, uint32_t now, uint32_t standbyEnteredAt,
                                 int timeoutMs, int mainBrightness, int standbyBrightness) {
    const uint32_t delay = timeoutMs > 0 ? static_cast<uint32_t>(timeoutMs) : 0;
    const bool dim = standby && standbyEnteredAt != 0 &&
                     static_cast<uint32_t>(now - standbyEnteredAt) >= delay;
    return boundedBrightness(dim ? standbyBrightness : mainBrightness);
}
}
