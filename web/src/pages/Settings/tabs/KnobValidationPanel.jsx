import { useContext, useEffect, useState } from 'preact/hooks';
import { ApiServiceContext } from '../../../services/ApiService.js';
import { screenOtaRequest, otaStateText } from '../../../utils/screenOta.js';
import { ScreenUpdateConfirmation, ScreenUpdateProgress } from './FirmwareVersions.jsx';
import { useScreenUpdate } from './useScreenUpdate.js';

export function ReplayControls({ snapshot = {}, busy, onAction }) {
  return (
    <div className='space-y-3'>
      <p className='font-semibold'>Local data replay — no machine connection</p>
      <p className='text-sm'>
        Synthetic traces feed the real display protocol and UI. Commands terminate locally; process
        starts, history writes and Bluetooth pairing are unavailable.
      </p>
      <p className='text-sm'>
        {snapshot.trace || 'idle'} · {((snapshot.positionMs || 0) / 1000).toFixed(1)} s ·
        {snapshot.connected ? ' Simulated peer connected' : ' Disconnected'} · Commands captured:{' '}
        {snapshot.commandsTerminated || 0}
      </p>
      <div className='flex flex-wrap gap-2'>
        {['idle', 'warmup', 'shot'].map(trace => (
          <button
            key={trace}
            type='button'
            className='btn btn-outline btn-sm'
            disabled={busy}
            onClick={() => {
              if (!busy) onAction('play', trace);
            }}
          >
            Replay {trace}
          </button>
        ))}
        {['pause', 'reset', 'disconnect'].map(action => (
          <button
            key={action}
            type='button'
            className='btn btn-outline btn-sm'
            disabled={busy}
            onClick={() => {
              if (!busy) onAction(action);
            }}
          >
            {action}
          </button>
        ))}
      </div>
      <p className='text-sm'>
        Disconnect replay before screen-only OTA tests. Simulated values do not qualify BLE or
        brewing.
      </p>
    </div>
  );
}
function ReplayPanel({ updating }) {
  const [snapshot, setSnapshot] = useState({});
  const [sending, setSending] = useState(false);
  const [error, setError] = useState('');
  useEffect(() => {
    let active = true;
    const refresh = async () => {
      try {
        const response = await fetch('/api/validation');
        if (!response.ok) throw new Error('Replay status unavailable');
        const data = await response.json();
        if (active) setSnapshot(data);
      } catch (e) {
        if (active) setError(e.message);
      }
    };
    refresh();
    const timer = setInterval(refresh, 2000);
    return () => {
      active = false;
      clearInterval(timer);
    };
  }, []);
  const action = async (name, trace = 'idle') => {
    if (updating || sending) return;
    setSending(true);
    setError('');
    try {
      const response = await fetch('/api/validation', {
        method: 'POST',
        body: new URLSearchParams({ action: name, trace }),
      });
      if (!response.ok) throw new Error('Replay request refused; check update/status and retry.');
      setSnapshot(await response.json());
    } catch (e) {
      setError(e.message);
    } finally {
      setSending(false);
    }
  };
  return (
    <div className='border-warning space-y-3 rounded-lg border p-4 md:col-span-2'>
      <ReplayControls snapshot={snapshot} busy={updating || sending} onAction={action} />
      {error && <p role='alert'>{error}</p>}
    </div>
  );
}

export function IsolatedScreenOta({ settings, submitting, onCheck, onUpdate }) {
  if (
    settings.validationBuild !== true ||
    settings.screenOtaStandaloneQualificationAvailable !== true
  )
    return null;
  return (
    <div className='border-warning space-y-3 rounded-lg border p-4'>
      <p className='font-semibold'>Isolated screen OTA qualification</p>
      <p className='text-sm'>
        Disconnect replay, enter standby and keep USB recovery available before testing an update.
      </p>
      <p className='text-sm'>{otaStateText(settings.screenOtaStandalonePreflight)}</p>
      <div className='flex flex-wrap gap-2'>
        <button
          type='button'
          className='btn btn-outline btn-sm'
          disabled={settings.pairStatus === 'checking' || settings.updating || submitting}
          onClick={onCheck}
        >
          Check matched releases
        </button>
        {[true, false].map(verifyOnly => {
          const options = { qualification: true, screenOnly: true, verifyOnly };
          const allowed = screenOtaRequest(settings, options, submitting);
          return (
            <button
              key={String(verifyOnly)}
              type='button'
              className='btn btn-outline btn-sm'
              disabled={!allowed}
              onClick={() => {
                if (allowed) onUpdate(options);
              }}
            >
              {verifyOnly ? 'Verify screen image only' : 'Test-install screen image'}
            </button>
          );
        })}
      </div>
      <ScreenUpdateProgress settings={settings} />
    </div>
  );
}

export default function KnobValidationPanel() {
  const apiService = useContext(ApiServiceContext);
  const [settings, setSettings] = useState({});
  const {
    screenSelection,
    screenMessage,
    screenSubmitting,
    prepareScreenUpdate,
    confirmScreenUpdate,
    cancelScreenUpdate,
  } = useScreenUpdate(apiService, settings);
  useEffect(() => {
    const listener = apiService.on('res:ota-settings', setSettings);
    const refresh = () => {
      try {
        apiService.send({ tp: 'req:ota-settings' });
      } catch {
        // Poll again after the WebSocket reconnects.
      }
    };
    refresh();
    const timer = setInterval(refresh, settings.updating ? 2000 : 5000);
    return () => {
      clearInterval(timer);
      apiService.off('res:ota-settings', listener);
    };
  }, [apiService, settings.updating]);
  return (
    <div className='mt-6 space-y-4'>
      <ReplayPanel updating={Boolean(settings.updating) || screenSubmitting} />
      <IsolatedScreenOta
        settings={settings}
        submitting={screenSubmitting}
        onCheck={() => apiService.send({ tp: 'req:ota-settings', refreshPair: true })}
        onUpdate={prepareScreenUpdate}
      />
      {screenMessage && (
        <p role='status' className='text-sm'>
          {screenMessage}
        </p>
      )}
      <ScreenUpdateConfirmation
        selection={screenSelection}
        settings={settings}
        submitting={screenSubmitting}
        onCancel={cancelScreenUpdate}
        onConfirm={confirmScreenUpdate}
      />
    </div>
  );
}
