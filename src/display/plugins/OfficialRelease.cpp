#include "OfficialRelease.h"
#ifdef GAGGIMATE_KNOB
#include "GitHubOTA.h"
#include "OtaPolicy.h"
namespace official_release {
void Discovery::request(const std::string &channel, const std::string &controllerVersion) {
    const auto generation = mailbox.enqueue(channel, controllerVersion);
    if (!task && xTaskCreate(worker, "OfficialRelease", 8192, this, 1, &task) != pdPASS) {
        task = nullptr;
        Snapshot failed; failed.status = "error"; failed.message = "Official release checker could not start.";
        mailbox.publish(generation, failed); return;
    }
    xTaskNotifyGive(task);
}
void Discovery::worker(void *context) {
    auto *self = static_cast<Discovery *>(context);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        Request request;
        while (self->mailbox.take(request)) {
            // Fresh, task-owned state cannot reuse an old channel/version on failure.
            GitHubOTA checker(FirmwareOtaPolicy::displayComparisonVersion,
                request.controllerVersion.empty() ? "v0.0.0" : request.controllerVersion.c_str(),
                String("https://github.com/jniebuhr/gaggimate/releases/") +
                    (request.channel == "nightly" ? "tag/nightly" : "latest"),
                [](uint8_t) {}, [](uint8_t, int) {});
            checker.setCheckHandshakeTimeout(8);
            checker.checkForUpdates();
            Snapshot result; result.version = checker.getCurrentVersion().c_str();
            if (result.version.empty()) {
                result.status = "error"; result.message = "Official update check failed; refresh to retry.";
            } else {
                result.status = "ready";
                result.displayUpdate = checker.isUpdateAvailable(false);
                result.controllerUpdate = !request.controllerVersion.empty() && checker.isUpdateAvailable(true);
            }
            self->mailbox.publish(request.generation, result);
        }
    }
}
}
#endif
