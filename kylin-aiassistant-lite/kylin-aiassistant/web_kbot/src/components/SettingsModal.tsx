import { useEffect } from 'react';
import { X, Settings, Sun, Moon, Monitor, Laptop, Check } from 'lucide-react';
import { useTheme } from '@/hooks/useTheme';
import { t } from '@/lib/i18n';
import type { AccentColor, ThemeMode } from '@/contexts/ThemeContextDef';

const themeOptions: { value: ThemeMode; icon: typeof Sun; labelKey: string }[] = [
  { value: 'system', icon: Laptop, labelKey: 'theme.system' },
  { value: 'dark', icon: Moon, labelKey: 'theme.dark' },
  { value: 'light', icon: Sun, labelKey: 'theme.light' },
  { value: 'oled', icon: Monitor, labelKey: 'theme.oled' },
];

const accentOptions: { value: AccentColor; color: string }[] = [
  { value: 'cyan', color: '#22d3ee' },
  { value: 'violet', color: '#8b5cf6' },
  { value: 'emerald', color: '#10b981' },
  { value: 'amber', color: '#f59e0b' },
  { value: 'rose', color: '#f43f5e' },
  { value: 'blue', color: '#3b82f6' },
];

interface Props {
  open: boolean;
  onClose: () => void;
}

export function SettingsModal({ open, onClose }: Props) {
  const {
    theme, accent,
    setTheme, setAccent,
  } = useTheme();

  useEffect(() => {
    if (!open) return;
    const handler = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose();
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [open, onClose]);

  if (!open) return null;

  return (
    <div
      role="dialog"
      aria-modal="true"
      aria-label={t('settings.theme_title')}
      className="fixed inset-0 z-50 flex items-center justify-center"
      onClick={onClose}
    >
      <div className="absolute inset-0" style={{ background: 'rgba(0,0,0,0.6)', backdropFilter: 'blur(8px)' }} />
      <div
        className="relative w-full max-w-2xl mx-4 rounded-3xl border shadow-2xl animate-fade-in"
        style={{ background: 'var(--pc-bg-base)', borderColor: 'var(--pc-border)' }}
        onClick={(e) => e.stopPropagation()}
      >
        <div
          className="flex items-center justify-between px-6 py-4 border-b"
          style={{ borderColor: 'var(--pc-border)' }}
        >
          <div className="flex items-center gap-2.5">
            <Settings size={18} style={{ color: 'var(--pc-accent-light)' }} />
            <h2 className="text-sm font-semibold" style={{ color: 'var(--pc-text-primary)' }}>{t('settings.theme_title')}</h2>
          </div>
          <button
            onClick={onClose}
            className="h-8 w-8 rounded-xl flex items-center justify-center transition-colors"
            style={{ color: 'var(--pc-text-muted)', background: 'transparent', border: 'none', cursor: 'default' }}
            onMouseEnter={(e) => { e.currentTarget.style.color = 'var(--pc-text-primary)'; e.currentTarget.style.background = 'var(--pc-hover)'; }}
            onMouseLeave={(e) => { e.currentTarget.style.color = 'var(--pc-text-muted)'; e.currentTarget.style.background = 'transparent'; }}
            aria-label="Close"
          >
            <X size={16} />
          </button>
        </div>

        <div className="px-6 py-4 max-h-[65vh] overflow-y-auto space-y-4">
          <div>
            <div className="text-xs mb-2" style={{ color: 'var(--pc-text-secondary)' }}>{t('theme.mode')}</div>
            <div className="flex gap-1.5">
              {themeOptions.map(opt => {
                const Icon = opt.icon;
                const active = theme === opt.value;
                return (
                  <button
                    key={opt.value}
                    onClick={() => setTheme(opt.value)}
                    aria-pressed={active}
                    className="flex-1 flex flex-col items-center gap-1 py-2 rounded-xl border text-xs transition-all"
                    style={active
                      ? { borderColor: 'var(--pc-accent-dim)', background: 'var(--pc-accent-glow)', color: 'var(--pc-accent-light)' }
                      : { borderColor: 'var(--pc-border)', color: 'var(--pc-text-muted)', background: 'transparent' }
                    }
                    onMouseEnter={(e) => { if (!active) e.currentTarget.style.background = 'var(--pc-hover)'; }}
                    onMouseLeave={(e) => { if (!active) e.currentTarget.style.background = 'transparent'; }}
                  >
                    <Icon size={16} />
                    <span>{t(opt.labelKey)}</span>
                  </button>
                );
              })}
            </div>
          </div>

          <div>
            <div className="text-xs mb-2" style={{ color: 'var(--pc-text-secondary)' }}>{t('theme.accent')}</div>
            <div className="flex gap-2">
              {accentOptions.map(opt => (
                <button
                  key={opt.value}
                  onClick={() => setAccent(opt.value)}
                  className="relative h-7 w-7 rounded-full transition-all flex items-center justify-center"
                  style={{
                    backgroundColor: opt.color,
                    border: accent === opt.value ? `2px solid ${opt.color}` : '2px solid transparent',
                    boxShadow: accent === opt.value ? `0 0 8px ${opt.color}40` : 'none',
                  }}
                  aria-pressed={accent === opt.value}
                  aria-label={`${opt.value} accent`}
                >
                  {accent === opt.value && <Check size={14} style={{ color: 'white' }} />}
                </button>
              ))}
            </div>
          </div>

        </div>
      </div>
    </div>
  );
}
