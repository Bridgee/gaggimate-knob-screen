// Documentation scenes use the actual native UI and extension APIs, with fixture data.
#include <lvgl.h>
#include "screens.h"
#include "ui.h"
#include "structs.h"
#include "KnobMenuBattery.h"
#include "KnobAdjustment.h"
#include "StandbyDisplay.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>

using namespace gaggimate_ui;
void advancePreviewTicks(uint32_t);
void applyPreviewMenuLayout(bool);
static void paint() { advancePreviewTicks(35); lv_timer_handler(); }
static void tick() { ui_tick(); lv_obj_update_layout(lv_scr_act()); paint(); }
static void menu(bool grinder) {
    auto system = SystemStatusValue(eez::flow::getGlobalVariable(FLOW_GLOBAL_VARIABLE_SYSTEM));
    system.grind_available(grinder);
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SYSTEM, system);
    eez_flow_set_screen(SCREEN_ID_MENU_SCREEN_NEW, LV_SCR_LOAD_ANIM_NONE, 0, 0);
    tick();
    applyPreviewMenuLayout(grinder);
    lv_obj_update_layout(lv_scr_act());
    paint();
}
static lv_point_t center(lv_obj_t *object) {
    lv_area_t area; lv_obj_get_coords(object, &area);
    return {lv_coord_t((area.x1 + area.x2 + 1) / 2), lv_coord_t((area.y1 + area.y2 + 1) / 2)};
}
void renderDocumentationFrames(const char *directory, int theme, void(*save)(const char *, const char *)) {
    const auto foreground = lv_color_hex(theme_colors[theme][0]);
    const auto focus = lv_color_hex(theme == 1 ? 0x0069ad : 0x80cfff);
    // Both capability layouts must receive the runtime placement effect.
    menu(false);
    auto brew = center(objects.btn_brew_1), steam = center(objects.btn_steam_1), water = center(objects.btn_water_1);
    assert(brew.x == 180 && brew.y < 180 && steam.x > 180 && water.x < 180);
    assert(!lv_obj_is_visible(objects.btn_grind_1));
    menu(true);
    brew = center(objects.btn_brew_1); steam = center(objects.btn_steam_1); water = center(objects.btn_water_1);
    auto grind = center(objects.btn_grind_1);
    assert(lv_obj_is_visible(objects.btn_grind_1));
    assert(brew.x < 180 && brew.y < 180 && steam.x > 180 && steam.y < 180);
    assert(water.x > 180 && water.y > 180 && grind.x < 180 && grind.y > 180);
    assert(brew.x == grind.x && steam.x == water.x && brew.y == steam.y && water.y == grind.y);
    assert(std::abs(brew.x + steam.x - 360) <= 1 && std::abs(brew.y + grind.y - 360) <= 1);
    updateKnobStatusBattery(objects.status_icons, guition_knob::PowerSource::External, -1);
    paint(); save(directory, "doc-menu-charging.ppm");
    updateKnobStatusBattery(objects.status_icons, guition_knob::PowerSource::Battery, 90);
    paint(); save(directory, "doc-menu-battery-high.ppm");
    updateKnobStatusBattery(objects.status_icons, guition_knob::PowerSource::Battery, 25);
    paint(); save(directory, "doc-menu-low.ppm");
    updateKnobStatusBattery(objects.status_icons, guition_knob::PowerSource::Battery, 10);
    paint(); save(directory, "doc-menu-critical.ppm");

    eez_flow_set_screen(SCREEN_ID_STEAM_SCREEN, LV_SCR_LOAD_ANIM_NONE, 0, 0);
    auto boiler = BoilerValue(eez::flow::getGlobalVariable(FLOW_GLOBAL_VARIABLE_BOILER));
    boiler.target_temperature(145);
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_BOILER, boiler);
    tick();
    {
        KnobAdjustment knob;
        auto targets = visibleKnobTargets(SCREEN_ID_STEAM_SCREEN); assert(targets.count == 1);
        knob.update(SCREEN_ID_STEAM_SCREEN, 0, true, targets, focus);
        assert(knob.selected() == AdjustmentId::Temperature);
        paint(); save(directory, "doc-knob-single.ppm");
    }
    boiler.target_temperature(93);
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_BOILER, boiler);
    auto flags = UIFlagsValue(eez::flow::getGlobalVariable(FLOW_GLOBAL_VARIABLE_UI_FLAGS));
    flags.brew_adjustments(true);
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_UI_FLAGS, flags);
    eez_flow_set_screen(SCREEN_ID_BREW_SCREEN, LV_SCR_LOAD_ANIM_NONE, 0, 0);
    tick();
    {
        KnobAdjustment knob;
        auto targets = visibleKnobTargets(SCREEN_ID_BREW_SCREEN); assert(targets.count == 2);
        knob.update(SCREEN_ID_BREW_SCREEN, 0, true, targets, focus);
        assert(knob.selected() == AdjustmentId::None);
        lv_event_send(targets.items[0].row, LV_EVENT_PRESSED, nullptr);
        assert(knob.selected() == AdjustmentId::Temperature);
        paint(); save(directory, "doc-knob-multiple.ppm");
    }
    eez_flow_set_screen(SCREEN_ID_STANDBY_SCREEN, LV_SCR_LOAD_ANIM_NONE, 0, 0);
    tick();
    updateKnobStatusBattery(objects.standby_icons, guition_knob::PowerSource::Battery, 60);
    StandbyAppearance appearance;
    appearance.content = StandbyContent::Text; std::strcpy(appearance.text, "MY COFFEE");
    updateStandbyDisplay(objects.standby_screen, objects.obj3, objects.time, appearance, "10:27", foreground);
    paint(); save(directory, "doc-standby-text.ppm");
    appearance.content = StandbyContent::Clock;
    updateStandbyDisplay(objects.standby_screen, objects.obj3, objects.time, appearance, "10:27", foreground);
    paint(); save(directory, "doc-standby-clock.ppm");
    printf("PASS: %s documentation scenes; runtime menu placement, power, focus and standby APIs\n", theme ? "Light" : "Dark");
}
