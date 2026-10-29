#pragma once
#include <lvgl.h>
#include <display/drivers/GuitionKnob/LedGaugePolicy.h>

namespace guition_knob {
inline void collectLedGauges(lv_obj_t *object, GaugeFrame &frame) {
    if (!object || !lv_obj_is_visible(object)) return;
    if (lv_obj_check_type(object, &lv_meter_class)) {
        auto *meter = reinterpret_cast<lv_meter_t *>(object);
        for (auto *indicator = static_cast<lv_meter_indicator_t *>(_lv_ll_get_head(&meter->indicator_ll)); indicator;
             indicator = static_cast<lv_meter_indicator_t *>(_lv_ll_get_next(&meter->indicator_ll, indicator))) {
            if (indicator->type != LV_METER_INDICATOR_TYPE_SCALE_LINES || !indicator->opa || frame.count >= frame.arcs.size()) continue;
            const auto *scale = indicator->scale;
            // Read the exact range, orientation, fill and current theme color
            // used by the displayed meter. Never retain LVGL pointers off-task.
            auto &arc = frame.arcs[frame.count++];
            arc.minimum = scale->min; arc.maximum = scale->max;
            arc.rotation = scale->rotation; arc.sweep = scale->angle_range;
            arc.start = indicator->start_value; arc.end = indicator->end_value;
            arc.color = lv_color_to32(indicator->type_data.scale_lines.color_start) & 0xffffff;
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(object); ++i) collectLedGauges(lv_obj_get_child(object, i), frame);
}
inline GaugeFrame captureLedGauges(lv_obj_t *screen, bool active) {
    GaugeFrame result{};
    if (active) collectLedGauges(screen, result);
    result.active = active && result.count > 0;
    return result;
}
} // namespace guition_knob
