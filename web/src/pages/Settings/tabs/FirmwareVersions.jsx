import {
  screenOtaRequest,
  selectionCurrent,
  otaStateText,
  installedPairText,
} from '../../../utils/screenOta.js';

export function canInstallOta(settings, component, submitting = false) {
  return (
    (component === 'display' || component === 'controller') &&
    !settings.pairSupported &&
    settings.otaAllowed !== false &&
    Boolean(settings[`${component}UpdateAvailable`]) &&
    !submitting
  );
}

const componentStates = {
  'update-available': 'Newer version published',
  'up-to-date': 'Same version',
  'local-newer': 'Installed version is newer',
  unknown: 'Installed version unavailable',
};

export function MatchedVersions({ settings, onCheckPair, onScreenUpdate, submitting = false }) {
  const ready = settings.pairStatus === 'ready';
  const checking = settings.pairStatus === 'checking';
  const compatibility = {
    matched: 'Target, protocol and controller version match.',
    incompatible: 'This pair does not match the current protocol or controller version.',
    unknown: 'Compatibility is unknown until the controller is connected and identified.',
  };
  return (
    <>
      <div className='space-y-2 md:col-span-2'>
        <div className='flex flex-wrap items-center justify-between gap-2'>
          <a
            href='https://github.com/Bridgee/gaggimate-knob-screen/releases'
            target='_blank'
            rel='noreferrer'
            className='link link-primary font-semibold'
          >
            Knob Screen matched releases
          </a>
          <button
            type='button'
            className='btn btn-secondary btn-sm'
            disabled={checking || Boolean(settings.updating) || submitting}
            onClick={onCheckPair}
          >
            {checking ? 'Checking…' : 'Check matched releases'}
          </button>
        </div>
        <p className='text-base-content/70 text-sm'>
          Screen updates: Bridgee/gaggimate-knob-screen. Controller image: unmodified official
          GaggiMate, pinned to the version required by each pair. Controller OTA remains disabled.
        </p>
        <p role='status' className='text-sm'>
          Installed screen base: {settings.displayUpstreamVersion || 'Unknown'} · Protocol{' '}
          {settings.displayProtocol || 'Unknown'}.{' '}
          {installedPairText(settings.installedCompatibility)}
        </p>
        {ready ? (
          <div className='flex flex-wrap items-center gap-2'>
            <a href={settings.pairReleaseUrl} target='_blank' rel='noreferrer' className='link'>
              {settings.pairReleaseTag}
            </a>
            {settings.pairPrerelease && <span className='badge badge-warning'>Pre-release</span>}
            <span className='text-base-content/70 text-sm'>
              Based on official {settings.pairUpstreamVersion || settings.pairControllerVersion} ·
              Protocol {settings.pairProtocol}
            </span>
          </div>
        ) : (
          <p role='status' className='text-base-content/70 text-sm'>
            {checking
              ? 'Checking published pairs…'
              : settings.pairMessage || 'The first check is pending.'}
          </p>
        )}
        {settings.pairIncludesPrereleases && (
          <p className='text-base-content/60 text-xs'>
            This candidate build includes published pre-releases in its checks.
          </p>
        )}
      </div>
      {['controller', 'display'].map(component => {
        const label = component === 'controller' ? 'Controller' : 'Display';
        const key = component === 'controller' ? 'Controller' : 'Display';
        return (
          <div key={component} className='flex flex-col space-y-2'>
            <span className='text-base-content/70 text-sm font-medium'>{label} Version</span>
            <span className='text-base-content font-semibold break-all'>
              {settings[`${component}Version`] || 'Not connected'}
            </span>
            <span className='text-base-content/70 text-sm'>
              Published: {ready ? settings[`pair${key}Version`] : 'Unavailable'}
            </span>
            {ready && (
              <span className='text-sm'>
                {componentStates[settings[`pair${key}State`]] || componentStates.unknown}
              </span>
            )}
            <button
              type='button'
              className='btn btn-secondary btn-sm self-start'
              disabled={component !== 'display' || !screenOtaRequest(settings, {}, submitting)}
              aria-describedby='ota-installation-notice'
              onClick={() => {
                if (component === 'display' && screenOtaRequest(settings, {}, submitting))
                  onScreenUpdate?.({});
              }}
            >
              Update {label}
            </button>
          </div>
        );
      })}
      {ready && (
        <div role='status' className='text-base-content/70 text-sm md:col-span-2'>
          {settings.updating
            ? 'Screen update in progress.'
            : compatibility[settings.pairCompatibility] || compatibility.unknown}
        </div>
      )}
      {(settings.displayUpdateAvailable || settings.controllerUpdateAvailable) && (
        <div role='note' className='alert alert-info md:col-span-2'>
          <p>
            Official GaggiMate {settings.latestVersion} is available. This is an upstream notice;
            compatibility with Knob Screen must be reviewed separately.
          </p>
        </div>
      )}
      {settings.screenOtaPhase && settings.screenOtaPhase !== 'idle' && (
        <div role='status' className='text-sm md:col-span-2'>
          Screen update: {otaStateText(settings.screenOtaPhase)} ({settings.screenOtaProgress || 0}
          %).
          {settings.screenOtaError && <span role='alert'> Error: {settings.screenOtaError}</span>}
          {settings.screenOtaVerified && <p>Image hash and flash readback verified.</p>}
          {settings.screenOtaPhase === 'rebooting' && (
            <p>Startup health: {settings.screenBootHealth || 'Waiting'}</p>
          )}
        </div>
      )}
      <div id='ota-installation-notice' role='note' className='alert alert-info md:col-span-2'>
        <div>
          <p className='font-semibold'>
            {settings.screenOtaInstallationAllowed === true
              ? 'Screen web updates'
              : 'Web installation not enabled'}
          </p>
          <p>{otaStateText(settings.screenOtaPreflight)}</p>
          {settings.screenOtaInstallationAllowed !== true && (
            <p>
              Use the K718 USB package for manual screen updates. This build keeps normal web
              installation disabled.
            </p>
          )}
          <p>Controller OTA remains disabled. Official notices do not authorize an installation.</p>
        </div>
      </div>
      {settings.screenOtaQualificationAvailable === true && (
        <details className='rounded-lg border p-4 md:col-span-2'>
          <summary className='cursor-pointer font-semibold'>Screen OTA bench tests</summary>
          <p className='my-3 text-sm'>
            Candidate-only qualification on a separate controller bench. Verify only writes the
            inactive screen slot without changing boot selection. Test install reboots into the
            verified image.
          </p>
          <div className='flex flex-wrap gap-2'>
            {[true, false].map(verifyOnly => (
              <button
                key={String(verifyOnly)}
                type='button'
                className='btn btn-outline btn-sm'
                disabled={
                  !screenOtaRequest(settings, { qualification: true, verifyOnly }, submitting)
                }
                onClick={() => {
                  const options = { qualification: true, verifyOnly };
                  if (screenOtaRequest(settings, options, submitting)) onScreenUpdate?.(options);
                }}
              >
                {verifyOnly ? 'Verify inactive image' : 'Test screen install'}
              </button>
            ))}
          </div>
        </details>
      )}
    </>
  );
}

