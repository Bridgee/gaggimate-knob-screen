"""Exercise the compiled firmware policy and version generation without a device."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


class OtaPolicyTest(unittest.TestCase):
    def test_generated_base_version_and_both_target_policies(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'src').mkdir()
            (root / 'port').mkdir()
            (root / 'scripts').mkdir()
            (root / 'scripts/k718_ota_config.py').write_text((ROOT / 'scripts/k718_ota_config.py').read_text())
            (root / 'port/config.json').write_text((ROOT / 'port/config.json').read_text())

            class Env:
                def subst(self, value):
                    return str(root)

            def inject(name):
                namespace['env'] = Env()

            namespace = {'Import': inject, '__file__': str(ROOT / 'scripts/knob_firmware_version.py')}
            with patch('subprocess.check_output', side_effect=['12345678abcdef', '', '1700000000']):
                exec(compile((ROOT / 'scripts/knob_firmware_version.py').read_text(), 'knob_firmware_version.py', 'exec'), namespace)
            header = (root / 'src/version.h').read_text()
            base = json.loads((root / 'port/config.json').read_text())['upstream']['version']
            self.assertIn(f'#define BUILD_UPSTREAM_VERSION "{base}"', header)
            self.assertIn(f'#define BUILD_GIT_VERSION "{base}-k718.', header)
            (root / 'policy.cpp').write_text(r'''#include "display/plugins/OtaPolicy.h"
#include "semver.h"
#include <cassert>
#include <cstring>
int main() {
#ifdef GAGGIMATE_KNOB
    assert(!FirmwareOtaPolicy::installationAllowed);
    assert(std::strstr(FirmwareOtaPolicy::disabledReason, "Display and controller OTA installation"));
    assert(std::strstr(FirmwareOtaPolicy::disabledReason, "USB"));
    assert(std::strcmp(FirmwareOtaPolicy::displayComparisonVersion, BUILD_UPSTREAM_VERSION) == 0);
    const char *baseVersion = FirmwareOtaPolicy::displayComparisonVersion;
    semver_t base = {}, same = {}, next = {};
    assert(semver_parse(baseVersion + 1, &base) == 0);
    assert(semver_parse(baseVersion + 1, &same) == 0);
    assert(semver_compare(same, base) == 0);
    assert(semver_parse(baseVersion + 1, &next) == 0);
    semver_bump(&next);
    assert(semver_compare(next, base) > 0);
    semver_free(&base); semver_free(&same); semver_free(&next);
#else
    assert(FirmwareOtaPolicy::installationAllowed);
    assert(std::strcmp(FirmwareOtaPolicy::displayComparisonVersion, BUILD_GIT_VERSION) == 0);
    assert(std::strlen(FirmwareOtaPolicy::disabledReason) == 0);
#endif
}
''')
            for knob in (False, True):
                binary = root / ('knob' if knob else 'upstream')
                command = ['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                           '-I' + str(root / 'src'), '-I' + str(ROOT / 'src'),
                           '-I' + str(ROOT / 'lib/OTA/src')]
                if knob:
                    command.append('-DGAGGIMATE_KNOB')
                # Compile the original version comparator as C, then link it into the policy harness.
                obj = root / 'semver.o'
                subprocess.run(['cc', '-fsanitize=address,undefined', '-c', str(ROOT / 'lib/OTA/src/semver.c'), '-o', str(obj)], check=True)
                subprocess.run(command + [str(root / 'policy.cpp'), str(obj), '-o', str(binary)], check=True)
                subprocess.run([str(binary)], check=True)

    def test_policy_is_enforced_before_update_and_separate_from_discovery(self):
        source = (ROOT / 'src/display/plugins/WebUIPlugin.cpp').read_text()
        handler = source.split('void WebUIPlugin::handleOTAStart', 1)[1].split('// Booleans', 1)[0]
        self.assertLess(handler.index('!FirmwareOtaPolicy::installationAllowed'), handler.index('updating = true'))
        self.assertLess(handler.index('return;'), handler.index('updating = true'))
        self.assertIn('doc["otaAllowed"] = FirmwareOtaPolicy::installationAllowed;', source)
        self.assertIn('doc["displayUpdateAvailable"] = ota->isUpdateAvailable(false);', source)
        self.assertIn('doc["controllerUpdateAvailable"] = ota->isUpdateAvailable(true);', source)
        self.assertIn('return ota->isUpdateAvailable() || ota->isUpdateAvailable(true);', source)
        self.assertIn('ota->checkForUpdates();', source)
        self.assertIn('FirmwareOtaPolicy::displayComparisonVersion, controller->getSystemInfo().version', source)
