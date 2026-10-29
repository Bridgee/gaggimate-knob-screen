#include "display/drivers/GuitionKnob/Geometry.h"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace guition_knob;

int main() {
    uint16_t x = 999, y = 999;
    assert(decodeTouch({1, 0, 180, 0, 180}, x, y) && x == 180 && y == 180);
    assert(decodeTouch({1, 0x81, 0x67, 0x41, 0x67}, x, y) && x == 359 && y == 359);
    assert(!decodeTouch({0, 0, 0, 0, 0}, x, y));
    assert(!decodeTouch({2, 0, 0, 0, 0}, x, y));
    assert(!decodeTouch({1, 1, 104, 0, 0}, x, y));
    assert(!decodeTouch({1, 0, 0, 1, 104}, x, y));
    assert(backlightDuty(-1) == 0 && backlightDuty(0) == 0);
    assert(backlightDuty(1) == 48);
    for (int level = 2; level < 16; ++level) {
        assert(backlightDuty(level) == (level * 1023 + 8) / 16);
        assert(backlightDuty(level) > backlightDuty(level - 1));
    }
    assert(backlightDuty(8) == 512 && backlightDuty(16) == 1023 && backlightDuty(99) == 1023);
    puts("PASS: native360 touch decode, hardware flags, bounds and brightness clamping");
}
