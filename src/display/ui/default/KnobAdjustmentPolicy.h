#pragma once
#include <cstdint>
#include <algorithm>

namespace gaggimate_ui {
// Consume motion as it arrives, including while unselected. Never replay old
// motion after a touch selection, page change, disconnect or modal dialog.
class KnobStepDecoder {
  public:
    void clearFraction() { fraction = 0; }
    int consume(int32_t raw, bool armed) {
        const uint32_t now = static_cast<uint32_t>(raw);
        if (!initialized) { previous = now; initialized = true; return 0; }
        int64_t difference = uint32_t(now - previous);
        previous = now;
        if (difference > INT32_MAX) difference -= int64_t(1) << 32;
        if (!armed || difference > 64 || difference < -64) { fraction = 0; return 0; }
        fraction += int(difference) * direction;
        const int steps = fraction / edgesPerStep;
        fraction %= edgesPerStep;
        return std::clamp(steps, -4, 4);
    }
    void calibrate(int edges, int sign) {
        edgesPerStep = edges == 1 ? 1 : edges == 2 ? 2 : 4;
        direction = sign < 0 ? -1 : 1;
        fraction = 0;
    }
  private:
    bool initialized = false;
    uint32_t previous = 0;
    int fraction = 0, edgesPerStep = 4, direction = 1;
};
} // namespace gaggimate_ui
