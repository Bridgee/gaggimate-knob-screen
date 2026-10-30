#include "display/plugins/ScreenOtaPolicy.h"
#include <cassert>
#include <iostream>
using namespace screen_ota;
static void state(const Preflight &p, const char *expected) {
    assert(std::string(p.state) == expected);
    if (!p.ready()) assert(p.plan.tag.empty() && p.plan.image.url.empty());
}
int main() {
    matched_release::Release release;
    release.tag = "v0.4.0-rc.1"; release.preview = true;
    release.manifest = {"pair.json", std::string(matched_release::ReleaseRoot) + "download/" + release.tag + "/pair.json", std::string(64, 'a'), 1500};
    release.screen = {"display-knob-firmware.bin", std::string(matched_release::ReleaseRoot) + "download/" + release.tag + "/display-knob-firmware.bin", std::string(64, 'b'), 4000000};
    matched_release::Pair pair{"0.4.0-rc.1", "v1.9.0", "v1.9.0", std::string(40, 'c'), 5};
    Context local{"0.3.8", "v1.9.0", 5, 5, 0x10000, SlotBytes, 0x650000, SlotBytes, true, false, false};
    const auto ready = inspect(true, release, pair, local, true);
    state(ready, "metadata-ready");
    Request request{"display", release.tag, release.manifest.sha, release.screen.sha};
    assert(matchesRequest(ready, request));
    assert(!screenInstallationAllowed && !controllerInstallationAllowed);
    assert(!mayInstall(ready, request)); // metadata readiness must never authorize an installer
    assert(!matchesRequest({}, request));
    for (const char *component : {"controller", "both", "", "DISPLAY"}) {
        auto bad = request; bad.component = component; assert(!matchesRequest(ready, bad));
        assert(!mayInstall(ready, bad));
    }
    for (int field = 0; field < 3; ++field) {
        auto bad = request;
        if (field == 0) bad.tag = "v0.4.0-rc.2";
        if (field == 1) bad.manifestSha = std::string(64, 'd');
        if (field == 2) bad.imageSha = std::string(64, 'd');
        assert(!matchesRequest(ready, bad));
    }
    // The plan is a frozen copy, not a reference into mutable discovery state.
    auto newerSelection = release; newerSelection.manifest.sha = std::string(64, 'e');
    assert(ready.plan.manifest.sha == release.manifest.sha);
    assert(!matchesRequest(inspect(true, newerSelection, pair, local, true), request));
    state(inspect(false, release, pair, local, true), "metadata-unavailable");
    state(inspect(true, release, pair, local, false), "preview-excluded");
    auto hiddenPreview = release; hiddenPreview.preview = false;
    state(inspect(true, hiddenPreview, pair, local, false), "preview-excluded");
    for (const char *version : {"0.4.0-rc.1", "0.4.0", "0.5.0"}) {
        auto current = local; current.portVersion = version;
        state(inspect(true, release, pair, current, true), "not-newer");
    }
    auto invalidVersion = local; invalidVersion.portVersion = "bad";
    state(inspect(true, release, pair, invalidVersion, true), "invalid-release");
    for (int kind = 0; kind < 9; ++kind) {
        auto bad = release;
        switch (kind) {
        case 0: bad.screen.url = "https://example.com/display-knob-firmware.bin"; break;
        case 1: bad.screen.name = "board-firmware.bin"; break;
        case 2: bad.screen.sha = std::string(64, 'A'); break;
        case 3: bad.screen.bytes = 0; break;
        case 4: bad.screen.bytes = SlotBytes + 1; break;
        case 5: bad.manifest.url += "?other=1"; break;
        case 6: bad.manifest.bytes = 16385; break;
        case 7: bad.manifest.sha.clear(); break;
        case 8: bad.tag = "v0.4.0-rc.2"; break;
        }
        state(inspect(true, bad, pair, local, true), "invalid-release");
    }
    auto invalidTree = pair; invalidTree.sourceTree = "not-a-source-tree";
    state(inspect(true, release, invalidTree, local, true), "invalid-release");
    for (int kind = 0; kind < 5; ++kind) {
        auto bad = local;
        if (kind == 0) bad.controllerReady = false;
        if (kind == 1) bad.controllerProtocol = 0;
        if (kind == 2) bad.controllerProtocol = 8;
        if (kind == 3) bad.localProtocol = 8;
        if (kind == 4) bad.controllerVersion = "v1.10.0";
        state(inspect(true, release, pair, bad, true), "incompatible-controller");
    }
    auto busy = local; busy.activeProcess = true;
    state(inspect(true, release, pair, busy, true), "busy");
    busy = local; busy.updating = true;
    state(inspect(true, release, pair, busy, true), "busy");
    auto otherSlot = local; otherSlot.runningOffset = 0x650000; otherSlot.updateOffset = 0x10000;
    state(inspect(true, release, pair, otherSlot, true), "metadata-ready");
    for (int kind = 0; kind < 4; ++kind) {
        auto bad = local;
        if (kind == 0) bad.updateOffset = bad.runningOffset;
        if (kind == 1) bad.runningOffset = 0;
        if (kind == 2) --bad.runningBytes;
        if (kind == 3) --bad.updateBytes;
        state(inspect(true, release, pair, bad, true), "unsupported-partitions");
    }
    auto full = release; full.screen.bytes = SlotBytes;
    state(inspect(true, full, pair, local, true), "metadata-ready");
    std::cout << "PASS: own-asset binding, stale request rejection, channels, versions, controller/busy/slot guards; both installers remain denied\n";
}
