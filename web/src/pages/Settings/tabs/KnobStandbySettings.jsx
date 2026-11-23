import { SettingsFormField } from '../../../components/SettingsFormField.jsx';
export function KnobStandbySettings({ formData, onChange }) {
  return (
    <div className='grid grid-cols-1 gap-4 md:grid-cols-2'>
      <SettingsFormField label='Standby Content' htmlFor='standbyContent' noMargin>
        <select
          id='standbyContent'
          name='standbyContent'
          className='select select-bordered w-full'
          value={formData.standbyContent ?? 'logo'}
          onChange={onChange('standbyContent')}
        >
          <option value='logo'>GaggiMate logo (default)</option>
          <option value='text'>Custom text</option>
          <option value='clock'>Centered clock only</option>
        </select>
      </SettingsFormField>
      <SettingsFormField
        label='Standby Text'
        htmlFor='standbyText'
        helpText='Up to 24 English letters, numbers, spaces or punctuation. Changes apply after saving.'
        noMargin
      >
        <input
          id='standbyText'
          name='standbyText'
          type='text'
          maxLength={24}
          pattern='[\x20-\x7E]*'
          className='input input-bordered w-full'
          value={formData.standbyText ?? 'GaggiMate'}
          onChange={onChange('standbyText')}
          disabled={(formData.standbyContent ?? 'logo') !== 'text'}
        />
      </SettingsFormField>
    </div>
  );
}
