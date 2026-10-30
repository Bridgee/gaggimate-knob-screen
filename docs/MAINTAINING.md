# Maintaining the port

## Source layout

Keep the upstream UI project and EEZ input files intact. `scripts/generate_knob_ui.py` derives native 360 × 360 geometry/resources into `src/display/ui/default/knob360/`.
Change the generator or resource inputs, regenerate, review and commit the outputs; do not hand-edit only generated files.
Existing UI checks compare all page handlers and flow bytes against the upstream input.

Hardware drivers, UI adaptation, optional extensions and build tooling are separate modules. Preserve upstream controller/PID/process behavior and original button actions.
OTA installation for both the display and controller is disabled in this port. Official release checks, channel selection and update notifications remain active. Display comparisons use the official base version in `port/config.json`, while the installed version retains its full port identity. Use USB with compatible firmware; the display requires a K718 build. A compatible OTA path remains future work.

## Upstream updates

`port/config.json` records the exact official commit and controller protocol. Export the complete port stack, then replay it against the reviewed new official tag:

```sh
python scripts/k718.py export --out dist/port-stack
python scripts/k718.py replay --stack dist/port-stack   --dest ../gaggimate-next --upstream NEW_OFFICIAL_TAG
```

Review `.port/replay.json` and any stopped Git conflicts. Resolve every required patch; do not skip a layer or change a protocol number to bypass compatibility checks.
Review driver interfaces, LVGL/EEZ changes, web assets, controller messages, settings/storage and partitions even if patches apply cleanly.

After review, in the replayed checkout:

```sh
python scripts/k718.py bind-upstream   --version NEW_OFFICIAL_VERSION --controller-protocol REVIEWED_PROTOCOL
```

Update the port version, adapt code/locks/resources, commit, then export/replay and build a new candidate. Update housekeeping filters if upstream adds necessary firmware paths.
An upstream update can require focused manual adaptation; the replay process preserves a traceable starting point.

## Matched firmware pairs

The first delivery stage pairs our K718 application with the **unmodified official controller release**, rather than rebuilding the controller for distribution. `port/controller-release.json` pins the official repository/tag/commit, release/asset IDs, size, SHA-256 and controller protocol. The digest was checked against GitHub's release asset metadata and the downloaded original file.

After preparing the normal display package:

```sh
python scripts/package_pair.py dist/publish --out dist/paired
```

The tool downloads only the pinned controller asset. For offline use, add `--controller PATH_TO_ORIGINAL_BOARD_FIRMWARE`; the same hash checks apply. It rejects altered/truncated images, incompatible upstream commits/protocols and existing output directories.

The output contains `display-knob-firmware.bin`, `board-firmware.bin`, `pair.json`, `SHA256SUMS` and notices. Both application images remain byte-for-byte unchanged. Keep the separate display USB bundle for installation; the pair contains application images, not first-install images.

`pair.json` records independent screen/controller versions, the display source commit/tree and partition bounds, official controller provenance, and package validation scope. Preparing a pair does not certify an on-device OTA transfer or enable installation. Candidate builds check our published GitHub releases and read `pair.json` in a separate background worker. They compare screen and controller versions independently and show target/protocol/controller-version compatibility. Candidate builds include explicitly labelled pre-releases; stable builds exclude both GitHub-marked and version-tagged prereleases. Drafts and legacy releases without a complete pair are ignored.

Metadata reads use HTTPS with bounded response sizes, and the manifest digest and both asset records must match the GitHub release metadata. Failed or incompatible checks cannot enable installation. A matching manifest is not hardware or OTA-transfer qualification. The web page offers a read-only refresh; official Stable/Nightly checks and upstream notices remain separate. On K718, official checks also run on an isolated low-priority worker. HTTPS never runs on the controller logic loop; stale channel results are discarded, and network failures do not suspend telemetry. Development adds a read-only screen OTA preflight and separate disabled screen/controller permissions. It checks immutable own-release URLs/digests, version/channel, controller compatibility, busy state and both application-slot bounds. Metadata readiness is not installation permission. Development also adds explicit USB recovery/trials and health-gated boot confirmation; consult [Building and flashing](BUILDING.md). Their hardware qualification is recorded separately from host tests. Firmware-image downloading and installation remain disabled until the recovery prerequisites pass.

