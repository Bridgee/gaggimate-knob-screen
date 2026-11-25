# Changelog

## 0.5.0-rc.17

- Recommend the newest published K718 release for the connected controller version and protocol, even when a newer port release targets a different controller.
- Move validation-only replay and isolated screen OTA tests to Settings → Plugins. The physical screen uses the regular interface during replay.

## 0.5.0-rc.16

- Create the validation simulation banner before the LVGL task starts, removing a cross-core startup race.
- Retain the distinct K718 hotspot and early Wi-Fi diagnostics from rc.15.

## 0.5.0-rc.15

- Name the K718 fallback/setup hotspot `gaggimate_knob` so it can be distinguished from an official screen. The native information page uses the same configured name.
- Retain the saved hotspot password and `http://4.4.4.1` address. This name change does not alter station credentials, authentication or controller pairing.
- Subscribe to K718 Wi-Fi events before station startup, logging event/reason codes through the nonblocking diagnostic queue without credentials.
- Compile identity and information-label checks for upstream, normal K718 and validation K718 roles.

## 0.5.0-rc.14

- Turn the knob on the idle Brew main page to open the existing Profile picker and preview the previous/next entry; the original checkmark still confirms the selection.
- Block the shortcut during active processes, updates, overlays, transitions and unavailable controller/profile state. Preserve parameter-panel focus, original touch actions and no disabled-motion backlog.
- Add actual-callback guard tests and native LVGL regressions in both themes. Candidate device acceptance remains tied to its exact image.

## 0.5.0-rc.13

- Fix the first device replay activation: keep the synthetic identity out of the legacy physical-controller OTA version parser.
- Encode the SystemInfo capability presence flag so pressure, dimming and hardware-scale capabilities reach the real display callbacks.
- Add regressions using the actual ready callback, unchanged legacy parser and decoded wire capabilities. Normal controller/OTA behavior is retained.
- USB/replay/OTA qualification remains tied to the exact image and recorded device checks; rc.12 replay failed its first physical run and was not published.

## 0.5.0-rc.12

- Add an explicit validation-only target with local generated-protocol replay, bounded command/ACK handling, visible simulation labels and isolated settings/profiles. Physical BLE/UART and external integrations are not initialized in this target.
- Add separate stopped-replay screen-only OTA qualification, preserving ordinary controller guards and length/hash/SDK/readback/boot checks. Normal/controller installation remains disabled.
- Distinguish validation image/manifest identity, require explicit test-image USB options, and reject validation binaries in normal release/pair tools. Build and test both targets in CI.
- Source/host evidence only until physical execution; no installed-machine or pairing changes are implied.

## 0.5.0-rc.11

- Report installed official base/protocol and controller compatibility independently of available releases. Distinguish disconnected, unidentified, protocol and version mismatches.
- Wire guarded screen web selection, frozen confirmation and progress to the own-repository endpoint; correlate replies, reject stale choices and avoid automatic retry after a lost reply. Keep both normal OTA permissions and candidate qualification disabled.
- Add gated candidate bench controls and an independent USB-controller test procedure; update release/compatibility documentation. This candidate has no new physical or OTA acceptance.

## 0.5.0-rc.10

- Retain the two-response image-body quota and normal TCP filling; remove per-dispatch pacing, which made some individual reads exceed their deadline in rc9 despite avoiding DMA exhaustion.
- Keep validated PSRAM images, nonblocking admission and response-owned permit cleanup. Cover cancellation before and after acquiring permits.

## 0.5.0-rc.9

- Admit at most two active K718 image bodies; additional HTTP responses yield without advancing content, retaining normal successful image responses. Hold permits until final response destruction, including disconnects.
- Test quota bounds, shared-owner lifetime, blocked offsets and permit release with sanitizers. RC8 returned all six-way image bytes but concurrent metadata reads still hit DMA pressure; that combined outcome remains required.

## 0.5.0-rc.8

- Yield K718 image responses after each 2 KiB batch using the pinned server retry signal, reducing simultaneous TCP-window bursts without sleeps or changed image bytes.
- Extend sanitizer tests across fragmented windows, retry offsets, complete content and buffer lifetime. RC7 completed all image reads but still recorded DMA pressure during six-way image loading and metadata refresh.

## 0.5.0-rc.7

- Select the pinned framework static Wi-Fi TX/RX pools before initialization and report DMA-specific heap diagnostics. Avoid on-demand allocation of up to 32 DMA TX buffers during image/HTTPS traffic.
- RC6 passed 55 repeated/two-connection image reads, but six browser-like connections plus refresh exposed 1,626-byte internal/DMA allocation failures. Keep that larger regression pending for this candidate.

