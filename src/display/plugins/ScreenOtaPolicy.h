#pragma once
#include "MatchedReleasePolicy.h"
#include <cstdint>

// Shared preflight for the own-repository screen installer. It neither
// downloads nor writes flash. These permissions must not enable the legacy OTA path.
namespace screen_ota {
constexpr bool screenInstallationAllowed = false;
constexpr bool controllerInstallationAllowed = false;
constexpr uint32_t SlotBytes = 0x640000;

struct Context {
    std::string portVersion, controllerVersion;
    unsigned localProtocol = 0, controllerProtocol = 0;
    uint32_t runningOffset = 0, runningBytes = 0, updateOffset = 0, updateBytes = 0;
    bool controllerReady = false, activeProcess = false, updating = false;
    bool bootHealthy = false, standby = false, bootSelectionCurrent = false;
};
struct Plan {
    std::string tag, sourceTree;
    matched_release::Asset manifest, image;
    unsigned protocol = 0;
    uint32_t runningOffset = 0, updateOffset = 0;
    std::string controllerVersion;
};
struct Preflight {
    const char *state = "metadata-unavailable";
    Plan plan;
    bool ready() const { return std::strcmp(state, "metadata-ready") == 0; }
};
struct Request {
    std::string component, tag, manifestSha, imageSha;
    bool qualification = false, verifyOnly = false;
};
inline bool metadataFresh(uint32_t checkedAt, uint32_t now) {
    return checkedAt != 0 && uint32_t(now - checkedAt) <= 300000;
}
inline bool ownAsset(const matched_release::Asset &a, const std::string &tag,
                     const char *name, size_t minimum, size_t maximum) {
    return a.name == name && a.url == std::string(matched_release::ReleaseRoot) + "download/" + tag + "/" + name &&
        matched_release::hex(a.sha, 64) && a.bytes >= minimum && a.bytes <= maximum;
}
inline Preflight inspect(bool metadataValidated, const matched_release::Release &release,
                         const matched_release::Pair &pair, const Context &local, bool previews) {
    if (!metadataValidated) return {};
    if (!matched_release::version(release.tag) || release.tag != "v" + pair.screenVersion ||
        !matched_release::version(pair.screenVersion) || !matched_release::version(local.portVersion) ||
        !matched_release::hex(pair.sourceTree, 40) ||
        !ownAsset(release.manifest, release.tag, "pair.json", 1, 16384) ||
        !ownAsset(release.screen, release.tag, "display-knob-firmware.bin", 288, SlotBytes))
        return {"invalid-release", {}};
    const bool preview = release.preview || release.tag.substr(0, release.tag.find('+')).find('-') != std::string::npos;
    if (preview && !previews) return {"preview-excluded", {}};
    if (matched_release::order(pair.screenVersion, local.portVersion) <= 0) return {"not-newer", {}};
    if (local.activeProcess || local.updating) return {"busy", {}};
    if (!pair.protocol || pair.protocol != local.localProtocol ||
        !matched_release::version(pair.controllerVersion) || pair.controllerVersion != pair.upstreamVersion ||
        std::strcmp(matched_release::compatibility(pair, local.localProtocol, local.controllerProtocol,
            local.controllerReady, local.controllerVersion), "matched") != 0)
        return {"incompatible-controller", {}};
    if (!local.bootHealthy) return {"startup-unverified", {}};
    if (!local.bootSelectionCurrent) return {"restart-required", {}};
    if (!local.standby) return {"standby-required", {}};
    const bool slots = (local.runningOffset == 0x10000 && local.updateOffset == 0x650000) ||
                       (local.runningOffset == 0x650000 && local.updateOffset == 0x10000);
    if (!slots || local.runningBytes != SlotBytes || local.updateBytes != SlotBytes)
        return {"unsupported-partitions", {}};
    return {"metadata-ready", {release.tag, pair.sourceTree, release.manifest, release.screen, pair.protocol,
        local.runningOffset, local.updateOffset, pair.controllerVersion}};
}
// Recompute preflight from the live server snapshot at request time. A request
// for an old tag/manifest/image is rejected; clients cannot provide a download URL.
inline bool matchesRequest(const Preflight &current, const Request &request) {
    return current.ready() && request.component == "display" && !current.plan.tag.empty() &&
        request.tag == current.plan.tag && request.manifestSha == current.plan.manifest.sha &&
        request.imageSha == current.plan.image.sha;
}
inline bool mayInstall(const Preflight &current, const Request &request) {
    return screenInstallationAllowed && matchesRequest(current, request);
}
inline bool mayQualify(const Preflight &current, const Request &request, bool compiledPermission) {
    return compiledPermission && request.qualification && matchesRequest(current, request);
}
// While flash is being staged, only update/status queries are accepted here.
inline bool readOnlyDuringUpdate(const std::string &type) {
    return type == "req:ota-settings" || type == "req:screen-ota-start";
}
} // namespace screen_ota
