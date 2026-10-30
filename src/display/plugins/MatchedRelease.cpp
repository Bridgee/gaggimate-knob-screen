#ifdef GAGGIMATE_KNOB
#include "MatchedRelease.h"
#include <EspHttpTransport.h>
#include <esp32-hal.h>
#include <esp_heap_caps.h>
#include <display/util/PsramAllocator.h>
#include <mbedtls/sha256.h>
#include <memory>
#include <algorithm>
#include <cstdio>

namespace matched_release {
namespace {
struct HeapFree { void operator()(uint8_t *p) const { heap_caps_free(p); } };
struct Body {
    std::unique_ptr<uint8_t, HeapFree> data;
    size_t size = 0;
    std::string error;
};
Body fetch(const std::string &url, size_t maximum) {
    Body result;
    EspHttpTransport transport(8000);
    if (!transport.begin(url)) { result.error = "Unable to start HTTPS check."; return result; }
    transport.setHeader("Accept", "application/vnd.github+json");
    int status = 0, length = 0;
    for (unsigned redirects = 0;; ++redirects) {
        if (!transport.open(status, length)) { result.error = "Unable to reach the release server."; return result; }
        if (status < 300 || status >= 400) break;
        if (redirects >= 3 || !transport.followRedirect()) { result.error = "Release redirect rejected."; return result; }
    }
    if (status != 200) {
        result.error = status == 403 || status == 429 ? "Release check rate-limited; try again later." :
                       status == 404 ? "Release metadata is unavailable." : "Release server returned an error.";
        return result;
    }
    if (length > int(maximum)) { result.error = "Release metadata exceeds the size limit."; return result; }
    result.data.reset(static_cast<uint8_t *>(heap_caps_malloc(maximum + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    if (!result.data) { result.error = "Not enough memory to check releases."; return result; }
    const unsigned long start = millis();
    while (!transport.isComplete()) {
        if (millis() - start > 20000 || result.size >= maximum) {
            result.error = "Release metadata is incomplete or too large."; return result;
        }
        const int count = transport.read(result.data.get() + result.size, std::min(size_t(2048), maximum - result.size));
        if (count == 0 && transport.isComplete()) break;
        if (count <= 0) { result.error = "Release metadata transfer was interrupted."; return result; }
        result.size += size_t(count);
        vTaskDelay(1);
    }
    if (!result.size || (length > 0 && size_t(length) != result.size)) {
        result.error = "Release metadata length mismatch."; return result;
    }
    result.data.get()[result.size] = 0;
    return result;
}
std::string sha256(const uint8_t *data, size_t size) {
    unsigned char hash[32];
    if (mbedtls_sha256_ret(data, size, hash, 0) != 0) return {};
    char text[65];
    for (unsigned i = 0; i < 32; ++i) snprintf(text + 2 * i, 3, "%02x", hash[i]);
    return text;
}
} // namespace
void Discovery::request() {
    if (busy.exchange(true)) return;
    Snapshot checking; checking.status = "checking"; publish(checking);
    if (!task && xTaskCreate(worker, "MatchedRelease", 8192, this, 1, &task) != pdPASS) {
        Snapshot failed; failed.status = "error"; failed.message = "Release checker could not start.";
        publish(failed); busy = false; task = nullptr; return;
    }
    xTaskNotifyGive(task);
}
void Discovery::worker(void *context) {
    auto *self = static_cast<Discovery *>(context);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        self->publish(self->check());
        self->busy = false;
    }
}
Snapshot Discovery::check() {
    Snapshot result; result.status = "error";
    Body listing = fetch("https://api.github.com/repos/Bridgee/gaggimate-knob-screen/releases?per_page=5", 96 * 1024);
    if (!listing.error.empty()) { result.message = listing.error; return result; }
    JsonDocument filter(&psramAllocator);
    for (const char *key : {"tag_name", "draft", "prerelease", "published_at"}) filter[0][key] = true;
    for (const char *key : {"name", "browser_download_url", "size", "digest"}) filter[0]["assets"][0][key] = true;
    JsonDocument document(&psramAllocator);
    if (deserializeJson(document, static_cast<const uint8_t *>(listing.data.get()), listing.size, DeserializationOption::Filter(filter)) || !document.is<JsonArray>()) {
        result.message = "Invalid release listing."; return result;
    }
    Release release = selectRelease(document.as<JsonArrayConst>(), previews);
    if (release.tag.empty()) {
        result.status = "no-release";
        result.message = previews ? "No published matched release was found." : "No stable matched release was found.";
        return result;
    }
    listing.data.reset(); document.clear();
    Body body = fetch(release.manifest.url, 16384);
    if (!body.error.empty()) { result.message = body.error; return result; }
    if (body.size != release.manifest.bytes || sha256(body.data.get(), body.size) != release.manifest.sha) {
        result.message = "Matched release manifest checksum mismatch."; return result;
    }
    if (deserializeJson(document, static_cast<const uint8_t *>(body.data.get()), body.size) || !parsePair(document.as<JsonVariantConst>(), release, result.pair)) {
        result.message = "Matched release target, schema or asset metadata does not match."; return result;
    }
    result.release = release; result.status = "ready"; result.validatedAt = millis();
    result.message = "Manifest and published asset metadata checked. OTA installation is unverified and disabled.";
    return result;
}
} // namespace matched_release
#endif
