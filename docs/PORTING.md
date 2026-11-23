# Port architecture and migration

## Upstream foundation

- Official version: GaggiMate v1.10.0.
- Official commit: `34bf9fc5f6cf89c9929636fc23092b75284092f9`.
- Controller protocol: 16.
- Target: GUITION JC3636K718C_I_YR1, ESP32-S3, 360 × 360.

This is a display port. The original controller/PID/process behavior and coffee-control actions are retained. Hardware adaptation and requested extensions connect to the existing application rather than replacing its workflow.

## Implementation layers

| Layer | Implementation | Responsibility |
| --- | --- | --- |
| Board support | `boards/Guition-K718.json`, `src/display/drivers/GuitionKnob*` | LCD initialization, touch, backlight and board peripherals |
| Native UI | `scripts/generate_knob_ui.py`, `src/display/ui/default/knob360/` | Generate 360 × 360 geometry, fonts and image resources from upstream EEZ inputs |
| Power status | `KnobMenuBattery.*`, `KnobBatteryImages.*` | Shared menu/standby battery icon and power-source state |
| LED gauges | `KnobLedGauges.h`, `LedGaugePolicy.h`, `LedPreferences.h`, `LedSpiEncoding.h` | Mirror visible meter values/colors through the hardware worker |
| Rotary input | `EncoderDetents.h`, `KnobAdjustment.*` | Decode direction contacts; select a field and invoke its existing +/− action |
| Standby appearance | `StandbyDisplay.*`, `Settings.*`, General web settings | Optional custom text or centered clock with original wake behavior |
| Build/maintenance | `port/`, `scripts/k718*.py`, `test/knob*` | Locked inputs, checks, source replay, artifact identity and checked installation |

## Rendering and interaction

The original EEZ project remains the source design. Native generation changes geometry and display metadata, maps font sizes and prepares image resources offline.
LVGL renders directly at 360 × 360 and flushes dirty rectangles to the display; touch coordinates are native pixels.

Checks preserve the ten upstream pages, all 74 page event handlers and the flow asset bytes except display dimensions. The original drawing/click-area helpers have coordinate adaptations.
Rotary changes normally call existing button handlers. Grind uses 0.5 seconds or 0.5 grams per detent through the existing controller setters, retaining the original limits, change events and save behavior. Its time label includes tenths, mirrored by the web dashboard. Original touch-button steps remain unchanged. Multi-field pages require touch selection first. Profile browsing reuses the original previous/next actions, without an additional highlight or automatic load; the existing checkmark confirms the preview.

The additional power icon, rotary highlight, LED mirror, standby options and web attribution are extensions. Brightness updates and theme-cache handling are compatibility fixes.
OTA installation is an explicit functional difference: both display and controller installation are blocked in this port until a compatible OTA path is implemented. Official release discovery and notifications remain enabled. Use compatible firmware over USB; an official LilyGO binary does not contain the K718 driver.

## Reconstruct from official firmware

A complete ordered patch stack is generated from the pinned official commit, not from private development records:

```sh
python scripts/k718.py export --out dist/port-stack
python scripts/k718.py replay --stack dist/port-stack --dest ../knob-rebuild
cd ../knob-rebuild
python scripts/k718.py build --out dist/release
```

Same-base replay must reproduce the exact source tree. After merging a new official baseline, export emits a deterministic net-adaptation patch while recording the real source commit/tree; upstream commits are not replayed as port patches. Linear historical stacks remain supported. Export checksums, Git conflict state and build manifests make failures traceable; CRLF bytes in vendor inputs are preserved.
For a newer official release, follow [Maintaining](MAINTAINING.md): review changes, replay/adapt, bind the new upstream identity, build, verify and publish a new candidate.

Hardware APIs and tested configuration are documented in [Hardware interfaces](HARDWARE.md). Reuse those interfaces; verify changed behavior rather than repeating basic pin discovery.

## Official 1.10.0 migration

K718 profile-image HTTP responses preload the fixed-size image into PSRAM, validate its header and close the file before TCP pulls chunks. The immutable response owns its buffer until transmission ends; callbacks only copy bounded memory. Other display targets retain the upstream file response. The K718 target also disables the pinned HTTP library's optional chunk-inflight gate; a native test exercises its reduced-window starvation case. This is a target transport setting, not a controller/protocol change; validate large and concurrent transfers on the device.

The target adopts upstream protocol 16 and controller APIs without renumbering the old protocol. The new EEZ input has 74 handlers and 262 geometry sites; `port/ui-contract.json` pins the reviewed structure. The removed legacy ProfileScreen is replaced by upstream NewProfileScreen; knob browsing uses its original previous/next actions and explicit touch confirmation, including its list view. Native generation supports the new SVG/indexed artwork; dynamic profile images retain per-image zoom.

The original live charts, profile images, setup/welcome flow and settings persistence checks remain present. The welcome status row also receives the shared battery icon. Screen-only LED/standby changes keep their isolated save path. Original upstream language catalogs and CA certificate fingerprints have exact-content publication exceptions; port documentation stays English and private-path/secret checks still apply.

This candidate has not completed hardware acceptance. Screen OTA qualification from the prior v1.9.0 candidate does not transfer to it; the test interface and ordinary installers remain disabled. Matching controller packages use the original official v1.10.0 application, verified by size/hash/image digest.
