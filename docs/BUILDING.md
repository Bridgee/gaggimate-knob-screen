# Building and flashing

## Requirements

- Apple Silicon, macOS 26 or later for the supplied reproducible filesystem tool.
- Python 3.12, Node 24.22.1, a C/C++ compiler and Git.
- A complete Git checkout; builds use its source identity and pinned upstream commit.

```sh
python3.12 -m venv .venv
source .venv/bin/activate
python -m pip install -r port/requirements.lock.txt
nvm install
nvm use
python scripts/k718.py doctor
python scripts/k718.py build --out dist/release
```

Use a new `--out` directory for each build. The build installs/verifies pinned dependencies, checks the native UI, builds and validates the compressed web interface, then produces the application and filesystem images.
`release.json` records source identity, compatibility and artifact hashes. Local tools default to the ignored `.tools/` directory; `K718_TOOLS` can select another cache.

## Flashing

Use a USB data cable. Disable HID in the factory firmware, then enter the manufacturer's download mode if a serial port is unavailable.

Inspect a package without touching hardware:

```sh
python scripts/k718_flash.py plan dist/release
```

For an existing compatible GaggiMate installation, the checked updater backs up the current state and writes only the application:

```sh
python scripts/k718_flash.py update dist/release   --write --validation   --port SERIAL_PORT --expected-mac SCREEN_MAC   --backup LOCAL_BACKUP_DIRECTORY
```

Replace the placeholders with the actual screen identity and a new local backup directory. Never select a controller port by accident.
The updater rejects incompatible partitions/boot selection and verifies the application after writing. Keep backups private.

`first-install.bin` is a merged image written at address `0x0`; it includes the filesystem and partition table. It is for a first installation, not a normal update. Back up factory firmware before using it.

## Rollback

Keep the previous application package and a local device backup. If partition and stored-data formats remain compatible, use the same checked updater with the previous package.
Do not substitute a factory dump or another device's settings for an application rollback.

## Checks

```sh
python scripts/check_public.py
python scripts/k718.py check
```

The second command needs the pinned LVGL dependencies installed by the build, or by `python scripts/k718.py deps`.
Software checks do not automatically qualify a binary as hardware-tested.
