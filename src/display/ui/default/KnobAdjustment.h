#pragma once
#include <lvgl.h>
#include <array>
#include <functional>
#include <utility>
#include "KnobAdjustmentPolicy.h"

namespace gaggimate_ui {
enum class AdjustmentId : uint8_t { None, Temperature, Duration, Weight };
struct AdjustmentTarget {
    AdjustmentId id = AdjustmentId::None;
    lv_obj_t *row = nullptr, *lower = nullptr, *raise = nullptr, *value = nullptr;
};
struct AdjustmentTargets {
    std::array<AdjustmentTarget, 3> items{};
    uint8_t count = 0;
};
AdjustmentTargets visibleKnobTargets(int screen);

class KnobAdjustment {
  public:
    KnobAdjustment();
    ~KnobAdjustment();
    void update(int screen, int32_t raw, bool enabled, const AdjustmentTargets &targets, lv_color_t focusColor);
    void calibrate(int edges, int direction) { decoder.calibrate(edges, direction); }
    void setGrindAdjustment(std::function<void(AdjustmentId, int)> adjust) { grindAdjustment = std::move(adjust); }
    AdjustmentId selected() const { return selectedId; }
    uint8_t targetCount() const { return count; }
  private:
    struct Binding {
        KnobAdjustment *owner = nullptr;
        AdjustmentTarget target;
        bool rowWasClickable = false;
    };
    std::array<Binding, 3> bindings{};
    lv_style_t focusStyle;
    KnobStepDecoder decoder;
    std::function<void(AdjustmentId, int)> grindAdjustment;
    int page = -1;
    uint8_t count = 0;
    bool ready = false, styleReady = false, haveFocusColor = false;
    uint32_t focusRgb = 0;
    AdjustmentId selectedId = AdjustmentId::None;
    void unbind();
    void highlight();
    void choose(AdjustmentId id);
    static void pressed(lv_event_t *event);
    static void deleted(lv_event_t *event);
};
} // namespace gaggimate_ui
