#include <lvgl.h>
#include "screens.h"
#include "ui.h"
#include "structs.h"
#include "KnobAdjustment.h"
#include "GrindKnobPolicy.h"
#include "display/core/constants.h"
#include "StandbyDisplay.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace gaggimate_ui;
static void countClick(lv_event_t *event){++*static_cast<int *>(lv_event_get_user_data(event));}
void advancePreviewTicks(uint32_t);
void assertPreviewPixels(lv_obj_t *);
static void paint(){advancePreviewTicks(35);lv_timer_handler();}
static void tick(){ui_tick();lv_obj_update_layout(lv_scr_act());paint();}
void checkUiExtensions(const char *directory,int theme,void(*save)(const char *,const char *)) {
    const auto foreground=lv_color_hex(theme_colors[theme][0]);
    const auto focus=lv_color_hex(theme==1?0x0069ad:0x80cfff);
    eez_flow_set_screen(SCREEN_ID_STEAM_SCREEN,LV_SCR_LOAD_ANIM_NONE,0,0);tick();
    {
        KnobAdjustment knob;knob.calibrate(1,1);
        auto targets=visibleKnobTargets(SCREEN_ID_STEAM_SCREEN);assert(targets.count==1);
        int raises=0,lowers=0;
        lv_obj_add_event_cb(targets.items[0].raise,countClick,LV_EVENT_CLICKED,&raises);
        lv_obj_add_event_cb(targets.items[0].lower,countClick,LV_EVENT_CLICKED,&lowers);
        knob.update(5,0,true,targets,focus);assert(knob.selected()==AdjustmentId::Temperature);
        knob.update(5,1,true,targets,focus);assert(raises==1 && lowers==0);
        knob.update(5,0,true,targets,focus);assert(raises==1 && lowers==1);
        lv_event_send(targets.items[0].raise,LV_EVENT_CLICKED,nullptr);assert(raises==2); // original touch event retained
        assert(lv_obj_get_style_outline_width(targets.items[0].value,0)==2);
        paint();save(directory,"knob-steam-selected.ppm");
        // Physical K718 trace: clockwise is GPIO1 => negative raw detents.
        knob.calibrate(1,-1);
        knob.update(5,-1,true,targets,focus);assert(raises==3 && lowers==1);
        knob.update(5,0,true,targets,focus);assert(raises==3 && lowers==2);
        knob.calibrate(1,1);
        knob.update(5,2,false,targets,focus);assert(raises==3 && knob.selected()==AdjustmentId::None);
        // Full screen deletion must never leave callbacks/styles pointing at freed objects.
        eez_flow_set_screen(SCREEN_ID_BREW_SCREEN,LV_SCR_LOAD_ANIM_NONE,0,0);
        UIFlagsValue flags(eez::flow::getGlobalVariable(FLOW_GLOBAL_VARIABLE_UI_FLAGS));flags.brew_adjustments(true);
        eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_UI_FLAGS,flags);tick();
        targets=visibleKnobTargets(SCREEN_ID_BREW_SCREEN);assert(targets.count==2);
        int first=0,second=0;
        lv_obj_add_event_cb(targets.items[0].raise,countClick,LV_EVENT_CLICKED,&first);
        lv_obj_add_event_cb(targets.items[1].raise,countClick,LV_EVENT_CLICKED,&second);
        knob.update(2,100,true,targets,focus);knob.update(2,104,true,targets,focus);
        assert(first==0 && second==0 && knob.selected()==AdjustmentId::None);
        lv_event_send(targets.items[1].row,LV_EVENT_PRESSED,nullptr);
        knob.update(2,105,true,targets,focus);assert(second==1 && first==0);
        assert(lv_obj_has_state(targets.items[1].value,LV_STATE_USER_1));
        assert(!lv_obj_has_state(targets.items[0].value,LV_STATE_USER_1));
        lv_event_send(targets.items[0].row,LV_EVENT_PRESSED,nullptr);
        knob.update(2,106,true,targets,focus);assert(first==1 && second==1);
        assert(!lv_obj_has_state(targets.items[1].value,LV_STATE_USER_1));
        paint();save(directory,"knob-brew-selected.ppm");
        knob.update(2,110,false,targets,focus);assert(first==1);
        lv_event_send(targets.items[0].raise,LV_EVENT_CLICKED,nullptr);assert(first==2);
        eez_flow_set_screen(4,LV_SCR_LOAD_ANIM_NONE,0,0);tick();
        auto menu=visibleKnobTargets(4);assert(menu.count==0);knob.update(4,150,true,menu,focus);
        eez_flow_set_screen(6,LV_SCR_LOAD_ANIM_NONE,0,0);tick();
        auto water=visibleKnobTargets(6);assert(water.count==1);knob.update(6,160,true,water,focus);
        assert(knob.selected()==AdjustmentId::Temperature);
        eez_flow_set_screen(SCREEN_ID_GRIND_SCREEN,LV_SCR_LOAD_ANIM_NONE,0,0);tick();
        int grindMs=25000,grindCalls=0;double grindGrams=18.0;
        knob.setGrindAdjustment([&](AdjustmentId field,int direction){
            ++grindCalls;
            if(field==AdjustmentId::Duration)grindMs=grindKnobDuration(grindMs,direction,BREW_MIN_DURATION_MS,BREW_MAX_DURATION_MS);
            else grindGrams=grindKnobWeight(grindGrams,direction,BREW_MIN_VOLUMETRIC,BREW_MAX_VOLUMETRIC);
            char time[12];formatGrindKnobDuration(grindMs,time,sizeof(time));
            eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_GRIND_TIME_TARGET,StringValue(time));
            eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_GRIND_WEIGHT_TARGET,FloatValue(grindGrams));
        });
        int raw=170;
        for(bool byWeight:{false,true}) {
            UIFlagsValue grindFlags(eez::flow::getGlobalVariable(FLOW_GLOBAL_VARIABLE_UI_FLAGS));
            grindFlags.grind_volumetric(byWeight);eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_UI_FLAGS,grindFlags);tick();
            auto grind=visibleKnobTargets(SCREEN_ID_GRIND_SCREEN);assert(grind.count==1);
            knob.update(8,raw,true,grind,focus);
            assert(knob.selected()==(byWeight?AdjustmentId::Weight:AdjustmentId::Duration));
            int originalClicks=0;
            lv_obj_add_event_cb(grind.items[0].raise,countClick,LV_EVENT_CLICKED,&originalClicks);
            const int calls=grindCalls;
            knob.update(8,++raw,true,grind,focus);tick();
            assert(grindCalls==calls+1 && originalClicks==0);
            assert(byWeight ? grindGrams==18.5 && grindMs==25000 : grindMs==25500 && grindGrams==18.0);
            if(!byWeight) {
                assert(!std::strcmp(lv_label_get_text(grind.items[0].value),"0:25.5"));
                lv_point_t size;lv_txt_get_size(&size,"5:00.0",lv_obj_get_style_text_font(grind.items[0].value,LV_PART_MAIN),
                    lv_obj_get_style_text_letter_space(grind.items[0].value,LV_PART_MAIN),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
                assert(size.x<=lv_obj_get_content_width(grind.items[0].value));
            }
            knob.update(8,--raw,true,grind,focus);tick();
            assert(grindMs==25000 && grindGrams==18.0);
            lv_event_send(grind.items[0].raise,LV_EVENT_CLICKED,nullptr);
            assert(originalClicks==1 && grindCalls==calls+2); // touch keeps the upstream action
            lv_obj_remove_event_cb_with_user_data(grind.items[0].raise,countClick,&originalClicks);
            lv_obj_add_state(grind.items[0].raise,LV_STATE_DISABLED);
            knob.update(8,++raw,true,grind,focus);
            assert(grindCalls==calls+2 && grindMs==25000 && grindGrams==18.0);
            lv_obj_clear_state(grind.items[0].raise,LV_STATE_DISABLED);
            raw+=10;
        }
        // Returning to a multi-target page clears focus again.
        eez_flow_set_screen(2,LV_SCR_LOAD_ANIM_NONE,0,0);tick();targets=visibleKnobTargets(2);
        knob.update(2,180,true,targets,focus);assert(targets.count==2 && knob.selected()==AdjustmentId::None);
        int brewClicks=0;const int priorGrindCalls=grindCalls;
        lv_obj_add_event_cb(targets.items[1].raise,countClick,LV_EVENT_CLICKED,&brewClicks);
        lv_event_send(targets.items[1].row,LV_EVENT_PRESSED,nullptr);
        knob.update(2,181,true,targets,focus);
        assert(brewClicks==1 && grindCalls==priorGrindCalls);
        lv_obj_remove_event_cb_with_user_data(targets.items[1].raise,countClick,&brewClicks);
    }
    eez_flow_set_screen(1,LV_SCR_LOAD_ANIM_NONE,0,0);tick();
    StandbyAppearance appearance;
    auto *screen=objects.standby_screen;
    const auto children=lv_obj_get_child_cnt(screen);
    appearance.content=StandbyContent::Text;std::strcpy(appearance.text,"MY COFFEE 123");
    auto *label=updateStandbyDisplay(screen,objects.obj3,objects.time,appearance,"10:27",foreground);
    assert(label && lv_obj_has_flag(objects.obj3,LV_OBJ_FLAG_HIDDEN));
    assert(!std::strcmp(lv_label_get_text(label),"MY COFFEE 123"));
    paint();save(directory,"standby-custom-text.ppm");
    std::strcpy(appearance.text,"ABCDEFGHIJKLMNOPQRSTUVWX");
    updateStandbyDisplay(screen,objects.obj3,objects.time,appearance,"10:27",foreground);paint();
    assert(lv_obj_get_height(label)<=110);save(directory,"standby-text-max.ppm");
    appearance.content=StandbyContent::Clock;
    updateStandbyDisplay(screen,objects.obj3,objects.time,appearance,"10:27",foreground);paint();
    assert(lv_obj_has_flag(objects.obj3,LV_OBJ_FLAG_HIDDEN) && lv_obj_get_style_opa(objects.time,0)==LV_OPA_TRANSP);
    assert(!std::strcmp(lv_label_get_text(label),"10:27"));save(directory,"standby-clock24.ppm");
    updateStandbyDisplay(screen,objects.obj3,objects.time,appearance,"10:27 PM",foreground);paint();
    assert(lv_obj_get_width(label)<=300);save(directory,"standby-clock12.ppm");
    for(auto *icon:{objects.wifi_icon,objects.bluetooth_icon}) {
        assert(lv_obj_is_visible(icon));
        assertPreviewPixels(icon);
    }
    updateStandbyDisplay(screen,objects.obj3,objects.time,appearance,"",foreground);
    assert(!std::strcmp(lv_label_get_text(label),"--:--"));
    assert(lv_obj_get_child_cnt(screen)==children+1); // updates reuse exactly one view
    assert(lv_obj_get_style_text_color(label,0).full==foreground.full);
    appearance.content=StandbyContent::Logo;tick();
    updateStandbyDisplay(screen,objects.obj3,objects.time,appearance,"10:27",foreground);
    assert(!lv_obj_has_flag(objects.obj3,LV_OBJ_FLAG_HIDDEN) && lv_obj_has_flag(label,LV_OBJ_FLAG_HIDDEN));
    // Recreated screen gets its own view, independent of the deleted prior one.
    eez_flow_set_screen(4,LV_SCR_LOAD_ANIM_NONE,0,0);tick();eez_flow_set_screen(1,LV_SCR_LOAD_ANIM_NONE,0,0);tick();
    appearance.content=StandbyContent::Text;
    assert(updateStandbyDisplay(objects.standby_screen,objects.obj3,objects.time,appearance,"10:27",foreground));
    printf("PASS: %s rotary single/multi/touch/deletion/disabled and standby text/clock/layout/recreation\n",theme?"Light":"Dark");
}
