#pragma once
#include <algorithm>
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
} // namespace profile_http
