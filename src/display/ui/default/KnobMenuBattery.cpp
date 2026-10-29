#ifdef GAGGIMATE_KNOB
#include "KnobMenuBattery.h"
#include "KnobBatteryImages.h"

namespace {
uint8_t batteryMarker;
constexpr int LowBatteryPercent = 25;
constexpr int CriticalBatteryPercent = 10;
}

void updateKnobStatusBattery(lv_obj_t *statusIcons, guition_knob::PowerSource source, int percentage) {
    if (!statusIcons) return;
    // Each screen owns its image. No global widget pointer can be accidentally
    // reused when standby/menu screens coexist or are destroyed and recreated.
    lv_obj_t *battery = nullptr;
    const auto children = lv_obj_get_child_cnt(statusIcons);
    for (uint32_t i = 0; i < children; ++i) {
        auto *child = lv_obj_get_child(statusIcons, i);
        if (lv_obj_get_user_data(child) == &batteryMarker) { battery = child; break; }
    }
    if (!battery) {
        // Standby originally used border padding between the same icons;
        // use the menu's explicit spacing so both status rows match.
        lv_obj_set_width(statusIcons, 150);
        lv_obj_set_style_pad_column(statusIcons, 15, LV_PART_MAIN);
        for (uint32_t i = 0; i < children; ++i)
            lv_obj_set_style_border_width(lv_obj_get_child(statusIcons, i), 0, LV_PART_MAIN);
        battery = lv_img_create(statusIcons);
        lv_obj_clear_flag(battery, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_img_recolor_opa(battery, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_user_data(battery, &batteryMarker);
    }
    const bool charging = source == guition_knob::PowerSource::External;
    const auto *image = charging ? &knob_battery_charging
        : percentage >= 88 ? &knob_battery_full
        : percentage >= 63 ? &knob_battery_threequarters
        : percentage >= 38 ? &knob_battery_half
        : percentage >= 16 ? &knob_battery_quarter : &knob_battery_empty;
    const bool onBattery = source == guition_knob::PowerSource::Battery && percentage >= 0;
    const auto foreground = lv_obj_get_style_img_recolor(lv_obj_get_child(statusIcons, 0), LV_PART_MAIN);
    const bool lightTheme = lv_color_brightness(foreground) < 128;
    const auto color = onBattery && percentage <= CriticalBatteryPercent ? lv_color_hex(lightTheme ? 0xc62828 : 0xff4545)
        : onBattery && percentage <= LowBatteryPercent ? lv_color_hex(lightTheme ? 0x9a6700 : 0xffc857)
        : foreground;
    if (image != lv_img_get_src(battery)) {
        lv_img_set_src(battery, image);
    }
    if (lv_color_to32(color) != lv_color_to32(lv_obj_get_style_img_recolor(battery, LV_PART_MAIN))) {
        lv_obj_set_style_img_recolor(battery, color, LV_PART_MAIN);
    }
}
#endif
