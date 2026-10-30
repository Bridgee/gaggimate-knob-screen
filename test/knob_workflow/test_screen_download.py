from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
from k718_ota_config import qualification_enabled
ROOT = Path(__file__).resolve().parents[2]

class ScreenDownloadTest(unittest.TestCase):
    def test_candidate_permission_cannot_enable_stable_builds(self):
        self.assertFalse(qualification_enabled({}))
        good = dict(port_version='0.4.0-rc.3', release_channel='candidate', screen_ota_qualification=True)
        self.assertTrue(qualification_enabled(good))
        for changes in ({'screen_ota_qualification': 1}, {'release_channel': 'stable'}, {'port_version': '0.4.0'}):
            with self.assertRaises(RuntimeError): qualification_enabled(dict(good, **changes))

    def test_actual_stream_adapter_and_failure_paths(self):
        with tempfile.TemporaryDirectory() as folder:
            binary = Path(folder) / 'screen-download'
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                            '-I' + str(ROOT / 'src'), '-I' + str(ROOT / 'lib/OTA/src'),
                            '-I' + str(ROOT / '.pio/libdeps/display-knob/ArduinoJson/src'),
                            str(ROOT / 'test/knob_native/check_screen_download.cpp'),
                            str(ROOT / 'lib/OTA/src/ResumableDownloader.cpp'), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

    def test_actual_permission_queue_rejects_stale_component_and_busy_requests(self):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder); (out / 'freertos').mkdir()
            (out / 'freertos/FreeRTOS.h').write_text('#pragma once\n')
            (out / 'freertos/task.h').write_text('#pragma once\nusing TaskHandle_t = void *;\n')
            binary = out / 'screen-queue'
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                            '-DGAGGIMATE_KNOB', '-I' + str(out), '-I' + str(ROOT / 'src'),
                            '-I' + str(ROOT / '.pio/libdeps/display-knob/ArduinoJson/src'),
                            str(ROOT / 'test/knob_native/check_screen_queue.cpp'),
                            str(ROOT / 'src/display/plugins/ScreenOtaQueue.cpp'), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=10)

    def test_actual_shared_network_worker_reservation_priority_and_failure(self):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder); (out / 'freertos').mkdir()
            (out / 'freertos/FreeRTOS.h').write_text('#pragma once\n#define pdTRUE 1\n#define pdPASS 1\n#define portMAX_DELAY 0xffffffffu\n')
            (out / 'freertos/task.h').write_text('#pragma once\n#include "FreeRTOS.h"\nusing TaskHandle_t = void *;\nint xTaskCreate(void (*)(void *), const char *, unsigned, void *, unsigned, TaskHandle_t *);\nvoid xTaskNotifyGive(TaskHandle_t);\nunsigned ulTaskNotifyTake(int, unsigned);\n')
            binary = out / 'network-worker'
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                            '-DGAGGIMATE_KNOB', '-I' + str(out), '-I' + str(ROOT / 'src'),
                            str(ROOT / 'test/knob_native/check_network_worker.cpp'),
                            str(ROOT / 'src/display/plugins/NetworkWorker.cpp'), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=10)

    def test_actual_sdk_image_sequential_erase_bounds_and_abort(self):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            (out / 'esp_ota_ops.h').write_text('#pragma once\n#include <cstddef>\n#include <cstdint>\n#define ESP_OK 0\n#define OTA_WITH_SEQUENTIAL_WRITES 0xfffffffeu\nstruct esp_partition_t { uint32_t address; size_t size; };\nusing esp_ota_handle_t = unsigned;\nconst esp_partition_t *esp_ota_get_running_partition();\nint esp_ota_begin(const esp_partition_t *, size_t, esp_ota_handle_t *);\nint esp_ota_write(esp_ota_handle_t, const void *, size_t);\nint esp_ota_end(esp_ota_handle_t);\nint esp_ota_abort(esp_ota_handle_t);\nint esp_partition_read(const esp_partition_t *, size_t, void *, size_t);\n')
            binary = out / 'sdk-image'
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                            '-DGAGGIMATE_KNOB', '-I' + str(out), '-I' + str(ROOT / 'src'),
                            '-I' + str(ROOT / 'lib/OTA/src'), '-I' + str(ROOT / '.pio/libdeps/display-knob/ArduinoJson/src'),
                            str(ROOT / 'test/knob_native/check_sdk_ota_image.cpp'), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=10)
