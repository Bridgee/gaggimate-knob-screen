#pragma once
#ifdef GAGGIMATE_KNOB
#include "VerifiedScreenDownload.h"
#include <esp_ota_ops.h>

namespace screen_ota {
class SlotImage final : public ImageSink {
  public:
    explicit SlotImage(const esp_partition_t *partition) : partition(partition) {}
    ~SlotImage() override { abort(); }
    bool begin(size_t bytes) override {
        abort();
        const auto *running = esp_ota_get_running_partition();
        if (!running || !partition || running->address == partition->address || !bytes || bytes > partition->size)
            return false;
        // Erase as sequential chunks arrive. A bulk erase stalled telemetry for
        // several seconds on K718 and caused the live-controller guard to abort.
        open = esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &handle) == ESP_OK;
        if (open) expected = bytes;
        return open;
    }
    bool write(const uint8_t *data, size_t bytes) override {
        if (!open || bytes > expected - written || esp_ota_write(handle, data, bytes) != ESP_OK) return false;
        written += bytes; return true;
    }
    bool finish() override {
        if (!open || written != expected) return false;
        open = false; return esp_ota_end(handle) == ESP_OK;
    }
    bool read(size_t offset, uint8_t *data, size_t bytes) override {
        return expected && offset <= expected && bytes <= expected - offset &&
            esp_partition_read(partition, offset, data, bytes) == ESP_OK;
    }
    void abort() override {
        if (open) esp_ota_abort(handle);
        open = false; expected = written = 0;
    }
  private:
    const esp_partition_t *partition;
    esp_ota_handle_t handle = 0;
    size_t expected = 0, written = 0;
    bool open = false;
};
} // namespace screen_ota
#endif
