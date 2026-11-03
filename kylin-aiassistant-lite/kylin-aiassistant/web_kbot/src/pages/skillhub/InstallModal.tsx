import { useState } from 'react';
import { Loader2, Check, X } from 'lucide-react';
import { installSkill } from '@/lib/skillhubApi';
import { t } from '@/lib/i18n';

interface InstallModalProps {
  slug: string;
  name: string;
  isOpen: boolean;
  onClose: () => void;
  onSuccess: () => void;
}

type InstallState = 'confirm' | 'installing' | 'success' | 'error';

export default function InstallModal({
  slug,
  name,
  isOpen,
  onClose,
  onSuccess,
}: InstallModalProps) {
  const [state, setState] = useState<InstallState>('confirm');
  const [force, setForce] = useState(false);
  const [errorMsg, setErrorMsg] = useState('');

  if (!isOpen) return null;

  const handleInstall = async () => {
    setState('installing');
    setErrorMsg('');
    try {
      const source = slug.includes(':') ? slug : `clawhub:${slug}`;
      const result = await installSkill(source, force, 60);
      if (result.success) {
        setState('success');
        onSuccess();
        setTimeout(() => {
          onClose();
          setState('confirm');
        }, 2000);
      } else {
        setState('error');
        setErrorMsg(result.message);
      }
    } catch (err) {
      setState('error');
      setErrorMsg(err instanceof Error ? err.message : t('skillhub.modal.error'));
    }
  };

  const handleClose = () => {
    if (state !== 'installing') {
      onClose();
      setState('confirm');
      setErrorMsg('');
    }
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center px-4" onClick={handleClose}>
      <div className="absolute inset-0" style={{ background: 'rgba(0,0,0,0.5)' }} />
      <div
        className="relative surface-panel p-6 w-full max-w-md rounded-2xl animate-fade-in-scale"
        onClick={(e) => e.stopPropagation()}
      >
        <button
          onClick={handleClose}
          disabled={state === 'installing'}
          className="absolute top-4 right-4 p-1 rounded-lg transition-colors"
          style={{ color: 'var(--pc-text-muted)' }}
          onMouseEnter={(e) => {
            e.currentTarget.style.color = 'var(--pc-text-primary)';
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.color = 'var(--pc-text-muted)';
          }}
        >
          <X className="h-4 w-4" />
        </button>

        <h3 className="text-lg font-semibold mb-4" style={{ color: 'var(--pc-text-primary)' }}>
          {t('skillhub.modal.install_title')}
        </h3>

        {state === 'confirm' && (
          <>
            <p className="text-sm mb-4" style={{ color: 'var(--pc-text-muted)' }}>
              {t('skillhub.modal.install_confirm').replace('{name}', name || slug)}
            </p>
            <label className="flex items-center gap-2 mb-6 cursor-pointer">
              <input
                type="checkbox"
                checked={force}
                onChange={(e) => setForce(e.target.checked)}
                className="rounded"
              />
              <span className="text-xs" style={{ color: 'var(--pc-text-muted)' }}>
                {t('skillhub.modal.force_overwrite')}
              </span>
            </label>
            <div className="flex justify-end gap-3">
              <button
                onClick={handleClose}
                className="px-4 py-2 rounded-lg text-sm font-medium transition-colors"
                style={{ color: 'var(--pc-text-muted)', border: '1px solid var(--pc-border)' }}
              >
                {t('skillhub.modal.cancel')}
              </button>
              <button onClick={handleInstall} className="btn-electric px-4 py-2 text-sm font-medium">
                {t('skillhub.modal.install_btn')}
              </button>
            </div>
          </>
        )}

        {state === 'installing' && (
          <div className="flex flex-col items-center gap-3 py-4">
            <Loader2 className="h-8 w-8 animate-spin" style={{ color: 'var(--pc-accent)' }} />
            <p className="text-sm" style={{ color: 'var(--pc-text-muted)' }}>
              {t('skillhub.modal.installing').replace('{name}', name || slug)}
            </p>
          </div>
        )}

        {state === 'success' && (
          <div className="flex flex-col items-center gap-3 py-4">
            <Check className="h-8 w-8" style={{ color: 'var(--color-status-success)' }} />
            <p className="text-sm" style={{ color: 'var(--color-status-success)' }}>
              {t('skillhub.modal.success').replace('{name}', name || slug)}
            </p>
          </div>
        )}

        {state === 'error' && (
          <>
            <div className="flex flex-col items-center gap-3 py-4">
              <X className="h-8 w-8" style={{ color: 'var(--color-status-error)' }} />
              <p className="text-sm text-center" style={{ color: 'var(--color-status-error)' }}>
                {t('skillhub.modal.error')}
              </p>
              <p
                className="text-xs text-center max-w-full break-words"
                style={{ color: 'var(--pc-text-muted)' }}
              >
                {errorMsg}
              </p>
            </div>
            <div className="flex justify-end gap-3">
              <button
                onClick={handleClose}
                className="px-4 py-2 rounded-lg text-sm font-medium transition-colors"
                style={{ color: 'var(--pc-text-muted)', border: '1px solid var(--pc-border)' }}
              >
                {t('skillhub.modal.close')}
              </button>
              <button
                onClick={handleInstall}
                className="btn-electric px-4 py-2 text-sm font-medium"
              >
                {t('skillhub.modal.retry')}
              </button>
            </div>
          </>
        )}
      </div>
    </div>
  );
}
