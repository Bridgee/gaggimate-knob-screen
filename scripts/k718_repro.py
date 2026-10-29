"""Pinned reproducible-build inputs and offline artifact comparison."""
import filecmp
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import time
import tarfile
import tempfile

ARTIFACTS = {'firmware.bin', 'firmware.elf', 'littlefs.bin', 'first-install.bin',
             'bootloader.bin', 'partitions.bin', 'boot_app0.bin'}


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def load_lock(root):
    path = Path(root) / 'port/reproducibility.json'
    lock = json.loads(path.read_text())
    if lock.get('schema') != 1:
        raise RuntimeError('Unsupported reproducibility lock')
    return lock, digest(path)


def checked_input(root, entry):
    root = Path(root).resolve()
    path = (root / entry['path']).resolve()
    if not path.is_relative_to(root) or not path.is_file() or digest(path) != entry['sha256']:
        raise RuntimeError('Reproducible tool input checksum/path mismatch: ' + entry['path'])
    return path


def prepare_tool(root, tools, platform_key=None):
    root, tools = Path(root).resolve(), Path(tools).resolve()
    lock, lock_hash = load_lock(root)
    source = checked_input(root, lock['source'])
    patch = checked_input(root, lock['patch'])
    key = platform_key or (platform.system().lower() + '-' + platform.machine().lower())
    if key not in lock['platforms']:
        raise RuntimeError('No reviewed reproducible filesystem tool for ' + key)
    entry = lock['platforms'][key]
    archive = checked_input(root, entry)
    if entry['member'] != 'mklittlefs':
        raise RuntimeError('Invalid filesystem tool archive member')
    identity = {'lock_sha256': lock_hash, 'platform': key, 'source_sha256': digest(source),
                'patch_sha256': digest(patch), 'binary_sha256': entry['binary_sha256']}
    cache = tools / 'repro-tools' / lock_hash / key
    binary = cache / 'mklittlefs'

    def verify_cache():
        if cache.is_symlink() or binary.is_symlink() or not binary.is_file():
            raise RuntimeError('Incomplete or unsafe reproducible tool cache')
        record = cache / 'identity.json'
        if not record.is_file() or json.loads(record.read_text()) != identity:
            raise RuntimeError('Reproducible tool cache identity mismatch')
        if binary.stat().st_size != entry['binary_bytes'] or digest(binary) != entry['binary_sha256']:
            raise RuntimeError('Reproducible tool cached binary checksum mismatch')
        if not os.access(binary, os.X_OK):
            raise RuntimeError('Reproducible filesystem tool is not executable')
        return {**identity, 'binary': str(binary), 'tool': lock['tool']}

    if cache.exists() or cache.is_symlink():
        return verify_cache()
    # Never extract arbitrary paths, symlinks or extra archive members.
    with tarfile.open(archive, 'r:gz') as tar:
        members = tar.getmembers()
        if len(members) != 1 or members[0].name != 'mklittlefs' or not members[0].isfile():
            raise RuntimeError('Unsafe reproducible tool archive layout')
        if members[0].size != entry['binary_bytes']:
            raise RuntimeError('Filesystem tool archive size mismatch')
        payload = tar.extractfile(members[0]).read()
    if hashlib.sha256(payload).hexdigest() != entry['binary_sha256']:
        raise RuntimeError('Filesystem tool archive binary checksum mismatch')
    cache.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.prepare-', dir=cache.parent) as temp:
        temp = Path(temp)
        (temp / 'mklittlefs').write_bytes(payload)
        (temp / 'mklittlefs').chmod(0o555)
        (temp / 'identity.json').write_text(json.dumps(identity, indent=2) + '\n')
        try:
            temp.rename(cache)
        except FileExistsError:
            pass  # Another preparation completed; it must pass the same checks.
    return verify_cache()


def compiler_settings(root, tools, core, epoch):
    epoch = str(epoch)
    if not epoch.isascii() or not epoch.isdecimal() or int(epoch) > 0xffffffff:
        raise RuntimeError('A uint32 SOURCE_DATE_EPOCH is required')
    maps = [(Path(root).resolve(), '/k718/src'), (Path(tools).resolve(), '/k718/tools'),
            (Path(core).resolve(), '/k718/tools/pio-core')]
    return {'epoch': epoch, 'flags': [f'-ffile-prefix-map={a}={b}' for a, b in maps],
            'define': ('K718_SOURCE_DATE_EPOCH', epoch)}


