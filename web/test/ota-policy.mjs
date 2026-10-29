import assert from 'node:assert/strict';
import { createServer } from 'vite';
import renderToString from 'preact-render-to-string';

const server = await createServer({
  root: new URL('../', import.meta.url).pathname,
  server: { middlewareMode: true },
  appType: 'custom',
  optimizeDeps: { noDiscovery: true, entries: [] },
});
try {
  const { FirmwareVersions, canInstallOta } = await server.ssrLoadModule(
    '/src/pages/Settings/tabs/FirmwareVersions.jsx',
  );
  const settings = {
    controllerVersion: 'v1.9.0',
    displayVersion: 'v1.9.0-k718.0.3.5-rc.2+example',
    latestVersion: '1.10.0',
    controllerUpdateAvailable: true,
    displayUpdateAvailable: true,
    otaAllowed: false,
    otaDisabledReason: 'Wireless installation is disabled. Use USB with a K718 build.',
  };
  function buttons(node) {
    if (Array.isArray(node)) return node.flatMap(buttons);
    if (!node || typeof node !== 'object') return [];
    if (node.type === 'button') return [node];
    return buttons(node.props?.children);
  }
  function check(state, submitting, expectedEnabled) {
    const sent = [];
    const tree = FirmwareVersions({ settings: state, submitting, onUpdate: cp => sent.push(cp) });
    const controls = buttons(tree);
    assert.equal(controls.length, 2);
    assert.deepEqual(
      controls.map(b => !b.props.disabled),
      expectedEnabled,
    );
    controls.forEach(b => b.props.onClick());
    assert.deepEqual(
      sent,
      ['controller', 'display'].filter((_, i) => expectedEnabled[i]),
    );
    return renderToString(tree);
  }
  const blocked = check(settings, false, [false, false]);
  assert.equal((blocked.match(/Update available: 1.10.0/g) || []).length, 2);
  assert.match(blocked, /USB installation required/);
  assert.match(blocked, /Wireless installation is disabled/);
  assert.equal((blocked.match(/aria-describedby="ota-installation-notice"/g) || []).length, 2);
  assert.equal((blocked.match(/<button\b[^>]*\sdisabled(?:[ >]|=)/g) || []).length, 2);
  const unavailable = check(
    { ...settings, displayUpdateAvailable: false, controllerUpdateAvailable: false },
    false,
    [false, false],
  );
  assert.doesNotMatch(unavailable, /Update available:/);
  assert.match(unavailable, /USB installation required/);
  assert.match(
    check({ ...settings, otaDisabledReason: '' }, false, [false, false]),
    /Official update checks and notifications remain enabled/,
  );
  const upstream = { ...settings, otaAllowed: true };
  assert.doesNotMatch(check(upstream, false, [true, true]), /USB installation required/);
  check({ ...upstream, otaAllowed: undefined }, false, [true, true]);
  check({ ...upstream, displayUpdateAvailable: false }, false, [true, false]);
  check(upstream, true, [false, false]);
  assert.equal(canInstallOta(upstream, 'both'), false);
  assert.equal(canInstallOta(upstream, ''), false);
  console.log(
    'OTA web policy: disabled controls, guarded actions, update notices and legacy behavior passed.',
  );
} finally {
  await server.close();
}
