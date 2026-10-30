#include <display/drivers/GuitionKnob/BootHealthPolicy.h>
#include <cassert>
using namespace guition_knob;
BootHealthSample live(uint32_t now) {
    BootHealthSample s;
    s.setup = s.storage = s.settings = s.panel = s.touch = s.frame = true;
    s.mainAt = s.uiAt = s.logicAt = now;
    s.mainCount = s.uiCount = s.logicCount = 3;
    return s;
}
int main() {
    BootHealthWindow window;
    assert(!window.observe(100, live(100)));
    assert(!window.observe(20099, live(20099)));
    assert(window.observe(20100, live(20100)));
    for (int missing = 0; missing < 9; ++missing) {
        BootHealthWindow w;
        assert(!w.observe(0, live(0)));
        auto s = live(20000);
        switch (missing) {
            case 0: s.setup = false; break;
            case 1: s.storage = false; break;
            case 2: s.settings = false; break;
            case 3: s.panel = false; break;
            case 4: s.touch = false; break;
            case 5: s.frame = false; break;
            case 6: s.mainAt = 17000; break;
            case 7: s.uiAt = 17000; break;
            case 8: s.logicAt = 17000; break;
        }
        assert(!w.observe(20000, s));
        assert(!w.observe(20001, live(20001)));
        assert(w.observe(40001, live(40001)));
    }
    BootHealthWindow wrap;
    assert(!wrap.observe(0xfffffff0, live(0xfffffff0)));
    assert(wrap.observe(20000, live(20000)));
}
