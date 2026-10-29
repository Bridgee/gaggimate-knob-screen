import assert from 'node:assert/strict';
import { createServer } from 'vite';
import renderToString from 'preact-render-to-string';
import { ledOnlyPatch } from '../src/utils/knobLedSettings.js';

const schedules = [{ time: '07:00', days: [true, true, true, true, true, true, true] }];
const saved = {
  knobLedSupported: true,
  knobLedEnabled: true,
  knobLedBrightness: 45,
  targetSteamTemp: 145,
};
const patch = current => ledOnlyPatch(current, saved, schedules, schedules);
assert.deepEqual(
  [...patch({ ...saved, knobLedEnabled: false }).entries()],
  [
    ['knobLedEnabled', '0'],
    ['knobLedBrightness', '45'],
  ],
);
assert.deepEqual(
  [...patch({ ...saved, knobLedBrightness: '0' }).entries()],
  [
    ['knobLedEnabled', '1'],
    ['knobLedBrightness', '0'],
  ],
);
assert.equal(patch({ ...saved, knobLedBrightness: '45' }), null);
assert.equal(patch({ ...saved, knobLedBrightness: 100, targetSteamTemp: 146 }), null);
assert.equal(ledOnlyPatch({ ...saved, knobLedBrightness: 100 }, saved, [], schedules), null);
assert.equal(
  ledOnlyPatch({ ...saved, knobLedBrightness: 100 }, saved, schedules, schedules, true),
  null,
);
assert.equal(patch({ ...saved, knobLedSupported: false, knobLedBrightness: 100 }), null);
assert.equal(ledOnlyPatch(saved, null, schedules, schedules), null);

const server = await createServer({
  root: new URL('../', import.meta.url).pathname,
  server: { middlewareMode: true },
  appType: 'custom',
  ssr: { noExternal: ['@fortawesome/react-fontawesome', '@fortawesome/fontawesome-svg-core'] },
  optimizeDeps: { noDiscovery: true, entries: [] },
});
try {
  const { KnobLedSettings } = await server.ssrLoadModule(
    '/src/pages/Settings/tabs/KnobLedSettings.jsx',
  );
  const render = data =>
    renderToString(KnobLedSettings({ formData: data, onChange: () => () => {} }));
  assert.equal(render({}), '');
  for (const percent of [0, 45, 100]) {
    const html = render({ ...saved, knobLedBrightness: percent });
    assert.match(html, new RegExp(`aria-valuetext="${percent}%"`));
    assert.match(html, /min="0"/);
    assert.match(html, /max="100"/);
    assert.doesNotMatch(html, / disabled/);
    assert.match(html, /restored after a restart/);
  }
  const off = render({ ...saved, knobLedEnabled: false });
  assert.match(off, /<input[^>]*type="range"[^>]*disabled/);
  assert.match(off, /45%/);
  console.log(
    'LED settings: explicit off/zero serialization, isolated updates, capabilities and rendered controls passed.',
  );
} finally {
  await server.close();
}
