import hashlib
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from types import ModuleType
from unittest.mock import patch
from k718 import write_json
from k718_lock import digest
from k718_flash import flash
from k718_otadata import record, selected


def metadata(seq=1, state=2):
    return record(seq, state) + b'\xff' * (8192 - 32)


class UsbSlotsTest(unittest.TestCase):
    def exercise(self, mode='update', seq=1, state=2, fault=None):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            entries = [('nvs', 1, 2, 0x9000, 0x5000), ('otadata', 1, 0, 0xe000, 0x2000),
                       ('app0', 0, 0x10, 0x10000, 0x640000), ('app1', 0, 0x11, 0x650000, 0x640000)]
            table = b''.join(struct.pack('<HBBII16sI', 0x50aa, t, st, off, size, name.encode(), 0)
                             for name, t, st, off, size in entries)
            files = {'partitions.bin': table, 'firmware.bin': b'new verified application',
                     'boot_app0.bin': metadata(1, 0xffffffff), 'bootloader.bin': b'reviewed bootloader'}
            for name, data in files.items(): (root / name).write_bytes(data)
            manifest = dict(schema=1, target='display-knob', chip='ESP32-S3', application_offset=0x10000,
                            application_size=0x640000, artifacts={name: dict(bytes=len(data), sha256=digest(root / name))
                                                               for name, data in files.items()})
            write_json(root / 'release.json', manifest)
            device = bytearray(b'\xff' * 0x1000000)
            device[:len(files['bootloader.bin'])] = files['bootloader.bin']
            device[0x8000:0x8000+len(table)] = table
            device[0x9000:0xe000] = b'N' * 0x5000
            device[0xe000:0x10000] = metadata(seq, state)
            device[0x10000:0x650000] = b'A' * 0x640000
            device[0x650000:0xc90000] = b'B' * 0x640000
            device[0xc90000:] = b'F' * (0x1000000 - 0xc90000)
            if fault == 'bootloader': device[0] ^= 1
            original = bytes(device)
            writes = []
            class Port:
                def setDTR(self, value): pass
                def close(self): pass
            class Loader:
                IS_STUB = True
                def __init__(self, *args): self._port = Port()
                def connect(self, *args, **kwargs): pass
                def read_mac(self): return bytes(range(6))
                def run_stub(self): return self
                def change_baud(self, *args): pass
                def flash_set_parameters(self, *args): pass
                def read_flash(self, off, size): return bytes(device[off:off+size])
                def flash_md5sum(self, off, size):
                    if fault == 'backup' and off == 0x9000: return 'bad'
                    return hashlib.md5(device[off:off+size]).hexdigest()
                def hard_reset(self): pass
            def write(loader, args):
                off, stream = args.addr_filename[0]; data = stream.read(); writes.append((off, len(data)))
                device[off:off+len(data)] = data
                if fault == 'write': device[off] ^= 1
            target = ModuleType('esptool.targets.esp32s3'); target.ESP32S3ROM = Loader
            cmds = ModuleType('esptool.cmds'); cmds.write_flash = write
            modules = {'esptool': ModuleType('esptool'), 'esptool.targets': ModuleType('esptool.targets'),
                       'esptool.targets.esp32s3': target, 'esptool.cmds': cmds}
            error = None
            with patch.dict(sys.modules, modules), patch('k718_flash.tool_env', return_value={'PLATFORMIO_CORE_DIR': tmp}):
                try:
                    flash(root, 'fake', ':'.join(f'{x:02x}' for x in range(6)), root / 'backup', validation=True, mode=mode)
                except RuntimeError as exc: error = str(exc)
            self.assertEqual(device[0x9000:0xe000], original[0x9000:0xe000])
            self.assertEqual(device[0xc90000:], original[0xc90000:])
            if fault or (mode == 'update' and state == 1):
                self.assertIsNotNone(error)
                self.assertEqual(device[0xe000:0x10000], original[0xe000:0x10000])
                if fault != 'write': self.assertEqual(writes, [])
                active = 0x10000 if seq % 2 else 0x650000
                if mode != 'recover': self.assertEqual(device[active:active+0x640000], original[active:active+0x640000])
                return
            self.assertIsNone(error)
            receipt = json.loads((root / 'backup/flash.json').read_text())
            self.assertTrue(receipt['readback_verified'])
            if mode == 'recover':
                self.assertEqual(device[0xe000:0x10000], files['boot_app0.bin'])
                self.assertEqual(device[0x650000:0xc90000], original[0x650000:0xc90000])
                self.assertEqual(writes[-1][0], 0xe000)
            elif mode in ('trial', 'update'):
                self.assertEqual(writes[-1][0], 0xf000)
                self.assertEqual(device[0xe000:0xf000], original[0xe000:0xf000])
                active = 0x10000 if seq % 2 else 0x650000
                self.assertEqual(device[active:active+0x640000], original[active:active+0x640000])
                self.assertEqual(writes[0][0], 0x650000 if seq % 2 else 0x10000)
            for name, entry in receipt['backups'].items():
                self.assertEqual(digest(root / 'backup' / (name + '.bin')), entry['sha256'])

    def test_settled_app1_update(self): self.exercise(seq=2)
    def test_settled_app0_update(self): self.exercise()
    def test_pending_update_refused(self): self.exercise(seq=2, state=1)
    def test_pending_recovery(self): self.exercise(mode='recover', seq=2, state=1)
    def test_app1_recovery(self): self.exercise(mode='recover', seq=2)
    def test_trial_keeps_fallback(self): self.exercise(mode='trial')
    def test_trial_requires_reviewed_bootloader(self): self.exercise(mode='trial', fault='bootloader')
    def test_backup_failure_never_writes(self): self.exercise(mode='recover', fault='backup')
    def test_image_failure_never_commits_selection(self): self.exercise(mode='trial', fault='write')
