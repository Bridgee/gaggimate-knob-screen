#ifdef GAGGIMATE_KNOB
#include "Diagnostics.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <rom/ets_sys.h>
#include <cstdarg>
#include <cstdio>
#include <algorithm>

namespace {
QueueHandle_t queue = nullptr;
uint32_t dropped = 0;

// Log producers never wait for USB space. This also replaces Arduino's
// per-character debug hook, which otherwise waits up to100ms per character.
void IRAM_ATTR enqueue(char c) {
    if (!queue) return;
    BaseType_t ok;
    if (xPortInIsrContext()) ok = xQueueSendFromISR(queue, &c, nullptr);
    else ok = xQueueSend(queue, &c, 0);
    if (ok != pdTRUE) __atomic_fetch_add(&dropped, 1, __ATOMIC_RELAXED);
}

void writer(void *) {
    char bytes[64];
    for (;;) {
        // All potentially blocking CDC operations are confined to this task.
        // A closed/slow host may lose diagnostics, never stall UI/BLE work.
        if (!Serial) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
        const int space = Serial.availableForWrite();
        const unsigned capacity = space > 0 ? std::min<unsigned>(sizeof(bytes), space) : 0;
        unsigned count = 0;
        while (count < capacity && xQueueReceive(queue, &bytes[count], 0) == pdTRUE) ++count;
        if (count) Serial.write(reinterpret_cast<const uint8_t *>(bytes), count);
        else vTaskDelay(pdMS_TO_TICKS(5));
    }
}
}

void knobDiagnosticBegin() {
    queue = xQueueCreate(2048, sizeof(char));
    if (!queue) return; // Diagnostics are optional; the display must run.
    // A zero timeout underflows Arduino2 HWCDC's retry counter; use1ms for
    // the background writer, never call it from producer tasks.
    Serial.setTxTimeoutMs(1);
    Serial.setDebugOutput(true);
    ets_install_putc2(enqueue);
    if (xTaskCreatePinnedToCore(writer, "K718::log", configMINIMAL_STACK_SIZE * 3,
                              nullptr, 1, nullptr, 0) != pdPASS) {
        ets_install_putc2(nullptr);
        vQueueDelete(queue);
        queue = nullptr;
    }
}

void knobDiagnosticPrintf(const char *format, ...) {
    char message[256];
    va_list args; va_start(args, format);
    const int length = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    const int count = std::min<int>(length, sizeof(message) - 1);
    for (int i = 0; i < count; ++i) enqueue(message[i]);
}

uint32_t knobDiagnosticDropped() { return __atomic_load_n(&dropped, __ATOMIC_RELAXED); }
#endif
