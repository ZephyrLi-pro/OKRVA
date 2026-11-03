import type { CSSProperties } from 'react';

/**
 * Matches `Skills.tsx` AddSkillDialog / UninstallConfirmDialog outer shell — tuned for kylin-aiassistant
 * embedding (same viewport offset + stacking as manual-add flows that already work there).
 * The narrow-view horizontal correction lives in `.model-dialog-shell` because media queries
 * cannot be expressed by React inline styles.
 */
export const SKILL_EMBEDDED_DIALOG_SHELL: CSSProperties = {
  position: 'fixed',
  top: 0,
  left: 0,
  width: '100vw',
  height: '100vh',
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'center',
  zIndex: 200,
  transform: 'translate(-28px, -40px)',
};

/** Stacked above {@link SKILL_EMBEDDED_DIALOG_SHELL} (e.g. overwrite confirm over add-model). */
export const SKILL_EMBEDDED_DIALOG_SHELL_ABOVE: CSSProperties = {
  ...SKILL_EMBEDDED_DIALOG_SHELL,
  zIndex: 300,
};
