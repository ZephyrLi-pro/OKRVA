/**
 * Older Qt WebEngine / Chromium kernels used with KylinBot's embedded `_app`:
 * - Qt 5.12 line ≈ Chromium 69 — no `Object.fromEntries` (Chrome 73+).
 * - Some stacks still lag `Object.hasOwn` (Chrome 93+).
 * Load this module first from `main.tsx`; smol-toml expects `hasOwn`; app code uses `fromEntries`.
 */
type ObjectMaybeMissingStatics = ObjectConstructor & {
  fromEntries?(entries: Iterable<Iterable<any>>): object;
  hasOwn?(o: object, p: PropertyKey): boolean;
};
const O = Object as ObjectMaybeMissingStatics;

if (typeof O.fromEntries !== 'function') {
  Object.defineProperty(Object, 'fromEntries', {
    value(entries: Iterable<Iterable<any>>): object {
      const out: Record<PropertyKey, unknown> = {};
      if (entries === null || entries === undefined || typeof (entries as any)[Symbol.iterator] !== 'function') {
        throw new TypeError('Object.fromEntries requires an iterable object');
      }
      for (const pair of entries) {
        if (pair === undefined || pair === null || typeof (pair as any)[Symbol.iterator] !== 'function') {
          throw new TypeError('Iterator value must be an object that can be iterated as a key/value pair');
        }
        const it = (pair as Iterable<any>)[Symbol.iterator]();
        const keyStep = it.next();
        const valStep = it.next();
        if (keyStep.done) {
          throw new TypeError('Each entry pair must yield at least two values');
        }
        Reflect.set(out, keyStep.value as PropertyKey, valStep.value);
      }
      return out;
    },
    writable: true,
    configurable: true,
  });
}

if (typeof O.hasOwn !== 'function') {
  Object.defineProperty(Object, 'hasOwn', {
    value: (obj: object, prop: PropertyKey) =>
      Object.prototype.hasOwnProperty.call(obj, prop),
    writable: true,
    configurable: true,
  });
}

/**
 * kylin-aiassistant (and similar) hosts this UI inside Qt Quick WebEngine; on Qt 5.12 / Chromium 69-era
 * stacks, backdrop-filter paths have been observed to correlate with SEGV inside libQt5WebEngine during
 * scene graph sync — not recoverable from JS. Strip those effects via CSS hooks (see index.css).
 * Host apps should append `?kybotEmbed=qtwebengine` (Chat.qml does this); UA `QtWebEngine` also enables it.
 */
function installEmbeddedQtWebEngineQuirksClass(): void {
  if (typeof document === 'undefined' || typeof navigator === 'undefined') {
    return;
  }
  const mark = (): void => {
    try {
      document.documentElement.classList.add('kybot-embedded-qt-webengine');
    } catch {
      /* ignore */
    }
  };
  try {
    // Explicit hint from kylin-aiassistant (or any host) — does not rely on User-Agent quirks.
    if (typeof window !== 'undefined' && typeof URLSearchParams === 'function') {
      const v = (
        new URLSearchParams(window.location.search).get('kybotEmbed') ?? ''
      ).toLowerCase();
      if (v === 'qtwebengine' || v === 'qt-webengine' || v === 'qt_we') {
        mark();
        return;
      }
    }
  } catch {
    /* ignore */
  }
  try {
    const ua = navigator.userAgent;
    // Distinct substring in Qt WebEngine user agents (Kylin embedding path).
    if (ua.includes('QtWebEngine')) {
      mark();
    }
  } catch {
    /* ignore — never block boot */
  }
}
installEmbeddedQtWebEngineQuirksClass();
