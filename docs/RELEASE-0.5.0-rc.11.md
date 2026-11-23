# 0.5.0-rc.11 — release and web-update integration candidate

Based on official **GaggiMate v1.10.0**, commit `34bf9fc5f6cf89c9929636fc23092b75284092f9`, controller protocol **16**. Target: **GUITION JC3636K718C_I_YR1**, native 360 × 360.

Required controller: official **v1.10.0**. The matched package carries its unmodified official image; the screen is the independent K718 adaptation. Protocol-5 controllers are incompatible.

## Changes

- Preserve the v1.10.0 native UI and K718 display/touch/brightness, battery status, LED controls, rotary parameter/Profile browsing and custom standby extensions.
- Retain the rc.10 profile-image ownership/admission and memory/network fixes.
- Report installed screen/base/protocol and controller compatibility independently of release discovery.
- Wire screen web selection, explicit confirmation and progress to the own-repository installer. Distinguish disconnected, unknown-version, protocol/version mismatch, expired metadata, standby, boot and partition guards.
- Keep normal screen/controller/combined installation disabled. Explicit candidate-only bench controls remain gated by the build permission, which is false in this package. Official notices remain separate.
- Document an independent USB-controller bench, update behavior and stable-release requirements.

## Validation and limits

This is a software-validation candidate, not a stable or OTA-qualified release. See the matching build/CI and package hashes for its exact source identity. Earlier rc.10 standalone USB/data/network tests and historical protocol-5 peripheral/OTA tests remain evidence for those earlier packages, not physical acceptance of this image.

Current-protocol connected BLE/telemetry, changed physical interaction and the web-install/recovery flow require the isolated bench. Real brewing dynamics remain explicitly unverified and outside USB acceptance. Battery percentage is uncalibrated; continuous audio/haptics are not qualified.

## Installation and rollback

Use the checked screen USB updater with a verified device identity and private backup; do not write the merged first-install image over an existing installation. The pair's application files are not first-install images. Keep the previous compatible v1.10.0 K718 package; the older protocol-5 stable release is not a compatible fallback for a protocol-16 controller. See [Building and flashing](BUILDING.md).

Publishing a final 0.5.0 requires a new stable source/version identity, software/CI checks and focused final-package acceptance. A successful host build or a draft Release does not satisfy those hardware checks.
