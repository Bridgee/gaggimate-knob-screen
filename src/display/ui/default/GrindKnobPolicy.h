#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
namespace gaggimate_ui {
inline int grindKnobDuration(int current, int direction, int minimum, int maximum) {
    const int step = direction > 0 ? 500 : direction < 0 ? -500 : 0;
    return static_cast<int>(std::clamp<int64_t>(int64_t(current) + step, minimum, maximum));
}
inline double grindKnobWeight(double current, int direction, double minimum, double maximum) {
    return std::clamp(current + (direction > 0 ? 0.5 : direction < 0 ? -0.5 : 0.0), minimum, maximum);
}
inline void formatGrindKnobDuration(unsigned long ms, char *buffer, size_t size) {
    std::snprintf(buffer, size, "%lu:%02lu.%lu", ms / 60000UL, (ms / 1000UL) % 60UL, (ms / 100UL) % 10UL);
}
}
