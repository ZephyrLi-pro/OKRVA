// Tauri detection utilities for KylinBot Desktop.

declare global {
  interface Window {
    __TAURI__?: unknown;
    __TAURI_INTERNALS__?: unknown;
    __KYLINBOT_GATEWAY__?: string;
    isTauri?: boolean;
  }
}

/** Returns true when running inside a Tauri WebView. */
export const isTauri = (): boolean =>
  Boolean(window.isTauri || window.__TAURI_INTERNALS__ || window.__TAURI__);

/** Gateway base URL when running inside Tauri (defaults to localhost). */
export const tauriGatewayUrl = (): string =>
  window.__KYLINBOT_GATEWAY__ ?? 'http://127.0.0.1:42617';

async function getCurrentWindowHandle() {
  if (!isTauri()) return null;
  try {
    const mod = await import('@tauri-apps/api/window');
    return mod.getCurrentWindow();
  } catch (e) {
    console.error('Failed to import Tauri window API:', e);
    return null;
  }
}

export async function closeCurrentWindow(): Promise<void> {
  const currentWindow = await getCurrentWindowHandle();
  await currentWindow?.close();
}

export async function minimizeCurrentWindow(): Promise<void> {
  const currentWindow = await getCurrentWindowHandle();
  await currentWindow?.minimize();
}

export async function toggleMaximizeCurrentWindow(): Promise<void> {
  const currentWindow = await getCurrentWindowHandle();
  await currentWindow?.toggleMaximize();
}

export async function toggleAlwaysOnTopCurrentWindow(): Promise<void> {
  const currentWindow = await getCurrentWindowHandle();
  if (!currentWindow) {
    console.warn('Not running in Tauri environment');
    return;
  }
  try {
    const alwaysOnTop = await currentWindow.isAlwaysOnTop();
    await currentWindow.setAlwaysOnTop(!alwaysOnTop);
  } catch (e) {
    console.error('Failed to toggle always on top:', e);
  }
}

export async function isCurrentWindowAlwaysOnTop(): Promise<boolean> {
  const currentWindow = await getCurrentWindowHandle();
  if (!currentWindow) return false;
  try {
    return await currentWindow.isAlwaysOnTop();
  } catch (e) {
    console.error('Failed to get always on top status:', e);
    return false;
  }
}

export async function startCurrentWindowDragging(): Promise<void> {
  const currentWindow = await getCurrentWindowHandle();
  await currentWindow?.startDragging();
}

const CLOSE_ACTION_KEY = 'kylinbot_close_action';

export function getCloseAction(): 'hide' | 'exit' {
  try {
    const stored = localStorage.getItem(CLOSE_ACTION_KEY);
    if (stored === 'hide' || stored === 'exit') {
      return stored;
    }
  } catch {}
  return 'exit';
}

export async function handleCloseButtonClick(): Promise<void> {
  const action = getCloseAction();
  if (action === 'hide') {
    await minimizeCurrentWindow();
  } else {
    await closeCurrentWindow();
  }
}
