const TOKEN_KEY = 'kylinbot_token';

function consumeTokenFromUrl(): void {
  if (typeof window === 'undefined') return;

  try {
    // qrc:// URLs are not parseable by `new URL()`; fall back to URLSearchParams.
    const params = new URLSearchParams(window.location.search);
    const token = params.get('token')?.trim();
    if (!token) return;

    localStorage.setItem(TOKEN_KEY, token);
    // In qrc mode history.replaceState may not work; skip URL cleanup silently.
    try {
      params.delete('token');
      const qs = params.toString();
      const cleanUrl = `${window.location.pathname}${qs ? '?' + qs : ''}${window.location.hash}`;
      window.history.replaceState(window.history.state, '', cleanUrl);
    } catch {
      // qrc:// or restricted environment — token is stored, URL cleanup skipped.
    }
  } catch {
    // localStorage or history may be unavailable in restricted environments.
  }
}

consumeTokenFromUrl();

/**
 * Retrieve the stored authentication token.
 */
export function getToken(): string | null {
  try {
    return localStorage.getItem(TOKEN_KEY);
  } catch {
    return null;
  }
}

/**
 * Store an authentication token.
 */
export function setToken(token: string): void {
  try {
    localStorage.setItem(TOKEN_KEY, token);
  } catch {
    // localStorage may be unavailable (e.g. in some private browsing modes)
  }
}

/**
 * Remove the stored authentication token.
 */
export function clearToken(): void {
  try {
    localStorage.removeItem(TOKEN_KEY);
  } catch {
    // Ignore
  }
}

/**
 * Returns true if a token is currently stored.
 */
export function isAuthenticated(): boolean {
  const token = getToken();
  return token !== null && token.length > 0;
}