Before enabling controller-only OTA, qualify the pinned official controller image through the actual BLE path: complete length/hash verification before transfer, target/version/protocol guards, reboot/reconnection and retained settings, interrupted-transfer recovery and a proven USB fallback. Screen OTA success cannot substitute for these checks. Combined updates remain a later stage.

For a new official version, review and update the controller pin together with the selected upstream base/protocol, verify the downloaded asset against its official digest, and test the intended hardware combination. Do not automatically adopt the newest official asset. Never replace a published pair in place.

Controller self-builds remain a separate reproducibility investigation. Use a clean official checkout with its original tag and recorded tools/dependencies; compare the actual application SHA-256 against the official release. A differing hash is not proof of equivalent behavior. Do not silently substitute a self-built controller image for the pinned official binary.

## Reproducible builds

Tool/dependency versions and the filesystem tool are locked under `port/`. Build timestamps use the committed author epoch, compiler paths are normalized, and filesystem metadata is fixed.
Do not replace the pinned tool silently or alter embedded ELF fingerprints.

Build the same committed stack in two clean directories, then compare:

```sh
python scripts/k718.py verify-repro FIRST_PACKAGE SECOND_PACKAGE   --out dist/reproducibility.json
```

The seven firmware artifacts must match byte for byte for identical supported build environments and inputs. Logs may differ in paths and audit times. Same-host reproducibility has been demonstrated; byte equality between local and CI hosts, across operating systems, or with historical official controller binaries is not established.

## Stable-release acceptance

The [roadmap](ROADMAP.md) separates release requirements from ongoing maintenance and optional work. OTA installation, controller self-builds, extra host platforms and battery calibration are not prerequisites for a USB-only stable release.

1. Identify the exact candidate source, CI run and application hash. Back up the target and install with the checked USB updater; record readback verification and controller identity/protocol.
2. Validate changed behavior on the device and its real web UI. For matched discovery, check the network refresh, component versions/compatibility, retained upstream notices and disabled installers. A metadata match is not OTA qualification.
3. Check restart/reconnection, settings retention and the existing interaction paths. Reuse prior peripheral evidence when unchanged, and label it as prior evidence rather than acceptance of the exact new binary. Record any untested real-machine use explicitly.
4. Generate the stable version/channel, run software/CI checks and test the resulting final package. Do not rename an RC binary or transfer its acceptance automatically to a different hash. Stable discovery excludes prereleases, so validate its channel behavior too.
5. Publish immutable versioned assets with checksums, compatibility and concise acceptance/limitation notes. Retain the previous compatible application and documented USB rollback path privately; do not imply that an unperformed rollback test passed.

Keep private maintenance status as a current summary with evidence links. Archive superseded snapshots instead of retaining contradictory active pending lists. Completed pin discovery, LED persistence and previously accepted interaction features are not new tasks unless a change requires regression testing.

## Releases

Stage only the intended firmware files before running `scripts/check_public.py`; the publication check inspects the Git index. Run the publication check, source replay, software checks and build before publishing. Test changed behavior on hardware as needed; do not reuse an old binary's acceptance for a new one.
Use annotated version tags and GitHub Releases for selected firmware images, checksums and concise release notes. Keep device dumps, NVS, pairing data, raw logs and internal records outside this repository.
The working tree is the publication boundary; the official base commit is retained for provenance, without importing private development history.

Prepare uploadable artifacts from a verified local build:

```sh
python scripts/package_release.py dist/release --out dist/publish
```

Upload only the resulting bundle. It contains firmware files, minimal source/compatibility metadata and checksums; local audit logs, device identities and tool/cache paths are omitted. Source archives are supplied by the GitHub tag.

Artifact inspection recognizes build paths only from hash-verified vendor libraries and distinguishes TLS parser delimiters from key material. It never rewrites firmware bytes.

Nanopb first-use generation is prepared from a pinned descriptor input. Both pristine and prepared package identities are verified; source-file mismatches are not ignored.
