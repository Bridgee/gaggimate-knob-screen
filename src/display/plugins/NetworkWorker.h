#pragma once
#ifdef GAGGIMATE_KNOB
#include "NetworkWorkQueue.h"
namespace network_work {
// One preallocated internal stack for all K718 HTTPS work, independent of UI/logic.
bool begin();
bool post(Kind kind, std::function<void()> work);
} // namespace network_work
#endif
