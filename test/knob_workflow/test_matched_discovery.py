from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class MatchedDiscoveryTest(unittest.TestCase):
    def test_native_pair_parser_and_version_compatibility(self):
        with tempfile.TemporaryDirectory() as folder:
            binary = Path(folder) / 'matched'
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                            '-I' + str(ROOT / 'src'), '-I' + str(ROOT / 'lib/OTA/src'),
                            '-I' + str(ROOT / '.pio/libdeps/display-knob/ArduinoJson/src'),
                            str(ROOT / 'test/knob_native/check_matched_release.cpp'), '-o', str(binary)], check=True)
            subprocess.run([str(binary), str(ROOT / 'test/knob_native/fixtures')], check=True)

    def test_screen_ota_preflight_remains_read_only(self):
        with tempfile.TemporaryDirectory() as folder:
            binary = Path(folder) / 'screen-ota'
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                            '-I' + str(ROOT / 'src'),
                            '-I' + str(ROOT / '.pio/libdeps/display-knob/ArduinoJson/src'),
                            str(ROOT / 'test/knob_native/check_screen_ota.cpp'), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
