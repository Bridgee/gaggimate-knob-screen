#pragma once
#include <stdint.h>

namespace guition_knob {
enum class PowerSource : uint8_t { Unknown, Battery, External };
constexpr PowerSource estimatedPowerSource(uint16_t railMv, PowerSource previous = PowerSource::Unknown, bool usbHostActive = false) {
    if (usbHostActive) return PowerSource::External;
    if (railMv >= 2800 && railMv <= 4250) return PowerSource::Battery;
    if (railMv >= 4350 && railMv <= 5500) return PowerSource::External;
    // Keep a previously identified external supply through its small load dip.
    // An unclassified startup sample in this gap must remain unknown.
    if (railMv > 4250 && railMv < 4350 && previous == PowerSource::External) return previous;
    return PowerSource::Unknown;
}
// GPIO6 measures SYS/2 through two10k resistors, not the cell terminal.
// This is a coarse voltage estimate only. A high SYS voltage cannot report SOC.
constexpr int estimatedBatteryPercent(uint16_t railMv) {
    if (railMv < 2800 || railMv > 4250) return -1;
    constexpr uint16_t mv[] = {3300, 3500, 3600, 3700, 3800, 3900, 4000, 4100, 4200};
    constexpr int pct[] = {0, 5, 10, 20, 40, 60, 75, 90, 100};
    if (railMv <= mv[0]) return 0;
    for (unsigned i = 1; i < sizeof(mv) / sizeof(mv[0]); ++i)
        if (railMv <= mv[i]) return pct[i - 1] + (railMv - mv[i - 1]) * (pct[i] - pct[i - 1]) / (mv[i] - mv[i - 1]);
    return 100;
}
} // namespace guition_knob
