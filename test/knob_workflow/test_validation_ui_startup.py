"""Keep validation notices out of the shared screen UI."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ValidationUiStartupTest(unittest.TestCase):
    def test_no_validation_overlay_or_controller_owned_lvgl_widgets(self):
        controller = (ROOT / 'src/display/core/Controller.cpp').read_text()
        ui = (ROOT / 'src/display/ui/default/DefaultUI.cpp').read_text()
        # Controller setup resumes after the UI constructor starts another core.
        # Any LVGL widget creation there races screen animation and rendering.
        self.assertEqual(re.findall(r'\blv_\w+(?=\s*\()', controller), [])
        self.assertNotIn('SIMULATED / NO CONTROLLER', ui)
        self.assertNotIn('GAGGIMATE_KNOB_VALIDATION', ui)


if __name__ == '__main__':
    unittest.main()
