import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
from k718_lock import digest
from package_pair import package, check_image, checked_policy, HttpsRedirect, ROOT


def image(marker):
    data = bytearray(320)
    data[0] = 0xe9
    data[12:14] = (9).to_bytes(2, 'little')
    data[23] = 1
    data[32:36] = bytes.fromhex('3254cdab')
    data[240:240 + len(marker)] = marker
    return bytes(data) + hashlib.sha256(data).digest()


class PairTest(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(); self.addCleanup(temp.cleanup)
        self.root = Path(temp.name); self.screen = self.root / 'screen'; self.screen.mkdir()
        self.output = self.root / 'paired'
        self.controller = self.root / 'original.bin'; self.controller.write_bytes(image(b'official'))
        self.policy = json.loads((ROOT / 'port/controller-release.json').read_text())
        self.policy['asset'].update(bytes=self.controller.stat().st_size, sha256=digest(self.controller))
        (self.screen / 'firmware.bin').write_bytes(image(b'knob'))
        (self.screen / 'boot_app0.bin').write_bytes(b'\xff' * 8192)
        (self.screen / 'partitions.bin').write_bytes(struct.pack('<HBBII16sI', 0x50aa, 0, 0x10, 0x10000, 0x640000, b'app0', 0))
        self.manifest = {
            'schema': 1, 'target': 'display-knob', 'chip': 'ESP32-S3',
            'source_commit': 'a' * 40, 'source_tree': 'b' * 40, 'port_version': '0.3.6-rc.1',
            'upstream': {'commit': self.policy['commit'], 'version': self.policy['tag']},
            'controller_protocol': 5, 'application_offset': 0x10000, 'application_size': 0x640000,
            'artifacts': {p.name: {'bytes': p.stat().st_size, 'sha256': digest(p)} for p in self.screen.glob('*.bin')},
        }
        self.save()

    def save(self):
        (self.screen / 'release.json').write_text(json.dumps(self.manifest))

    def run_package(self):
        return package(self.screen, self.output, self.controller, copy.deepcopy(self.policy))

    def test_pair_preserves_both_images_and_independent_versions(self):
        self.run_package(); doc = json.loads((self.output / 'pair.json').read_text())
        self.assertEqual(doc['screen']['version'], '0.3.6-rc.1')
        self.assertEqual(doc['controller']['version'], 'v1.9.0')
        self.assertFalse(doc['installation_allowed'])
        self.assertFalse(doc['compatibility']['controller_ota_verified'])
        self.assertEqual((self.output / 'board-firmware.bin').read_bytes(), self.controller.read_bytes())
        self.assertEqual((self.output / 'display-knob-firmware.bin').read_bytes(), (self.screen / 'firmware.bin').read_bytes())
        for line in (self.output / 'SHA256SUMS').read_text().splitlines():
            sha, name = line.split('  '); self.assertEqual(sha, digest(self.output / name))

    def test_wrong_controller_file_rejected_atomically(self):
        self.controller.write_bytes(image(b'changed!'))
        with self.assertRaisesRegex(RuntimeError, 'checksum'): self.run_package()
        self.assertFalse(self.output.exists())

    def test_changed_screen_rejected(self):
        (self.screen / 'firmware.bin').write_bytes(image(b'wrong'))
        with self.assertRaisesRegex(RuntimeError, 'checksum'): self.run_package()

    def test_protocol_mismatch_rejected(self):
        self.manifest['controller_protocol'] = 8; self.save()
        with self.assertRaisesRegex(RuntimeError, 'compatibility'): self.run_package()

    def test_upstream_mismatch_rejected(self):
        self.manifest['upstream']['commit'] = 'c' * 40; self.save()
        with self.assertRaisesRegex(RuntimeError, 'compatibility'): self.run_package()

    def test_existing_output_never_replaced(self):
        self.output.mkdir(); (self.output / 'keep').write_text('preserve')
        with self.assertRaisesRegex(RuntimeError, 'new pair'): self.run_package()
        self.assertEqual((self.output / 'keep').read_text(), 'preserve')

    def test_wrong_chip_or_corrupt_image_digest_rejected(self):
        data = bytearray(image(b'test')); data[12] = 0
        data[-32:] = hashlib.sha256(data[:-32]).digest()
        with self.assertRaisesRegex(RuntimeError, 'application'): check_image(data)
        data = bytearray(image(b'test')); data[-1] ^= 1
        with self.assertRaisesRegex(RuntimeError, 'digest'): check_image(data)

    def test_unreviewed_source_or_install_permission_rejected(self):
        for field, value in [('url', 'https://example.com/file'), ('name', '../file')]:
            policy = copy.deepcopy(self.policy); policy['asset'][field] = value
            with self.assertRaisesRegex(RuntimeError, 'policy'): checked_policy(policy)
        self.policy['installation_allowed'] = True
        with self.assertRaisesRegex(RuntimeError, 'policy'): self.run_package()

    def test_non_https_redirect_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'non-HTTPS'):
            HttpsRedirect().redirect_request(None, None, 302, '', {}, 'http://example.com')


if __name__ == '__main__':
    unittest.main()
