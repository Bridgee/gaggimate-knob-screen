#include "main.h"
#include "drivers/GuitionKnob/Diagnostics.h"
#ifdef GAGGIMATE_KNOB
#include "drivers/GuitionKnob/Hardware.h"
#include "drivers/GuitionKnob/BootHealth.h"
#endif

#ifndef GAGGIMATE_HEADLESS
#include <lvgl.h>
#endif

Controller controller;

void setup() {
    Serial.begin(115200);
#ifdef GAGGIMATE_KNOB
    knobDiagnosticBegin();
    guition_knob::bootHealthBegin();
    knobDiagnosticPrintf("K718 display: starting original UI; nonblocking diagnostics\n");
#endif
    controller.setup();
#ifdef GAGGIMATE_KNOB
    knobDiagnosticPrintf("K718 display: setup complete; heap=%u, PSRAM free=%u\n", ESP.getFreeHeap(), ESP.getFreePsram());
#endif
}

void loop() {
    controller.loop();
#ifdef GAGGIMATE_KNOB
    guition_knob::bootHealthMainPulse();
    static unsigned long lastDiagnostic = 0;
    if (millis() - lastDiagnostic >= 30000) {
        lastDiagnostic = millis();
        knobDiagnosticPrintf("K718 display: alive; heap=%u, PSRAM free=%u logDropped=%u\n",
                             ESP.getFreeHeap(), ESP.getFreePsram(), knobDiagnosticDropped());
        const auto h = guition_knob::Hardware::instance().snapshot();
        knobDiagnosticPrintf("K718 hardware: ready=%d encoder=%d ticks=%ld SYS=%umV batteryEstimate=%d busy=%d\n",
                             h.ready, h.encoderReady, static_cast<long>(h.encoderTicks), h.railMv, h.batteryPercent, h.busy);
    }
#endif
    delay(50);
}
