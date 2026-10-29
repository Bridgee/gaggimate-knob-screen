#include "display/plugins/MatchedReleasePolicy.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace matched_release;
static void load(const std::string &path, JsonDocument &doc) {
    std::ifstream stream(path); std::stringstream text; text << stream.rdbuf();
    assert(!deserializeJson(doc, text.str()));
}
int main(int argc, char **argv) {
    assert(argc == 2); std::string folder = argv[1];
    JsonDocument listing, manifest;
    load(folder + "/matched-list.json", listing); load(folder + "/matched-pair.json", manifest);
    const Release good = selectRelease(listing.as<JsonArrayConst>(), true);
    assert(good.tag == "v0.3.6-rc.1" && good.preview);
    assert(selectRelease(listing.as<JsonArrayConst>(), false).tag.empty());
    listing[0]["prerelease"] = false;
    assert(selectRelease(listing.as<JsonArrayConst>(), false).tag.empty());
    assert(selectRelease(listing.as<JsonArrayConst>(), true).preview);
    listing[0]["prerelease"] = true;
    Pair pair; assert(parsePair(manifest.as<JsonVariantConst>(), good, pair));
    assert(pair.screenVersion == "0.3.6-rc.1" && pair.controllerVersion == "v1.9.0" && pair.protocol == 5);
    assert(std::string(componentState("0.3.5", pair.screenVersion)) == "update-available");
    assert(std::string(componentState("0.3.7-rc.1", pair.screenVersion)) == "local-newer");
    assert(std::string(componentState("v1.9.0+build", pair.controllerVersion)) == "up-to-date");
    assert(std::string(componentState("", pair.controllerVersion)) == "unknown");
    assert(std::string(compatibility(pair, 5, 5, true, "v1.9.0")) == "matched");
    assert(std::string(compatibility(pair, 5, 5, false, "v1.9.0")) == "unknown");
    assert(std::string(compatibility(pair, 5, 0, true, "v1.9.0")) == "unknown");
    assert(std::string(compatibility(pair, 5, 8, true, "v1.9.0")) == "incompatible");
    assert(std::string(compatibility(pair, 8, 5, true, "v1.9.0")) == "incompatible");
    assert(std::string(compatibility(pair, 5, 5, true, "v1.10.0")) == "incompatible");
    assert(order("0.3.10", "0.3.9") > 0 && order("0.3.7-rc.2", "0.3.7-rc.1") > 0);
    assert(order("0.3.7", "0.3.7-rc.2") > 0);
    assert(order("0.3.7-rc.10", "0.3.7-rc.2") > 0);
    assert(order("0.3.7-alpha.1", "0.3.7-alpha") > 0);
    assert(order("0.3.7-alpha", "0.3.7-1") > 0);
    assert(order("0.3.7+one", "0.3.7+two") == 0);
    for (const char *bad : {"", "1.2", "1.2.3.4", "01.2.3", "1.2.3-", "1.2.3-01", "99999999.0.0", "v1.2.3/evil", "1.2.3+"})
        assert(!version(bad));
    for (const char *goodVersion : {"1.2.3", "v1.2.3", "1.2.3-rc.2", "1.2.3+abc.01"}) assert(version(goodVersion));
    listing[0]["draft"] = true; assert(selectRelease(listing.as<JsonArrayConst>(), true).tag.empty());
    load(folder + "/matched-list.json", listing);
    for (JsonObject a : listing[0]["assets"].as<JsonArray>())
        if (a["name"] == "pair.json") a["browser_download_url"] = "https://example.com/pair.json";
    assert(selectRelease(listing.as<JsonArrayConst>(), true).tag.empty());
    load(folder + "/matched-list.json", listing);
    for (JsonObject a : listing[0]["assets"].as<JsonArray>())
        if (a["name"] == "pair.json") a["size"] = 20000;
    assert(selectRelease(listing.as<JsonArrayConst>(), true).tag.empty());
    load(folder + "/matched-list.json", listing);
    for (JsonObject a : listing[0]["assets"].as<JsonArray>())
        if (a["name"] == "pair.json") a["digest"] = "sha256:bad";
    assert(selectRelease(listing.as<JsonArrayConst>(), true).tag.empty());
    const char *paths[][2] = {{"screen", "board"}, {"screen", "target"}, {"controller", "origin"},
                              {"controller", "upstream_repository"}, {"compatibility", "upstream_commit"}};
    for (auto &path : paths) {
        load(folder + "/matched-pair.json", manifest); manifest[path[0]][path[1]] = "wrong";
        assert(!parsePair(manifest.as<JsonVariantConst>(), good, pair));
    }
    load(folder + "/matched-pair.json", manifest); manifest["installation_allowed"] = true;
    assert(!parsePair(manifest.as<JsonVariantConst>(), good, pair));
    load(folder + "/matched-pair.json", manifest); manifest["screen"]["asset"]["sha256"] = std::string(64, '0');
    assert(!parsePair(manifest.as<JsonVariantConst>(), good, pair));
    load(folder + "/matched-pair.json", manifest); manifest["screen"]["application_offset"] = 0;
    assert(!parsePair(manifest.as<JsonVariantConst>(), good, pair));
    load(folder + "/matched-pair.json", manifest); manifest["screen"]["version"] = "0.3.7-rc.1";
    assert(!parsePair(manifest.as<JsonVariantConst>(), good, pair));
    std::cout << "PASS: published pair parsing, channels, independent versions, unknown/incompatible controllers and rejected metadata\n";
}
