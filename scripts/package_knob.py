"""Package first-install K718 images after sequential PlatformIO builds."""
import hashlib
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
core = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
esptool = core / "packages/tool-esptoolpy/esptool.py"
boot_app = core / "packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
parser = argparse.ArgumentParser()
parser.add_argument("--target", action="append", choices=("display-knob",))
parser.add_argument("--build-root", type=Path, default=root / ".pio/build")
args = parser.parse_args()
targets = args.target or ("display-knob",)
output = root / "dist/k718"
output.mkdir(parents=True, exist_ok=True)
config = json.loads((root / "port/config.json").read_text())
manifest = {"chip": "ESP32-S3", "panel": "GUITION JC3636K718 360x360", "hardware_tested": False,
            "upstream": config["upstream"], "port_version": config["port_version"],
            "controller_protocol": config["controller_protocol"], "images": {}}

for target in targets:
    build = args.build_root / target
    parts = [(0, build / "bootloader.bin"), (0x8000, build / "partitions.bin"),
             (0xe000, boot_app), (0x10000, build / "firmware.bin")]
    if target == "display-knob":
        parts.append((0xc90000, build / "littlefs.bin"))
    target_dir = output / target
    target_dir.mkdir(exist_ok=True)
    # ESP32-S3 ROM must load the second-stage bootloader in DIO. The
    # software bootloader then enables QIO for the application as configured.
    # Forcing QIO into this header makes ROM fail before the driver can run.
    command = [sys.executable, str(esptool), "--chip", "esp32s3", "merge_bin", "--flash_mode", "dio",
               "--flash_freq", "80m", "--flash_size", "16MB", "-o", str(target_dir / "first-install.bin")]
    manifest["images"][target] = {"flash_offset": 0, "components": []}
    for offset, path in parts:
        if not path.is_file():
            raise SystemExit(f"Missing {path}; build {target} first")
        command += [hex(offset), str(path)]
        shutil.copyfile(path, target_dir / path.name)
        manifest["images"][target]["components"].append({"offset": offset, "file": path.name})
    subprocess.run(command, cwd=root, check=True)
    if (build / "firmware.elf").is_file():
        shutil.copyfile(build / "firmware.elf", target_dir / "firmware.elf")
    header = (target_dir / "first-install.bin").read_bytes()[:4]
    if len(header) != 4 or header[0] != 0xe9 or header[2] != 2:
        raise SystemExit(f"Invalid ROM boot header for {target}; expected ESP image in DIO mode")
    manifest["images"][target]["rom_boot_mode"] = "DIO"

for path in output.rglob("*.bin"):
    manifest.setdefault("sha256", {})[str(path.relative_to(output))] = hashlib.sha256(path.read_bytes()).hexdigest()
(output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print("Packaged images:", output)
