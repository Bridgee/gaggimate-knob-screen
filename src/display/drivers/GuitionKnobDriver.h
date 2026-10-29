#pragma once

#include "Driver.h"
#include "GuitionKnob/KnobPanel.h"
#include <lvgl.h>

class GuitionKnobDriver : public Driver {
  public:
    static GuitionKnobDriver *getInstance() {
        static GuitionKnobDriver instance;
        return &instance;
    }
    bool isCompatible() override { return true; } // Explicit build target; no GPIO probing.
    void init() override;
    void setBrightness(int level) override { panel.setBrightness(level); }
    bool supportsSDCard() override { return true; }
    bool installSDCard() override;

  private:
    guition_knob::KnobPanel panel;
    lv_disp_draw_buf_t drawBuffer{};
    lv_disp_drv_t displayDriver{};
    lv_indev_drv_t touchDriver{};
    lv_color_t *renderBuffer = nullptr;
    uint32_t frames = 0, renderMs = 0, maxRenderMs = 0, renderedPixels = 0, sentPixels = 0;
    uint32_t lastReport = 0;
    static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels);
    static void monitor(lv_disp_drv_t *driver, uint32_t time, uint32_t pixels);
    static void touch(lv_indev_drv_t *driver, lv_indev_data_t *data);
};
