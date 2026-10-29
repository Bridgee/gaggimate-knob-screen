#pragma once
#ifdef GAGGIMATE_KNOB
#include <lvgl.h>
#include <display/drivers/GuitionKnob/HardwarePolicy.h>
void updateKnobStatusBattery(lv_obj_t *statusIcons, guition_knob::PowerSource source, int percentage);
#endif
