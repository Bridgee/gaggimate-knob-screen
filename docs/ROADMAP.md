# Roadmap

## Implemented

- Native 360 × 360 display, touch, backlight and upstream controller connection.
- Battery/power status, optional LED gauge mirror with persistent enable/brightness controls, and rotary parameter adjustment.
- Custom standby text/clock and callable speaker/microphone interfaces.
- Dedicated validation-only image with framed local replay, command termination, isolated settings/profiles and separate screen-only OTA qualification; physical acceptance pending.
- English firmware repository, exact official-base source reconstruction and reproducible-build tooling.
- Matched packages containing our screen image and a hash-pinned, unmodified official controller image.
- Matched-release discovery, independent installed/release compatibility, and own-release screen web selection/confirmation/progress; official notices retained. Both OTA installers remain disabled.

Implemented does not mean that every new binary has completed hardware acceptance. See the release notes for the exact tested package.

## Stable-release requirements

When an installed controller is paired to another screen, continue the [controller-free validation route](VALIDATION.md#validation-without-changing-an-installed-machine). Follow the [single-controller workflow](TESTING.md); extra hardware is not required. Preserve the daily-use pairing and defer real BLE acceptance until an intentional changeover. Current host checks and prior peripheral acceptance reduce repeated work, but leave the focused current-binary controller checks pending; they do not silently satisfy them.

- Validate the current candidate on the target: real GitHub metadata refresh, separate component versions/compatibility, retained official notices and disabled installation controls.
- Check normal operation after the update and restart: controller reconnection/telemetry, display/touch, rotary adjustment, LED preferences, standby/wake and saved settings. Reuse established peripheral evidence; repeat basic hardware tests only when their implementation or inputs change.
- Record the scope of real-machine use. Dynamic temperature/pressure and LED following during brewing remain unverified and are explicitly excluded from the USB validation scope. They do not block this USB release; complete that check before claiming brewing acceptance.
- Build the final stable identity, pass software/CI checks and verify that exact final package on hardware. Archive source identity, artifact checksums, compatibility, acceptance results and the previous package/USB rollback instructions before publishing.

## Ongoing maintenance (not first-release blockers)

- Keep the cross-version migration and exact source reconstruction checks passing. The 0.5.0 candidate targets official 1.10.0 / protocol 16; complete its declared focused acceptance with the existing controller at a planned changeover before a stable release.
- Incorporate compatibility reports from additional units of the same model and keep documentation current.
- Keep published tags/assets immutable and validate changed behavior for each release.

## Next development stage: screen OTA

Read-only preflight, separate disabled permissions, two-slot USB recovery/trials and local startup-health confirmation are implemented in development. Recovery/health have been exercised on a development build. The explicit candidate-only interface passed USB-bench download/readback, interrupted staging, unconfirmed-boot rollback, healthy installation, settings and UI checks in 0.4.0-rc.8. The web action is now connected to the own-release backend, with confirmation, stale-selection rejection and progress. Normal web installation remains disabled; current-protocol bench and normal-flow qualification are still pending.

1. Implemented: bind installation requests to a freshly validated own-repository release and exact manifest/image hashes.
2. Exercised on hardware: support USB recovery from either application slot and application health-gated boot confirmation.
3. Exercised on hardware: own-repository screen download/install, length/hash/SDK/readback verification, interrupted staging and unconfirmed-boot recovery. Malformed/corrupt-image cases also have host tests.
4. Implemented: wire the screen web action to the own-release path, retaining the legacy installer block and separate disabled controller permission. Implemented: isolated validation-only replay and Knob-only screen qualification with stopped-replay/healthy-slot guards. Pending: actual device download/readback/boot recovery, followed by ordinary connected-flow qualification before enabling normal installation. Record physical power loss and other untested cases explicitly.

Keep the old K718 installer denied. Controller OTA needs independent qualification; combined updates follow later. These stages do not block the existing USB stable release.

## Optional future work

- Reproducible controller self-builds compared against official binaries. Continue distributing pinned official controller images until a replacement is independently qualified.
- Pinned build tools for additional host platforms and cross-host reproducibility evidence.
- Better battery estimation and power-source reporting where hardware permits.
- Optional knob/audio interactions without changing the original touch workflow.

Optional work does not block a USB-only stable release. These are directions, not promised dates. Battery estimates, supported build hosts and validation limits must remain explicit.
