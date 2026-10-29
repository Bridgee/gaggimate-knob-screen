#include "display/ui/default/BrightnessPolicy.h"
#include <cassert>
#include <cstdio>

using gaggimate_ui::brightnessForState;

int main() {
    // Active screens apply saved main brightness without standby/wake.
    assert(brightnessForState(false, 1000, 0, 60000, 16, 2) == 16);
    assert(brightnessForState(false, 1001, 0, 60000, 1, 2) == 1);
    // Standby retains main during delay, applies even0 exactly at deadline.
    assert(brightnessForState(true, 69999, 10000, 60000, 8, 0) == 8);
    assert(brightnessForState(true, 70000, 10000, 60000, 8, 0) == 0);
    // Saving a new standby value while already dimmed applies directly.
    assert(brightnessForState(true, 72000, 10000, 60000, 8, 3) == 3);
    assert(brightnessForState(true, 72001, 10000, 60000, 8, 1) == 1);
    // A longer saved delay restores main until the revised deadline.
    assert(brightnessForState(true, 72001, 10000, 90000, 8, 1) == 8);
    assert(brightnessForState(true, 100000, 10000, 90000, 8, 1) == 1);
    // Wake uses main, irrespective of the old standby timer.
    assert(brightnessForState(false, 100001, 10000, 90000, 7, 0) == 7);
    // Timer wraparound and zero delay must not prevent dimming.
    assert(brightnessForState(true, 16, 0xfffffff0u, 32, 8, 2) == 2);
    assert(brightnessForState(true, 15, 0xfffffff0u, 32, 8, 2) == 8);
    assert(brightnessForState(true, 100, 100, 0, 8, 0) == 0);
    assert(brightnessForState(true, 100, 100, -1, 8, 0) == 0);
    assert(brightnessForState(true, 200, 0, 0, 8, 0) == 8); // Timer not armed.
    assert(brightnessForState(false, 1, 0, 1, 99, 0) == 16);
    assert(brightnessForState(true, 200, 100, 1, 8, -9) == 0);
    puts("PASS: saved settings in current state, standby deadline, off, wake, timer wrap and bounds");
}
