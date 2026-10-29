const LED_KEYS = new Set(['knobLedEnabled', 'knobLedBrightness']);
const canonical = value => (value === undefined ? undefined : String(value));

// Keep an LED-only save out of the existing coffee-machine settings path.
export function ledOnlyPatch(current, saved, schedules, savedSchedules, restart = false) {
  if (!current.knobLedSupported || !saved || restart) return null;
  if (JSON.stringify(schedules) !== JSON.stringify(savedSchedules)) return null;
  const keys = new Set([...Object.keys(current), ...Object.keys(saved)]);
  keys.delete('knobLedSupported');
  const changed = [...keys].filter(key => canonical(current[key]) !== canonical(saved[key]));
  if (!changed.length || changed.some(key => !LED_KEYS.has(key))) return null;
  const data = new FormData();
  data.set('knobLedEnabled', current.knobLedEnabled ? '1' : '0');
  data.set('knobLedBrightness', String(current.knobLedBrightness));
  return data;
}