export function ScreenUpdateConfirmation({ selection, settings, submitting, onCancel, onConfirm }) {
  if (!selection) return null;
  const current = selectionCurrent(settings, selection.request, submitting);
  return (
    <div
      role='dialog'
      aria-modal='true'
      aria-labelledby='screen-update-title'
      className='fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4'
    >
      <div className='bg-base-100 max-h-full w-full max-w-lg overflow-auto rounded-xl p-6 shadow-xl'>
        <h3 id='screen-update-title' className='mb-3 text-lg font-semibold'>
          {selection.request.verifyOnly ? 'Verify screen image' : 'Confirm screen update'}
        </h3>
        <p>
          Knob Screen {selection.request.tag}, based on official {selection.upstreamVersion}.
        </p>
        <p>
          Required controller: {selection.controllerVersion} · Protocol {selection.protocol}.
        </p>
        <p className='my-3 text-sm'>
          {selection.request.verifyOnly
            ? 'The inactive screen slot will be written and verified. No boot selection change or restart.'
            : 'The verified screen image will be installed and the screen will restart. Keep USB power connected.'}{' '}
          Controller firmware, pairing, settings and profiles are not written by this operation.
        </p>
        {selection.request.qualification && (
          <p className='text-warning'>Candidate bench test; not ordinary OTA qualification.</p>
        )}
        {!current && <p role='alert'>{otaStateText('selection-changed')}</p>}
        <div className='mt-4 flex justify-end gap-2'>
          <button type='button' className='btn btn-ghost' disabled={submitting} onClick={onCancel}>
            Cancel
          </button>
          <button
            type='button'
            className='btn btn-primary'
            disabled={!current}
            onClick={() => {
              if (current) onConfirm();
            }}
          >
            Confirm
          </button>
        </div>
      </div>
    </div>
  );
}

