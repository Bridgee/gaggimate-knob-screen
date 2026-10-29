"""Copy verified firmware artifacts and minimal metadata into a shareable bundle."""
import argparse
import json
import re
from pathlib import Path
import shutil

from check_public import content_issues
from k718_flash import verified_release
from k718_repro import ARTIFACTS, digest
from k718 import tool_env

ROOT = Path(__file__).resolve().parents[1]
VENDOR_PATH = re.compile(rb'/(?:Users)/[^\x00\r\n]{1,1024}')

def vendor_build_paths(toolchain, lock):
    toolchain = Path(toolchain).resolve()
    known = set()
    for entry in lock["libraries"]:
        library = (toolchain / entry["path"]).resolve()
        if not library.is_relative_to(toolchain) or not library.is_file() or digest(library) != entry["sha256"]:
            raise RuntimeError("Vendor library provenance mismatch: " + entry["path"])
        known.update(VENDOR_PATH.findall(library.read_bytes()))
    return known

def artifact_issues(name, data, known_paths):
    # Inspect a filtered view only. The published firmware remains byte-identical.
    view = VENDOR_PATH.sub(lambda match: b"<verified-vendor-build-path>" if match[0] in known_paths else match[0], data)
    # NUL-terminated PEM delimiters are TLS parser constants, not key material.
    view = re.sub(rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----\x00", b"<PEM-parser-delimiter>\x00", view)
    return content_issues(name, view)


def package(source, output):
    source, manifest, _ = verified_release(source)
    output = Path(output).resolve()
    if output.exists():
        raise RuntimeError('Choose a new output directory')
    if not ARTIFACTS.issubset(manifest['artifacts']):
        raise RuntimeError('A complete firmware package is required')
    metadata = {name: manifest[name] for name in (
        'upstream', 'port_version', 'release_channel', 'source_commit', 'source_tree',
        'target', 'chip', 'controller_protocol', 'application_offset', 'application_size')}
    metadata.update(schema=1, qualification=manifest.get('qualification', 'software-verified'),
                    hardware_verified=bool(manifest.get('hardware_verified', False)),
                    artifacts={name: manifest['artifacts'][name] for name in sorted(ARTIFACTS)})
    payload = (json.dumps(metadata, indent=2) + '\n').encode()
    problems = content_issues('release.json', payload)
    vendor_lock = json.loads((ROOT / 'port/vendor-artifact-metadata.json').read_text())
    toolchain = Path(tool_env()['PLATFORMIO_CORE_DIR']) / 'packages' / vendor_lock['package']
    known_paths = vendor_build_paths(toolchain, vendor_lock)
    for name in ARTIFACTS:
        problems.extend(artifact_issues(name, (source / name).read_bytes(), known_paths))
    if problems:
        raise RuntimeError('\n'.join(problems))
    output.mkdir(parents=True)
    for name in ARTIFACTS:
        shutil.copyfile(source / name, output / name)
    (output / 'release.json').write_bytes(payload)
    (output / 'SHA256SUMS').write_text(''.join(
        f'{digest(path)}  {path.name}\n' for path in sorted(output.iterdir()) if path.is_file()))
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    print(package(args.source, args.out))
