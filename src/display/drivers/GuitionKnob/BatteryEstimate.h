#pragma once
#include "HardwarePolicy.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>

namespace guition_knob {
// Trim two readings at each end of a 16-sample ADC burst, then undo SYS/2.
// Keep this instantaneous value separate from the slower SOC filter.
inline uint16_t trimmedRailMilliVolts(std::array<uint16_t, 16> readings) {
    std::sort(readings.begin(), readings.end());
    uint32_t total = 0;
    for (unsigned i = 2; i < 14; ++i) total += readings[i];
    return static_cast<uint16_t>(std::min(uint32_t(65535), (total * 2 + 6) / 12));
}

// For the existing 2-second sampler. This stabilizes a coarse SYS-voltage
// proxy; it does not add a cell/current sensor or calibrate remaining capacity.
class BatteryEstimate {
  public:
    int update(PowerSource source, uint16_t railMv, uint32_t now) {
        if (source != PowerSource::Battery || estimatedBatteryPercent(railMv) < 0) {
            reset(); return -1; // Never expose cached SOC on USB/unknown supply.
        }
        if (haveTime && uint32_t(now - lastAt) > 10000) reset();
        lastAt = now; haveTime = true;
        recent[next] = railMv; next = (next + 1) % recent.size();
        if (count < recent.size()) ++count;
        if (count < recent.size()) return -1;
        auto ordered = recent; std::sort(ordered.begin(), ordered.end());
        const uint16_t median = ordered[1];
        if (reported < 0) {
            filteredQ8 = int32_t(median) * 256;
            reported = estimatedBatteryPercent(median); return reported;
        }
        // A sustained critical input must not remain hidden by the slow filter.
        criticalRun = estimatedBatteryPercent(median) <= 10 ? std::min(3u, criticalRun + 1) : 0;
        if (criticalRun >= 3 && reported > 10) {
            filteredQ8 = int32_t(median) * 256;
            publish(estimatedBatteryPercent(median)); return reported;
        }
        filteredQ8 += (int32_t(median) * 256 - filteredQ8) / 4;
        const int wanted = estimatedBatteryPercent(filteredMilliVolts());
        if ((wanted <= 10 && reported > 10) || (wanted <= 25 && reported > 25)) {
            publish(wanted); return reported; // Warning entry takes priority over display hysteresis.
        }
        const int difference = wanted - reported;
        if (difference > -2 && difference < 2) {
            direction = 0; directionRun = 0; return reported;
        }
        const int sign = difference > 0 ? 1 : -1;
        directionRun = direction == sign ? directionRun + 1 : 1;
        direction = sign;
        if (directionRun >= 3) publish(wanted);
        return reported;
    }
    uint16_t filteredMilliVolts() const {
        return reported < 0 ? 0 : static_cast<uint16_t>((filteredQ8 + 128) / 256);
    }
    void reset() {
        recent = {}; count = next = 0; filteredQ8 = 0; reported = -1;
        direction = 0; directionRun = criticalRun = 0; haveTime = false; lastAt = 0;
    }
  private:
    void publish(int value) { reported = value; direction = 0; directionRun = 0; }
    std::array<uint16_t, 3> recent{};
    std::size_t count = 0, next = 0;
    int32_t filteredQ8 = 0;
    int reported = -1, direction = 0;
    unsigned directionRun = 0, criticalRun = 0;
    uint32_t lastAt = 0;
    bool haveTime = false;
};
} // namespace guition_knob
