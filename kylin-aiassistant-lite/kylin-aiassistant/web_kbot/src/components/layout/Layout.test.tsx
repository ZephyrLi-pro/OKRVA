import { render, screen } from '@testing-library/react';
import { MemoryRouter, Route, Routes } from 'react-router-dom';
import { afterEach, describe, expect, it, vi } from 'vitest';
import Layout from './Layout';
import { ThemeContext, type ThemeContextValue } from '@/contexts/ThemeContextDef';

vi.mock('@/components/layout/Header', () => ({
  default: () => <div data-testid="header-mock">header</div>,
}));

vi.mock('@/App', () => ({
  ErrorBoundary: ({ children }: { children: React.ReactNode }) => <>{children}</>,
}));

function renderLayout(initialEntry: string) {
  const themeValue: ThemeContextValue = {
    theme: 'light',
    accent: 'blue',
    colorTheme: 'default-light',
    uiFont: 'system',
    monoFont: 'jetbrains',
    uiFontSize: 15,
    monoFontSize: 14,
    resolvedTheme: 'light',
    setTheme: () => {},
    setAccent: () => {},
    setColorTheme: () => {},
    setUiFont: () => {},
    setMonoFont: () => {},
    setUiFontSize: () => {},
    setMonoFontSize: () => {},
  };

  return render(
    <ThemeContext.Provider value={themeValue}>
      <MemoryRouter initialEntries={[initialEntry]}>
        <Routes>
          <Route element={<Layout />}>
            <Route path="/agent" element={<div>agent page</div>} />
            <Route path="/general" element={<div>general page</div>} />
            <Route path="/model" element={<div>model page</div>} />
            <Route path="/skills" element={<div>skills page</div>} />
            <Route path="/cron" element={<div>cron page</div>} />
          </Route>
        </Routes>
      </MemoryRouter>
    </ThemeContext.Provider>,
  );
}

describe('Layout', () => {
  afterEach(() => {
    document.documentElement.classList.remove('kybot-embedded-qt-webengine');
  });

  it('uses agent nav mode on the agent route', () => {
    renderLayout('/agent');

    expect(screen.getByTestId('layout-main-shell')).toHaveAttribute('data-nav-mode', 'agent');
  });

  it('uses config nav mode on settings routes', () => {
    renderLayout('/general');

    expect(screen.getByTestId('layout-main-shell')).toHaveAttribute('data-nav-mode', 'config');
  });

  it('keeps Header on standalone /model (no embedded class / no query)', () => {
    renderLayout('/model');

    expect(screen.getByTestId('header-mock')).toBeInTheDocument();
  });

  it('omits Header on /model when kybot embedded Qt quirks class is on <html>', () => {
    document.documentElement.classList.add('kybot-embedded-qt-webengine');
    renderLayout('/model');

    expect(screen.queryByTestId('header-mock')).not.toBeInTheDocument();
  });

  it('omits Header on /cron when kybot embedded Qt quirks class is on <html>', () => {
    document.documentElement.classList.add('kybot-embedded-qt-webengine');
    renderLayout('/cron');

    expect(screen.queryByTestId('header-mock')).not.toBeInTheDocument();
  });

  it('omits Header on /skills when kybot embedded Qt quirks class is on <html>', () => {
    document.documentElement.classList.add('kybot-embedded-qt-webengine');
    renderLayout('/skills');

    expect(screen.queryByTestId('header-mock')).not.toBeInTheDocument();
  });

  it('keeps Header on /agent when embedded (only task web tabs omit chrome)', () => {
    document.documentElement.classList.add('kybot-embedded-qt-webengine');
    renderLayout('/agent');

    expect(screen.getByTestId('header-mock')).toBeInTheDocument();
  });

  it('omits Header on /model when kybotModelNoChrome query is set', () => {
    renderLayout('/model?kybotModelNoChrome=1');

    expect(screen.queryByTestId('header-mock')).not.toBeInTheDocument();
  });
});
