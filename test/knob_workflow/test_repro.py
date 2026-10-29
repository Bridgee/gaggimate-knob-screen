import copy
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
from k718_repro import (prepare_tool, compiler_settings, compare_releases,
                        verify_image_fingerprints, verify_filesystem, ARTIFACTS, digest)

REPO = Path(__file__).resolve().parents[2]


class ManagedToolTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'source'; self.root.mkdir()
        self.tools = Path(self.temp.name) / 'tools'
        (self.root / 'port').mkdir()
        self.payload = b'#!/bin/sh\nexit 0\n'
        self.lock = {'schema': 1, 'tool': 'test',
                     'source': self.input('source.tar.gz', b'source'),
                     'patch': self.input('changes.patch', b'patch'),
                     'platforms': {'test-platform': self.archive()}}
        self.save()
    def input(self, name, data):
        (self.root / name).write_bytes(data)
        return {'path': name, 'sha256': hashlib.sha256(data).hexdigest()}
    def archive(self, member='mklittlefs', extra=False):
        data = io.BytesIO()
        with tarfile.open(fileobj=data, mode='w:gz') as tar:
            info = tarfile.TarInfo(member); info.size = len(self.payload)
            tar.addfile(info, io.BytesIO(self.payload))
            if extra:
                info = tarfile.TarInfo('../escape'); info.size = 0
                tar.addfile(info, io.BytesIO())
        return {**self.input('tool.tar.gz', data.getvalue()), 'member': 'mklittlefs',
                'binary_bytes': len(self.payload), 'binary_sha256': hashlib.sha256(self.payload).hexdigest()}
    def save(self):
        (self.root / 'port/reproducibility.json').write_text(json.dumps(self.lock))
    def prepare(self):
        return prepare_tool(self.root, self.tools, 'test-platform')
    def test_fresh_cache_and_reuse_have_same_verified_identity(self):
        first = self.prepare(); second = self.prepare()
        self.assertEqual(first, second)
        self.assertEqual(Path(first['binary']).read_bytes(), self.payload)
    def test_tampered_cached_binary_is_rejected_not_replaced(self):
        binary = Path(self.prepare()['binary']); binary.chmod(0o755); binary.write_bytes(b'changed')
        with self.assertRaisesRegex(RuntimeError, 'cached binary checksum'): self.prepare()
        self.assertEqual(binary.read_bytes(), b'changed')
    def test_tampered_source_archive_is_rejected(self):
        (self.root / 'source.tar.gz').write_bytes(b'changed')
        with self.assertRaisesRegex(RuntimeError, 'checksum/path'): self.prepare()
    def test_path_escape_is_rejected(self):
        self.lock['source']['path'] = '../outside'; self.save()
        with self.assertRaisesRegex(RuntimeError, 'checksum/path'): self.prepare()
    def test_extra_archive_paths_are_never_extracted(self):
        self.lock['platforms']['test-platform'] = self.archive(extra=True); self.save()
        with self.assertRaisesRegex(RuntimeError, 'Unsafe.*archive'): self.prepare()
        self.assertFalse((self.tools / 'escape').exists())
    def test_unknown_platform_has_no_silent_tool_fallback(self):
        with self.assertRaisesRegex(RuntimeError, 'No reviewed'): prepare_tool(self.root, self.tools, 'unknown')
    def test_wrong_binary_digest_is_rejected_before_cache_write(self):
        self.lock['platforms']['test-platform']['binary_sha256'] = '0' * 64; self.save()
        with self.assertRaisesRegex(RuntimeError, 'archive binary checksum'): self.prepare()
    def test_corrupt_cache_identity_is_not_accepted(self):
        identity = Path(self.prepare()['binary']).parent / 'identity.json'
        identity.write_text('{}')
        with self.assertRaisesRegex(RuntimeError, 'identity mismatch'): self.prepare()
    def test_epoch_validation_and_path_normalization(self):
        for epoch in ['', '-1', '1.2', str(2**32), '\u0661']:
            with self.assertRaises(RuntimeError): compiler_settings(self.root, self.tools, self.tools / 'pio', epoch)
        settings = compiler_settings(self.root, self.tools, self.tools / 'pio', '1793249420')
        self.assertEqual(settings['define'], ('K718_SOURCE_DATE_EPOCH', '1793249420'))
        self.assertIn('/k718/src', settings['flags'][0])


class ArtifactComparisonTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.first, self.second = [Path(self.temp.name) / n for n in ('a', 'b')]
        for folder in (self.first, self.second):
            folder.mkdir(); (folder / 'firmware.elf').write_bytes(b'test full ELF')
            image = bytearray(240); image[0] = 0xe9; image[32:36] = bytes.fromhex('3254cdab')
            image[176:208] = bytes.fromhex(digest(folder / 'firmware.elf'))
            image[-32:] = hashlib.sha256(image[:-32]).digest(); (folder / 'firmware.bin').write_bytes(image)
            for name in ARTIFACTS - {'firmware.bin', 'firmware.elf'}: (folder / name).write_bytes(name.encode())
        self.manifest = {'source_commit': 'source', 'source_tree': 'tree', 'source_date_epoch': '123',
                         'port_version': '0.3.4', 'upstream': {'commit': 'official'}, 'controller_protocol': 5,
                         'dependency_lock_sha256': 'dependencies', 'npm_lock_sha256': 'npm',
                         'environment': {'tools': {'compiler': 'locked'}, 'python': '3.12'},
                         'reproducibility': {k: k for k in ['lock_sha256', 'platform', 'binary_sha256', 'source_sha256', 'patch_sha256']},
                         'artifacts': {k: {} for k in ARTIFACTS}}
    def compare(self, other=None):
        return compare_releases(self.first, self.second, (self.manifest, other or self.manifest))
    def test_all_bytes_and_real_fingerprints_match(self):
        self.assertEqual(self.compare()['status'], 'software-reproducible')
    def test_changed_artifact_fails_even_if_old_manifest_claims_match(self):
        (self.second / 'littlefs.bin').write_bytes(b'changed')
        with self.assertRaisesRegex(RuntimeError, 'Reproducibility mismatch'): self.compare()
    def test_different_epoch_is_not_the_same_build_input(self):
        other = copy.deepcopy(self.manifest); other['source_date_epoch'] = '124'
        with self.assertRaisesRegex(RuntimeError, 'Different reproducibility input'): self.compare(other)
    def test_wrong_embedded_elf_fingerprint_is_rejected(self):
        (self.first / 'firmware.elf').write_bytes(b'another ELF')
        with self.assertRaisesRegex(RuntimeError, 'Embedded ELF'): verify_image_fingerprints(self.first)
    def test_invalid_image_digest_is_rejected(self):
        image = bytearray((self.first / 'firmware.bin').read_bytes()); image[-1] ^= 1
        (self.first / 'firmware.bin').write_bytes(image)
        with self.assertRaisesRegex(RuntimeError, 'image SHA256'): verify_image_fingerprints(self.first)


class RealFilesystemTest(unittest.TestCase):
    def test_actual_locked_tool_ignores_mtime_and_creation_order(self):
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp); tool = prepare_tool(REPO, temp / 'tools')['binary']
            images = []
            for number, names, mtime in [(1, ['z', 'a', 'b'], 1700000000), (2, ['b', 'a', 'z'], 1900000000)]:
                data = temp / str(number); data.mkdir(); (data / 'p').mkdir()
                for name in names:
                    path = data / 'p' / name; path.write_text(name + ' stable'); os.utime(path, (mtime, mtime))
                os.utime(data / 'p', (mtime, mtime)); output = temp / (str(number) + '.bin')
                env = {**os.environ, 'SOURCE_DATE_EPOCH': '1793249420'}
                subprocess.run([tool, '-c', str(data), '-s', '131072', '-p', '256', '-b', '4096', str(output)],
                               env=env, check=True, capture_output=True)
                images.append(output.read_bytes())
                self.assertTrue(verify_filesystem(output, tool, '1793249420')['creation_time_verified'])
                with self.assertRaisesRegex(RuntimeError, 'fixed epoch'):
                    verify_filesystem(output, tool, '1700000001')
            self.assertEqual(images[0], images[1])
            env = dict(os.environ); env.pop('SOURCE_DATE_EPOCH', None)
            result = subprocess.run([tool, '-c', str(temp / '1'), '-s', '131072', str(temp / 'invalid.bin')],
                                    env=env, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((temp / 'invalid.bin').exists())
