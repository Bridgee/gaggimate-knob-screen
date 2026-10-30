#include "GuitionKnobDriver.h"
#include "GuitionKnob/Diagnostics.h"
#include "GuitionKnob/Hardware.h"
#include "GuitionKnob/BootHealth.h"
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include <algorithm>

using namespace guition_knob;

void GuitionKnobDriver::init() {
    if (!panel.begin()) {
        knobDiagnosticPrintf("K718: panel initialization failed\n");
        while (true) delay(1000);
    }
    bootHealthPanel(panel.hasTouch());
    Hardware::instance().begin();
    lv_init();
    static_assert(sizeof(lv_color_t) == sizeof(uint16_t), "K718 requires RGB565");
    renderBuffer = static_cast<lv_color_t *>(heap_caps_malloc(PanelSize * PanelSize * sizeof(lv_color_t),
                                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!renderBuffer) {
        knobDiagnosticPrintf("K718: native framebuffer allocation failed\n");
        while (true) delay(1000);
    }
    lv_disp_draw_buf_init(&drawBuffer, renderBuffer, nullptr, PanelSize * PanelSize);
    lv_disp_drv_init(&displayDriver);
    displayDriver.hor_res = PanelSize;
    displayDriver.ver_res = PanelSize;
    displayDriver.draw_buf = &drawBuffer;
    // Native dirty rectangles: no logical480 canvas or frame resampling.
    displayDriver.full_refresh = 0;
    displayDriver.flush_cb = flush;
    displayDriver.monitor_cb = monitor;
    displayDriver.user_data = this;
    lv_disp_drv_register(&displayDriver);

    lv_indev_drv_init(&touchDriver);
    touchDriver.type = LV_INDEV_TYPE_POINTER;
    touchDriver.read_cb = touch;
    touchDriver.scroll_limit = 8;
    touchDriver.gesture_limit = 38;
    touchDriver.user_data = this;
    lv_indev_drv_register(&touchDriver);
    knobDiagnosticPrintf("K718: native360 LVGL, direct pixel transfer, native touch coordinates\n");
}

void GuitionKnobDriver::flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
    auto *self = static_cast<GuitionKnobDriver *>(driver->user_data);
    if (area->x1 < 0 || area->y1 < 0 || area->x2 >= PanelSize || area->y2 >= PanelSize ||
        area->x2 < area->x1 || area->y2 < area->y1) {
        ESP_LOGE("K718", "Invalid native LVGL flush bounds");
        lv_disp_flush_ready(driver);
        return;
    }
    const uint16_t width = area->x2 - area->x1 + 1, height = area->y2 - area->y1 + 1;
    // QSPI copies into its DMA buffer and waits before reusing pixels.
    self->panel.display().draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(pixels), width, height);
    self->sentPixels += width * height;
    bootHealthFrame();
    lv_disp_flush_ready(driver);
}

void GuitionKnobDriver::monitor(lv_disp_drv_t *driver, uint32_t time, uint32_t pixels) {
    auto *self = static_cast<GuitionKnobDriver *>(driver->user_data);
    ++self->frames;
    self->renderMs += time;
    self->maxRenderMs = std::max(self->maxRenderMs, time);
    self->renderedPixels += pixels;
    const uint32_t now = millis();
    if (now - self->lastReport >= 10000) {
        knobDiagnosticPrintf("K718 native render: frames=%u avg=%ums max=%ums rendered=%u/frame sent=%u/frame\n",
                      self->frames, self->renderMs / self->frames, self->maxRenderMs,
                      self->renderedPixels / self->frames, self->sentPixels / self->frames);
        self->frames = self->renderMs = self->maxRenderMs = self->renderedPixels = self->sentPixels = 0;
        self->lastReport = now;
    }
}

void GuitionKnobDriver::touch(lv_indev_drv_t *driver, lv_indev_data_t *data) {
    auto *self = static_cast<GuitionKnobDriver *>(driver->user_data);
    uint16_t x, y;
    if (self->panel.readTouch(x, y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

bool GuitionKnobDriver::installSDCard() {
    SD_MMC.setPins(39, 38, 40, 41, 48, 47);
    return SD_MMC.begin("/sdcard", false, false, SDMMC_FREQ_DEFAULT, 10);
}
