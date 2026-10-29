#pragma once
#include <array>
#include <cstdint>
#include <cmath>

namespace guition_knob {
constexpr unsigned RingLedCount = 13;
enum class LedMode : uint8_t { Off, Ui, Identify, Red, Green, Blue, White };
struct LedConfig {
    LedMode mode = LedMode::Off;
    uint8_t topIndex = 0, brightness = 32;
    bool clockwise = true;
    uint8_t count = 12;
    uint32_t white = 0xffffff;
};
struct GaugeArc {
    int32_t minimum = 0, maximum = 160, start = 0, end = 0;
    uint16_t rotation = 0, sweep = 0;
    uint32_t color = 0;
};
struct GaugeFrame {
    std::array<GaugeArc, 3> arcs{};
    uint8_t count = 0;
    bool active = false;
};
using LedFrame = std::array<uint32_t, RingLedCount>;
inline const char *ledModeName(LedMode mode) {
    switch(mode) {
        case LedMode::Ui:return "ui"; case LedMode::Identify:return "identify";
        case LedMode::Red:return "red"; case LedMode::Green:return "green";
        case LedMode::Blue:return "blue"; case LedMode::White:return "white";
        default:return "off";
    }
}
inline uint32_t dimLed(uint32_t color, uint8_t brightness) {
    uint32_t result = 0;
    for (unsigned shift : {0u, 8u, 16u})
        result |= ((((color >> shift) & 255u) * brightness + 127u) / 255u) << shift;
    return result;
}
inline LedFrame renderGaugeLeds(const GaugeFrame &gauges, const LedConfig &config, bool fresh) {
    LedFrame frame{};
    if (!config.count || config.count > RingLedCount || config.topIndex >= config.count || config.white > 0xffffff) return frame;
    if (config.mode == LedMode::Identify) {
        frame[config.topIndex] = dimLed(config.white, config.brightness);
        return frame;
    }
    if (config.mode >= LedMode::Red && config.mode <= LedMode::White) {
        const uint32_t color = config.mode == LedMode::Red ? 0xff0000 : config.mode == LedMode::Green ? 0x00ff00 : config.mode == LedMode::Blue ? 0x0000ff : config.white;
        for (unsigned i=0;i<config.count;++i) frame[i]=dimLed(color,config.brightness);
        return frame;
    }
    if (config.mode != LedMode::Ui || !fresh || !gauges.active || !gauges.count) return frame;
    for (unsigned position = 0; position < config.count; ++position) {
        // LVGL angles: 0 right, 90 bottom. Position zero is physical 12 o'clock.
        const float angle = std::fmod(270.0f + position * (360.0f / config.count), 360.0f);
        uint32_t color = 0; // no UI arc at this angle: keep the upper/lower gaps dark
        for (unsigned i = 0; i < gauges.count && i < gauges.arcs.size(); ++i) {
            const auto &arc = gauges.arcs[i];
            if (!arc.sweep || arc.sweep > 360 || arc.minimum == arc.maximum) continue;
            const float offset = std::fmod(angle - (arc.rotation % 360) + 360.0f, 360.0f);
            if (offset > arc.sweep) continue;
            color = config.white;
            const float value = arc.minimum + (float(arc.maximum) - arc.minimum) * (offset / arc.sweep);
            if (value >= arc.start && value <= arc.end) color = arc.color;
        }
        const unsigned index = (config.topIndex + (config.clockwise ? position : config.count - position)) % config.count;
        frame[index] = dimLed(color, config.brightness);
    }
    return frame;
}
} // namespace guition_knob
