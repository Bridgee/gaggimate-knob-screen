#include "KnobAdjustment.h"
#include "eez/screens.h"

namespace gaggimate_ui {
namespace {
bool exists(lv_obj_t *object) { return object && lv_obj_is_valid(object); }
bool visible(lv_obj_t *object) { return exists(object) && lv_obj_is_visible(object); }
}
AdjustmentTargets visibleKnobTargets(int screen) {
    AdjustmentTargets result;
    auto add = [&](AdjustmentId id, lv_obj_t *row, lv_obj_t *lower, lv_obj_t *raise, lv_obj_t *value) {
        if (!visible(row) || !visible(lower) || !visible(raise) || !visible(value)) return;
        if (lv_obj_has_state(lower, LV_STATE_DISABLED) && lv_obj_has_state(raise, LV_STATE_DISABLED)) return;
        if (result.count < result.items.size()) result.items[result.count++] = {id, row, lower, raise, value};
    };
    switch (screen) {
    case SCREEN_ID_BREW_SCREEN:
        add(AdjustmentId::Temperature, objects.temp_container, objects.down_temp_button, objects.up_temp_button, objects.target_temp);
        add(AdjustmentId::Duration, objects.brew_target_time_container, objects.down_duration_button, objects.up_duration_button, objects.target_duration);
        add(AdjustmentId::Weight, objects.brew_target_weight_container, objects.down_weight_button, objects.up_weight_button, objects.target_weight_1);
        break;
    case SCREEN_ID_STEAM_SCREEN:
        add(AdjustmentId::Temperature, objects.target_temp2 ? lv_obj_get_parent(objects.target_temp2) : nullptr,
            objects.steam_down_temp_button, objects.steam_up_temp_button, objects.target_temp2);
        break;
    case SCREEN_ID_WATER_SCREEN:
        add(AdjustmentId::Temperature, objects.target_temp3 ? lv_obj_get_parent(objects.target_temp3) : nullptr,
            objects.water_down_temp_button, objects.water_up_temp_button, objects.target_temp3);
        break;
    case SCREEN_ID_GRIND_SCREEN:
        add(AdjustmentId::Weight, objects.target_weight, objects.grind_down_weight_button, objects.grind_up_weight_button, objects.target_duration3_1);
        add(AdjustmentId::Duration, objects.target_time, objects.grind_down_duration_button, objects.grind_up_duration_button, objects.target_duration3);
        break;
    default: break;
    }
    return result;
}

KnobAdjustment::KnobAdjustment() {
    for (auto &binding : bindings) binding.owner = this;
}
KnobAdjustment::~KnobAdjustment() { unbind(); if(styleReady)lv_style_reset(&focusStyle); }
void KnobAdjustment::unbind() {
    for (auto &binding : bindings) {
        const auto &t = binding.target;
        if (exists(t.row)) {
            if(exists(t.value)){lv_obj_clear_state(t.value, LV_STATE_USER_1);lv_obj_remove_style(t.value, &focusStyle, LV_STATE_USER_1); }
            lv_obj_remove_event_cb_with_user_data(t.row, deleted, &binding);
            if (!binding.rowWasClickable) lv_obj_clear_flag(t.row, LV_OBJ_FLAG_CLICKABLE);
        }
        for (auto *object : {t.row, t.lower, t.raise})
            if (exists(object)) lv_obj_remove_event_cb_with_user_data(object, pressed, &binding);
        binding.target = {};
    }
    count = 0; selectedId = AdjustmentId::None;
}
void KnobAdjustment::pressed(lv_event_t *event) {
    auto *binding = static_cast<Binding *>(lv_event_get_user_data(event));
    if (!binding || !binding->owner->ready) return;
    auto *target = lv_event_get_target(event);
    if (target == binding->target.row || target == binding->target.lower || target == binding->target.raise)
        binding->owner->choose(binding->target.id);
}
void KnobAdjustment::deleted(lv_event_t *event) {
    auto *binding = static_cast<Binding *>(lv_event_get_user_data(event));
    if (binding && lv_event_get_target(event) == binding->target.row) {
        binding->target = {};
        binding->owner->selectedId = AdjustmentId::None;
        binding->owner->decoder.clearFraction();
    }
}
void KnobAdjustment::choose(AdjustmentId id) {
    selectedId = id; decoder.clearFraction(); highlight();
}
void KnobAdjustment::highlight() {
    for (unsigned i=0;i<count;++i) {
        auto &t = bindings[i].target;
        if (!exists(t.value)) continue;
        if (ready && t.id == selectedId) lv_obj_add_state(t.value, LV_STATE_USER_1);
        else lv_obj_clear_state(t.value, LV_STATE_USER_1);
    }
}
void KnobAdjustment::update(int screen, int32_t raw, bool enabled, const AdjustmentTargets &targets, lv_color_t focusColor) {
    if(!styleReady) {
        lv_style_init(&focusStyle);
        lv_style_set_outline_width(&focusStyle, 2);
        lv_style_set_outline_pad(&focusStyle, 3);
        lv_style_set_outline_opa(&focusStyle, LV_OPA_COVER);
        lv_style_set_radius(&focusStyle,4);
        styleReady=true;
    }
    bool changed = page != screen || count != targets.count;
    for (unsigned i=0;i<targets.count && !changed;++i) {
        const auto &a=bindings[i].target;const auto &b=targets.items[i];
        changed = a.id!=b.id || a.row!=b.row || a.lower!=b.lower || a.raise!=b.raise || a.value!=b.value;
    }
    if (changed) {
        unbind(); page = screen; count = targets.count;
        for (unsigned i=0;i<count;++i) {
            auto &binding=bindings[i];binding.target=targets.items[i];
            binding.rowWasClickable=lv_obj_has_flag(binding.target.row,LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(binding.target.row,LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_style(binding.target.value,&focusStyle,LV_STATE_USER_1);
            lv_obj_add_event_cb(binding.target.row,deleted,LV_EVENT_DELETE,&binding);
            for(auto *object:{binding.target.row,binding.target.lower,binding.target.raise})
                lv_obj_add_event_cb(object,pressed,LV_EVENT_PRESSED,&binding);
        }
        selectedId = count==1 ? bindings[0].target.id : AdjustmentId::None;
        decoder.clearFraction();
    }
    if (!enabled) selectedId = AdjustmentId::None;
    if (enabled && count==1) selectedId=bindings[0].target.id;
    ready=enabled;
    const auto rgb=lv_color_to32(focusColor);
    if(!haveFocusColor || rgb!=focusRgb) {
        focusRgb=rgb;haveFocusColor=true;lv_style_set_outline_color(&focusStyle,focusColor);
        lv_obj_report_style_change(&focusStyle);
    }
    highlight();
    int steps=decoder.consume(raw,enabled && !changed && selectedId!=AdjustmentId::None);
    for(unsigned i=0;i<count && steps;++i) {
        auto t=bindings[i].target;
        if(t.id!=selectedId)continue;
        auto *button=steps>0?t.raise:t.lower;
        for(int n=0;n<(steps>0?steps:-steps);++n) {
            if(!visible(button) || lv_obj_has_state(button,LV_STATE_DISABLED))break;
            // The existing click handler owns steps, clamping and dirty/save semantics.
            lv_event_send(button,LV_EVENT_CLICKED,nullptr);
        }
        break;
    }
}
} // namespace gaggimate_ui
