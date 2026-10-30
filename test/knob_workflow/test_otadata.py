import struct
import unittest
from k718_otadata import inspect, record, selected, trial


def metadata(a=None, b=None):
    return (a or b'\xff' * 32) + b'\xff' * (4096 - 32) + (b or b'\xff' * 32) + b'\xff' * (4096 - 32)


class OtaSelectionTest(unittest.TestCase):
    def test_vendor_boot_app0_crc(self):
        self.assertEqual(record(1, 0xffffffff)[28:], bytes.fromhex('9a984347'))
        self.assertEqual(selected(metadata(record(1, 0xffffffff)))['slot'], 'app0')

    def test_erased_baseline(self):
        self.assertEqual(selected(metadata())['slot'], 'app0')

    def test_slot_one_and_mirror(self):
        self.assertEqual(selected(metadata(record(1, 2), record(2, 2)))['slot'], 'app1')
        self.assertEqual(selected(metadata(record(4, 2), record(3, 2)))['slot'], 'app1')
        self.assertEqual(selected(metadata(record(7, 2), record(7, 2)))['copy'], 0)

    def test_invalid_aborted_and_bad_crc_ignored(self):
        for state in (3, 4):
            self.assertEqual(selected(metadata(record(1, 2), record(2, state)))['slot'], 'app0')
        broken = bytearray(record(2, 2)); broken[28] ^= 1
        self.assertEqual(selected(metadata(record(1, 2), bytes(broken)))['slot'], 'app0')
        with self.assertRaisesRegex(RuntimeError, 'No settled'):
            selected(metadata(bytes(broken)))

    def test_pending_trial_and_unknown_rejected(self):
        for state in (0, 1):
            with self.assertRaisesRegex(RuntimeError, 'Unsettled'):
                selected(metadata(record(1, 2), record(2, state)))
        bad = bytearray(record(1, 2)); struct.pack_into('<I', bad, 24, 9)
        with self.assertRaisesRegex(RuntimeError, 'Unknown'):
            inspect(metadata(bytes(bad)))
        with self.assertRaisesRegex(RuntimeError, 'size'): selected(b'')

    def test_trial_preserves_current_copy_and_alternates(self):
        for current, expected in ((1, 'app1'), (2, 'app0')):
            data = metadata(record(current, 2))
            slot, offset, sector, new = trial(data)
            self.assertEqual(slot, expected)
            self.assertEqual(offset, 4096)
            self.assertEqual(new[:4096], data[:4096])
            self.assertEqual(new[4096:], sector)
            self.assertEqual(inspect(new)[1]['state'], 0)
        with self.assertRaisesRegex(RuntimeError, 'exhausted'):
            trial(metadata(record(0xfffffffe, 2)))
        with self.assertRaisesRegex(RuntimeError, 'settled'):
            trial(metadata())
