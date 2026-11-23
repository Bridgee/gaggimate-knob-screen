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
  const {
    FirmwareVersions,
    MatchedVersions,
    OfficialUpdateChannel,
    ScreenUpdateConfirmation,
    canInstallOta,
  } = await server.ssrLoadModule('/src/pages/Settings/tabs/FirmwareVersions.jsx');
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
  assert.match(pairedHtml, /Target, protocol and controller version match/);
  assert.doesNotMatch(pairedHtml, /OTA installation is unverified|USB installation required/);
  assert.match(pairedHtml, /Web installation not enabled/);
  assert.match(pairedHtml, /Controller OTA remains disabled/);
  const candidateHtml = renderToString(
    MatchedVersions({ settings: { ...pair, screenOtaQualificationAvailable: true } }),
  );
  assert.match(candidateHtml, /Screen OTA bench tests/);
  assert.match(candidateHtml, /Verify inactive image/);
  const stableHtml = renderToString(
    MatchedVersions({ settings: { ...pair, screenOtaQualificationAvailable: false } }),
  );
  assert.doesNotMatch(stableHtml, /Screen OTA bench tests/);
  const notices = OfficialUpdateChannel({ settings: { ...pair, channel: 'nightly' } });
  assert.equal(notices.type, 'details');
  assert.equal(notices.props.open, undefined); // advanced upstream controls start collapsed
  const noticeHtml = renderToString(notices);
  assert.match(noticeHtml, /Official GaggiMate update notices/);
  assert.match(noticeHtml, /notifications only/);
  assert.match(noticeHtml, /Knob Screen firmware is/);
  assert.match(noticeHtml, /Save notice channel &amp; refresh/);
  assert.match(noticeHtml, /value="nightly" selected/);
  assert.equal(buttons(notices)[0].props.disabled, false);
  assert.equal(
    buttons(OfficialUpdateChannel({ settings: { ...pair, updating: true } }))[0].props.disabled,
    true,
  );
  assert.equal(
    buttons(OfficialUpdateChannel({ settings: pair, submitting: true }))[0].props.disabled,
    true,
  );
  const originalChannel = OfficialUpdateChannel({ settings: { channel: 'latest' } });
  assert.equal(originalChannel.type, 'form');
  const originalHtml = renderToString(originalChannel);
  assert.match(originalHtml, /Update Channel/);
  assert.match(originalHtml, /Save Channel &amp; Refresh/);
  assert.doesNotMatch(originalHtml, /notifications only|Upstream notification channel/);
  assert.equal(canInstallOta({ ...pair, otaAllowed: true }, 'controller'), false);
  assert.equal(canInstallOta({ ...pair, otaAllowed: true }, 'display'), false);
  // Read-only next-generation fields cannot unlock the legacy official installer.
  const hints = {
    ...pair,
    otaAllowed: true,
    screenOtaInstallationAllowed: true,
    controllerOtaInstallationAllowed: true,
    screenOtaPreflight: 'metadata-ready',
  };
  assert.equal(canInstallOta(hints, 'display'), false);
  assert.equal(canInstallOta(hints, 'controller'), false);
  assert.deepEqual(
    buttons(MatchedVersions({ settings: hints })).map(b => !!b.props.disabled),
    [false, true, true],
  );
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
  const { screenOtaRequest, selectionCurrent, otaStateText, installedPairText } =
    await server.ssrLoadModule('/src/utils/screenOta.js');
  const ready = {
    ...pair,
    controllerConnected: true,
    displayVersion: 'v1.10.0-k718.0.5.0-rc.11+example',
    pairReleaseTag: 'v0.5.0-rc.12',
    pairDisplayVersion: '0.5.0-rc.12',
    pairControllerVersion: 'v1.10.0',
    pairUpstreamVersion: 'v1.10.0',
    pairProtocol: 16,
    displayUpstreamVersion: 'v1.10.0',
    displayProtocol: 16,
    installedCompatibility: 'matched',
    pairDisplayState: 'update-available',
    screenOtaPreflight: 'metadata-ready',
    screenOtaInstallationAllowed: true,
    screenOtaSelectionTag: 'v0.5.0-rc.12',
    screenOtaManifestSha256: 'a'.repeat(64),
    screenOtaImageSha256: 'b'.repeat(64),
  };
  const request = screenOtaRequest(ready);
  assert.equal(Object.isFrozen(request), true);
  assert.deepEqual(request, {
    tp: 'req:screen-ota-start',
    component: 'display',
    tag: 'v0.5.0-rc.12',
    manifestSha256: 'a'.repeat(64),
    imageSha256: 'b'.repeat(64),
    qualification: false,
    verifyOnly: false,
  });
  assert.equal('url' in request, false);
  for (const changes of [
    { pairSupported: false },
    { controllerConnected: false },
    { controllerConnected: undefined },
    { screenOtaInstallationAllowed: false },
    { screenOtaInstallationAllowed: undefined },
    { screenOtaInstallationAllowed: 'true' },
    { pairStatus: 'checking' },
    { pairStatus: 'error' },
    { pairStatus: 'no-release' },
    { pairCompatibility: 'unknown' },
    { pairCompatibility: 'incompatible' },
    { pairDisplayState: 'up-to-date' },
    { pairDisplayState: 'local-newer' },
    { screenOtaSelectionTag: 'v0.5.0-rc.13' },
    { screenOtaImageSha256: 'bad' },
    { screenOtaManifestSha256: 'A'.repeat(64) },
    { updating: true },
  ])
    assert.equal(screenOtaRequest({ ...ready, ...changes }), null);
  assert.equal(screenOtaRequest(ready, {}, true), null);
  assert.equal(screenOtaRequest(ready, { verifyOnly: true }), null);
  for (const state of [
    'refresh-required',
    'busy',
    'controller-disconnected',
    'controller-unidentified',
    'protocol-mismatch',
    'controller-version-mismatch',
    'startup-unverified',
    'restart-required',
    'standby-required',
    'unsupported-partitions',
    'invalid-release',
    'preview-excluded',
  ]) {
    assert.equal(screenOtaRequest({ ...ready, screenOtaPreflight: state }), null);
    assert.notEqual(otaStateText(state), otaStateText('unknown-future-state'));
  }
  let selected = [];
  const wired = buttons(
    MatchedVersions({ settings: ready, onScreenUpdate: value => selected.push(value) }),
  );
  assert.deepEqual(
    wired.map(b => !!b.props.disabled),
    [false, true, false],
  );
  wired[1].props.onClick();
  wired[2].props.onClick();
  assert.deepEqual(selected, [{}]); // controller action never routes into either installer
  assert.equal(canInstallOta(ready, 'display'), false); // legacy endpoint remains locked
  assert.equal(screenOtaRequest(ready, { qualification: true }), null);
  const bench = {
    ...ready,
    screenOtaInstallationAllowed: false,
    screenOtaQualificationAvailable: true,
  };
  const verify = screenOtaRequest(bench, { qualification: true, verifyOnly: true });
  assert.equal(verify.verifyOnly, true);
  assert.equal(verify.qualification, true);
  assert.equal(screenOtaRequest(bench), null);
  selected = [];
  const benchControls = buttons(
    MatchedVersions({ settings: bench, onScreenUpdate: v => selected.push(v) }),
  );
  assert.deepEqual(
    benchControls.map(b => !!b.props.disabled),
    [false, true, true, false, false],
  );
  benchControls[3].props.onClick();
  benchControls[4].props.onClick();
  assert.deepEqual(selected, [
    { qualification: true, verifyOnly: true },
    { qualification: true, verifyOnly: false },
  ]);
  const selection = {
    request,
    upstreamVersion: 'v1.10.0',
    controllerVersion: 'v1.10.0',
    protocol: 16,
  };
  assert.equal(selectionCurrent(ready, request), true);
  let confirmed = 0;
  const confirmation = ScreenUpdateConfirmation({
    selection,
    settings: ready,
    onConfirm: () => confirmed++,
  });
  assert.match(renderToString(confirmation), /based on official v1.10.0/);
  assert.match(renderToString(confirmation), /Required controller: v1.10.0/);
  buttons(confirmation)[1].props.onClick();
  assert.equal(confirmed, 1);
  for (const changes of [
    { screenOtaImageSha256: 'c'.repeat(64) },
    { updating: true },
    { screenOtaPreflight: 'refresh-required' },
    { screenOtaInstallationAllowed: false },
  ]) {
    const changed = { ...ready, ...changes };
    assert.equal(selectionCurrent(changed, request), false);
    const dialog = ScreenUpdateConfirmation({
      selection,
      settings: changed,
      onConfirm: () => confirmed++,
    });
    assert.equal(buttons(dialog)[1].props.disabled, true);
    buttons(dialog)[1].props.onClick();
    assert.equal(confirmed, 1);
  }
  assert.equal(ScreenUpdateConfirmation({ selection: null, settings: ready }), null);
  for (const state of [
    'controller-disconnected',
    'protocol-mismatch',
    'controller-version-mismatch',
  ]) {
    const html = renderToString(
      MatchedVersions({ settings: { ...ready, installedCompatibility: state } }),
    );
    assert.match(html, new RegExp(installedPairText(state).replace(/[.*+?^${}()|[\]\\]/g, '\\$&')));
    assert.doesNotMatch(html, /installed screen and controller versions match/);
  }
  for (const phase of [
    'downloading',
    'readback',
    'verified',
    'rebooting',
    'error',
    'recovery-required',
  ]) {
    const html = renderToString(
      MatchedVersions({ settings: { ...ready, screenOtaPhase: phase, screenOtaProgress: 80 } }),
    );
    assert.match(html, /Screen update:/);
    assert.equal(html.includes(otaStateText(phase)), true);
  }
  const disconnected = renderToString(
    MatchedVersions({
      settings: {
        ...ready,
        controllerConnected: false,
        controllerVersion: 'v1.9.0',
        pairControllerState: 'up-to-date',
        installedCompatibility: 'controller-disconnected',
        displayPortVersion: '0.5.0-rc.11',
        screenBootHealth: 'confirmed',
      },
    }),
  );
  assert.match(disconnected, /Not connected/);
  assert.match(disconnected, /Installed version unavailable/);
  assert.doesNotMatch(disconnected, /v1.9.0|Same version/);
  assert.doesNotMatch(disconnected, /Target, protocol and controller version match/);
  assert.match(disconnected, /Current running screen: 0.5.0-rc.11/);
  assert.match(disconnected, /Startup health: confirmed/);
  console.log(
    'OTA web policy: disabled controls, guarded actions, update notices and legacy behavior passed.',
  );
} finally {
  await server.close();
}
