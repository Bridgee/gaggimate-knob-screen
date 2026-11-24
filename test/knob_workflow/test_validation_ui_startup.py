"""Guard LVGL ownership: validation widgets must exist before its UI task starts."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ValidationUiStartupTest(unittest.TestCase):
    def test_notice_is_created_before_ui_task_can_run(self):
        controller = (ROOT / 'src/display/core/Controller.cpp').read_text()
        ui = (ROOT / 'src/display/ui/default/DefaultUI.cpp').read_text()
        # Controller setup resumes after the UI constructor starts another core.
        # Any LVGL widget creation there races screen animation and rendering.
        self.assertEqual(re.findall(r'\blv_\w+(?=\s*\()', controller), [])
        panel = ui.split('void DefaultUI::setupPanel() {', 1)[1].split('void DefaultUI::setupState()', 1)[0]
        self.assertIn('SIMULATED / NO CONTROLLER', panel)
        self.assertIn('#ifdef GAGGIMATE_KNOB_VALIDATION', panel)
        constructor = ui.split('DefaultUI::DefaultUI(', 1)[1].split('void DefaultUI::init()', 1)[0]
        self.assertLess(constructor.index('setupPanel();'), constructor.index('xTaskCreatePinnedToCore('))
        self.assertLess(panel.index('SIMULATED / NO CONTROLLER'), panel.index('lv_task_handler();'))


if __name__ == '__main__':
    unittest.main()
