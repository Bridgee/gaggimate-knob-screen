#include <display/drivers/GuitionKnob/LedPreferences.h>
#include <cassert>
using namespace guition_knob;
int main() {
    for (const char *text : {"0", "1", "45", "100"}) assert(validLedPercent(text));
    for (const char *text : {"", "-1", "101", "256", "1.5", " 45", "45x", "1000"}) assert(!validLedPercent(text));
    assert(!validLedPercent(nullptr));
    assert(validLedEnabled("0") && validLedEnabled("1"));
    for (const char *text : {"", "2", "true", "false", "01"}) assert(!validLedEnabled(text));
    assert(!validLedEnabled(nullptr));
    assert(savedLedConfig({true, 45}).brightness == 115);
    assert(savedLedConfig({true, 100}).brightness == 255);
    assert(savedLedConfig({true, 0}).brightness == 0);
    assert(savedLedConfig({true, -5}).brightness == 0);
    assert(savedLedConfig({true, 1000}).brightness == 255);
    const auto off = savedLedConfig({false, 45});
    assert(off.mode == LedMode::Off && off.brightness == 115);
    assert(off.count == 12 && off.topIndex == 0 && off.clockwise && off.white == 0xffffff);
    LedPreferenceApplier applier;
    bool ready = false, busy = false; unsigned writes = 0; LedConfig actual;
    auto apply = [&](const LedConfig &desired) {
        if (!ready || busy) return false;
        ++writes; actual = desired; return true;
    };
    assert(!applier.update({true, 45}, apply));
    ready = true;
    assert(applier.update({true, 45}, apply) && writes == 1 && actual.mode == LedMode::Ui);
    assert(!applier.update({true, 45}, apply) && writes == 1);
    actual.mode = LedMode::Red; // Low-level diagnostic override.
    assert(!applier.update({true, 45}, apply) && actual.mode == LedMode::Red);
    busy = true;
    assert(!applier.update({true, 45}, apply, true));
    busy = false;
    assert(applier.update({true, 45}, apply) && actual.mode == LedMode::Ui);
    assert(applier.update({false, 45}, apply) && actual.mode == LedMode::Off);
    assert(applier.update({true, 0}, apply) && actual.brightness == 0);
    // Startup must apply the persisted value even if it equals the old instance's last value.
    LedPreferenceApplier rebooted;
    assert(rebooted.update({true, 0}, apply) && actual.mode == LedMode::Ui);
    assert(applier.update({true, 100}, apply) && actual.brightness == 255);
}
