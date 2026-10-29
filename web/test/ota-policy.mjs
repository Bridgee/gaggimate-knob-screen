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
  const { FirmwareVersions, MatchedVersions, canInstallOta } = await server.ssrLoadModule(
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
  const pair = {
    ...settings,
    pairSupported: true,
    pairStatus: 'ready',
    pairReleaseTag: 'v0.3.6-rc.1',
    pairReleaseUrl: 'https://github.com/Bridgee/gaggimate-knob-screen/releases/tag/v0.3.6-rc.1',
    pairPrerelease: true,
    pairIncludesPrereleases: true,
    pairProtocol: 5,
    pairDisplayVersion: '0.3.6-rc.1',
    pairControllerVersion: 'v1.9.0',
    pairDisplayState: 'local-newer',
    pairControllerState: 'up-to-date',
    pairCompatibility: 'matched',
  };
  let refreshes = 0;
  const pairedTree = MatchedVersions({ settings: pair, onCheckPair: () => refreshes++ });
  const pairedButtons = buttons(pairedTree);
  assert.equal(pairedButtons.length, 3);
  assert.deepEqual(
    pairedButtons.map(b => !!b.props.disabled),
    [false, true, true],
  );
  pairedButtons[0].props.onClick();
  assert.equal(refreshes, 1);
  const pairedHtml = renderToString(pairedTree);
  assert.match(pairedHtml, /Published: 0.3.6-rc.1/);
  assert.match(pairedHtml, /Published: v1.9.0/);
  assert.match(pairedHtml, /Installed version is newer/);
  assert.match(pairedHtml, /Pre-release/);
  assert.match(pairedHtml, /Official GaggiMate 1.10.0 is available/);
  assert.match(pairedHtml, /OTA installation is unverified/);
  assert.equal(canInstallOta({ ...pair, otaAllowed: true }, 'controller'), false);
  assert.equal(canInstallOta({ ...pair, otaAllowed: true }, 'display'), false);
  for (const [status, message] of [
    ['error', 'Release metadata transfer was interrupted.'],
    ['no-release', 'No stable matched release was found.'],
    ['checking', ''],
  ]) {
    const tree = MatchedVersions({
      settings: { ...pair, pairStatus: status, pairMessage: message },
    });
    const html = renderToString(tree);
    assert.doesNotMatch(html, /Published: 0.3.6-rc.1/);
    assert.match(html, /Published: Unavailable/);
    assert.equal(!!buttons(tree)[0].props.disabled, status === 'checking');
    assert.equal(buttons(tree).filter(b => b.props.disabled).length >= 2, true);
  }
  for (const state of ['unknown', 'incompatible']) {
    const html = renderToString(
      MatchedVersions({ settings: { ...pair, pairCompatibility: state } }),
    );
    assert.doesNotMatch(html, /Target, protocol and controller version match/);
  }
  console.log(
    'OTA web policy: disabled controls, guarded actions, update notices and legacy behavior passed.',
  );
} finally {
  await server.close();
}