export function FirmwareVersions({ settings, submitting, onUpdate, onCheckPair, onScreenUpdate }) {
  if (settings.pairSupported)
    return (
      <MatchedVersions
        settings={settings}
        submitting={submitting}
        onCheckPair={onCheckPair}
        onScreenUpdate={onScreenUpdate}
      />
    );
  const blocked = settings.otaAllowed === false;
  return (
    <>
      {['controller', 'display'].map(component => {
        const label = component === 'controller' ? 'Controller' : 'Display';
        const allowed = canInstallOta(settings, component, submitting);
        return (
          <div key={component} className='flex flex-col space-y-2'>
            <span className='text-base-content/70 text-sm font-medium'>{label} Version</span>
            <div className='flex flex-wrap items-center gap-4'>
              <span className='text-base-content font-semibold break-all'>
                {settings[`${component}Version`]}
              </span>
              {settings[`${component}UpdateAvailable`] && (
                <span className='text-primary text-sm font-bold'>
                  Update available: {settings.latestVersion}
                </span>
              )}
              <button
                type='button'
                className='btn btn-secondary btn-sm'
                disabled={!allowed}
                aria-describedby={blocked ? 'ota-installation-notice' : undefined}
                onClick={() => {
                  if (allowed) onUpdate(component);
                }}
              >
                Update {label}
              </button>
            </div>
          </div>
        );
      })}
      {blocked ? (
        <div id='ota-installation-notice' role='note' className='alert alert-info md:col-span-2'>
          <div>
            <p className='font-semibold'>USB installation required</p>
            <p>
              {settings.otaDisabledReason ||
                'Wireless installation is disabled. Official update checks and notifications remain enabled. Use USB and a compatible firmware build to update.'}
            </p>
          </div>
        </div>
      ) : (
        <div className='alert alert-warning md:col-span-2'>
          <span>
            Make sure to backup your profiles from the profiles screen before updating the display.
          </span>
        </div>
      )}
    </>
  );
}

export function OfficialUpdateChannel({ settings, submitting, formRef, onSubmit }) {
  const noticesOnly = Boolean(settings.pairSupported);
  const busy = submitting || (noticesOnly && Boolean(settings.updating));
  const form = (
    <form ref={formRef} onSubmit={onSubmit} className='space-y-4'>
      <div className='form-control max-w-md'>
        <label htmlFor='channel' className='mb-2 block text-sm font-medium'>
          {noticesOnly ? 'Upstream notification channel' : 'Update Channel'}
        </label>
        <div className='flex w-full items-center gap-2'>
          <select
            id='channel'
            name='channel'
            className='select select-bordered grow'
            disabled={noticesOnly && busy}
          >
            <option value='latest' selected={settings.channel === 'latest'}>
              Stable
            </option>
            <option value='nightly' selected={settings.channel === 'nightly'}>
              Nightly
            </option>
          </select>
          <button type='submit' className='btn btn-secondary' disabled={busy}>
            {noticesOnly ? 'Save notice channel & refresh' : 'Save Channel & Refresh'}
          </button>
        </div>
      </div>
    </form>
  );
  if (!noticesOnly) return form;
  return (
    <details className='border-base-content/10 rounded-lg border p-4'>
      <summary className='cursor-pointer font-semibold'>Official GaggiMate update notices</summary>
      <div className='mt-4 space-y-3'>
        <p className='text-base-content/70 text-sm'>
          Stable/Nightly selects official GaggiMate notifications only. Knob Screen firmware is
          checked separately using the matched releases above. Changing this channel does not enable
          installation or controller OTA.
        </p>
        {settings.officialCheckStatus && (
          <p role='status' className='text-sm'>
            Official check: {settings.officialCheckStatus}. {settings.officialCheckMessage || ''}
          </p>
        )}
        {form}
      </div>
    </details>
  );
}
