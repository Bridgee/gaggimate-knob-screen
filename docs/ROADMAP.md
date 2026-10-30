# Roadmap

## Implemented

- Native 360 × 360 display, touch, backlight and upstream controller connection.
- Battery/power status, optional LED gauge mirror with persistent enable/brightness controls, and rotary parameter adjustment.
- Custom standby text/clock and callable speaker/microphone interfaces.
- English firmware repository, exact official-base source reconstruction and reproducible-build tooling.
- Matched packages containing our screen image and a hash-pinned, unmodified official controller image.
- Read-only matched-release discovery, independent component versions and compatibility status; official upstream notices retained. Both OTA installers remain disabled.

Implemented does not mean that every new binary has completed hardware acceptance. See the release notes for the exact tested package.

## Stable-release requirements

- Validate the current candidate on the target: real GitHub metadata refresh, separate component versions/compatibility, retained official notices and disabled installation controls.
- Check normal operation after the update and restart: controller reconnection/telemetry, display/touch, rotary adjustment, LED preferences, standby/wake and saved settings. Reuse established peripheral evidence; repeat basic hardware tests only when their implementation or inputs change.
- Record the scope of real-machine use. Dynamic temperature/pressure and LED following during brewing remain unverified and are explicitly excluded from the USB validation scope. They do not block this USB release; complete that check before claiming brewing acceptance.
- Build the final stable identity, pass software/CI checks and verify that exact final package on hardware. Archive source identity, artifact checksums, compatibility, acceptance results and the previous package/USB rollback instructions before publishing.

## Ongoing maintenance (not first-release blockers)

- Exercise the documented replay/adaptation process against the next selected official firmware version. Same-base reconstruction is verified; a cross-version migration still needs review and testing.
- Incorporate compatibility reports from additional units of the same model and keep documentation current.
- Keep published tags/assets immutable and validate changed behavior for each release.

## Next development stage: screen OTA

Read-only preflight, separate disabled permissions, two-slot USB recovery/trials and local startup-health confirmation are implemented in development. Recovery/health have been exercised on a development build. An explicit candidate-only download/install test interface is implemented; normal installation remains disabled pending end-to-end qualification.

1. Implemented: bind installation requests to a freshly validated own-repository release and exact manifest/image hashes.
2. Exercised on hardware: support USB recovery from either application slot and application health-gated boot confirmation.
3. Implemented for candidate tests: background screen downloader/installer; verify complete length and hash before changing boot selection.
4. Validate real updates, settings/reconnection and corrupt/interrupted/failed-boot recovery before enabling screen installation.

Keep the old K718 installer denied. Controller OTA needs independent qualification; combined updates follow later. These stages do not block the existing USB stable release.

## Optional future work

- Reproducible controller self-builds compared against official binaries. Continue distributing pinned official controller images until a replacement is independently qualified.
- Pinned build tools for additional host platforms and cross-host reproducibility evidence.
- Better battery estimation and power-source reporting where hardware permits.
- Optional knob/audio interactions without changing the original touch workflow.

Optional work does not block a USB-only stable release. These are directions, not promised dates. Battery estimates, supported build hosts and validation limits must remain explicit.
