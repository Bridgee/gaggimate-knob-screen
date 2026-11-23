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

A test-only protocol replay/simulated peer is a next development step for exercising connected page states, synthetic temperature/pressure changes, rotary actions and LED following on the physical Knob. It must remain isolated from production communication and clearly labelled as simulated. It is **not implemented or qualified by this document**, and cannot establish real BLE pairing, timing or controller behavior. Existing host codec/UI tests are not that physical replay harness.

Do not add a production setting that pretends an absent controller is connected. Do not forward test control commands to the installed machine. Raw recordings, device identities and backups stay outside the public repository.

## Screen OTA without a second controller

Screen image download, inactive-slot writes/readback, trial boot and recovery can be investigated on the Knob independently of controller firmware. This is the next qualification route to implement, not an existing bypass.

**The current network installer still requires a compatible connected controller and idle standby.** Normal screen/controller permissions and candidate qualification remain disabled. A future disconnected screen-only qualification path must be explicit, isolated from normal installation, preserve running-image/USB recovery and never claim controller compatibility. It must not weaken the production preflight to make a test pass.

Use immutable own-repository candidate images and exact manifests; drafts are not discovered. For the future isolated path, verify complete length/hash/SDK/readback before boot selection, interrupted staging, unconfirmed-boot rollback, retained data and recovery. Record physical power-loss behavior only if actually tested. Current-protocol ordinary connected web flow remains unverified until exercised with the existing controller.

Controller OTA is separate: actual receiver/reboot/recovery qualification requires the one real controller. Defer enabling it until a planned controller update can be validated with an available USB recovery route. It is not a prerequisite for USB screen releases or screen-only development. No routine fault injection is assumed on the daily-use controller.

## Focused acceptance at the eventual changeover

When the user is ready to use Knob with the existing controller, record the exact final screen image, official controller version/protocol and backup/recovery route before changing pairing. Do not promise that the old bond can be restored by simply turning LilyGO back on.

Check handshake/reconnection, fresh ambient telemetry, changed connected pages, profiles, rotary focus/half-steps, LED preferences, standby/wake and restart retention. Reuse accepted geometry/pins/LED orientation. Process activation or real brewing is not needed for this USB scope; brewing dynamics remain explicitly unverified.

Stable publication still needs its declared focused final-package acceptance. The absence of a spare board is not a reason to repeat hardware discovery, disturb the daily-use pairing or hold software/candidate development open indefinitely. See [Validation scope](VALIDATION.md), [Updates](UPDATES.md) and [Building and flashing](BUILDING.md).
