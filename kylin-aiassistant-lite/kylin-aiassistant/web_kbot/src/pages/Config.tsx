import {
  Save,
  CheckCircle,
  AlertTriangle,
} from 'lucide-react';
import { t } from '@/lib/i18n';
import { useConfigState } from './config/useConfigState';
import ConfigFormView from './config/ConfigFormView';

export default function Config() {
  const {
    parsedConfig,
    loading,
    saving,
    error,
    success,
    parseError,
    updateField,
    save,
  } = useConfigState();

  if (loading) {
    return (
      <div className="flex items-center justify-center h-64">
        <div className="h-8 w-8 border-2 rounded-full animate-spin" style={{ borderColor: 'var(--pc-border)', borderTopColor: 'var(--pc-accent)' }} />
      </div>
    );
  }

  return (
    <div className="flex h-full flex-col gap-2 overflow-hidden px-6 pb-6 pt-0 animate-fade-in">
      <div className="flex justify-end flex-shrink-0">
        <button
          type="button"
          onClick={() => void save()}
          disabled={saving}
          className="flex items-center gap-2 rounded-xl border-0 px-4 py-2 text-sm font-medium text-white transition-opacity bg-[#3790FA] hover:opacity-90 disabled:cursor-not-allowed disabled:opacity-40"
        >
          <Save className="h-4 w-4" />{saving ? t('config.saving') : t('config.save')}
        </button>
      </div>

      {/* Success message */}
      {success && (
        <div className="flex items-center gap-2 rounded-xl p-3 border animate-fade-in flex-shrink-0" style={{ borderColor: 'rgba(0, 230, 138, 0.2)', background: 'rgba(0, 230, 138, 0.06)' }}>
          <CheckCircle className="h-4 w-4 flex-shrink-0" style={{ color: 'var(--color-status-success)' }} />
          <span className="text-sm" style={{ color: 'var(--color-status-success)' }}>{success}</span>
        </div>
      )}

      {/* Error / parse error message */}
      {(error || parseError) && (
        <div className="flex items-center gap-2 rounded-xl p-3 border animate-fade-in flex-shrink-0" style={{ borderColor: 'rgba(239, 68, 68, 0.2)', background: 'rgba(239, 68, 68, 0.06)' }}>
          <AlertTriangle className="h-4 w-4 flex-shrink-0" style={{ color: 'var(--color-status-error)' }} />
          <span className="text-sm" style={{ color: 'var(--color-status-error)' }}>{error || parseError}</span>
        </div>
      )}

      <ConfigFormView config={parsedConfig} onUpdate={updateField} />
    </div>
  );
}
