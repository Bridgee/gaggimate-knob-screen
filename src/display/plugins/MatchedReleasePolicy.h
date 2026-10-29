#pragma once
#include <ArduinoJson.h>
#include <climits>
#include <cstring>
#include <string>

namespace matched_release {
constexpr const char *Repository = "Bridgee/gaggimate-knob-screen";
constexpr const char *ReleaseRoot = "https://github.com/Bridgee/gaggimate-knob-screen/releases/";

inline bool hex(const std::string &s, size_t size) {
    if (s.size() != size) return false;
    for (char c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
inline std::string text(JsonVariantConst v) {
    if (!v.is<const char *>()) return {};
    auto s = v.as<JsonString>();
    if (s.size() != std::strlen(s.c_str())) return {};
    return std::string(s.c_str(), s.size());
}
inline bool version(std::string s) {
    if (!s.empty() && s[0] == 'v') s.erase(0, 1);
    if (s.empty() || s.size() > 80) return false;
    size_t i = 0;
    for (int part = 0; part < 3; ++part) {
        size_t start = i; unsigned value = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            if (i - start >= 6) return false;
            value = value * 10 + unsigned(s[i++] - '0');
        }
        if (start == i || value > 999999 || (i - start > 1 && s[start] == '0')) return false;
        if (part < 2 && (i == s.size() || s[i++] != '.')) return false;
    }
    if (i == s.size()) return true;
    bool metadata = s[i] == '+';
    if (s[i] != '-' && !metadata) return false;
    ++i; size_t start = i;
    while (i <= s.size()) {
        if (i == s.size() || s[i] == '.' || (!metadata && s[i] == '+')) {
            if (start == i) return false;
            bool numeric = true;
            for (size_t j = start; j < i; ++j) if (s[j] < '0' || s[j] > '9') numeric = false;
            if (!metadata && numeric && i - start > 1 && s[start] == '0') return false;
            if (i == s.size()) return true;
            if (s[i] == '+') metadata = true;
            start = ++i; continue;
        }
        char c = s[i++];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-')) return false;
    }
    return false;
}
// SemVer precedence for the independent port version axis. Build metadata is
// ignored; a final release sorts after its prereleases. Do not change upstream OTA.
inline int order(std::string a, std::string b) {
    if (!version(a) || !version(b)) return 0;
    if (a[0] == 'v') a.erase(0, 1);
    if (b[0] == 'v') b.erase(0, 1);
    a = a.substr(0, a.find('+')); b = b.substr(0, b.find('+'));
    size_t x = 0, y = 0;
    for (int part = 0; part < 3; ++part) {
        unsigned u = 0, v = 0;
        while (x < a.size() && a[x] >= '0' && a[x] <= '9') u = u * 10 + unsigned(a[x++] - '0');
        while (y < b.size() && b[y] >= '0' && b[y] <= '9') v = v * 10 + unsigned(b[y++] - '0');
        if (u != v) return u > v ? 1 : -1;
        if (part < 2) { ++x; ++y; }
    }
    if (x == a.size() || y == b.size()) return x == a.size() ? (y == b.size() ? 0 : 1) : -1;
    ++x; ++y;
    for (;;) {
        size_t xe = a.find('.', x), ye = b.find('.', y);
        std::string u = a.substr(x, xe - x), v = b.substr(y, ye - y);
        if (u != v) {
            bool un = u.find_first_not_of("0123456789") == std::string::npos;
            bool vn = v.find_first_not_of("0123456789") == std::string::npos;
            if (un != vn) return un ? -1 : 1;
            if (un && u.size() != v.size()) return u.size() > v.size() ? 1 : -1;
            return u > v ? 1 : -1;
        }
        if (xe == std::string::npos || ye == std::string::npos)
            return xe == std::string::npos ? (ye == std::string::npos ? 0 : -1) : 1;
        x = xe + 1; y = ye + 1;
    }
}
struct Asset { std::string name, url, sha; size_t bytes = 0; };
struct Release {
    std::string tag;
    bool preview = false;
    Asset manifest, screen, controller;
};
struct Pair {
    std::string screenVersion, controllerVersion, upstreamVersion, sourceTree;
    unsigned protocol = 0;
};
inline Asset asset(JsonArrayConst assets, const std::string &tag, const char *name, size_t maximum) {
    Asset result;
    const std::string expected = std::string(ReleaseRoot) + "download/" + tag + "/" + name;
    for (JsonObjectConst entry : assets) {
        if (text(entry["name"]) != name) continue;
        const std::string sha = text(entry["digest"]);
        if (result.bytes || text(entry["browser_download_url"]) != expected || !entry["size"].is<unsigned>() ||
            sha.compare(0, 7, "sha256:") != 0 || !hex(sha.substr(7), 64)) return {};
        result = {name, expected, sha.substr(7), entry["size"].as<unsigned>()};
        if (!result.bytes || result.bytes > maximum) return {};
    }
    return result;
}
inline Release selectRelease(JsonArrayConst releases, bool previews) {
    Release selected;
    for (JsonObjectConst entry : releases) {
        std::string tag = text(entry["tag_name"]);
        if (!version(tag) || tag.empty() || tag[0] != 'v' || !entry["draft"].is<bool>() || entry["draft"].as<bool>() ||
            !entry["prerelease"].is<bool>() || (!previews && (entry["prerelease"].as<bool>() || tag.substr(0, tag.find('+')).find('-') != std::string::npos)) || text(entry["published_at"]).empty()) continue;
        Release candidate; candidate.tag = tag; candidate.preview = entry["prerelease"].as<bool>() || tag.substr(0, tag.find('+')).find('-') != std::string::npos;
        auto assets = entry["assets"].as<JsonArrayConst>();
        candidate.manifest = asset(assets, tag, "pair.json", 16384);
        candidate.screen = asset(assets, tag, "display-knob-firmware.bin", 0x640000);
        candidate.controller = asset(assets, tag, "board-firmware.bin", 8 * 1024 * 1024);
        if (candidate.manifest.bytes && candidate.screen.bytes && candidate.controller.bytes &&
            (selected.tag.empty() || order(tag, selected.tag) > 0)) selected = candidate;
    }
    return selected;
}
inline bool parsePair(JsonVariantConst doc, const Release &release, Pair &result) {
    auto screen = doc["screen"]; auto controller = doc["controller"]; auto compatibility = doc["compatibility"];
    Pair pair; pair.screenVersion = text(screen["version"]); pair.controllerVersion = text(controller["version"]);
    pair.upstreamVersion = text(screen["upstream_version"]); pair.sourceTree = text(screen["source_tree"]);
    pair.protocol = compatibility["controller_protocol"].as<unsigned>();
    auto matches = [](JsonVariantConst entry, const Asset &expected) {
        return text(entry["name"]) == expected.name && text(entry["sha256"]) == expected.sha &&
            entry["bytes"].is<unsigned>() && entry["bytes"].as<unsigned>() == expected.bytes;
    };
    if (!doc["schema"].is<unsigned>() || doc["schema"].as<unsigned>() != 1 ||
        text(doc["kind"]) != "gaggimate-knob-screen-firmware-pair" || text(doc["release_repository"]) != Repository ||
        !doc["installation_allowed"].is<bool>() || doc["installation_allowed"].as<bool>() ||
        !version(pair.screenVersion) || !version(pair.controllerVersion) || !version(pair.upstreamVersion) ||
        release.tag != "v" + pair.screenVersion || pair.controllerVersion != pair.upstreamVersion ||
        text(screen["target"]) != "display-knob" || text(screen["board"]) != "Guition-K718" ||
        text(screen["chip"]) != "ESP32-S3" || text(controller["chip"]) != "ESP32-S3" ||
        text(controller["board"]) != "Gaggimate-Controller" || text(controller["origin"]) != "official-release-unmodified" ||
        text(controller["upstream_repository"]) != "jniebuhr/gaggimate" ||
        !hex(pair.sourceTree, 40) || !hex(text(screen["source_commit"]), 40) ||
        !hex(text(controller["upstream_commit"]), 40) ||
        text(controller["upstream_commit"]) != text(compatibility["upstream_commit"]) ||
        text(compatibility["status"]) != "package-verified" ||
        !compatibility["controller_protocol"].is<unsigned>() || !pair.protocol ||
        !screen["application_offset"].is<unsigned>() || screen["application_offset"].as<unsigned>() != 0x10000 ||
        !screen["application_size"].is<unsigned>() || screen["application_size"].as<unsigned>() != 0x640000 ||
        !matches(screen["asset"], release.screen) || !matches(controller["asset"], release.controller)) return false;
    result = pair; return true;
}
inline const char *componentState(const std::string &current, const std::string &published) {
    if (!version(current) || !version(published)) return "unknown";
    const int comparison = order(published, current);
    return comparison > 0 ? "update-available" : comparison < 0 ? "local-newer" : "up-to-date";
}
inline const char *compatibility(const Pair &pair, unsigned localProtocol, unsigned controllerProtocol, bool connected,
                                  const std::string &controllerVersion) {
    if (pair.protocol != localProtocol) return "incompatible";
    if (!connected || !controllerProtocol || controllerVersion.empty()) return "unknown";
    if (controllerProtocol != pair.protocol || !version(controllerVersion) || order(controllerVersion, pair.controllerVersion) != 0)
        return "incompatible";
    return "matched";
}
} // namespace matched_release
