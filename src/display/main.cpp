#include "main.h"
#include "drivers/GuitionKnob/Diagnostics.h"
#ifdef GAGGIMATE_KNOB
#include "drivers/GuitionKnob/Hardware.h"
#include "drivers/GuitionKnob/BootHealth.h"
#include <esp_heap_caps.h>
#include <WiFi.h>
#include <esp32-hal-psram.h>
#endif

#ifndef GAGGIMATE_HEADLESS
#include <lvgl.h>
#endif

#ifdef GAGGIMATE_KNOB_VALIDATION
#include "validation/ValidationMarker.h"
#endif

Controller controller;

void setup() {
    Serial.begin(115200);
#ifdef GAGGIMATE_KNOB_VALIDATION
    Serial.println(knob_validation::imageMarker());
#endif
#ifdef GAGGIMATE_KNOB
    // Preserve internal RAM for Wi-Fi/TCP, RTOS stacks and DMA. The framework
    // otherwise keeps ordinary allocations up to4096 bytes in internal RAM.
    // Capability-specific allocations retain their explicit requirements.
    if (psramFound()) heap_caps_malloc_extmem_enable(0);
    // Reserve the pinned SDK Wi-Fi TX/RX pools instead of allocating up to32
    // DMA TX buffers on demand while profile images and HTTPS share the link.
    WiFi.useStaticBuffers(true);
    knobDiagnosticBegin();
    // Subscribe before station startup so early association/authentication failures are visible.
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        const unsigned reason = event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED ? info.wifi_sta_disconnected.reason : 0;
        knobDiagnosticPrintf("K718 WiFi event: kind=%u reason=%u status=%u\n",
                             static_cast<unsigned>(event), reason, static_cast<unsigned>(WiFi.status()));
    });
    heap_caps_register_failed_alloc_callback([](size_t bytes, uint32_t caps, const char *function) {
        knobDiagnosticPrintf("K718 allocation failed: bytes=%u caps=0x%x function=%s\n",
                             static_cast<unsigned>(bytes), static_cast<unsigned>(caps), function ? function : "?");
    });
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
        knobDiagnosticPrintf("K718 display: alive; heap=%u largest=%u min=%u PSRAM free=%u logDropped=%u\n",
                             ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                             heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                             ESP.getFreePsram(), knobDiagnosticDropped());
        constexpr uint32_t dmaCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
        knobDiagnosticPrintf("K718 DMA: free=%u largest=%u min=%u\n",
                             heap_caps_get_free_size(dmaCaps), heap_caps_get_largest_free_block(dmaCaps),
                             heap_caps_get_minimum_free_size(dmaCaps));
        const auto h = guition_knob::Hardware::instance().snapshot();
        knobDiagnosticPrintf("K718 hardware: ready=%d encoder=%d ticks=%ld SYS=%umV batteryEstimate=%d busy=%d\n",
                             h.ready, h.encoderReady, static_cast<long>(h.encoderTicks), h.railMv, h.batteryPercent, h.busy);
    }
#endif
    delay(50);
}
