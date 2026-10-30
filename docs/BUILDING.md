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
npm install --global npm@11.6.2
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
The updater accepts the reviewed two-slot partition layout and settled app0/app1 selections. It backs up the selected application, partition table, NVS and boot metadata, then updates only that application. It refuses pending trials and unknown metadata. Keep backups private.

Development recovery commands (hardware qualification must be recorded separately):

```sh
# Restore a compatible package to app0, including its reviewed boot selection.
python scripts/k718_flash.py update PREVIOUS_PACKAGE --write --validation --mode recover \
  --port SERIAL_PORT --expected-mac SCREEN_MAC --backup NEW_PRIVATE_BACKUP_DIRECTORY

# Stage a candidate in the inactive slot, retaining the current application.
python scripts/k718_flash.py update CANDIDATE_PACKAGE --write --validation --mode trial \
  --port SERIAL_PORT --expected-mac SCREEN_MAC --backup NEW_PRIVATE_BACKUP_DIRECTORY
```

Recovery explicitly changes boot selection; normal updates preserve it. Recovery and trials back up both application slots before writing. A trial requires the exact packaged bootloader on the device, writes only the inactive application, verifies it, then commits a `NEW` selector in the other metadata sector. Bootloader, partition table, settings, pairing and filesystems are not written. Recovery writes app0 and restores the package's settled app0 selector only after application readback; app1 remains available in its backup.

If application writing fails, boot selection is not committed. Interrupted operations retain their private receipt/backups; do not treat an incomplete receipt as success. Unknown layouts/states require review rather than an automatic erase.

K718 trial boots defer framework acceptance until local startup remains healthy for 20 seconds. Missing controller/Wi-Fi alone is not a failure. Local startup failure within the 120-second trial deadline requests bootloader rollback when a valid fallback exists; USB recovery remains necessary if no fallback can boot. These checks establish local software progress, not physical pixel quality or brewing qualification.

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
