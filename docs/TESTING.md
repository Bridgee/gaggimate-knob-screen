# Testing without interrupting a daily-use machine

Keep the installed LilyGO display and its controller paired. Do not use that controller for repeated development updates or fault injection. A passing host test is not a replacement for a real BLE endpoint.

## Three reusable levels

| Level | Equipment and checks | Evidence |
| --- | --- | --- |
| Host regression | Locked tools; native LVGL pages/handlers, both themes, status icons, rotary policies, generated protocol/queue, web update states, downloads and package reconstruction | Repeatable source and software checks; no physical radio or controller qualification |
| Knob alone | Knob on USB; checked installation/readback, startup health, web/profile/image loading, settings retention, standby/wake and battery-only behavior when convenient | Local screen and network evidence without changing an installed controller's pairing; disconnected state is expected |
| Independent controller bench | Spare matching GaggiMate controller board, original official firmware and its required sensors; USB only, with no machine loads connected | Real BLE handshake/reconnection, ambient telemetry, connected screen behavior and OTA/recovery tests |

A spare controller is the simplest durable test fixture. Use the same board variant and the unmodified official image pinned in `port/controller-release.json`. Consult the official hardware documentation for its power and sensor requirements. A generic ESP32 board, mocked telemetry or a desktop simulator must not be labelled equivalent to that controller. The bench never needs a heater, pump, grinder or coffee machine attached.

If no bench controller is available, continue levels one and two and distribute an accurately labelled candidate. Do not silently promote historical protocol-5 acceptance to protocol 16 or disturb a daily-use pairing to finish a checklist.

## Host checks

```sh
python scripts/k718.py check
python scripts/k718.py build --out dist/release
python test/knob_native/check_protocol.py
python scripts/k718.py export --out dist/port-stack
python scripts/k718.py replay --stack dist/port-stack --dest ../knob-rebuild
```

The full build runs the generated-protocol test, rendered OTA web tests, upstream web regressions and package checks. Use a new output directory and a clean committed source. Compare real CI artifacts when claiming reproducibility; a source replay alone does not establish binary equality.

## Focused controller-bench acceptance

Record the exact screen version/application hash, official controller version/protocol and results. Keep private identities and raw evidence outside this repository.

1. Pair the Knob with the bench controller once. Confirm the expected controller version/protocol, ambient temperature and plausible idle pressure; no process start is needed.
2. Restart the Knob, then disconnect/reconnect only the bench controller's USB supply. Check reconnection, fresh telemetry and correct disconnected/version-mismatch messages.
3. Check changed connected pages, profile/list browsing, parameter focus and Grind half-steps, LED gauges/preferences, both themes and standby/wake. Turn targets only; do not press brew, steam, water or grinder start.
4. Repeat profile/image loading and release refresh while interacting. Check saved settings after an ordinary restart without a serial reader attached.
5. Exercise the exact final stable package after generating its final identity. Keep earlier receipts attached to their original image; do not rename an RC image as a stable build.

Reuse accepted pin assignments, rotary direction and LED orientation unless those inputs change. Real brewing dynamics are explicitly outside this USB acceptance scope.

## OTA qualification on the same isolated bench

Normal screen and controller installation permissions remain off. A candidate may explicitly enable `screen_ota_qualification` for the separate **Screen OTA bench tests** controls; this flag cannot enable normal or controller OTA. Candidate bench controls use the same own-release selection, confirmation and staging backend as the future normal screen button.

Use two immutable, published candidate versions with complete matching assets; drafts are not discovered. Start in idle standby with a healthy settled boot and a compatible bench controller. Keep a known compatible USB package and private backup available.

- First use **Verify inactive image**: require complete download/hash/SDK/flash-readback success and unchanged boot selection. This writes the inactive slot, not the running application.
- Use **Test screen install** for the forward candidate; confirm the new exact version, startup health, retained settings, reconnection and local UI.
- Check interrupted staging and unconfirmed-boot recovery with the documented USB fallback. Host tests additionally cover malformed images, wrong size/hash, stale selections and changed guards. Do not mark physical power-loss recovery passed unless it was actually exercised.
- Verify rejection when busy, not in standby, disconnected, mismatched, metadata-expired or an old confirmation is submitted. No failed preflight should begin a transfer.

Only after current-protocol qualification should normal screen web permission be enabled and that exact normal user flow tested. Controller OTA requires a separate official-image BLE-transfer, reboot/reconnect, settings-retention, interruption and USB-recovery qualification. Combined or cross-protocol updates remain separate work.

See [Updates](UPDATES.md), [Validation scope](VALIDATION.md) and [Building and flashing](BUILDING.md).
