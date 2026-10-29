export function canInstallOta(settings, component, submitting = false) {
  return (
    (component === 'display' || component === 'controller') &&
    settings.otaAllowed !== false &&
    Boolean(settings[`${component}UpdateAvailable`]) &&
    !submitting
  );
}

export function FirmwareVersions({ settings, submitting, onUpdate }) {
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
