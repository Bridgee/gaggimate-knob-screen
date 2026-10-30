#include <display/drivers/GuitionKnob/ScreenUpdateGate.h>
#include <cassert>
#include <thread>
using guition_knob::ActionReservation;
int main() {
    std::recursive_mutex mutex; std::atomic<bool> updating{false}; unsigned actions = 0;
    {
        ActionReservation action(mutex, actions, updating);
        assert(action && actions == 1);
        bool free = false;
        std::thread other([&] { if (mutex.try_lock()) { free = true; mutex.unlock(); } }); other.join();
        assert(free); // Long user callbacks must not block the control loop's mutex.
        { ActionReservation nested(mutex, actions, updating); assert(nested && actions == 2); }
        assert(actions == 1);
        std::lock_guard<std::recursive_mutex> lock(mutex);
        assert(actions != 0); // OTA must refuse the in-flight action.
    }
    assert(actions == 0);
    { std::lock_guard<std::recursive_mutex> lock(mutex); updating = true; }
    { ActionReservation denied(mutex, actions, updating); assert(!denied && actions == 0); }
    updating = false;
    { ActionReservation resumed(mutex, actions, updating); assert(resumed && actions == 1); }
    assert(actions == 0);
}
