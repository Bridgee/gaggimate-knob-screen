# Port architecture and migration

## Upstream foundation

- Official version: GaggiMate v1.9.0.
- Official commit: `43d2a7ba2299c196fd67c001f455985e07e57a49`.
- Controller protocol: 5.
- Target: GUITION JC3636K718C_I_YR1, ESP32-S3, 360 × 360.

This is a display port. The original controller/PID/process behavior and coffee-control actions are retained. Hardware adaptation and requested extensions connect to the existing application rather than replacing its workflow.

## Implementation layers

| Layer | Implementation | Responsibility |
| --- | --- | --- |
| Board support | `boards/Guition-K718.json`, `src/display/drivers/GuitionKnob*` | LCD initialization, touch, backlight and board peripherals |
| Native UI | `scripts/generate_knob_ui.py`, `src/display/ui/default/knob360/` | Generate 360 × 360 geometry, fonts and image resources from upstream EEZ inputs |
| Power status | `KnobMenuBattery.*`, `KnobBatteryImages.*` | Shared menu/standby battery icon and power-source state |
| LED gauges | `KnobLedGauges.h`, `LedGaugePolicy.h`, `LedSpiEncoding.h` | Mirror visible meter values/colors through the hardware worker |
| Rotary input | `EncoderDetents.h`, `KnobAdjustment.*` | Decode direction contacts; select a field and invoke its existing +/− action |
| Standby appearance | `StandbyDisplay.*`, `Settings.*`, General web settings | Optional custom text or centered clock with original wake behavior |
| Build/maintenance | `port/`, `scripts/k718*.py`, `test/knob*` | Locked inputs, checks, source replay, artifact identity and checked installation |

## Rendering and interaction

The original EEZ project remains the source design. Native generation changes geometry and display metadata, maps font sizes and prepares image resources offline.
LVGL renders directly at 360 × 360 and flushes dirty rectangles to the display; touch coordinates are native pixels.

Checks preserve the ten upstream pages, all 64 page event handlers and the flow asset bytes except display dimensions. The original drawing/click-area helpers have coordinate adaptations.
Rotary changes call existing button handlers, retaining their step sizes, limits and save behavior. Multi-field pages require touch selection first.

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

Same-base replay must reproduce the exact source tree. Export checksums, Git conflict state and build manifests make failures traceable; CRLF bytes in vendor inputs are preserved.
For a newer official release, follow [Maintaining](MAINTAINING.md): review changes, replay/adapt, bind the new upstream identity, build, verify and publish a new candidate.

Hardware APIs and tested configuration are documented in [Hardware interfaces](HARDWARE.md). Reuse those interfaces; verify changed behavior rather than repeating basic pin discovery.
