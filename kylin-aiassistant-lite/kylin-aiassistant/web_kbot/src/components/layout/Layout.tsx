import { useState } from 'react';
import { useEffect } from 'react';
import { Outlet, useLocation, useSearchParams } from 'react-router-dom';
import Header from '@/components/layout/Header';
import { ErrorBoundary } from '@/App';
import { useTheme } from '@/hooks/useTheme';
import { resolveNavMode } from '@/components/layout/navModes';
import { AgentNewSessionProvider, useAgentNewSession } from '@/contexts/AgentNewSessionContext';
import { t } from '@/lib/i18n';
import { assetBasePath } from '@/lib/basePath';
import { isKybotEmbeddedQtWebEngine } from '@/lib/embedHost';

export interface AgentPageOutletContext {
  setAgentGenerating: (value: boolean) => void;
}

function LayoutShell() {
  const { pathname } = useLocation();
  const [searchParams] = useSearchParams();
  const { requestNewSession } = useAgentNewSession();
  const [isAgentGenerating, setIsAgentGenerating] = useState(false);
  const [tipVisible, setTipVisible] = useState(false);
  const [tipText, setTipText] = useState('');
  const { resolvedTheme } = useTheme();
  const isLight = resolvedTheme === 'light';
  const currentMode = resolveNavMode(pathname);
  /** Embedded shell or explicit query: hosts without in-app header must not reserve the 40px row. */
  const hideChromeQuery =
    searchParams.get('kybotModelNoChrome') === '1' ||
    searchParams.get('kybotModelNoChrome')?.toLowerCase() === 'true';
  /** Embedded shell or explicit query: QML tabs replace the 40px in-app Header on task web pages. */
  const hideHeaderForEmbeddedChromelessPage =
    (pathname === '/model' || pathname === '/skills' || pathname === '/cron') &&
    (isKybotEmbeddedQtWebEngine() || hideChromeQuery);

  const showGeneratingTip = () => {
    setTipText(t('agent.generating_block_tip'));
    setTipVisible(true);
  };

  const handleBlockedOrRun = (action: () => void) => {
    if (isAgentGenerating) {
      showGeneratingTip();
      return;
    }
    action();
  };

  useEffect(() => {
    if (!tipVisible) return;
    const timer = window.setTimeout(() => {
      setTipVisible(false);
    }, 3000);
    return () => window.clearTimeout(timer);
  }, [tipVisible, tipText]);

  return (
    <div
      className="min-h-screen"
      style={{
        background: 'var(--pc-bg-base)',
        color: 'var(--pc-text-primary)',
      }}
    >

      <div
        data-testid="layout-main-shell"
        data-nav-mode={currentMode.key}
        data-embedded-chromeless={hideHeaderForEmbeddedChromelessPage ? 'true' : undefined}
        className="ml-0 flex flex-col flex-1 min-w-0 h-screen"
        style={isLight ? { background: '#FFFFFF' } : undefined}
      >
        {!hideHeaderForEmbeddedChromelessPage && (
          <Header
            onNewAgentSession={() => handleBlockedOrRun(requestNewSession)}
          />
        )}

        {/* Page content — ErrorBoundary keyed by pathname so the nav shell
            survives a page crash and the boundary resets on route change */}
        <main
          className="flex-1 overflow-y-auto overflow-x-hidden min-h-0"
          style={isLight ? { background: '#FFFFFF' } : undefined}
        >
          <ErrorBoundary key={pathname}>
            <Outlet context={{ setAgentGenerating: setIsAgentGenerating } satisfies AgentPageOutletContext} />
          </ErrorBoundary>
        </main>
      </div>
      {tipVisible && (
        <div className="pointer-events-none fixed inset-0 z-[70] flex items-center justify-center px-4">
          <div
            className="inline-flex min-h-[52px] w-fit max-w-[calc(100vw-2rem)] items-center gap-2 rounded-lg border px-4 py-3"
            style={{
              background: '#FFFFFF',
              borderColor: 'rgba(0, 0, 0, 0.1)',
              boxShadow: '0 4px 20px rgba(0, 0, 0, 0.2)',
            }}
            role="status"
            aria-live="polite"
          >
            <img
              src={`${assetBasePath}/icons/newbuttontips.svg`}
              alt=""
              className="h-4 w-4 shrink-0"
              aria-hidden="true"
            />
            <span className="text-sm" style={{ color: '#000000' }}>
              {tipText}
            </span>
          </div>
        </div>
      )}
    </div>
  );
}

export default function Layout() {
  return (
    <AgentNewSessionProvider>
      <LayoutShell />
    </AgentNewSessionProvider>
  );
}
