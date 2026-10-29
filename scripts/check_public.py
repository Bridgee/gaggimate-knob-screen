"""Check the staged firmware publication boundary; never uploads files."""
import argparse
import io
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[1]
FORBIDDEN_PARTS = {'backups', 'baselines', 'hardware-tests', 'references', 'replays',
                   'releases', '.tools', '.tooling', '.pio', '.venv', 'node_modules',
                   '__pycache__', 'dist', 'context.json', 'AGENTS.md', '.DS_Store'}
LOCAL_PATH = re.compile(r'/(?:Users|private/(?:tmp|var))/|' + re.escape('.' + 'codex') + r'(?:/|\b)')
SECRET = re.compile(r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|AKIA[A-Z0-9]{16})')
MAC = re.compile(r'\b(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}\b')
CJK = re.compile(r'[\u4e00-\u9fff]')
EXAMPLE_MACS = {'00:00:00:00:00:00', 'ff:ff:ff:ff:ff:ff', 'aa:bb:cc:dd:ee:ff'}


def content_issues(name, data):
    issues = []
    ascii_text = data.decode('latin1')
    for label, pattern in [('local absolute path', LOCAL_PATH), ('credential/private key', SECRET)]:
        if pattern.search(ascii_text): issues.append(f'{name}: {label}')
    if any(m.lower() not in EXAMPLE_MACS for m in MAC.findall(ascii_text)):
        issues.append(f'{name}: literal device MAC address')
    try: text = data.decode('utf-8')
    except UnicodeDecodeError: return issues
    if CJK.search(text): issues.append(f'{name}: non-English project text')
    return issues


def inspect(entries, policy):
    """Inspect an exact Git index snapshot, including approved source archives."""
    issues = []
    for name, (mode, data) in entries.items():
        path = PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts or any(p in FORBIDDEN_PARTS for p in path.parts):
            issues.append(f'{name}: private/generated path'); continue
        allowed = (len(path.parts) > 1 and path.parts[0] in policy['allowed_roots']) or name in policy['allowed_root_files']
        if not allowed: issues.append(f'{name}: outside firmware publication roots')
        if mode not in {'100644', '100755'}: issues.append(f'{name}: symlink/submodule or unsupported file mode')
        if CJK.search(name): issues.append(f'{name}: non-English filename')
        if name.endswith('.eez-project-ui-state'): issues.append(f'{name}: local editor state')
        if path.suffix in {'.bin', '.elf', '.log', '.dump', '.pcap', '.sqlite', '.zip'}:
            issues.append(f'{name}: generated binary or device artifact')
        issues.extend(content_issues(name, data))
        if name.endswith(('.tar.gz', '.tgz')):
            if name not in policy.get('archives', []):
                issues.append(f'{name}: archive is not an approved build input'); continue
            try:
                with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
                    for member in archive:
                        member_path = PurePosixPath(member.name)
                        if member_path.is_absolute() or '..' in member_path.parts or member.issym() or member.islnk():
                            issues.append(f'{name}: unsafe archive member'); continue
                        if member.uname or member.gname:
                            issues.append(f'{name}: named archive owner metadata')
                        if member.isfile():
                            issues.extend(content_issues(name + ':' + member.name, archive.extractfile(member).read()))
            except (tarfile.TarError, OSError): issues.append(f'{name}: invalid build-input archive')
    return issues


def staged_entries(root):
    records = subprocess.check_output(['git', 'ls-files', '--stage', '-z'], cwd=root).split(b'\0')
    entries = {}
    for record in filter(None, records):
        metadata, name = record.split(b'\t', 1)
        mode, oid, stage = metadata.decode().split()
        if stage != '0': raise RuntimeError('Unresolved Git index entry')
        content = subprocess.check_output(['git', 'cat-file', 'blob', oid], cwd=root)
        entries[name.decode()] = (mode, content)
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    args = parser.parse_args()
    entries = staged_entries(args.root)
    policy = json.loads(entries['port/publication.json'][1])
    issues = inspect(entries, policy)
    if issues:
        print('\n'.join(issues), file=sys.stderr)
        return 1
    print(f'PASS: {len(entries)} tracked firmware files; English text, isolated paths and build-input archives checked.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
