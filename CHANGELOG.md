# Changelog

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
