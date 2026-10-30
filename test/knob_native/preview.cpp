#include <lvgl.h>
#include "screens.h"
#include "ui.h"
#include "structs.h"
#include "KnobMenuBattery.h"
#include "KnobBatteryImages.h"
#include "KnobLedGauges.h"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <cstdlib>

void checkUiExtensions(const char *,int,void(*)(const char *,const char *));
void applyPreviewMenuLayout(bool);
void renderDocumentationFrames(const char *, int, void(*)(const char *, const char *));
static uint32_t ticks;
void advancePreviewTicks(uint32_t amount){ticks+=amount;}
extern "C" uint32_t millis(void) { return ticks; }
static lv_color_t frame[360 * 360], render[360 * 360];
static unsigned transfers;

static void flush(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *p) {
    assert(a->x1 >= 0 && a->y1 >= 0 && a->x2 < 360 && a->y2 < 360);
    unsigned w = a->x2 - a->x1 + 1;
    for (int y = a->y1; y <= a->y2; ++y) {
        memcpy(frame + y * 360 + a->x1, p, w * sizeof(*p));
        p += w;
    }
    ++transfers;
    lv_disp_flush_ready(drv);
}

void assertPreviewPixels(lv_obj_t *object) {
    lv_area_t bounds; lv_obj_get_coords(object,&bounds);
    unsigned visible=0;
    const auto background=lv_obj_get_style_bg_color(lv_scr_act(),LV_PART_MAIN).full;
    for(int y=bounds.y1;y<=bounds.y2;++y)for(int x=bounds.x1;x<=bounds.x2;++x) {
        assert(x>=0 && x<360 && y>=0 && y<360);
        visible+=frame[y*360+x].full!=background;
    }
    assert(visible>=8);
}

static void publish() {
    SystemStatusValue system;
    system.bluetooth(true); system.wifi(true); system.error(false);
    system.error_label(""); system.time("10:27"); system.controller_version("v1.9.0");
    system.display_version("native360"); system.pressure_available(true);
    system.grind_available(true); system.volumetric_available(true);
    system.network("GaggiMate"); system.ip("4.4.4.1"); system.ap_active(true); system.mode(1);
    system.qrcodeContent("http://4.4.4.1/");
    BoilerValue boiler;
    boiler.current_temperature(25.5); boiler.target_temperature(93);
    boiler.max_temperature(160); boiler.current_pressure(0.04);
    boiler.target_pressure(9); boiler.max_pressure(12);
    ProfileInfoValue profile;
    profile.name("Classic espresso"); profile.temperature(93); profile.time("0:30");
    profile.phases(3); profile.steps(5); profile.target_weight(36);
    profile.is_volumetric(true); profile.is_current(true);
    UIFlagsValue flags;
    flags.temperature_stable(true); flags.has_next_profile(true); flags.has_prev_profile(true);
    WarningsValue warnings;
    BrewProcessValue process;
    process.elapsed_time("0:00"); process.phase_name("Preinfusion");
    process.boiler_target_temperature(93); process.profile_temperature(93);
    process.profile_time("0:30"); process.profile_target_weight(36);
    process.elapsed_percentage(0); process.current_volume(0);
    process.phase_type("pressure"); process.phase_value_current(0); process.phase_value_target(9);
    process.profile_phases(3); process.profile_steps(5); process.profile_is_volumetric(true);
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SYSTEM, system);
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_BOILER, boiler);
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_PREVIEW_PROFILE, profile);
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SELECTED_PROFILE, profile);
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_UI_FLAGS, flags);
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WARNINGS, warnings);
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_BREW_PROCESS_INFO, process);
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SCALE_WEIGHT_CURRENT, FloatValue(0));
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_GRIND_WEIGHT_TARGET, FloatValue(18));
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_GRIND_TIME_TARGET, StringValue("0:10.0"));
}

static void saveFrame(const char *directory, const char *filename) {
char path[1024]; snprintf(path, sizeof(path), "%s/%s", directory, filename);
    FILE *file = fopen(path, "wb"); assert(file);
    fprintf(file, "P6\n360 360\n255\n");
    for (int y = 0; y < 360; ++y) for (int x = 0; x < 360; ++x) {
        auto p = frame[y * 360 + x];
        unsigned char rgb[] = {(unsigned char)(p.ch.red * 255 / 31),
                               (unsigned char)(p.ch.green * 255 / 63), (unsigned char)(p.ch.blue * 255 / 31)};
        if ((x - 179.5) * (x - 179.5) + (y - 179.5) * (y - 179.5) > 180 * 180)
            rgb[0] = rgb[1] = rgb[2] = 35;
        fwrite(rgb, 1, 3, file);
    }
    fclose(file);
}

