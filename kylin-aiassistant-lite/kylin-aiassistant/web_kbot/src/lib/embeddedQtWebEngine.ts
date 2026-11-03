/**
 * Qt Quick + Qt WebEngine embedding (kylin-aiassistant on Qt 5.12 / Chromium 69-era).
 * `kybot-embedded-qt-webengine` is applied from `polyfills.ts` early in boot.
 */
export function isEmbeddedQtWebEngineHost(): boolean {
  if (typeof document === 'undefined') return false;
  try {
    return document.documentElement.classList.contains(
      'kybot-embedded-qt-webengine',
    );
  } catch {
    return false;
  }
}

/**
 * Defer heavy compositor/canvas work so the Scene Graph settles after mounting
 * dialogs (CHANNEL bind modals SEGV mitigation on old WebEngine).
 */
export function scheduleEmbeddedCompositorSafeWork(fn: () => void): number {
  const ms = isEmbeddedQtWebEngineHost() ? 150 : 0;
  return window.setTimeout(fn, ms);
}
