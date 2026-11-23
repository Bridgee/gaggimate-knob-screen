# Firmware updates

## Sources and versions

The System & Updates page treats the screen and controller as separate components:

- **Screen:** our K718 firmware from [Bridgee/gaggimate-knob-screen releases](https://github.com/Bridgee/gaggimate-knob-screen/releases). Each pair names its independent port version, official GaggiMate base, source identity, controller protocol and expected controller version.
- **Controller:** the unmodified official GaggiMate image, verified by the source/asset pin in `port/controller-release.json` and included unchanged in the same matched release. Do not substitute a self-build or automatically select the newest upstream controller.
- **Official notices:** Stable/Nightly checks remain a separate, read-only notification source. An upstream notice does not mean a compatible K718 port exists or authorize any installation.

Installed screen/controller compatibility is reported even when no newer release is available. A matching protocol alone is insufficient: the reviewed controller version must also match. Download assets and manifest hashes are bound to the selected own-repository release; arbitrary client URLs are not accepted.

## Current permissions

The current candidate wires the web screen action to the own-release installer, including confirmation and progress. **Normal screen installation, controller OTA and combined updates remain disabled.** Controls become usable only when their separate firmware permission and all live preconditions permit them; finding a matching release cannot enable an installer.

For now, install a compatible screen package over [checked USB](BUILDING.md). An explicitly enabled candidate-only bench interface can verify or install screen images; it is not general availability. See [Testing](TESTING.md#ota-qualification-on-the-same-isolated-bench).

## Messages and next steps

| Situation | Page behavior / action |
| --- | --- |
| Same release or installed screen newer | Show the independent component states; no downgrade/reinstall action |
| Newer matching K718 release | Show port version, official base, required controller and protocol; normal action still needs its own enabled permission |
| Controller absent or unidentified | Explain what is missing; block screen staging rather than infer compatibility from old telemetry |
| Controller version differs | Require the controller version specified by the pair; controller button stays disabled pending its own qualification |
| Protocol differs | Block screen OTA; use deliberately matched USB packages for a protocol transition. No automatic two-board update |
| Upstream-only update | Preserve the official notice; do not present the official LilyGO image as a K718 update |
| No matched release, offline, rate limit or malformed metadata | Show the check result; no installation. Drafts/incomplete pairs are ignored |
| Check older than five minutes | Refresh matched releases and confirm again |
| Active process, non-standby, unsettled boot or unsupported flash layout | Explain the live guard; require idle standby/healthy boot or the documented USB route |
| Selection changes after confirmation opens | Disable confirmation and require a fresh selection; server revalidates tag and both hashes again |
| Staging or readback in progress | Show progress; no parallel update. Controller-changing commands are refused by the existing update guard |
| Length/hash/SDK/readback failure | Show the error; boot selection is not committed by the staging failure |
| Restart after successful installation | Reconnect, check the installed version and startup health. A lost browser reply is not proof of failure; do not automatically retry |
| Recovery required | Keep the operation blocked and use the documented USB recovery procedure |

The confirmation names the exact selected screen and required controller. A screen update does not write controller firmware, pairing, settings or profiles. New software can still have storage-migration behavior, so keep a compatible package and backup. Local startup confirmation protects trial boot; it does not certify physical input or brewing behavior.

Candidate discovery includes labelled pre-releases; stable discovery excludes them. Publishing a stable port is distinct from enabling OTA, and each published artifact must remain immutable.
