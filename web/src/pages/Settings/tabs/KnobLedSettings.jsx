import Section from '../../../components/Card.jsx';
import { SettingsFormField, ToggleField } from '../../../components/SettingsFormField.jsx';

export function KnobLedSettings({ formData, onChange }) {
  if (!formData.knobLedSupported) return null;
  const brightness = Number(formData.knobLedBrightness ?? 45);
  return (
    <Section title='LED Ring' className='h-full'>
      <div className='space-y-5'>
        <ToggleField
          label='Enable LED ring'
          htmlFor='knobLedEnabled'
          checked={!!formData.knobLedEnabled}
          onChange={onChange('knobLedEnabled')}
        />
        <SettingsFormField label='Brightness' htmlFor='knobLedBrightness' noMargin>
          <div className='flex items-center gap-4'>
            <input
              id='knobLedBrightness'
              name='knobLedBrightness'
              type='range'
              className='range range-primary grow'
              min='0'
              max='100'
              step='1'
              value={brightness}
              aria-valuetext={`${brightness}%`}
              aria-describedby='knob-led-help'
              disabled={!formData.knobLedEnabled}
              onInput={onChange('knobLedBrightness')}
            />
            <output htmlFor='knobLedBrightness' className='w-12 text-right tabular-nums'>
              {brightness}%
            </output>
          </div>
        </SettingsFormField>
        <p id='knob-led-help' className='text-base-content/70 text-sm'>
          Follows the screen’s temperature and pressure bars. The ring is off in standby or when the
          controller is disconnected. Changes apply after saving and are restored after a restart.
        </p>
      </div>
    </Section>
  );
}
