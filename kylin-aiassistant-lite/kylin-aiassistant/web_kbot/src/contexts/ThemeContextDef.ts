import { createContext } from 'react';
import type { ColorThemeId } from './colorThemes';

export type ThemeMode = 'system' | 'dark' | 'light' | 'oled';
export type AccentColor = 'cyan' | 'violet' | 'emerald' | 'amber' | 'rose' | 'blue';
export type UiFont = 'system' | 'inter' | 'segoe' | 'sf';
export type MonoFont = 'jetbrains' | 'fira' | 'cascadia' | 'system-mono';

/** @deprecated Use ThemeMode instead. Kept for storage backward-compat. */
export type ThemeName = ThemeMode;

const cjkSansFallback = '"Source Han Sans SC", "Source Han Sans CN", "Source Han Sans", "Noto Sans CJK SC", "Noto Sans SC", "PingFang SC", "Hiragino Sans GB", "Microsoft YaHei", sans-serif';

export const uiFontStacks: Record<UiFont, string> = {
  system: `system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", ${cjkSansFallback}`,
  inter: `"Inter", system-ui, ${cjkSansFallback}`,
  segoe: `"Segoe UI", system-ui, ${cjkSansFallback}`,
  sf: `-apple-system, BlinkMacSystemFont, "SF Pro Text", ${cjkSansFallback}`,
};

export const monoFontStacks: Record<MonoFont, string> = {
  jetbrains: '"JetBrains Mono", "Fira Code", "Cascadia Code", monospace',
  fira: '"Fira Code", "JetBrains Mono", "Cascadia Code", monospace',
  cascadia: '"Cascadia Code", "JetBrains Mono", "Fira Code", monospace',
  'system-mono': 'ui-monospace, "SF Mono", "Cascadia Code", "Fira Code", monospace',
};

export interface ThemeContextValue {
  theme: ThemeMode;
  accent: AccentColor;
  colorTheme: ColorThemeId;
  uiFont: UiFont;
  monoFont: MonoFont;
  uiFontSize: number;
  monoFontSize: number;
  resolvedTheme: 'dark' | 'light' | 'oled';
  setTheme: (t: ThemeMode) => void;
  setAccent: (a: AccentColor) => void;
  setColorTheme: (c: ColorThemeId) => void;
  setUiFont: (f: UiFont) => void;
  setMonoFont: (f: MonoFont) => void;
  setUiFontSize: (size: number) => void;
  setMonoFontSize: (size: number) => void;
}

export const ThemeContext = createContext<ThemeContextValue>({
  theme: 'dark',
  accent: 'cyan',
  colorTheme: 'default-dark',
  uiFont: 'system',
  monoFont: 'jetbrains',
  uiFontSize: 15,
  monoFontSize: 14,
  resolvedTheme: 'dark',
  setTheme: () => {},
  setAccent: () => {},
  setColorTheme: () => {},
  setUiFont: () => {},
  setMonoFont: () => {},
  setUiFontSize: () => {},
  setMonoFontSize: () => {},
});
