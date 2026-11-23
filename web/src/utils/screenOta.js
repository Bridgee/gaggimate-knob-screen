// Only the firmware's own-release endpoint can authorize a screen installation.
// Do not use the legacy official OTA endpoint for K718.
export function screenOtaRequest(settings, options = {}, submitting = false) {
  const qualification = options.qualification === true;
  const verifyOnly = options.verifyOnly === true;
  const permission = qualification
    ? settings.screenOtaQualificationAvailable === true
    : settings.screenOtaInstallationAllowed === true;
  if (
    !settings.pairSupported ||
    settings.controllerConnected !== true ||
    !permission ||
    submitting ||
    settings.updating ||
    settings.pairStatus !== 'ready' ||
    settings.pairCompatibility !== 'matched' ||
    settings.pairDisplayState !== 'update-available' ||
    settings.screenOtaPreflight !== 'metadata-ready' ||
    (!qualification && verifyOnly) ||
    settings.screenOtaSelectionTag !== settings.pairReleaseTag ||
    !/^v[0-9]+\.[0-9]+\.[0-9]+(?:[-+][a-zA-Z0-9.+-]+)?$/.test(settings.screenOtaSelectionTag) ||
    !/^[a-f0-9]{64}$/.test(settings.screenOtaManifestSha256) ||
    !/^[a-f0-9]{64}$/.test(settings.screenOtaImageSha256)
  )
    return null;
  return Object.freeze({
    tp: 'req:screen-ota-start',
    component: 'display',
    tag: settings.screenOtaSelectionTag,
    manifestSha256: settings.screenOtaManifestSha256,
    imageSha256: settings.screenOtaImageSha256,
    qualification,
    verifyOnly,
  });
}

export function selectionCurrent(settings, selection, submitting = false) {
  if (!selection) return false;
  const current = screenOtaRequest(settings, selection, submitting);
  return current !== null && Object.keys(current).every(key => current[key] === selection[key]);
}

export function otaStateText(state) {
  return (
    {
      'metadata-unavailable': 'Check matched releases to obtain verified update metadata.',
      'refresh-required': 'The release check expired. Refresh matched releases before updating.',
      'invalid-release': 'Release metadata is invalid. Installation is blocked.',
      'preview-excluded': 'This build accepts stable releases only.',
      'not-newer': 'No newer compatible screen release is selected.',
      busy: 'An update or machine process is active. Wait until it finishes.',
      'controller-disconnected':
        'Connect the matching controller on the test bench before updating.',
      'controller-unidentified': 'Waiting for the controller version and protocol.',
      'protocol-mismatch':
        'The selected release uses a different controller protocol. Use the matched USB packages for this transition.',
      'controller-version-mismatch':
        'The controller version does not match this screen release. Install its specified official controller version first.',
      'incompatible-controller': 'The controller does not match the selected release.',
      'startup-unverified': 'Wait for screen startup health confirmation.',
      'restart-required': 'Restart the screen to settle its current boot selection.',
      'standby-required': 'Put the machine in standby before updating the screen.',
      'unsupported-partitions': 'The flash layout requires a checked USB installation.',
      'metadata-ready':
        'Release checks passed. Installation also requires a separately enabled permission.',
      'installation-disabled': 'Screen web installation has not been enabled for this build.',
      'selection-changed': 'The selected release changed. Refresh and confirm the new selection.',
      'invalid-request': 'The update request was rejected as invalid.',
      queued: 'The screen update was queued.',
      downloading: 'Downloading and staging the screen image.',
      verifying: 'Checking the downloaded image.',
      readback: 'Verifying the staged image against its published SHA-256.',
      error:
        'The screen update stopped. Check the error below; do not assume a new image was installed.',
      verified: 'The inactive image passed verification. Boot selection was not changed.',
      committing: 'Selecting the verified image for the next boot.',
      rebooting: 'The screen is restarting. Reconnect and check its version and startup health.',
      'recovery-required':
        'Boot selection needs recovery. Use the documented USB recovery procedure.',
    }[state] || 'The update could not proceed. Refresh status before retrying.'
  );
}

export function installedPairText(state) {
  return (
    {
      matched: 'The installed screen and controller versions match.',
      'controller-disconnected':
        'No controller is connected; installed-pair compatibility is unknown.',
      'controller-unidentified': 'Waiting for the connected controller version and protocol.',
      'protocol-mismatch': 'The installed screen and controller protocols differ.',
      'controller-version-mismatch':
        'The controller version differs from the installed screen official base.',
    }[state] || 'Installed-pair compatibility is unavailable.'
  );
}
