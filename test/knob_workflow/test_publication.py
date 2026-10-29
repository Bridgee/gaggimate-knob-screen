import io
import hashlib
import os
import tempfile
from unittest import mock
from pathlib import Path
import sys
import tarfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
from check_public import inspect


class PublicationBoundaryTest(unittest.TestCase):
    def setUp(self):
        self.policy = {'allowed_roots': ['src', 'port', 'test'], 'allowed_root_files': ['README.md'], 'archives': ['port/source.tar.gz']}

    def check(self, name, data=b'int main() {}', mode='100644'):
        return inspect({name: (mode, data)}, self.policy)

    def test_firmware_source_and_readme_allowed(self):
        self.assertEqual(self.check('src/main.cpp'), [])
        self.assertEqual(self.check('README.md', b'# Firmware'), [])

    def test_private_directory_is_denied_even_inside_source(self):
        self.assertTrue(self.check('src/backups/device.json'))
        self.assertTrue(self.check('context.json'))

    def test_local_path_and_device_identity_are_rejected(self):
        self.assertTrue(self.check('src/private.txt', b'/' + b'Users/example/project'))
        self.assertTrue(self.check('src/private.txt', b':'.join([b'12', b'34', b'56', b'78', b'90', b'ab'])))

    def test_credential_material_is_rejected(self):
        self.assertTrue(self.check('src/key.txt', b'ghp_' + b'x' * 40))

    def test_non_english_project_text_is_rejected(self):
        self.assertTrue(self.check('README.md', '\u4e2d\u6587'.encode()))

    def test_generated_images_and_symlinks_are_rejected(self):
        self.assertTrue(self.check('src/firmware.bin'))
        self.assertTrue(self.check('src/linked.cpp', b'../../private', '120000'))

    def test_unreviewed_archive_is_rejected(self):
        self.assertTrue(self.check('port/device.tar.gz', b'not an approved archive'))

    def test_approved_archive_member_paths_are_checked(self):
        buffer = io.BytesIO()
        with tarfile.open(fileobj=buffer, mode='w:gz') as archive:
            member = tarfile.TarInfo('../device.txt'); member.size = 1
            archive.addfile(member, io.BytesIO(b'x'))
        self.assertTrue(self.check('port/source.tar.gz', buffer.getvalue()))


class ToolIsolationTest(unittest.TestCase):
    def test_new_tool_cache_does_not_switch_platformio_home_after_creation(self):
        import k718
        with tempfile.TemporaryDirectory() as directory:
            tools = Path(directory) / 'tools'
            with mock.patch.dict(os.environ, {'K718_TOOLS': str(tools)}), mock.patch.object(k718, 'git', return_value='1000000000'):
                os.environ.pop('PLATFORMIO_CORE_DIR', None)
                before = k718.tool_env()['PLATFORMIO_CORE_DIR']
                tools.mkdir()
                after = k718.tool_env()['PLATFORMIO_CORE_DIR']
                self.assertEqual(before, str(tools.resolve() / 'pio-core'))
                self.assertEqual(after, before)


class FirmwareArtifactPrivacyTest(unittest.TestCase):
    def test_only_hash_verified_vendor_paths_are_exempt(self):
        from package_release import vendor_build_paths, artifact_issues
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / 'lib.a'
            vendor = b'/' + b'Users/vendor/build/source.c'
            library.write_bytes(vendor + b'\x00')
            lock = {'libraries': [{'path': 'lib.a', 'sha256': hashlib.sha256(library.read_bytes()).hexdigest()}]}
            known = vendor_build_paths(directory, lock)
            self.assertEqual(artifact_issues('app.bin', vendor + b'\x00', known), [])
            private = b'/' + b'Users/example/private/source.c'
            self.assertTrue(artifact_issues('app.bin', private + b'\x00', known))

    def test_changed_vendor_archive_is_rejected(self):
        from package_release import vendor_build_paths
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / 'lib.a').write_bytes(b'changed')
            with self.assertRaisesRegex(RuntimeError, 'provenance mismatch'):
                vendor_build_paths(directory, {'libraries': [{'path': 'lib.a', 'sha256': '0' * 64}]})

    def test_parser_delimiter_does_not_hide_real_pem_material(self):
        from package_release import artifact_issues
        header = b'-----BEGIN ' + b'PRIVATE KEY-----'
        self.assertEqual(artifact_issues('app.bin', header + b'\x00', set()), [])
        self.assertTrue(artifact_issues('app.bin', header + b'\n' + b'A' * 100, set()))
