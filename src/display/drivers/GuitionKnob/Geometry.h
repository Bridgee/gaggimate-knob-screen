#pragma once

#include <stdint.h>

namespace guition_knob {
constexpr uint16_t PanelSize = 360;
inline bool decodeTouch(const uint8_t (&data)[5], uint16_t &x, uint16_t &y) {
    // CST816 register 0x02: finger count, X high/low, Y high/low.
    if (data[0] != 1)
        return false;
    x = ((data[1] & 0x0f) << 8) | data[2];
    y = ((data[3] & 0x0f) << 8) | data[4];
    return x < PanelSize && y < PanelSize;
}

constexpr uint16_t backlightDuty(int level) {
    // Give the lowest visible step a little more dimming on the K718.
    // Keep off, maximum and all other saved levels unchanged.
    return level <= 0 ? 0 : level == 1 ? 48 : level >= 16 ? 1023 : (level * 1023 + 8) / 16;
}
} // namespace guition_knob
