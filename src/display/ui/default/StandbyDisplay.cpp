#include "StandbyDisplay.h"
#include <cstring>
namespace gaggimate_ui {
namespace { char Marker; }
lv_obj_t *updateStandbyDisplay(lv_obj_t *screen, lv_obj_t *logo, lv_obj_t *smallClock,
                             const StandbyAppearance &appearance, const char *timeText, lv_color_t foreground) {
    if(!screen || !logo || !smallClock)return nullptr;
    lv_obj_update_layout(screen);
    const auto clockOpacity=appearance.content==StandbyContent::Clock?LV_OPA_TRANSP:LV_OPA_COVER;
    if(lv_obj_get_style_opa(smallClock,0)!=clockOpacity) {
        lv_obj_set_style_opa(smallClock,clockOpacity,0);
        lv_obj_invalidate(screen);
    }
    lv_obj_t *label=nullptr;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(screen);++i) {
        auto *child=lv_obj_get_child(screen,i);
        if(lv_obj_get_user_data(child)==&Marker){label=child;break;}
    }
    if(appearance.content==StandbyContent::Logo) {
        lv_obj_clear_flag(logo,LV_OBJ_FLAG_HIDDEN);
        if(label)lv_obj_add_flag(label,LV_OBJ_FLAG_HIDDEN);
        // ui_tick restores the original small clock visibility on mode change.
        return label;
    }
    lv_obj_add_flag(logo,LV_OBJ_FLAG_HIDDEN);
    if(!label) {
        label=lv_label_create(screen);lv_obj_set_user_data(label,&Marker);
        lv_obj_clear_flag(label,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
        lv_obj_set_style_align(label,LV_ALIGN_CENTER,0);
        lv_obj_set_pos(label,0,0);
        lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
    }
    lv_obj_clear_flag(label,LV_OBJ_FLAG_HIDDEN);
    if(lv_obj_get_width(label)!=lv_obj_get_width(screen)-60)lv_obj_set_width(label,lv_obj_get_width(screen)-60);
    const bool clock=appearance.content==StandbyContent::Clock;
    const char *text=clock?(timeText && *timeText?timeText:"--:--"):appearance.text;
    const lv_font_t *font=clock?&lv_font_montserrat_48:
        std::strlen(text)<=12?&lv_font_montserrat_34:&lv_font_montserrat_26;
    if(clock) {
        // Opacity suppresses the old small clock without fighting EEZ hidden-state updates.
        lv_point_t size{};
        lv_txt_get_size(&size,text,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        if(size.x>lv_obj_get_width(screen)-60)font=&lv_font_montserrat_34;
    }
    if(lv_obj_get_style_text_font(label,0)!=font)lv_obj_set_style_text_font(label,font,0);
    if(lv_obj_get_style_text_color(label,0).full!=foreground.full)lv_obj_set_style_text_color(label,foreground,0);
    if(std::strcmp(lv_label_get_text(label),text))lv_label_set_text(label,text);
    return label;
}
} // namespace gaggimate_ui
