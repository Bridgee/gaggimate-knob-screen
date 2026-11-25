import { useRef, useState } from 'preact/hooks';
import { screenOtaRequest, selectionCurrent, otaStateText } from '../../../utils/screenOta.js';

export function useScreenUpdate(apiService, formData) {
  const [screenSelection, setScreenSelection] = useState(null);
  const [screenMessage, setScreenMessage] = useState('');
  const [screenSubmitting, setScreenSubmitting] = useState(false);
  const screenRequestPending = useRef(false);
  const liveOtaSettings = useRef(formData);
  liveOtaSettings.current = formData;
  const prepareScreenUpdate = options => {
    const request = screenOtaRequest(formData, options, screenRequestPending.current);
    if (!request) return;
    setScreenMessage('');
    setScreenSelection({
      request,
      upstreamVersion: formData.pairUpstreamVersion,
      controllerVersion: formData.pairControllerVersion,
      protocol: formData.pairProtocol,
    });
  };
  const confirmScreenUpdate = async () => {
    if (screenRequestPending.current) return;
    if (!selectionCurrent(liveOtaSettings.current, screenSelection?.request)) {
      setScreenMessage(otaStateText('selection-changed'));
      return;
    }
    const request = screenSelection.request;
    screenRequestPending.current = true;
    setScreenSubmitting(true);
    try {
      const result = await apiService.request(request);
      setScreenSelection(null);
      setScreenMessage(result.accepted ? otaStateText('queued') : otaStateText(result.state));
    } catch {
      // A missing reply does not prove that staging failed. Never auto-resubmit.
      setScreenSelection(null);
      setScreenMessage(
        'The update reply was interrupted. Reconnect and check screen status before retrying.',
      );
    } finally {
      screenRequestPending.current = false;
      setScreenSubmitting(false);
      try {
        apiService.send({ tp: 'req:ota-settings' });
      } catch {
        /* The installed screen may be restarting. */
      }
    }
  };

  return {
    screenSelection,
    screenMessage,
    screenSubmitting,
    prepareScreenUpdate,
    confirmScreenUpdate,
    cancelScreenUpdate: () => setScreenSelection(null),
  };
}
