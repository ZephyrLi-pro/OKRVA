/**
 * True when `polyfills.ts` marked the shell as Kylin/Qt WebEngine embedding
 * (`kybot-embedded-qt-webengine` on `<html>`). Host provides window chrome —
 * omit duplicate in-app header rows on selective routes (e.g. `/model`).
 */
export function isKybotEmbeddedQtWebEngine(): boolean {
  if (typeof document === 'undefined') return false;
  try {
    return document.documentElement.classList.contains('kybot-embedded-qt-webengine');
  } catch {
    return false;
  }
}
