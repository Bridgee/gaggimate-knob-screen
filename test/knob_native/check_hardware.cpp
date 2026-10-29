#include "display/drivers/GuitionKnob/HardwarePolicy.h"
#include <cassert>
#include <cstdio>
using namespace guition_knob;
int main() {
    assert(estimatedPowerSource(0) == PowerSource::Unknown);
    assert(estimatedPowerSource(2700) == PowerSource::Unknown);
    assert(estimatedPowerSource(3900) == PowerSource::Battery);
    assert(estimatedPowerSource(4250) == PowerSource::Battery);
    assert(estimatedPowerSource(4300) == PowerSource::Unknown);
    assert(estimatedPowerSource(4350) == PowerSource::External);
    assert(estimatedPowerSource(4500) == PowerSource::External);
    assert(estimatedPowerSource(4333, PowerSource::External) == PowerSource::External);
    assert(estimatedPowerSource(4251, PowerSource::External) == PowerSource::External);
    assert(estimatedPowerSource(4200, PowerSource::External) == PowerSource::Battery);
    assert(estimatedPowerSource(4300, PowerSource::Battery) == PowerSource::Unknown);
    assert(estimatedPowerSource(0, PowerSource::External) == PowerSource::Unknown);
    assert(estimatedPowerSource(4069, PowerSource::Unknown, true) == PowerSource::External);
    assert(estimatedPowerSource(0, PowerSource::Unknown, true) == PowerSource::External);
    assert(estimatedPowerSource(4069, PowerSource::External, false) == PowerSource::Battery);
    assert(estimatedPowerSource(5501) == PowerSource::Unknown);
    assert(estimatedBatteryPercent(0) == -1);
    assert(estimatedBatteryPercent(4500) == -1); // Never report a USB rail as100%.
    assert(estimatedBatteryPercent(4300) == -1);
    assert(estimatedBatteryPercent(3300) == 0);
    assert(estimatedBatteryPercent(3900) == 60);
    assert(estimatedBatteryPercent(3950) == 67);
    assert(estimatedBatteryPercent(4200) == 100);
    for (int mv = 2800; mv <= 4250; ++mv) {
        int pct = estimatedBatteryPercent(mv);
        assert(pct >= 0 && pct <= 100);
        if (mv > 2800) assert(pct >= estimatedBatteryPercent(mv - 1));
    }
    puts("PASS: invalid readings, ambiguous power range, external supply exclusion, monotonic coarse SOC estimate");
}