int main(int argc, char **argv) {
    assert(argc == 2 || argc == 3);
    const int theme = argc == 3 ? std::atoi(argv[2]) : 0;
    assert(theme == 0 || theme == 1);
    const char *themeName = theme == 1 ? "Light" : "Dark";
    lv_init();
    static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer, render, nullptr, 360 * 360);
    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res = drv.ver_res = 360; drv.draw_buf = &buffer; drv.flush_cb = flush;
    lv_disp_drv_register(&drv);
    eez::flow::onFlowErrorHook = [](eez::flow::FlowState *, int component, const char *message) {
        fprintf(stderr, "Flow error component %d: %s\n", component, message);
    };
    ui_init();
    eez_flow_set_theme(themeName);
    lv_obj_report_style_change(nullptr);
    assert(eez_flow_get_selected_theme_index() == unsigned(theme));
    publish();
    for (int screen = 1; screen <= 10; ++screen) {
        eez_flow_set_screen(screen, LV_SCR_LOAD_ANIM_NONE, 0, 0);
        for (int i = 0; i < 12; ++i) {
            ticks += 30; ui_tick();
            if (screen == 4) applyPreviewMenuLayout(true);
            if (screen == 1 || screen == 4) updateKnobStatusBattery(screen == 1 ? objects.standby_icons : objects.status_icons, guition_knob::PowerSource::External, -1);
            lv_timer_handler();
        }
        if (screen == 1 || screen == 4) {
            lv_obj_t *statusRow = screen == 1 ? objects.standby_icons : objects.status_icons;
            assert(lv_obj_get_child_cnt(statusRow) == 4);
            lv_obj_t *battery = lv_obj_get_child(statusRow, -1);
            assert(lv_obj_check_type(battery, &lv_img_class));
            assert(lv_img_get_src(battery) == &knob_battery_charging);
            updateKnobStatusBattery(statusRow, guition_knob::PowerSource::Battery, 60);
            assert(lv_img_get_src(battery) == &knob_battery_half);
            updateKnobStatusBattery(statusRow, guition_knob::PowerSource::Unknown, -1);
            assert(lv_img_get_src(battery) == &knob_battery_empty);
            updateKnobStatusBattery(statusRow, guition_knob::PowerSource::Battery, 84);
            const void *stableIcon = lv_img_get_src(battery);
            updateKnobStatusBattery(statusRow, guition_knob::PowerSource::Battery, 85);
            assert(stableIcon == lv_img_get_src(battery));
            updateKnobStatusBattery(statusRow, guition_knob::PowerSource::External, -1);
            const auto normal = lv_obj_get_style_img_recolor(lv_obj_get_child(statusRow, 0), LV_PART_MAIN);
            for (const int percentage : {26, 25, 11, 10, 0}) {
                updateKnobStatusBattery(statusRow, guition_knob::PowerSource::Battery, percentage);
                const auto expected = percentage <= 10 ? lv_color_hex(theme == 1 ? 0xc62828 : 0xff4545) : percentage <= 25 ? lv_color_hex(theme == 1 ? 0x9a6700 : 0xffc857) : normal;
                assert(lv_color_to32(lv_obj_get_style_img_recolor(battery, LV_PART_MAIN)) == lv_color_to32(expected));
            }
            updateKnobStatusBattery(statusRow, guition_knob::PowerSource::External, 10);
            assert(lv_img_get_src(battery) == &knob_battery_charging);
            assert(lv_color_to32(lv_obj_get_style_img_recolor(battery, LV_PART_MAIN)) == lv_color_to32(normal));
            updateKnobStatusBattery(statusRow, guition_knob::PowerSource::External, -1);
            assert(lv_obj_get_child_cnt(statusRow) == 4);
            // Live theme switching must recolor an already-created battery.
            eez_flow_set_theme(theme == 1 ? "Dark" : "Light");
            lv_obj_report_style_change(nullptr);
            ticks += 30; ui_tick();
            updateKnobStatusBattery(statusRow, guition_knob::PowerSource::Battery, 60);
            assert(lv_color_to32(lv_obj_get_style_img_recolor(battery, LV_PART_MAIN)) == lv_color_to32(lv_color_hex(theme_colors[1 - theme][0])));
            lv_timer_handler();
            eez_flow_set_theme(themeName);
            lv_obj_report_style_change(nullptr);
            ticks += 30; ui_tick();
            updateKnobStatusBattery(statusRow, guition_knob::PowerSource::External, -1);
            assert(lv_color_to32(lv_obj_get_style_img_recolor(battery, LV_PART_MAIN)) == lv_color_to32(lv_color_hex(theme_colors[theme][0])));
            lv_timer_handler();
        }
        assert(lv_obj_get_width(lv_scr_act()) == 360 && lv_obj_get_height(lv_scr_act()) == 360);
        if (screen == 7)
            assert(lv_obj_get_height(objects.profile_name_1) >= lv_obj_get_style_text_font(objects.profile_name_1, LV_PART_MAIN)->line_height);
        char filename[96]; snprintf(filename, sizeof(filename), "page-%02d.ppm", screen);
        saveFrame(argv[1], filename);
        if (screen == 1 || screen == 4) {
            lv_obj_t *row = screen == 1 ? objects.standby_icons : objects.status_icons;
            for (const int percentage : {60, 25, 10}) {
                ticks += 30;
                ui_tick();
                updateKnobStatusBattery(row, guition_knob::PowerSource::Battery, percentage);
                lv_timer_handler();
                // Check rendered siblings, not only style getters: stale theme
                // caches previously made standby Wi-Fi/BLE disappear here.
                for (int iconIndex = 0; iconIndex < 2; ++iconIndex) {
                    lv_area_t bounds; lv_obj_get_coords(lv_obj_get_child(row, iconIndex), &bounds);
                    unsigned visible = 0;
                    const auto background = lv_obj_get_style_bg_color(lv_scr_act(), LV_PART_MAIN).full;
                    for (int y = bounds.y1; y <= bounds.y2; ++y)
                        for (int x = bounds.x1; x <= bounds.x2; ++x)
                            visible += frame[y * 360 + x].full != background;
                    assert(visible >= 8);
                }
                snprintf(filename, sizeof(filename), "battery-%02d-%02d.ppm", screen, percentage);
                saveFrame(argv[1], filename);
            }
        }
        auto ledGauges = guition_knob::captureLedGauges(lv_scr_act(), true);
        assert(!guition_knob::captureLedGauges(lv_scr_act(), false).active);
        if (screen == 2 || screen == 4) assert(ledGauges.active && ledGauges.count == 2);
        if (ledGauges.active) {
            assert(ledGauges.count == 2);
            bool temp=false, pressure=false;
            for (unsigned i=0;i<ledGauges.count;++i) {
                const auto &arc=ledGauges.arcs[i];
                assert(arc.sweep==120);
                if(arc.minimum==0 && arc.maximum==160) {
                    temp=true;assert(arc.color==(lv_color_to32(lv_color_hex(theme_colors[theme][6]))&0xffffff));
                    assert(arc.end==25); // exact displayed integer, from25.5C fixture
                } else {
                    pressure=true;assert(arc.minimum==160 && arc.maximum==0);
                    assert(arc.color==(lv_color_to32(lv_color_hex(theme_colors[theme][7]))&0xffffff));
                    assert(arc.end==0); // displayed pressure fraction rounds to zero
                }
            }
            assert(temp && pressure);
        }
        printf("Native360 page %d rendered; transfer bounds checked.\n", screen);
    }
    // Revisiting a recreated EEZ screen must create a fresh battery object.
    eez_flow_set_screen(4, LV_SCR_LOAD_ANIM_NONE, 0, 0);
    ui_tick();
    updateKnobStatusBattery(objects.status_icons, guition_knob::PowerSource::Battery, 75);
    assert(lv_obj_get_child_cnt(objects.status_icons) == 4);
    assert(lv_img_get_src(lv_obj_get_child(objects.status_icons, -1)) == &knob_battery_threequarters);
    // Two live rows must update independently, with no shared-widget pointer.
    lv_obj_t *secondRow = lv_obj_create(lv_scr_act());
    lv_obj_t *templateIcon = lv_img_create(secondRow);
    lv_obj_set_style_img_recolor(templateIcon, lv_color_white(), LV_PART_MAIN);
    updateKnobStatusBattery(secondRow, guition_knob::PowerSource::Battery, 10);
    lv_obj_t *secondBattery = lv_obj_get_child(secondRow, -1);
    updateKnobStatusBattery(objects.status_icons, guition_knob::PowerSource::Battery, 75);
    assert(lv_img_get_src(lv_obj_get_child(objects.status_icons, -1)) == &knob_battery_threequarters);
    assert(lv_img_get_src(secondBattery) == &knob_battery_empty);
    assert(lv_color_to32(lv_obj_get_style_img_recolor(secondBattery, LV_PART_MAIN)) == lv_color_to32(lv_color_hex(0xff4545)));
    lv_obj_del(secondRow);
    eez_flow_set_screen(1, LV_SCR_LOAD_ANIM_NONE, 0, 0);
    ui_tick();
    updateKnobStatusBattery(objects.standby_icons, guition_knob::PowerSource::Battery, 25);
    assert(lv_obj_get_child_cnt(objects.standby_icons) == 4);
    assert(lv_img_get_src(lv_obj_get_child(objects.standby_icons, -1)) == &knob_battery_quarter);
    assert(eez_flow_get_selected_theme_index() == unsigned(theme));
    assert(lv_color_to32(lv_obj_get_style_bg_color(objects.standby_screen, LV_PART_MAIN)) == lv_color_to32(lv_color_hex(theme_colors[theme][1])));
    assert(transfers > 0);
    checkUiExtensions(argv[1],theme,saveFrame);
    publish();
    renderDocumentationFrames(argv[1],theme,saveFrame);
    return 0;
}
