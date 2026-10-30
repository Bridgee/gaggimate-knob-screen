#include <display/plugins/SdkOtaImage.h>
#include <cassert>
#include <cstring>
#include <vector>
namespace {
esp_partition_t active{0x10000, 1024}, next{0x650000, 1024};
unsigned begun = 0, ended = 0, aborted = 0;
size_t position = 0;
bool beginOkay = true, writeOkay = true, endOkay = true;
std::vector<uint8_t> memory(1024, 0xff);
}
const esp_partition_t *esp_ota_get_running_partition() { return &active; }
int esp_ota_begin(const esp_partition_t *p, size_t mode, esp_ota_handle_t *handle) {
    assert(p == &next && mode == OTA_WITH_SEQUENTIAL_WRITES); ++begun;
    if (!beginOkay) return -1;
    *handle = 1; position = 0; return ESP_OK;
}
int esp_ota_write(esp_ota_handle_t handle, const void *data, size_t bytes) {
    assert(handle == 1); if (!writeOkay) return -1;
    std::memcpy(memory.data() + position, data, bytes); position += bytes; return ESP_OK;
}
int esp_ota_end(esp_ota_handle_t handle) { assert(handle == 1); ++ended; return endOkay ? ESP_OK : -1; }
int esp_ota_abort(esp_ota_handle_t handle) { assert(handle == 1); ++aborted; return ESP_OK; }
int esp_partition_read(const esp_partition_t *p, size_t offset, void *data, size_t bytes) {
    assert(p == &next); std::memcpy(data, memory.data() + offset, bytes); return ESP_OK;
}
int main() {
    using screen_ota::SlotImage;
    uint8_t data[64]; std::memset(data, 0x42, sizeof(data)); uint8_t out[64]{};
    SlotImage current(&active), missing(nullptr);
    assert(!current.begin(64) && !missing.begin(64) && begun == 0);
    SlotImage image(&next);
    assert(!image.begin(0) && !image.begin(1025) && begun == 0);
    assert(!image.write(data, 1) && !image.read(0, out, 1));
    beginOkay = false; assert(!image.begin(64)); beginOkay = true;
    assert(image.begin(64));
    assert(image.write(data, 32) && !image.finish()); // incomplete image cannot finish
    writeOkay = false; assert(!image.write(data, 32)); writeOkay = true;
    assert(!image.write(data, 33)); // unknown SDK length does not remove our exact bound
    assert(image.write(data + 32, 32) && !image.write(data, 1));
    assert(image.finish() && ended == 1 && aborted == 0);
    assert(image.read(0, out, 64) && std::memcmp(data, out, 64) == 0);
    assert(!image.read(64, out, 1) && !image.read(65, out, 0));
    assert(image.begin(64) && image.write(data, 64)); endOkay = false;
    assert(!image.finish() && ended == 2); image.abort(); assert(aborted == 0);
    endOkay = true; assert(image.begin(64)); assert(image.begin(64) && aborted == 1);
    image.abort(); assert(aborted == 2 && !image.read(0, out, 1));
    { SlotImage unfinished(&next); assert(unfinished.begin(64)); }
    assert(aborted == 3);
}
