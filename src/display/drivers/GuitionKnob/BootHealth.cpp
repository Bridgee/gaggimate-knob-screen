#ifdef GAGGIMATE_KNOB
#include "BootHealth.h"
#include "BootHealthPolicy.h"
#include "Diagnostics.h"
#include <Arduino.h>
#include <atomic>
#include <esp_ota_ops.h>
#include <sdkconfig.h>

#if !CONFIG_APP_ROLLBACK_ENABLE || !CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
#error K718 startup health requires bootloader rollback support
#endif

// Arduino otherwise accepts PENDING_VERIFY before setup has even run.
extern "C" bool verifyRollbackLater() { return true; }

namespace guition_knob {
namespace {
std::atomic<bool> setupReady{false}, storageReady{false}, settingsReady{false};
std::atomic<bool> panelReady{false}, touchReady{false}, frameReady{false}, pending{false};
std::atomic<uint32_t> mainAt{0}, uiAt{0}, logicAt{0}, mainCount{0}, uiCount{0}, logicCount{0}, slot{0};
enum State { Starting, Healthy, Confirmed, Failed, ConfirmFailed };
std::atomic<State> state{Starting};

void supervise(void *) {
    BootHealthWindow window;
    const uint32_t start = millis();
    for (;;) {
        const uint32_t now = millis();
        BootHealthSample s;
        s.setup = setupReady; s.storage = storageReady; s.settings = settingsReady;
        s.panel = panelReady; s.touch = touchReady; s.frame = frameReady;
        s.mainAt = mainAt; s.uiAt = uiAt; s.logicAt = logicAt;
        s.mainCount = mainCount; s.uiCount = uiCount; s.logicCount = logicCount;
        if (window.observe(now, s)) {
            if (pending) {
                const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
                state = result == ESP_OK ? Confirmed : ConfirmFailed;
                if (result == ESP_OK) pending = false;
                knobDiagnosticPrintf("K718 boot health: confirmation result=%d\n", result);
            } else state = Healthy;
            break;
        }
        if (now - start >= BootHealthWindow::DeadlineMs) {
            state = Failed;
            knobDiagnosticPrintf("K718 boot health: local startup failed; pending=%d\n", bool(pending));
            if (pending) {
                // If no valid fallback exists, remain diagnosable over USB/network.
                const esp_err_t result = esp_ota_mark_app_invalid_rollback_and_reboot();
                knobDiagnosticPrintf("K718 boot health: rollback unavailable=%d\n", result);
            }
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    vTaskDelete(nullptr);
}
}
void bootHealthBegin() {
    const auto *running = esp_ota_get_running_partition();
    esp_ota_img_states_t otaState;
    if (running) {
        slot = running->address;
        pending = esp_ota_get_state_partition(running, &otaState) == ESP_OK && otaState == ESP_OTA_IMG_PENDING_VERIFY;
    }
    if (xTaskCreate(supervise, "K718 boot health", 4096, nullptr, 1, nullptr) != pdPASS) state = Failed;
}
void bootHealthSetup(bool storage, bool settings) { storageReady = storage; settingsReady = settings; setupReady = true; }
void bootHealthPanel(bool touch) { touchReady = touch; panelReady = true; }
void bootHealthFrame() { frameReady = true; }
void bootHealthMainPulse() { mainAt = millis(); ++mainCount; }
void bootHealthUiPulse() { uiAt = millis(); ++uiCount; }
void bootHealthLogicPulse() { logicAt = millis(); ++logicCount; }
const char *bootHealthState() {
    switch (state.load()) {
        case Healthy: return "healthy";
        case Confirmed: return "confirmed";
        case Failed: return "failed";
        case ConfirmFailed: return "confirmation-failed";
        default: return "starting";
    }
}
uint32_t bootHealthSlot() { return slot; }
bool bootHealthPending() { return pending; }
} // namespace guition_knob
#endif