## 0.5.0-rc.6

- Prefer PSRAM for all ordinary K718 allocations; keep explicit internal/DMA capability requests unchanged. Add nonblocking allocation-failure diagnostics.
- RC5 completed one-time image migration, but concurrent release refresh and image reads still exhausted internal memory. This candidate requires that combined-load regression before stability can be claimed.

## 0.5.0-rc.5

- Prefer PSRAM for K718 ordinary allocations above512 bytes before startup, preserving internal RAM for networking, task stacks and explicit DMA allocations. The framework default threshold is4096 bytes.
- Include largest/minimum internal heap in nonblocking diagnostics. This candidate investigates observed image-transfer stalls and network ENOMEM; device regression checks remain required.

## 0.5.0-rc.4

- Disable the optional ESPAsyncWebServer chunk-inflight gate only for K718; retain the pinned library and buffered image responses.
- Reproduce gate starvation after a final ACK and a temporarily reduced send window using the actual pinned gate code. Hardware bulk-transfer qualification remains separate.

## 0.5.0-rc.3

- Buffer K718 profile-image responses in PSRAM before sending HTTP chunks, so TCP response callbacks do not read from SD. Keep the original image format/API and other display targets unchanged.
- Check image size/header, short reads, allocation failure, fragmented reads and response-buffer lifetime with native sanitizers. Target transfer/network regression checks remain required.

## 0.5.0-rc.2

- Add hardware-independent checks of the firmware-generated protocol codec/queue and every four-icon status-row combination in both themes. Separate historical hardware evidence from current software validation, and document release-tag versus upstream-master differences.

- Normalize both absolute and project-relative dependency paths in compiler metadata. This covers CI caches inside `.tools/` and external local caches, which previously produced different framework path strings.
- Retain all 1.10.0 / protocol-16 adaptations from rc.1; hardware acceptance remains pending and all OTA installers remain disabled.

## 0.5.0-rc.1

- Adopt official GaggiMate v1.10.0 and controller protocol 16; pair with the hash-pinned, unmodified official controller image. Previous protocol-5 controllers are incompatible.
- Regenerate native 360 x 360 UI for the updated 74-handler EEZ design, live charts, profile previews/images and setup/welcome flow. Move rotary profile browsing to the new selector; preserve half-step Grind, parameter focus, LED gauges and custom standby content.
- Retain filtered battery estimates and lightning-only external power; add the shared battery status to the welcome screen.
- Preserve upstream settings save validation and multilingual web UI alongside isolated K718 LED/standby settings and attribution.
- Support append-only upstream merges with deterministic net-adaptation export and exact official-base replay. Keep all OTA installation disabled for this migration candidate.
- Software candidate only; target-device interaction, protocol-16 reconnection, settings migration, charts and real brewing remain unverified.

## 0.4.0-rc.9

- Stabilize the coarse battery estimate with trimmed ADC bursts, median/time filtering and display hysteresis. Preserve prompt low/critical indication, clear stale readings, and keep USB/external power lightning-only without cached SOC. The voltage curve remains uncalibrated.
- Clarify that screen OTA candidate testing has completed while installation from the ordinary web page is not enabled. Compatibility text no longer labels every matching pair as unverified. Keep normal/controller installation permissions unchanged.
- Move the official Stable/Nightly selector into a collapsed notifications section, separate from Knob Screen matched releases.
- Software candidate only; battery behavior and the revised web UI still need device validation.

## 0.4.0-rc.8

- Prepare a separate forward OTA target with the shared network worker and incremental erasure. Normal display/controller installation remains disabled pending qualification.

## 0.4.0-rc.7

- Erase the inactive OTA image incrementally during sequential writes, keeping controller telemetry live. Retain exact manifest length/hash limits, SDK validation and complete flash readback before boot selection.
- RC5-to-RC6 live staging began but aborted after the bulk erase interrupted telemetry. The running image and boot selection were retained; qualification continues.

## 0.4.0-rc.6

- Prepare a distinct forward OTA target for the shared-network-worker candidate. Normal display/controller installation remains disabled pending hardware qualification.

## 0.4.0-rc.5

- Share one preallocated background HTTPS worker between release checks and screen installation, reducing internal stack memory and serializing network work. Screen install jobs take priority over queued discovery checks.
- Keep normal display/controller OTA disabled. RC3-to-RC4 live attempts failed closed before boot selection; network qualification continues with new, immutable candidates.

