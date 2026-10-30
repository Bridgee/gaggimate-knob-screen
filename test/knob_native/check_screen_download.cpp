#include <display/plugins/VerifiedScreenDownload.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
using namespace screen_ota;
struct Environment : DownloadEnv {
    uint32_t now = 0; int allocated = 0, freed = 0, failAllocation = 0;
    void delayMs(uint32_t ms) override { now += ms; }
    void yieldAfterChunk() override { ++now; }
    bool waitForNetwork(uint32_t) override { return true; }
    uint8_t *allocBuffer(size_t size) override { ++allocated; return allocated == failAllocation ? nullptr : new uint8_t[size]; }
    void freeBuffer(uint8_t *p) override { ++freed; delete[] p; }
};
struct Hash : ImageHash {
    std::vector<uint8_t> bytes, expected; bool fail = false;
    explicit Hash(std::vector<uint8_t> body) : expected(std::move(body)) {}
    bool begin() override { bytes.clear(); return !fail; }
    bool add(const uint8_t *p, size_t n) override { bytes.insert(bytes.end(), p, p+n); return !fail; }
    std::string finish() override { return std::string(64, bytes == expected ? 'a' : 'b'); }
};
struct Image : ImageSink {
    std::vector<uint8_t> bytes; int begins = 0, finishes = 0, aborts = 0;
    bool open = false, failWrite = false, failImage = false, corruptReadback = false;
    bool begin(size_t) override { ++begins; bytes.clear(); open = true; return true; }
    bool write(const uint8_t *p, size_t n) override {
        assert(open); if (failWrite) return false; bytes.insert(bytes.end(), p, p+n); return true;
    }
    bool finish() override { ++finishes; open = false; return !failImage; }
    bool read(size_t off, uint8_t *p, size_t n) override {
        assert(off+n <= bytes.size()); std::memcpy(p, bytes.data()+off, n);
        if (corruptReadback && off == 0) p[0] ^= 1;
        return true;
    }
    void abort() override { ++aborts; open = false; }
};
struct Http : HttpTransport {
    std::vector<uint8_t> body; Environment &env; size_t position = 0, range = 0;
    int attempts = 0, code = 200, declared = 0;
    bool dropOnce = false, dropAlways = false, ignoreRange = false, badRange = false, slow = false;
    explicit Http(std::vector<uint8_t> value, Environment &e) : body(std::move(value)), env(e), declared(int(body.size())) {}
    bool begin(const std::string &url) override {
        assert(url.find(matched_release::ReleaseRoot) == 0); ++attempts; position = 0; range = 0; return true;
    }
    void setHeader(const char *key, const char *value) override {
        if (std::string(key) == "Range") { range = std::stoul(std::string(value).substr(6)); if (!ignoreRange) position = range; }
        if (std::string(key) == "If-Range") assert(std::string(value) == "fixture-v1");
    }
    bool open(int &status, int &length) override { status = range && !ignoreRange ? 206 : code; length = status == 206 ? declared - int(range) : declared; return true; }
    bool followRedirect() override { return false; }
    int read(uint8_t *out, size_t size) override {
        if ((dropAlways || (dropOnce && attempts == 1)) && position >= 4096) return -1;
        if (slow) env.now = 180001;
        size = std::min(size, body.size()-position); std::memcpy(out, body.data()+position, size); position += size; return int(size);
    }
    bool isComplete() override { return position == body.size(); }
    std::string header(const char *key) override {
        if (std::string(key) == "ETag") return "fixture-v1";
        if (std::string(key) == "Content-Range") return "bytes " + std::to_string(badRange ? range+1 : range) + "-" +
            std::to_string(body.size()-1) + "/" + std::to_string(body.size());
        return {};
    }
    void end() override {}
};
int main() {
    const std::vector<uint8_t> body(9000, 0xe9);
    Plan plan; plan.tag = "v0.4.0-rc.4";
    plan.image = {"display-knob-firmware.bin", std::string(matched_release::ReleaseRoot)+"download/"+plan.tag+"/display-knob-firmware.bin", std::string(64,'a'), body.size()};
    for (int test = 0; test < 16; ++test) {
        Environment env; Http http(body, env); Image image; Hash hash(body); auto selected = plan;
        bool allowed = true;
        if (test == 1) http.dropOnce = true;
        if (test == 2) { http.dropOnce = true; http.ignoreRange = true; }
        if (test == 3) http.declared--;
        if (test == 4) http.body.back() ^= 1;
        if (test == 5) image.failImage = true;
        if (test == 6) image.corruptReadback = true;
        if (test == 7) image.failWrite = true;
        if (test == 8) http.dropAlways = true;
        if (test == 9) allowed = false;
        if (test == 10) http.slow = true;
        if (test == 11) env.failAllocation = 2;
        if (test == 12) selected.image.url = "https://example.com/image.bin";
        if (test == 13) hash.fail = true;
        if (test == 14) http.body.pop_back();
        if (test == 15) http.body.push_back(0xe9);
        const auto result = stageImage(selected, http, env, image, hash, [&] { return env.now; }, [&] { return allowed; });
        const bool success = test <= 2;
        assert(result.verified == success);
        assert(!image.open);
        if (success) { assert(result.bytes == body.size()); assert(image.bytes == body); assert(image.finishes == 1); }
        else if (test != 6 && test != 11) assert(image.finishes == (test == 5 ? 1 : 0));
        if (test == 2) assert(image.begins == 2); // Range ignored: hash AND inactive image restart together.
        if (test == 4) assert(std::string(result.error) == "checksum-mismatch");
        if (test == 6) assert(std::string(result.error) == "readback-mismatch");
        assert(http.attempts <= 4);
        assert(env.allocated - env.freed == (env.failAllocation ? 1 : 0));
    }
    std::cout << "PASS: exact asset binding, complete bytes/hash, resume/restart, flash readback and rejected interrupted/corrupt images; staging has no boot-selection API\n";
}
