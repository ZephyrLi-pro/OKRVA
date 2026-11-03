import SettingsSection from '../controls/SettingsSection';
import FieldRow from '../controls/FieldRow';
import Select from '../controls/Select';
import Toggle from '../controls/Toggle';
import { t } from '@/lib/i18n';

interface Props {
  config: Record<string, unknown>;
  onUpdate: (field: string, value: unknown) => void;
}

const BACKEND_OPTIONS = [
  { value: 'sqlite', label: 'SQLite' },
  { value: 'markdown', label: 'Markdown' },
  { value: 'embeddings', label: 'Embeddings' },
  { value: 'hybrid', label: 'Hybrid' },
];

export default function MemorySection({ config, onUpdate }: Props) {
  const memory = (config.memory as Record<string, unknown>) ?? {};

  return (
    <SettingsSection title={t('config.section.memory')}>
      <FieldRow label={t('config.field.memory_enabled')} description={t('config.field.memory_enabled.desc')}>
        <Toggle
          value={(memory.enabled as boolean) ?? true}
          onChange={(v) => onUpdate('memory.enabled', v)}
        />
      </FieldRow>
      <FieldRow label={t('config.field.memory_backend')} description={t('config.field.memory_backend.desc')}>
        <Select
          value={(memory.backend as string) ?? 'sqlite'}
          onChange={(v) => onUpdate('memory.backend', v)}
          options={BACKEND_OPTIONS}
        />
      </FieldRow>
    </SettingsSection>
  );
}
