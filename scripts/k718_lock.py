"""Content identities for the installed, explicitly pinned dependency set."""
import hashlib
import json
import subprocess
from pathlib import Path

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def tree_digest(folder, git_files=False):
    folder = Path(folder)
    if git_files:
        names = subprocess.check_output(['git', '-C', str(folder), 'ls-files', '-z']).decode().split('\0')
        paths = [folder / n for n in names if n]
    else:
        paths = [p for p in folder.rglob('*') if p.is_file() and not any(
            n in {'.git', '.piopm', '__pycache__', '.DS_Store'} for n in p.relative_to(folder).parts)]
    result = hashlib.sha256()
    for p in sorted(paths):
        result.update(p.relative_to(folder).as_posix().encode() + b'\0')
        result.update(p.read_bytes())
        result.update(b'\0')
    return result.hexdigest()

def library_identity(folder):
    folder = Path(folder)
    metadata = folder / '.piopm'
    if not metadata.exists(): metadata = folder / '.git/.piopm'
    d = json.loads(metadata.read_text())
    uri = d['spec'].get('uri') or ''
    is_git = uri.startswith('git+')
    identity = {'name': d['name'], 'version': d['version'], 'kind': 'git' if is_git else 'registry'}
    if is_git:
        identity['commit'] = subprocess.check_output(['git', '-C', str(folder), 'rev-parse', 'HEAD'], text=True).strip()
        identity['url'] = uri[4:].split('#')[0]
        # PlatformIO may generate an untracked, timestamped library.json.
        # Git's actual tracked files and commit are the source identity.
        identity.pop('version')
    else: identity['owner'] = d['spec']['owner']
    identity['source_sha256'] = tree_digest(folder, is_git)
    return identity

def verify_libraries(root, lock, require_local=True):
    folder = Path(root) / '.pio/libdeps/display-knob'
    expected_names=set(lock['libraries']) | {'NayrodPID'}
    actual_names={p.name for p in folder.iterdir() if p.is_dir() and not p.name.startswith('.')}
    if actual_names - expected_names:
        raise RuntimeError('Unrecorded libraries resolved: '+', '.join(sorted(actual_names-expected_names)))
    # PlatformIO caches file:// packages without content identity. Use a
    # project-local symlink, resolved back to this committed checkout.
    local=folder/'NayrodPID'
    if local.exists():
        raise RuntimeError('Stale copied NayrodPID package; use a fresh checkout')
    link=folder/'NayrodPID.pio-link'
    if link.exists():
        data=json.loads(link.read_text())
        uri=data['spec']['uri']
        if not uri.startswith('symlink://'):
            raise RuntimeError('Unexpected local dependency link')
        path=Path(uri[10:])
        if not path.is_absolute():path=Path(data['cwd'])/path
        if path.resolve()!=(Path(root)/'lib/NayrodPID').resolve():
            raise RuntimeError('Local dependency points outside this checkout')
    elif require_local:
        # With a manifest, PlatformIO can satisfy symlink:// directly from
        # PROJECT_LIB_DIR, without creating a redundant .pio-link file.
        manifest=Path(root)/'lib/NayrodPID/library.json'
        if not manifest.is_file() or json.loads(manifest.read_text()).get('name')!='NayrodPID':
            raise RuntimeError('Missing committed-source NayrodPID manifest/link')
    for name, expected in lock['libraries'].items():
        actual = library_identity(folder / name)
        if actual != expected:
            raise RuntimeError('Dependency content/version mismatch: ' + name)

def hydrate(root, cache, lock):
    import shutil
    target = Path(root) / '.pio/libdeps/display-knob'
    if target.exists():
        raise RuntimeError('Dependency seed destination already exists; use a fresh checkout')
    # Verify the entire source before copying anything into the candidate.
    for name, expected in lock['libraries'].items():
        if library_identity(Path(cache) / name) != expected:
            raise RuntimeError('Unverified dependency cache: ' + name)
    target.mkdir(parents=True)
    for name, expected in lock['libraries'].items():
        shutil.copytree(Path(cache) / name, target / name, symlinks=True)
        if expected['kind'] == 'git':
            meta = target / name / '.git/.piopm'
            if not meta.exists(): meta = target / name / '.piopm'
            data = json.loads(meta.read_text())
            data['spec']['uri'] = 'git+' + expected['url'] + '#' + expected['commit']
            meta.write_text(json.dumps(data))
    verify_libraries(root, lock, require_local=False)
