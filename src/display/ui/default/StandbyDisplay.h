#pragma once
#include <lvgl.h>
#include "StandbyDisplayPolicy.h"
namespace gaggimate_ui {
// Locate the view through a marked child, so EEZ may freely delete/recreate
// standby screens. No retained widget pointers cross screen lifetimes.
lv_obj_t *updateStandbyDisplay(lv_obj_t *screen, lv_obj_t *logo, lv_obj_t *smallClock,
                             const StandbyAppearance &appearance, const char *timeText, lv_color_t foreground);
} // namespace gaggimate_ui
