#pragma once
#include "ScreenOtaPolicy.h"
#include <ResumableDownloader.h>
#include <functional>
#include <algorithm>

namespace screen_ota {
// Staging cannot select a boot partition: that operation is deliberately absent
// from this interface. The caller may commit only after stageImage succeeds.
struct ImageSink {
    virtual ~ImageSink() = default;
    virtual bool begin(size_t bytes) = 0;
    virtual bool write(const uint8_t *, size_t bytes) = 0;
    virtual bool finish() = 0;
    virtual bool read(size_t offset, uint8_t *, size_t bytes) = 0;
    virtual void abort() = 0;
};
struct ImageHash {
    virtual ~ImageHash() = default;
    virtual bool begin() = 0;
    virtual bool add(const uint8_t *, size_t bytes) = 0;
    virtual std::string finish() = 0;
};
struct StagingResult { bool verified = false; const char *error = "transfer-interrupted"; size_t bytes = 0; };
using StageProgress = std::function<void(const char *, unsigned)>;
inline StagingResult stageImage(const Plan &plan, HttpTransport &http, DownloadEnv &env,
                               ImageSink &image, ImageHash &hash, const std::function<uint32_t()> &clock,
                               const std::function<bool()> &allowed, StageProgress progress = {}) {
    StagingResult result;
    if (!ownAsset(plan.image, plan.tag, "display-knob-firmware.bin", 288, SlotBytes))
        return {false, "invalid-asset", 0};
    const uint32_t start = clock();
    bool prepared = false;
    auto permitted = [&] { return allowed() && uint32_t(clock() - start) < 180000; };
    DownloadSink sink;
    sink.prepare = [&](size_t size) {
        if (!permitted()) { result.error = "cancelled-or-expired"; return false; }
        if (size != plan.image.bytes) { result.error = "length-mismatch"; return false; }
        if (!image.begin(size) || !hash.begin()) { result.error = "staging-unavailable"; return false; }
        prepared = true; result.bytes = 0; return true;
    };
    sink.write = [&](const uint8_t *data, size_t size) {
        if (!permitted()) { result.error = "cancelled-or-expired"; return false; }
        if (!prepared || size > plan.image.bytes - result.bytes) { result.error = "length-mismatch"; return false; }
        if (!hash.add(data, size) || !image.write(data, size)) { result.error = "staging-write-failed"; return false; }
        result.bytes += size; return true;
    };
    sink.restart = [&] { image.abort(); prepared = false; result.bytes = 0; return permitted(); };
    DownloadPolicy policy;
    policy.maxAttempts = 2; policy.maxTotalAttempts = 4; policy.maxRedirects = 3;
    policy.initialBackoffMs = 500; policy.maxBackoffMs = 1000; policy.networkWaitMs = 5000;
    ResumableDownloader downloader(http, env, plan.image.url, sink,
        [&](size_t bytes, size_t) { if (progress) progress("downloading", unsigned(bytes * 80 / plan.image.bytes)); }, policy);
    if (!downloader.run() || !prepared || result.bytes != plan.image.bytes || !permitted()) {
        image.abort(); return result;
    }
    if (hash.finish() != plan.image.sha) { image.abort(); return {false, "checksum-mismatch", result.bytes}; }
    if (!image.finish()) { image.abort(); return {false, "invalid-image", result.bytes}; }
    if (progress) progress("readback", 80);
    uint8_t *buffer = env.allocBuffer(4096);
    if (!buffer || !hash.begin()) {
        if (buffer) env.freeBuffer(buffer);
        return {false, "readback-unavailable", result.bytes};
    }
    bool good = true;
    for (size_t offset = 0; offset < result.bytes;) {
        const size_t size = std::min(size_t(4096), result.bytes - offset);
        if (!permitted() || !image.read(offset, buffer, size) || !hash.add(buffer, size)) { good = false; break; }
        offset += size; env.yieldAfterChunk();
        if (progress) progress("readback", 80 + unsigned(offset * 15 / result.bytes));
    }
    env.freeBuffer(buffer);
    if (!good || hash.finish() != plan.image.sha) return {false, "readback-mismatch", result.bytes};
    return {true, "", result.bytes};
}
} // namespace screen_ota
