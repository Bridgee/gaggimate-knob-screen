#!/usr/bin/env python3
"""Compress and verify the exact blob/manifest consumed by the HTTP server."""
import argparse
import gzip
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import re
from embed_webui import content_type_for


def gzip_assets(directory):
    for path in sorted(Path(directory).rglob('*')):
        if not path.is_file() or path.suffix not in {'.html', '.js', '.css'}:
            continue
        target = path.with_name(path.name + '.gz')
        if target.exists() or path.read_bytes().startswith(b'\x1f\x8b'):
            raise RuntimeError('Already compressed or conflicting web asset: ' + str(path))
        # Match upstream build_webui.sh: suffix drives the embedded gzip flag.
        target.write_bytes(gzip.compress(path.read_bytes(), mtime=0))
        path.unlink()


class IndexReferences(HTMLParser):
    def __init__(self):
        super().__init__()
        self.references = set()
        self.module_scripts = 0

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == 'script' and attrs.get('type') == 'module' and attrs.get('src'):
            self.module_scripts += 1
        for key in ('src', 'href'):
            value = attrs.get(key, '')
            if value.startswith('/assets/'):
                self.references.add(value.split('?', 1)[0])


def validate_web_assets(directory):
    directory = Path(directory)
    header = (directory / 'web_ui_manifest.h').read_text()
    blob = (directory / 'web_ui.bin').read_bytes()
    entries = re.findall(r'^\s*\{"([^"]+)", (\d+)u, (\d+)u, "([^"]+)", (true|false)\},$', header, re.M)
    count = re.search(r'WEB_ASSETS_COUNT = (\d+)u;', header)
    if not count or not entries or len(entries) != int(count[1]):
        raise RuntimeError('Missing/incomplete embedded web manifest')
    decoded = {}
    compressed_count = 0
    end = 0
    for path, offset, length, mime, compressed in entries:
        offset, length = int(offset), int(length)
        if path in decoded or offset != end or length == 0 or offset + length > len(blob):
            raise RuntimeError('Invalid web asset path/range: ' + path)
        end = offset + length
        payload = blob[offset:end]
        encoded = compressed == 'true'
        if encoded != payload.startswith(b'\x1f\x8b'):
            raise RuntimeError('Gzip flag/payload mismatch: ' + path)
        if encoded:
            compressed_count += 1
            try:
                payload = gzip.decompress(payload)
            except (OSError, EOFError) as error:
                raise RuntimeError('Corrupt gzip web asset: ' + path) from error
            if payload.startswith(b'\x1f\x8b'):
                raise RuntimeError('Double gzip web asset: ' + path)
        if mime != content_type_for(path):
            raise RuntimeError('Wrong web MIME type: ' + path)
        if Path(path).suffix in {'.html', '.js', '.mjs', '.css'}:
            try:
                payload.decode('utf-8')
            except UnicodeError as error:
                raise RuntimeError('Invalid UTF-8 web asset: ' + path) from error
        decoded[path] = payload
    if end != len(blob):
        raise RuntimeError('Unreferenced embedded web bytes')
    index = decoded.get('/index.html', b'').decode('utf-8')
    if not index.lstrip().lower().startswith('<!doctype html>'):
        raise RuntimeError('Missing real HTML index')
    parser = IndexReferences()
    parser.feed(index)
    if not parser.module_scripts or not parser.references:
        raise RuntimeError('Missing web application entry')
    missing = parser.references - decoded.keys()
    if missing:
        raise RuntimeError('Missing index web references: ' + ', '.join(sorted(missing)))
    return {'assets': len(entries), 'gzip_assets': compressed_count,
            'blob_sha256': hashlib.sha256(blob).hexdigest(),
            'index_references': sorted(parser.references), 'passed': True}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validate', required=True)
    args = parser.parse_args()
    print(json.dumps(validate_web_assets(args.validate), indent=2))
