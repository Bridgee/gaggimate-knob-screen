# Changelog

## Unreleased maintenance

- Keep the checked USB updater in stub mode after esptool automatically reconnects, so post-write verification can read preserved partitions. This changes the host tool, not the running screen application.
- Separate stable-release acceptance requirements from ongoing maintenance and optional roadmap work.

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
