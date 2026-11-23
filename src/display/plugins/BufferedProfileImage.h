#pragma once
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

namespace profile_http {
// Own the complete validated image before TCP starts pulling response chunks.
// A response callback never retains a File or performs filesystem I/O.
class BufferedImage {
  public:
    template <class File, class Allocate>
    static BufferedImage load(File &file, size_t expectedBytes, uint32_t expectedHeader, Allocate allocate) {
        if (!file || expectedBytes < 4 || file.size() != expectedBytes) return {};
        auto bytes = allocate(expectedBytes);
        if (!bytes || file.read(bytes.get(), expectedBytes) != expectedBytes) return {};
        const uint32_t header = uint32_t(bytes.get()[0]) | (uint32_t(bytes.get()[1]) << 8) |
                                (uint32_t(bytes.get()[2]) << 16) | (uint32_t(bytes.get()[3]) << 24);
        if (header != expectedHeader) return {};
        return BufferedImage(std::move(bytes), expectedBytes);
    }
    BufferedImage() = default;
    size_t size() const { return length; }
    explicit operator bool() const { return bool(bytes); }
    size_t copy(uint8_t *destination, size_t capacity, size_t offset) const {
        if (!bytes || !destination || offset >= length || capacity == 0) return 0;
        const size_t count = std::min(capacity, length - offset);
        std::memcpy(destination, bytes.get() + offset, count);
        return count;
    }
  private:
    BufferedImage(std::shared_ptr<uint8_t> data, size_t size) : bytes(std::move(data)), length(size) {}
    std::shared_ptr<uint8_t> bytes;
    size_t length = 0;
};
// End each server fill dispatch after one bounded batch. Retry is the pinned
// server's nonblocking RESPONSE_TRY_AGAIN marker; it does not advance offsets.
class PacedImage {
  public:
    static constexpr size_t Retry = 0xffffffffu;
    static constexpr size_t BatchBytes = 2048;
    explicit PacedImage(BufferedImage image) : image(std::move(image)) {}
    size_t copy(uint8_t *destination, size_t capacity, size_t offset) {
        if (!destination || !capacity || offset >= image.size()) return 0;
        if (yield) { yield = false; return Retry; }
        const auto count = image.copy(destination, std::min(capacity, BatchBytes), offset);
        // A full small window already ends this dispatch; do not burn the
        // following ACK on a retry (which would wait for a later poll).
        yield = count != 0 && count < capacity;
        return count;
    }
  private:
    BufferedImage image;
    bool yield = false;
};
// A permit is retained until the HTTP response's final owner is destroyed,
// including disconnects. Waiting callbacks yield without occupying a slot.
class SendSlots {
  public:
    explicit SendSlots(unsigned limit) : limit(limit) {}
    bool acquire() {
        auto n = active.load(std::memory_order_relaxed);
        while (n < limit) {
            if (active.compare_exchange_weak(n, n + 1, std::memory_order_relaxed)) return true;
        }
        return false;
    }
    void release() { active.fetch_sub(1, std::memory_order_relaxed); }
    unsigned used() const { return active.load(std::memory_order_relaxed); }
  private:
    const unsigned limit;
    std::atomic<unsigned> active{0};
};
class ImageTransfer {
  public:
    ImageTransfer(BufferedImage image, SendSlots &slots) : length(image.size()), response(std::move(image)), slots(slots) {}
    ImageTransfer(const ImageTransfer &) = delete;
    ImageTransfer &operator=(const ImageTransfer &) = delete;
    ~ImageTransfer() { if (owned) slots.release(); }
    size_t copy(uint8_t *destination, size_t capacity, size_t offset) {
        if (!destination || !capacity || offset >= length) return 0;
        if (!owned) {
            if (!slots.acquire()) return PacedImage::Retry;
            owned = true;
        }
        return response.copy(destination, capacity, offset);
    }
  private:
    const size_t length;
    PacedImage response;
    SendSlots &slots;
    bool owned = false;
};
} // namespace profile_http
