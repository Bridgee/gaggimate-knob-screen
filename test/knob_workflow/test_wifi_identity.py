"""Compile the actual hotspot identity and native info-row expression for each role."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class WifiIdentityTest(unittest.TestCase):
    def test_target_identity_and_native_network_label(self):
        source = (ROOT / 'src/display/ui/default/DefaultUI.cpp').read_text()
        match = re.search(r'const String network = .*?;', source)
        self.assertIsNotNone(match)
        for defines, expected in [([], 'GaggiMate'), (['GAGGIMATE_KNOB'], 'gaggimate_knob'),
                                  (['GAGGIMATE_KNOB', 'GAGGIMATE_KNOB_VALIDATION'], 'gaggimate_knob')]:
            with self.subTest(defines=defines), tempfile.TemporaryDirectory() as folder:
                cpp = Path(folder) / 'identity.cpp'
                binary = Path(folder) / 'identity'
                cpp.write_text('''#include <display/config.h>
#include <string>
#include <cassert>
using String = std::string;
struct Settings { String getWifiSsid() { return "ExampleWiFi"; } };
struct Status { bool connected; bool wifi() { return connected; } };
String label(bool apActive, bool connected) {
    Settings settings; Status systemStatus{connected};
''' + match[0] + '''
    return network;
}
int main() {
    assert(std::string(WIFI_AP_SSID) == "''' + expected + '''");
    assert(label(true, false) == WIFI_AP_SSID);
    assert(label(true, true) == WIFI_AP_SSID);
    assert(label(false, true) == "ExampleWiFi");
    assert(label(false, false) == "Disconnected");
}
''')
                subprocess.run(['clang++', '-std=c++17', '-I'+str(ROOT/'src'),
                                *['-D'+d for d in defines], str(cpp), '-o', str(binary)], check=True)
                subprocess.run([str(binary)], check=True)
        controller = (ROOT/'src/display/core/Controller.cpp').read_text()
        self.assertIn('WiFi.softAP(WIFI_AP_SSID, secured ? apPassword.c_str() : nullptr)', controller)


if __name__ == '__main__':
    unittest.main()
