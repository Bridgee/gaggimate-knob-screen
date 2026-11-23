# Testing with one installed controller

The supported maintenance workflow assumes **one controller**, installed in a daily-use machine and paired to its LilyGO screen. Extra controller hardware is not a requirement. Leave that firmware and pairing unchanged during routine port development.

Official v1.10.0 uses a single paired display, directed advertising and an allowlist. Adopting a replacement display prunes foreign bonds. Disconnecting the LilyGO does not create a second read-only Knob connection, and temporary re-pairing is not a routine test step.

## Reusable validation levels

| Level | Equipment and checks | Evidence |
| --- | --- | --- |
| Host regression | Locked tools; native LVGL pages/handlers, both themes, status icons, rotary policies, generated protocol/queue, web update states, downloads and package reconstruction | Repeatable source and software checks; no physical radio or controller qualification |
| Knob alone | Knob and USB; checked installation/readback, startup health, web/profile/image loading, settings retention, standby/wake and battery behavior | Physical screen and network evidence without changing the installed machine; disconnected state is expected |
| Existing controller, when intentionally switching to Knob | A planned user-controlled changeover, followed by focused connection/telemetry and interaction checks | Real BLE acceptance; deferred while the daily-use LilyGO remains in service |

Reuse earlier peripheral acceptance when source and relevant inputs are unchanged. Repeat real-controller checks when protocol, transport, bonding, hardware configuration or relevant dependencies change. A software-verified candidate can be shared with explicit limits; do not convert missing radio evidence into a hardware pass.

## Host checks

```sh
python scripts/k718.py check
python scripts/k718.py build --out dist/release
python test/knob_native/check_protocol.py
python scripts/k718.py export --out dist/port-stack
python scripts/k718.py replay --stack dist/port-stack --dest ../knob-rebuild
```

The full build runs generated-protocol, rendered OTA web, upstream web and package checks. Use a new output directory and clean committed source. Compare actual CI artifacts when claiming hosted reproducibility. An exact source replay alone is not binary equality.

## Knob-only development

Current controller-free checks cover USB recovery/trial boot, local startup health, saved data, web image traffic and disconnected behavior. Local health confirmation does not require Wi-Fi or a controller.

The dedicated **display-knob-validation** image now implements local replay through the actual generated nanopb frames, LinkSelector, Endpoint and display facade callbacks. It never initializes the physical BLE/wired client. Idle, warmup and shot traces interpolate synthetic temperature/pressure/weight; pause/reset/disconnect are deterministic controls. Outbound commands terminate locally, with ACK/dedup and bounded command counters. This is data replay, not a thermal model or a controller substitute.

Build it separately from the normal image:

```sh
python scripts/k718.py build-validation --out dist/validation
python scripts/package_validation.py dist/validation --out dist/validation-publish
```

On that image, open **Settings → System → Local data replay**. The screen and web UI identify simulated data. `GET /api/validation` returns replay status; a form `POST` accepts `action=play|pause|reset|disconnect` and `trace=idle|warmup|shot`. The endpoint and local transport do not exist in normal firmware. Start at idle, inspect parameters/rotary/LED behavior, then replay the other traces; process starts and autotuning are disabled.

Validation settings use `knob-test` NVS, seeded once from read-only screen settings; profiles use `/validation/p` and their images use `/validation/pi`. The physical bond store is not initialized or cleared. HomeKit/MQTT/mDNS, Bluetooth-scale and external-grinder integrations are not started. History and machine-profile stores are unavailable to the validation API. This preserves the production workflow while allowing local parameter tests.

Host tests compile the real replay transport and generated codec with sanitizers, including malformed bounds, ACK/dedup, pause/reset/disconnect, hardware-status messages and concurrent command production. The ESP32 validation build uses the real Endpoint dispatch task. Neither a successful host test nor compilation establishes physical display, RTOS timing, real BLE or controller acceptance.
Do not add a production setting that pretends an absent controller is connected. Do not forward test control commands to the installed machine. Raw recordings, device identities and backups stay outside the public repository.

## Screen OTA without a second controller

The validation image now has a separate **Isolated screen OTA qualification** entry. Ordinary screen/controller permissions remain off, and normal preflight still requires the matching physical controller. The isolated entry does not assert controller compatibility or fabricate a connected controller.

1. Stop/disconnect replay and enter standby. The actual queue must be quiescent, startup healthy and boot selection settled. The validation firmware owns no physical controller link.
2. Refresh own-repository matched metadata. Only the pinned K718 target/protocol, asset URLs and manifest/image hashes are accepted; drafts are ignored. A matching release must be published before a real network test. Same-version staging is permitted only on this qualification path; downgrades are rejected.
3. **Verify screen image only** writes and verifies the inactive slot without changing boot selection. It checks full length/SHA-256, SDK validation and flash readback.
4. **Test-install screen image** additionally commits the verified slot and restarts. The selected image is a normal own-release screen image; after boot it leaves replay firmware. Keep the previous compatible image and checked USB recovery available.

Requests use the existing `req:screen-ota-start` endpoint with explicit `qualification: true`, `screenOnly: true`, `verifyOnly`, and frozen tag/manifest/image hashes. Both metadata preflight and queue permissions are recomputed by the server. Runtime reservations refuse replay/process changes during staging and recheck local isolation/standby before committing. An ordinary image cannot gain this permission through a web/NVS setting.

The downloader, sequential erasure, image checks, inactive-slot readback, trial boot and USB fallback are shared with the existing installer. Validate interruption, unconfirmed-boot rollback and retained data on the Knob when physical testing is scheduled. Record physical power loss only when actually tested. Current-protocol real connected user flow remains unverified until exercised with the existing controller.

Validation packages have target `display-knob-validation`, an embedded role marker and `validation_only: true`. Normal publication/pair and USB tools reject them. The checked USB tool requires explicit `--test-image` and `--validation` for a validation image; it still requires a verified screen identity and backup. These are code/build capabilities, not a claim that any current image has passed physical qualification.
Controller OTA is separate: actual receiver/reboot/recovery qualification requires the one real controller. Defer enabling it until a planned controller update can be validated with an available USB recovery route. It is not a prerequisite for USB screen releases or screen-only development. No routine fault injection is assumed on the daily-use controller.

## Focused acceptance at the eventual changeover

When the user is ready to use Knob with the existing controller, record the exact final screen image, official controller version/protocol and backup/recovery route before changing pairing. Do not promise that the old bond can be restored by simply turning LilyGO back on.

Check handshake/reconnection, fresh ambient telemetry, changed connected pages, profiles, rotary focus/half-steps, LED preferences, standby/wake and restart retention. Reuse accepted geometry/pins/LED orientation. Process activation or real brewing is not needed for this USB scope; brewing dynamics remain explicitly unverified.

Stable publication still needs its declared focused final-package acceptance. The absence of a spare board is not a reason to repeat hardware discovery, disturb the daily-use pairing or hold software/candidate development open indefinitely. See [Validation scope](VALIDATION.md), [Updates](UPDATES.md) and [Building and flashing](BUILDING.md).
