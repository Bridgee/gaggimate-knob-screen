"""Reject stale generated native360 resources without requiring Pillow to build."""
import runpy
from pathlib import Path

Import("env")
root = Path(env.subst("$PROJECT_DIR"))
runpy.run_path(str(root / 'scripts/generate_knob_ui.py'))['check']()
