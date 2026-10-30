"""Exercise the actual startup supervisor with a deterministic clock and OTA SDK."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class BootSupervisorTest(unittest.TestCase):
    def test_confirmation_deadline_sdk_failure_and_task_creation(self):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            (out / 'sdkconfig.h').write_text('#define CONFIG_APP_ROLLBACK_ENABLE 1\n#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1\n')
            (out / 'Arduino.h').write_text('''#pragma once
#include <cstdint>
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
using TaskFunction = void (*)(void *);
uint32_t millis();
int xTaskCreate(TaskFunction, const char *, unsigned, void *, unsigned, void *);
void vTaskDelay(unsigned);
void vTaskDelete(void *);
''')
            (out / 'esp_ota_ops.h').write_text('''#pragma once
#include <cstdint>
using esp_err_t = int;
constexpr int ESP_OK = 0;
struct esp_partition_t { uint32_t address; };
enum esp_ota_img_states_t { ESP_OTA_IMG_PENDING_VERIFY = 1, ESP_OTA_IMG_VALID = 2 };
const esp_partition_t *esp_ota_get_running_partition();
esp_err_t esp_ota_get_state_partition(const esp_partition_t *, esp_ota_img_states_t *);
esp_err_t esp_ota_mark_app_valid_cancel_rollback();
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot();
''')
            (out / 'main.cpp').write_text('''#include <display/drivers/GuitionKnob/BootHealth.h>
#include <display/drivers/GuitionKnob/BootHealthPolicy.h>
#include <Arduino.h>
#include <esp_ota_ops.h>
#include <cassert>
#include <cstring>
#include <string>
using namespace guition_knob;
extern "C" bool verifyRollbackLater();
namespace { uint32_t now = 0; int confirms = 0, rollbacks = 0; TaskFunction task = nullptr; std::string mode; }
uint32_t millis() { return now; }
int xTaskCreate(TaskFunction f, const char *, unsigned, void *, unsigned, void *) {
    if (mode == "task-failure") return 0;
    task = f; return pdPASS;
}
void vTaskDelay(unsigned ms) {
    now += ms;
    bootHealthMainPulse(); bootHealthUiPulse();
    if (mode != "stalled-logic") bootHealthLogicPulse();
}
void vTaskDelete(void *) {}
const esp_partition_t *esp_ota_get_running_partition() { static esp_partition_t p{0x650000}; return &p; }
esp_err_t esp_ota_get_state_partition(const esp_partition_t *, esp_ota_img_states_t *s) {
    *s = mode == "settled" ? ESP_OTA_IMG_VALID : ESP_OTA_IMG_PENDING_VERIFY; return ESP_OK;
}
esp_err_t esp_ota_mark_app_valid_cancel_rollback() {
    assert(now >= BootHealthWindow::StableMs && now < BootHealthWindow::DeadlineMs);
    ++confirms; return mode == "confirmation-failure" ? 9 : ESP_OK;
}
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot() {
    assert(now >= BootHealthWindow::DeadlineMs); ++rollbacks; return 9;
}
void knobDiagnosticPrintf(const char *, ...) {}
int main(int argc, char **argv) {
    assert(argc == 2); mode = argv[1];
    assert(verifyRollbackLater());
    bootHealthBegin(); assert(bootHealthSlot() == 0x650000);
    bootHealthSetup(true, true); bootHealthPanel(true); bootHealthFrame();
    if (task) task(nullptr);
    if (mode == "good") {
        assert(confirms == 1 && rollbacks == 0 && !bootHealthPending());
        assert(std::strcmp(bootHealthState(), "confirmed") == 0);
    } else if (mode == "settled") {
        assert(confirms == 0 && rollbacks == 0 && !bootHealthPending());
        assert(std::strcmp(bootHealthState(), "healthy") == 0);
    } else if (mode == "confirmation-failure") {
        assert(confirms == 1 && rollbacks == 0 && bootHealthPending());
        assert(std::strcmp(bootHealthState(), "confirmation-failed") == 0);
    } else {
        assert(confirms == 0 && bootHealthPending());
        assert(rollbacks == (mode == "stalled-logic" ? 1 : 0));
        assert(std::strcmp(bootHealthState(), "failed") == 0);
    }
}
''')
            binary = out / 'supervisor'
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                            '-DGAGGIMATE_KNOB', '-I' + str(out), '-I' + str(ROOT / 'src'),
                            str(ROOT / 'src/display/drivers/GuitionKnob/BootHealth.cpp'), str(out / 'main.cpp'),
                            '-o', str(binary)], check=True)
            for mode in ('good', 'settled', 'stalled-logic', 'confirmation-failure', 'task-failure'):
                subprocess.run([str(binary), mode], check=True)
