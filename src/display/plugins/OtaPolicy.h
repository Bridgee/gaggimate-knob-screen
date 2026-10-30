#pragma once
#include <version.h>

namespace FirmwareOtaPolicy {
#ifdef GAGGIMATE_KNOB
// Keep official update discovery independent of installation permission.
// Compare the official base, not the port's prerelease suffix.
constexpr const char *displayComparisonVersion = BUILD_UPSTREAM_VERSION;
constexpr bool installationAllowed = false;
constexpr const char *disabledReason =
    "Display and controller OTA installation from this page is disabled. "
    "Official update checks and notifications remain enabled. "
    "Use a K718 USB package for manual screen updates.";
#else
constexpr const char *displayComparisonVersion = BUILD_GIT_VERSION;
constexpr bool installationAllowed = true;
constexpr const char *disabledReason = "";
#endif
} // namespace FirmwareOtaPolicy
