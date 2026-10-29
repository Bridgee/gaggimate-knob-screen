# Maintaining the port

## Source layout

Keep the upstream UI project and EEZ input files intact. `scripts/generate_knob_ui.py` derives native 360 × 360 geometry/resources into `src/display/ui/default/knob360/`.
Change the generator or resource inputs, regenerate, review and commit the outputs; do not hand-edit only generated files.
Existing UI checks compare all page handlers and flow bytes against the upstream input.

Hardware drivers, UI adaptation, optional extensions and build tooling are separate modules. Preserve upstream controller/PID/process behavior and original button actions.
The display OTA restriction is deliberate: stock LilyGO images cannot drive this panel. Use a K718 build over USB.

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

## Reproducible builds

Tool/dependency versions and the filesystem tool are locked under `port/`. Build timestamps use the committed author epoch, compiler paths are normalized, and filesystem metadata is fixed.
Do not replace the pinned tool silently or alter embedded ELF fingerprints.

Build the same committed stack in two clean directories, then compare:

```sh
python scripts/k718.py verify-repro FIRST_PACKAGE SECOND_PACKAGE   --out dist/reproducibility.json
```

The seven firmware artifacts must match byte for byte for the same supported platform and inputs. Logs may differ in paths and audit times.

## Releases

Run the publication check, source replay, software checks and build before publishing. Test changed behavior on hardware as needed; do not reuse an old binary's acceptance for a new one.
Use annotated version tags and GitHub Releases for selected firmware images, checksums and concise release notes. Keep device dumps, NVS, pairing data, raw logs and internal records outside this repository.
The working tree is the publication boundary; the official base commit is retained for provenance, without importing private development history.

Prepare uploadable artifacts from a verified local build:

```sh
python scripts/package_release.py dist/release --out dist/publish
```

Upload only the resulting bundle. It contains firmware files, minimal source/compatibility metadata and checksums; local audit logs, device identities and tool/cache paths are omitted. Source archives are supplied by the GitHub tag.

Artifact inspection recognizes build paths only from hash-verified vendor libraries and distinguishes TLS parser delimiters from key material. It never rewrites firmware bytes.

Nanopb first-use generation is prepared from a pinned descriptor input. Both pristine and prepared package identities are verified; source-file mismatches are not ignored.
