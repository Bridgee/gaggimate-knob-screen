# Roadmap

## Implemented

- Native 360 × 360 display, touch, backlight and upstream controller connection.
- Battery/power status, optional LED gauge mirror with persistent enable/brightness controls, and rotary parameter adjustment.
- Custom standby text/clock and hardware audio interfaces.
- English firmware repository, source reconstruction and reproducible-build tooling.
- Matched-package preparation with our screen image and a hash-pinned, unmodified official controller image.
- Read-only matched-release discovery, independent component versions and compatibility status; official upstream notices retained.

## Next

- Complete target-hardware acceptance of the first public release candidate and promote a verified stable release.
- Exercise the documented update process against the next selected official firmware version.
- Refine documentation and compatibility reports from additional units of the same hardware.

## Proposed directions

- Verified OTA installation/recovery from matched releases, building on the read-only discovery path.
- Reproducible controller self-builds compared against official release binaries.
- Pinned build-tool packages for more host platforms.
- Better battery estimation and power-source reporting where hardware permits.
- Optional knob/audio interactions without changing the original touch workflow.

These are directions, not promised dates or already implemented features. Changes are reviewed against upstream behavior and published with explicit compatibility and validation scope.
