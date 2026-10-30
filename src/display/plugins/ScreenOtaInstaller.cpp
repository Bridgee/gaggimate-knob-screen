#ifdef GAGGIMATE_KNOB
#include "ScreenOtaInstaller.h"
#include "NetworkWorker.h"
#include "SdkOtaImage.h"
#include "VerifiedScreenDownload.h"
#include <EspHttpTransport.h>
#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <sdkconfig.h>
#include <mbedtls/sha256.h>
#include <cstdio>

namespace screen_ota {
namespace {
// Keep the upstream watchdog protection during flash operations; never unsubscribe tasks.
class ScopedOtaWatchdog {
  public:
    ScopedOtaWatchdog() : active(esp_task_wdt_init(60, true) == ESP_OK) {}
    ~ScopedOtaWatchdog() { if (active) esp_task_wdt_init(CONFIG_ESP_TASK_WDT_TIMEOUT_S, true); }
    const bool active;
};
class Sha256 final : public ImageHash {
  public:
    Sha256() { mbedtls_sha256_init(&context); }
    ~Sha256() override { mbedtls_sha256_free(&context); }
    bool begin() override { return mbedtls_sha256_starts_ret(&context, 0) == 0; }
    bool add(const uint8_t *data, size_t bytes) override { return mbedtls_sha256_update_ret(&context, data, bytes) == 0; }
    std::string finish() override {
        uint8_t hash[32]; char text[65];
        if (mbedtls_sha256_finish_ret(&context, hash) != 0) return {};
        for (unsigned i = 0; i < 32; ++i) snprintf(text + 2 * i, 3, "%02x", hash[i]);
        return text;
    }
  private:
    mbedtls_sha256_context context;
};

}
void Installer::start(const InstallJob &value, std::function<bool()> currentGuard) {
    if (!network_work::post(network_work::Kind::Screen, [this, value, currentGuard = std::move(currentGuard)] {
        run(value, currentGuard);
    })) fail("worker-unavailable");
}
void Installer::report(const char *phase, unsigned progress) {
    std::lock_guard<std::mutex> lock(mutex);
    if (status.phase != phase || status.progress != progress) {
        status.phase = phase; status.progress = progress; changed = true;
    }
}
void Installer::run(const InstallJob &value, const std::function<bool()> &allowed) {
    const auto *running = esp_ota_get_running_partition();
    const auto *boot = esp_ota_get_boot_partition();
    const auto *next = esp_ota_get_next_update_partition(nullptr);
    if (!running || !boot || !next || !allowed() || boot->address != running->address ||
        running->address != value.plan.runningOffset || next->address != value.plan.updateOffset ||
        running->address == next->address || running->size != SlotBytes || next->size != SlotBytes) {
        fail("startup-or-partition-changed"); return;
    }
    ScopedOtaWatchdog watchdog;
    if (!watchdog.active) { fail("watchdog-unavailable"); return; }
    SlotImage image(next); Sha256 hash; EspHttpTransport http(8000); EspDownloadEnv env;
    report("downloading", 0);
    const auto result = stageImage(value.plan, http, env, image, hash, [] { return uint32_t(millis()); }, allowed,
        [this](const char *phase, unsigned progress) { report(phase, progress); });
    if (!result.verified) { fail(result.error); return; }
    { std::lock_guard<std::mutex> lock(mutex); status.verified = true; status.verifiedSha = value.plan.image.sha; }
    boot = esp_ota_get_boot_partition();
    if (!allowed() || !boot || boot->address != running->address) { fail("commit-guard-changed", boot && boot->address != running->address); return; }
    if (value.verifyOnly) {
        std::lock_guard<std::mutex> lock(mutex);
        status.phase = "verified"; status.progress = 100; status.busy = false; changed = true; return;
    }
    report("committing", 95);
    const esp_err_t committed = esp_ota_set_boot_partition(next);
    boot = esp_ota_get_boot_partition();
    if (committed != ESP_OK || !boot || boot->address != next->address) {
        fail("boot-selection-failed", !boot || boot->address != running->address); return;
    }
    report("rebooting", 100);
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP.restart();
}
} // namespace screen_ota
#endif
