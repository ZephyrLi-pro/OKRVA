import { useEffect, useState } from 'react';
import { t } from '@/lib/i18n';
import {
  isCurrentWindowAlwaysOnTop,
  toggleAlwaysOnTopCurrentWindow,
} from '@/lib/tauri';

interface CloseActionSettings {
  alwaysOnTop: boolean;
  closeAction: 'hide' | 'exit';
}

const CLOSE_ACTION_KEY = 'kylinbot_close_action';

function getStoredCloseAction(): 'hide' | 'exit' {
  try {
    const stored = localStorage.getItem(CLOSE_ACTION_KEY);
    if (stored === 'hide' || stored === 'exit') {
      return stored;
    }
  } catch {}
  return 'exit';
}

function setStoredCloseAction(action: 'hide' | 'exit') {
  try {
    localStorage.setItem(CLOSE_ACTION_KEY, action);
  } catch {}
}

export default function General() {
  const [settings, setSettings] = useState<CloseActionSettings>({
    alwaysOnTop: false,
    closeAction: getStoredCloseAction(),
  });

  useEffect(() => {
    void isCurrentWindowAlwaysOnTop().then((value) => {
      setSettings((prev) => ({ ...prev, alwaysOnTop: value }));
    });
  }, []);

  const handleCloseActionChange = (action: 'hide' | 'exit') => {
    setSettings((prev) => ({ ...prev, closeAction: action }));
    setStoredCloseAction(action);
  };

  const toggleAlwaysOnTop = async () => {
    const newValue = !settings.alwaysOnTop;
    setSettings((prev) => ({ ...prev, alwaysOnTop: newValue }));
    await toggleAlwaysOnTopCurrentWindow();
  };

  return (
    <div className="p-6 animate-fade-in">
      <h2 className="mb-4 text-sm font-semibold text-black">{t('settings.window')}</h2>

      <div className="space-y-4">
        {/* 隐藏窗口置顶UI */}
        <div style={{ display: 'none' }}>
          <div
            className="h-[60px] rounded-[6px] px-4 flex items-center"
            style={{ background: '#F6F6F6' }}
          >
            <div className="flex items-center justify-between w-full">
              <div className="flex items-center gap-3">
                <span className="text-sm font-medium" style={{ color: '#222222' }}>
                  {t('modelswitch.always_on_top')}
                </span>
              </div>
              <button
                onClick={toggleAlwaysOnTop}
                className="relative w-10 h-6 rounded-full transition-colors"
                style={{
                  background: settings.alwaysOnTop ? '#3790FA' : '#DFDFDF',
                }}
                aria-pressed={settings.alwaysOnTop}
                aria-label={t('modelswitch.always_on_top')}
              >
                <span
                  className="absolute top-1 left-1 w-4 h-4 rounded-full bg-white transition-transform"
                  style={{
                    transform: settings.alwaysOnTop ? 'translateX(16px)' : 'translateX(0)',
                  }}
                />
              </button>
            </div>
          </div>
        </div>

        <div
          className="h-[60px] rounded-[6px] px-4 flex items-center"
          style={{ background: '#F6F6F6' }}
        >
          <div className="flex flex-wrap items-center justify-between gap-3 w-full">
            <span className="text-sm font-medium" style={{ color: '#222222' }}>
              {t('modelswitch.close_action')}
            </span>

            <div className="flex items-center gap-6">
              <label className="flex items-center gap-2 select-none">
                <input
                  type="radio"
                  name="closeAction"
                  checked={settings.closeAction === 'hide'}
                  onChange={() => handleCloseActionChange('hide')}
                  className="h-4 w-4"
                  style={{ accentColor: '#3790FA' }}
                />
                <span className="text-sm" style={{ color: '#444444' }}>
                  {t('modelswitch.close_action_hide')}
                </span>
              </label>
              <label className="flex items-center gap-2 select-none">
                <input
                  type="radio"
                  name="closeAction"
                  checked={settings.closeAction === 'exit'}
                  onChange={() => handleCloseActionChange('exit')}
                  className="h-4 w-4"
                  style={{ accentColor: '#3790FA' }}
                />
                <span className="text-sm" style={{ color: '#444444' }}>
                  {t('modelswitch.close_action_exit')}
                </span>
              </label>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}