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

export function MatchedVersions({ settings, onCheckPair }) {
  const ready = settings.pairStatus === 'ready';
  const checking = settings.pairStatus === 'checking';
  const compatibility = {
    matched: 'Target, protocol and controller version match. OTA installation is unverified.',
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
            disabled={checking || Boolean(settings.updating)}
            onClick={onCheckPair}
          >
            {checking ? 'Checking…' : 'Check matched releases'}
          </button>
        </div>
        {ready ? (
          <div className='flex flex-wrap items-center gap-2'>
            <a href={settings.pairReleaseUrl} target='_blank' rel='noreferrer' className='link'>
              {settings.pairReleaseTag}
            </a>
            {settings.pairPrerelease && <span className='badge badge-warning'>Pre-release</span>}
            <span className='text-base-content/70 text-sm'>Protocol {settings.pairProtocol}</span>
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
              disabled
              aria-describedby='ota-installation-notice'
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
          Screen update test: {settings.screenOtaPhase} ({settings.screenOtaProgress || 0}%).
          {settings.screenOtaError && ` ${settings.screenOtaError}`}
        </div>
      )}
      <div id='ota-installation-notice' role='note' className='alert alert-info md:col-span-2'>
        <div>
          <p className='font-semibold'>USB installation required</p>
          <p>
            Matched release checks are read-only. Normal display and controller updates remain
            disabled while screen OTA is being tested.
          </p>
        </div>
      </div>
    </>
  );
}

export function FirmwareVersions({ settings, submitting, onUpdate, onCheckPair }) {
  if (settings.pairSupported)
    return <MatchedVersions settings={settings} onCheckPair={onCheckPair} />;
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