def verify_image_fingerprints(folder):
    folder = Path(folder)
    image = (folder / 'firmware.bin').read_bytes()
    if len(image) < 240 or image[0] != 0xe9 or image[32:36] != bytes.fromhex('3254cdab'):
        raise RuntimeError('Review changed ESP32-S3 image/app-description layout')
    if image[176:208].hex() != digest(folder / 'firmware.elf'):
        raise RuntimeError('Embedded ELF SHA256 does not match the real ELF')
    if image[-32:] != hashlib.sha256(image[:-32]).digest():
        raise RuntimeError('Application image SHA256 is invalid')
    return {'embedded_real_elf_sha256': True, 'image_sha256': True}


def compare_releases(first, second, verified_manifests):
    first, second = Path(first).resolve(), Path(second).resolve()
    a, b = verified_manifests
    for key in ('source_tree', 'source_date_epoch', 'port_version', 'upstream',
                'controller_protocol', 'dependency_lock_sha256', 'npm_lock_sha256'):
        if key not in a or key not in b or a[key] != b[key]:
            raise RuntimeError('Different reproducibility input: ' + key)
    for key in ('tools', 'python'):
        if a['environment'].get(key) != b['environment'].get(key):
            raise RuntimeError('Different reproducibility tool environment: ' + key)
    for key in ('lock_sha256', 'platform', 'binary_sha256', 'source_sha256', 'patch_sha256'):
        if not a.get('reproducibility', {}).get(key) or a['reproducibility'][key] != b.get('reproducibility', {}).get(key):
            raise RuntimeError('Different reproducibility tool identity: ' + key)
    if set(a['artifacts']) != ARTIFACTS or set(b['artifacts']) != ARTIFACTS:
        raise RuntimeError('Incomplete reproducibility artifact set')
    checks = {}
    for name in sorted(ARTIFACTS):
        equal = filecmp.cmp(first / name, second / name, shallow=False)
        checks[name] = {'sha256': digest(first / name), 'byte_identical': equal}
        if not equal:
            raise RuntimeError('Reproducibility mismatch: ' + name)
    images = {'first': verify_image_fingerprints(first), 'second': verify_image_fingerprints(second)}
    return {'schema': 1, 'status': 'software-reproducible', 'first': str(first), 'second': str(second),
            'source_commits': [a['source_commit'], b['source_commit']], 'source_tree': a['source_tree'],
            'source_date_epoch': a['source_date_epoch'], 'artifacts': checks, 'image_fingerprints': images,
            'hardware_verified': False, 'device_contacted': False,
            'scope': 'Same locked host tools/platform and build inputs; audit timestamps/paths are excluded.'}


def verify_filesystem(image, tool, epoch):
    image = Path(image)
    expected = time.asctime(time.gmtime(int(epoch)))
    result = subprocess.run([str(tool), '-s', str(image.stat().st_size), '-p', '256', '-b', '4096',
                             '-l', str(image)], check=True, capture_output=True, text=True,
                            env={**os.environ, 'TZ': 'UTC', 'LC_ALL': 'C'})
    records = 0
    creation = False
    for line in result.stdout.splitlines():
        fields = line.split('\t')
        if line.startswith('Creation time:'):
            creation = True
            if len(fields) != 2 or fields[1] != expected:
                raise RuntimeError('Filesystem creation time is not the fixed epoch')
        elif len(fields) == 3 and (fields[0] == '<dir>' or fields[0].isdigit()):
            records += 1
            if fields[2] != expected:
                raise RuntimeError('Filesystem entry time is not the fixed epoch: ' + fields[1])
    if not creation:
        raise RuntimeError('Filesystem time metadata is missing')
    return {'source_date_epoch': str(epoch), 'entry_time_records_verified': records,
            'creation_time_verified': True}


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-filesystem', required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    tools = Path(os.environ.get('K718_TOOLS', root.parent / '.tooling'))
    identity = prepare_tool(root, tools)
    print(json.dumps(verify_filesystem(args.verify_filesystem, identity['binary'],
                                      os.environ['SOURCE_DATE_EPOCH']), indent=2))
