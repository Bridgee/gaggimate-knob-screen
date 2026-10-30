#include "display/drivers/GuitionKnob/BatteryEstimate.h"
#include <cassert>
#include <cstdio>
using namespace guition_knob;
namespace {
uint32_t tick = 0;
int sample(BatteryEstimate &filter, uint16_t mv, PowerSource source = PowerSource::Battery) {
    const int value = filter.update(source, mv, tick); tick += 2000; return value;
}
int prime(BatteryEstimate &filter, uint16_t mv) {
    assert(sample(filter, mv) == -1); assert(sample(filter, mv) == -1);
    return sample(filter, mv);
}
}
int main() {
    std::array<uint16_t, 16> burst{}; burst.fill(1950);
    assert(trimmedRailMilliVolts(burst) == 3900);
    burst[0] = burst[1] = 0; burst[14] = burst[15] = 65535;
    assert(trimmedRailMilliVolts(burst) == 3900); // burst outliers do not distort supply classification
    burst.fill(65535); assert(trimmedRailMilliVolts(burst) == 65535);

    BatteryEstimate estimate;
    assert(prime(estimate, 3900) == 60);
    assert(sample(estimate, 3200) == 60); // isolated load/ADC transient
    for (int i = 0; i < 10; ++i) assert(sample(estimate, 3900) == 60);
    estimate.reset(); assert(prime(estimate, 4060) == 84);
    for (int i = 0; i < 100; ++i) assert(sample(estimate, i % 2 ? 4067 : 4060) == 84);

    estimate.reset(); assert(prime(estimate, 3900) == 60);
    int value = -1;
    for (int i = 0; i < 50; ++i) value = sample(estimate, 3800);
    assert(value >= 40 && value <= 42 && estimate.filteredMilliVolts() <= 3801);
    for (int i = 0; i < 50; ++i) value = sample(estimate, 3900);
    assert(value >= 58 && value <= 60); // genuine voltage recovery is allowed, not a down-only fake gauge

    estimate.reset(); assert(prime(estimate, 3900) == 60);
    for (int i = 0; i < 4; ++i) value = sample(estimate, 3500);
    assert(value <= 10); // sustained critical voltage is visible within four 2-second samples
    for (int i = 0; i < 20; ++i) assert(sample(estimate, 3300) <= 10);

    estimate.reset(); assert(prime(estimate, 3730) == 26);
    for (int i = 0; i < 15; ++i) value = sample(estimate, 3725);
    assert(value == 25); // one-point downward crossing still enters low warning
    for (int i = 0; i < 20; ++i) assert(sample(estimate, 3730) == 25);
    for (int i = 0; i < 40; ++i) value = sample(estimate, 3740);
    assert(value > 25);
    estimate.reset(); assert(prime(estimate, 3610) == 11);
    for (int i = 0; i < 15; ++i) value = sample(estimate, 3600);
    assert(value == 10);

    const auto usb = estimatedPowerSource(4069, PowerSource::Battery, true);
    assert(sample(estimate, 4069, usb) == -1 && estimate.filteredMilliVolts() == 0);
    for (int i = 0; i < 5; ++i) assert(sample(estimate, 4200, PowerSource::External) == -1);
    assert(prime(estimate, 4100) == 90); // unplug never reuses the old battery percentage or USB rail
    assert(sample(estimate, 4300, PowerSource::Unknown) == -1 && estimate.filteredMilliVolts() == 0);
    assert(prime(estimate, 3900) == 60);
    tick += 10001; assert(sample(estimate, 3700) == -1);
    assert(sample(estimate, 3700) == -1); assert(sample(estimate, 3700) == 20);
    assert(sample(estimate, 0) == -1 && estimate.filteredMilliVolts() == 0);
    estimate.reset(); tick = 0xfffff000u; assert(prime(estimate, 3900) == 60);
    for (int i = 0; i < 10; ++i) assert(sample(estimate, 3900) == 60);
    puts("PASS: battery jitter/transients, real trend/recovery, warning entry, USB exclusion, stale gaps and clock wrap");
}
