#include <display/plugins/BufferedProfileImage.h>
#include <cassert>
#include <functional>
#include <limits>
#include <vector>

constexpr size_t Size = 4 + 300 * 300 * 2;
constexpr uint32_t Header = 4 | (300 << 10) | (300 << 21);
struct File {
    std::vector<uint8_t> data;
    bool open = true, shortRead = false;
    unsigned reads = 0;
    explicit operator bool() const { return open; }
    size_t size() const { return data.size(); }
    size_t read(uint8_t *out, size_t count) {
        assert(open); ++reads;
        const size_t n = shortRead ? count - 1 : count;
        std::memcpy(out, data.data(), n);
        return n;
    }
};
int main() {
    unsigned liveBuffers = 0;
    auto allocate = [&](size_t size) {
        ++liveBuffers;
        return std::shared_ptr<uint8_t>(new uint8_t[size], [&](uint8_t *p) { delete[] p; --liveBuffers; });
    };
    File source; source.data.resize(Size);
    for (size_t i = 0; i < Size; ++i) source.data[i] = uint8_t(i * 37);
    for (unsigned i = 0; i < 4; ++i) source.data[i] = uint8_t(Header >> (i * 8));
    auto image = profile_http::BufferedImage::load(source, Size, Header, allocate);
    assert(image && liveBuffers == 1 && source.reads == 1);
    source.open = false; // File is closed before the HTTP response starts.
    std::function<size_t(uint8_t *,size_t,size_t)> callback = [image](uint8_t *out,size_t n,size_t offset) { return image.copy(out,n,offset); };
    image = {}; // Request-local owner can leave scope before TCP completes.
    assert(liveBuffers == 1);
    std::vector<uint8_t> result(Size);
    size_t offset = 0;
    for (size_t chunk : {size_t(1),size_t(3),size_t(2920),size_t(137),size_t(8192)}) {
        offset = 0;
        while (offset < Size) offset += callback(result.data() + offset, chunk, offset);
        assert(offset == Size && result == source.data && source.reads == 1);
    }
    uint8_t guard[4] = {9,9,9,9};
    assert(callback(guard,4,Size-2) == 2 && guard[2] == 9 && guard[3] == 9);
    assert(callback(guard,4,Size) == 0);
    assert(callback(guard,4,std::numeric_limits<size_t>::max()) == 0);
    assert(callback(guard,0,0) == 0 && callback(nullptr,4,0) == 0);
    // Two live HTTP responses must retain independent content/lifetimes.
    source.open = true; source.data[10] ^= 0xff;
    auto second = profile_http::BufferedImage::load(source,Size,Header,allocate);
    assert(second && liveBuffers == 2);
    second.copy(guard,1,10); assert(guard[0] == source.data[10]);
    callback(guard,1,10); assert(guard[0] != source.data[10]);
    callback = {}; assert(liveBuffers == 1);
    second = {}; assert(liveBuffers == 0);
    source.shortRead = true;
    assert(!profile_http::BufferedImage::load(source,Size,Header,allocate));
    assert(liveBuffers == 0);
    source.shortRead = false; source.data[0] = 0;
    assert(!profile_http::BufferedImage::load(source,Size,Header,allocate));
    assert(liveBuffers == 0);
    source.data.pop_back();
    assert(!profile_http::BufferedImage::load(source,Size,Header,allocate));
    source.data.push_back(0); source.data[0] = uint8_t(Header);
    assert(!profile_http::BufferedImage::load(source,Size,Header,[](size_t) {return std::shared_ptr<uint8_t>{};}));
    assert(liveBuffers == 0);
}
