import { useEffect, useState } from 'react';
import { useLocation } from 'react-router-dom';
import { Menu } from 'lucide-react';
import { t } from '@/lib/i18n';
import { SettingsModal } from '@/components/SettingsModal';
import { useTheme } from '@/hooks/useTheme';
import { assetBasePath } from '@/lib/basePath';
import {
  handleCloseButtonClick,
  isCurrentWindowAlwaysOnTop,
  minimizeCurrentWindow,
  startCurrentWindowDragging,
  toggleAlwaysOnTopCurrentWindow,
  toggleMaximizeCurrentWindow,
} from '@/lib/tauri';

interface HeaderProps {
  /** Reserved for a mobile nav drawer; optional when no sidebar is mounted. */
  onMenuToggle?: () => void;
  onNewAgentSession?: () => void;
}

export default function Header({
  onMenuToggle = () => {},
  onNewAgentSession,
}: HeaderProps) {
  const location = useLocation();
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [alwaysOnTop, setAlwaysOnTop] = useState(false);
  const { resolvedTheme } = useTheme();
  const isLight = resolvedTheme === 'light';
  const isAgentPage = location.pathname === '/agent';

  useEffect(() => {
    let cancelled = false;
    void isCurrentWindowAlwaysOnTop().then((value) => {
      if (!cancelled) {
        setAlwaysOnTop(value);
      }
    });
    return () => {
      cancelled = true;
    };
  }, []);

  return (
    <>
      <header
        data-testid="app-header"
        data-header-mode="default"
        className="h-10 flex items-center justify-between animate-fade-in px-2"
        style={{
          background: 'transparent',
        }}
      >
        <div className="flex items-center gap-2.5">
          {/* 隐藏三联菜单按钮 */}
          <div style={{ display: 'none' }}>
            <button
              type="button"
              onClick={onMenuToggle}
              className="md:hidden p-1.5 -ml-1.5 rounded-lg transition-colors duration-200"
              style={{ color: 'var(--pc-text-muted)' }}
              onMouseEnter={(e) => { e.currentTarget.style.color = 'var(--pc-text-primary)'; e.currentTarget.style.background = 'var(--pc-hover)'; }}
              onMouseLeave={(e) => { e.currentTarget.style.color = 'var(--pc-text-muted)'; e.currentTarget.style.background = 'transparent'; }}
              aria-label="Open menu"
            >
              <Menu className="h-5 w-5" />
            </button>
          </div>

          {isAgentPage && (
            <button
              type="button"
              data-testid="header-new-session"
              className="flex h-9 w-9 shrink-0 items-center justify-center rounded-lg transition-colors"
              style={{ color: 'var(--pc-text-muted)', background: 'transparent', border: 'none', cursor: 'default' }}
              onMouseEnter={(e) => { e.currentTarget.style.background = 'var(--pc-hover)'; }}
              onMouseLeave={(e) => { e.currentTarget.style.background = 'transparent'; }}
              aria-label={t('header.new_session')}
              title={t('header.new_session')}
              onClick={() => onNewAgentSession?.()}
            >
              <HeaderMaskIcon fileName="new-chat.svg" sizeClass="h-4 w-4" />
            </button>
          )}
        </div>

        <div
          data-tauri-drag-region
          className="hidden md:block h-8 flex-1"
          onMouseDown={(event) => {
            if (event.button !== 0) return;
            void startCurrentWindowDragging();
          }}
        />

        <div className={`flex items-center ${isLight ? 'gap-1.5 h-8' : 'gap-2 h-8'}`}>
          {/* 隐藏主题设置按钮 */}
          {/* 
          <button
            type="button"
            onClick={() => setSettingsOpen(true)}
            className="flex h-9 w-9 shrink-0 items-center justify-center rounded-xl text-xs transition-all"
            style={{ color: 'var(--pc-text-muted)', background: 'transparent', border: 'none', cursor: 'default' }}
            onMouseEnter={(e) => { e.currentTarget.style.color = 'var(--pc-text-primary)'; e.currentTarget.style.background = 'var(--pc-hover)'; }}
            onMouseLeave={(e) => { e.currentTarget.style.color = 'var(--pc-text-muted)'; e.currentTarget.style.background = 'transparent'; }}
            aria-label={t('settings.theme_title')}
            title={t('settings.theme_title')}
          >
            <HeaderMaskIcon fileName="window.svg" sizeClass="h-[21px] w-[21px]" />
          </button>
          */}

          <div className={`ml-2 flex items-center ${isLight ? 'gap-1.5' : 'gap-2'}`}>
            {/* 隐藏置顶按钮 */}
            <div style={{ display: 'none' }}>
              <WindowControlIcon
                fileName={alwaysOnTop ? 'ukui-fixed-symbolic.svg' : 'ukui-unfixed-symbolic.svg'}
                // isLight={isLight}
                label={t('header.window_pin')}
                active={alwaysOnTop}
                onClick={async () => {
                  await toggleAlwaysOnTopCurrentWindow();
                  setAlwaysOnTop((current) => !current);
                }}
              />
            </div>
            <WindowControlIcon
              fileName="arrow-address-2.svg"
              label={t('header.window_minimize')}
              active={false}
              onClick={minimizeCurrentWindow}
            />
            <WindowControlIcon
              fileName="arrow-address.svg"
              label={t('header.window_maximize')}
              active={false}
              onClick={toggleMaximizeCurrentWindow}
            />
            <WindowControlIcon
              fileName="arrow-address-3.svg"
              label={t('header.window_close')}
              active={false}
              onClick={handleCloseButtonClick}
            />
          </div>
        </div>
      </header>

      <SettingsModal open={settingsOpen} onClose={() => setSettingsOpen(false)} />
    </>
  );
}

function WindowControlIcon({
  fileName,
  label,
  active,
  onClick,
}: {
  fileName: string;
  label: string;
  active: boolean;
  onClick: () => void | Promise<void>;
}) {
  return (
    <button
      type="button"
      onClick={() => {
        void onClick();
      }}
      className="hidden md:flex h-8 w-8 shrink-0 items-center justify-center rounded-[6px] transition-colors"
      aria-pressed={active}
      style={{
        background: active ? 'var(--pc-hover)' : 'transparent',
        border: 'none',
        cursor: 'default',
        color: active ? 'var(--pc-text-primary)' : 'var(--pc-text-muted)',
      }}
      onMouseEnter={(e) => { e.currentTarget.style.background = 'var(--pc-hover)'; }}
      onMouseLeave={(e) => { e.currentTarget.style.background = active ? 'var(--pc-hover)' : 'transparent'; }}
      aria-label={label}
      title={label}
    >
      <HeaderMaskIcon fileName={fileName} sizeClass="h-4 w-4" />
    </button>
  );
}

function HeaderMaskIcon({
  fileName,
  sizeClass,
}: {
  fileName: string;
  sizeClass: string;
}) {
  return (
    <span
      aria-hidden="true"
      className={`block ${sizeClass}`}
      style={{
        backgroundColor: 'currentColor',
        opacity: 0.88,
        WebkitMaskImage: `url(${assetBasePath}/icons/${fileName})`,
        WebkitMaskRepeat: 'no-repeat',
        WebkitMaskPosition: 'center',
        WebkitMaskSize: 'contain',
        maskImage: `url(${assetBasePath}/icons/${fileName})`,
        maskRepeat: 'no-repeat',
        maskPosition: 'center',
        maskSize: 'contain',
      }}
    />
  );
}
