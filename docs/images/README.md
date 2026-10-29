# Documentation images

These are native 360 × 360 LVGL renders of this port, using simulated state. They are not photographs, hardware captures or evidence of hardware acceptance.

- Power examples call `updateKnobStatusBattery()` with external power or a 60% battery fixture.
- Knob examples call `KnobAdjustment`: automatic single-field focus and touch-selected multi-field focus.
- Standby examples call `updateStandbyDisplay()` with custom text or a centered clock.
- Menu positioning uses the exact helper and runtime effect extracted from `DefaultUI.cpp`, after the EEZ screen is created. Exporting the EEZ design coordinates alone gives an incorrect menu layout.

With the pinned build dependencies installed:

```sh
python scripts/generate_doc_previews.py
python scripts/generate_doc_previews.py --check
```

The renderer checks three- and four-button menu geometry and both themes. The normal firmware checks verify that these images match fresh renders. `previews.json` records each image's scene, theme, purpose and checksum.

Do not reposition icons or draw focus/battery states onto exported images. Change the scene fixture or production source as appropriate, then regenerate and visually review the output.
