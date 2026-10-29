#pragma once

// Convert design dimensions only when constructing/updating UI objects.
// LVGL rasterizes their resulting geometry at native panel resolution.
constexpr int uiPixel(int design) {
#ifdef GAGGIMATE_KNOB
    return design < 0 ? -((-design * 3 + 2) / 4) : (design * 3 + 2) / 4;
#else
    return design;
#endif
}