## 0.4.0-rc.4 — screen OTA test target

- Retain the verified-download candidate implementation and its explicit bench-only permission in a separate forward-update version. Normal display/controller OTA remains disabled.

## 0.4.0-rc.3 — screen OTA candidate testing

- Add an isolated own-repository screen downloader/installer with complete length/SHA-256 checks, SDK image validation and flash readback before boot selection. Reuse the upstream resumable HTTPS transport.
- Add an explicit candidate-only test permission and verification-only staging. Normal screen/controller buttons and the legacy K718 installer remain disabled.
- Require a fresh matched selection, healthy settled boot and an idle standby state. Exclude concurrent control actions without holding the process mutex across callbacks.
- USB recovery and health-gated trial boot have been exercised on a development build. Network-install qualification remains separate from software checks.

## 0.4.0-rc.2 — USB recovery and startup health

- Recognize settled app0/app1 boot selections, including the vendor sequence-zero placeholder, in the checked USB updater. Normal updates stage in the inactive slot and retain the current application. Add explicit, backed-up app0 recovery and inactive-slot USB trials; commit boot selection only after application readback.
- Defer Arduino's early OTA acceptance. Confirm a trial only after local display/touch initialization, a completed frame, settings/filesystem access and 20 seconds of live UI/main/logic loops. Controller or Wi-Fi absence alone does not fail screen health.
- Expose read-only startup health, pending state and running-slot diagnostics. Keep both OTA installers disabled pending hardware qualification and download verification.

## 0.4.0-rc.1 — OTA groundwork

- Add read-only screen OTA preflight with immutable own-release asset binding, version/channel/controller checks, busy-state guards and reviewed A/B partition bounds. Expose separate screen/controller installation permissions, both disabled. No installer or download endpoint is enabled.

## 0.3.8 — USB stable

- Browse the screen Profile selector with the knob, one profile per detent. Reuse the original previous/next actions and limits, with no extra highlight; tap the existing checkmark to load the preview. Touch arrows remain available.
- Retain the USB-only installation policy and independent matched-release discovery. Screen OTA will be developed and qualified separately; both OTA installers remain disabled in this release.

## 0.3.7 — USB stable

- Set Grind knob adjustment to 0.5 seconds or 0.5 grams per detent; show fractional time targets on the screen and web dashboard. Keep the original touch-button steps.

- Isolate official update checks on a background worker so slow HTTPS cannot stall controller initialization or telemetry. Preserve official sources/channels and discard stale channel results.

- Use the final port identity and stable channel; matched-release checks exclude pre-releases while preserving independent screen/controller versions and official notices.
- Retain native K718 hardware support, rotary adjustments, persistent LED controls and standby customization. Both OTA installers remain disabled.
- Fix the checked USB updater's post-write verification after an automatic USB reconnect.
- Separate stable-release requirements from optional work and record real brewing dynamics as unverified, outside this USB release's validation scope.

## 0.3.7-rc.1 — candidate

- Check our published matched releases in a background worker and display independent component versions and compatibility in System & Updates.
- Validate HTTPS metadata, manifest/asset digests, target/partition and protocol metadata; keep both OTA installers disabled.
- Preserve separate official Stable/Nightly notices and handle missing releases, network errors and unknown controllers explicitly.
- Compare port versions using SemVer precedence, including final/prerelease ordering and build metadata.

## 0.3.6-rc.1 — pre-release

- Add LED-ring enable/brightness controls in Settings → General. Saved preferences apply immediately and return after reboot.
- Keep LED-only saves separate from coffee-machine parameter updates.
- Prepare matched firmware packages with the original, hash-pinned official controller application and independent component metadata. OTA installation remains disabled.

## 0.3.5-rc.2 — candidate

- Disable display and controller OTA installation in the web UI and firmware, with a USB installation notice.
- Preserve official update checks, channel selection and update notifications. Compare official releases against the upstream base version rather than the K718 port suffix.

## 0.3.5-rc.1 — pre-release

- Remove the hardware-test page while retaining callable hardware APIs.
- Prepare the firmware-only repository, English porting/hardware/roadmap documentation and publication checks.
- Preserve upstream web credits and add knob-screen attribution.

## Earlier port work

- Native 360 × 360 display/touch/backlight support.
- Battery status, optional LED gauge mirror and rotary parameter adjustment.
- Custom standby text/clock and reproducible-build tooling.

This is a source history summary, not a claim that every listed feature was tested on one identical binary. Published releases carry their own status.
