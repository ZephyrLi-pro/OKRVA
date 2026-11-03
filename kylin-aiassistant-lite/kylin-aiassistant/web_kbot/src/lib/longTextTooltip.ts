/** 长文本 tooltip：换行与截断（任务指令等，设计稿扩展型说明） */

export const TOOLTIP_ELLIPSIS = '...';
export const TOOLTIP_ELLIPSIS_CHAR_COUNT = 3;

/** 单行显示宽度上限（中文计 2、英文计 1，上限 100 单位） */
export const LINE_DISPLAY_UNIT_LIMIT = 100;

/** 总字符数上限：中文 ≤100；英文 <200；混排 ≤100（不区分中英） */
export const TOTAL_CHAR_LIMIT = {
  chinese: 100,
  english: 199,
  mixed: 100,
} as const;

export type TextScriptProfile = keyof typeof TOTAL_CHAR_LIMIT;

export function splitGraphemes(text: string): string[] {
  return [...text];
}

export function isCJKChar(char: string): boolean {
  const cp = char.codePointAt(0);
  if (cp === undefined) return false;
  return (
    (cp >= 0x4e00 && cp <= 0x9fff) ||
    (cp >= 0x3400 && cp <= 0x4dbf) ||
    (cp >= 0xf900 && cp <= 0xfaff) ||
    (cp >= 0x3000 && cp <= 0x303f) ||
    (cp >= 0xff00 && cp <= 0xffef)
  );
}

export function isEnglishChar(char: string): boolean {
  return /[A-Za-z0-9]/.test(char);
}

/** 混排时中文:英文显示单位 = 2:1 */
export function getCharDisplayUnits(char: string): number {
  return isCJKChar(char) ? 2 : 1;
}

export function getTextScriptProfile(text: string): TextScriptProfile {
  let hasCjk = false;
  let hasEnglish = false;
  for (const char of splitGraphemes(text)) {
    if (isCJKChar(char)) hasCjk = true;
    else if (isEnglishChar(char)) hasEnglish = true;
    if (hasCjk && hasEnglish) return 'mixed';
  }
  if (hasEnglish) return 'english';
  return 'chinese';
}

export function getTotalCharLimit(profile: TextScriptProfile): number {
  return TOTAL_CHAR_LIMIT[profile];
}

/**
 * 超出总字符上限时截断并追加「...」（省略号占 3 个字符位）。
 */
export function truncateLongTooltipText(text: string): string {
  const profile = getTextScriptProfile(text);
  const limit = getTotalCharLimit(profile);
  const chars = splitGraphemes(text);
  if (chars.length <= limit) return text;

  const keepCount = limit - TOOLTIP_ELLIPSIS_CHAR_COUNT;
  return chars.slice(0, keepCount).join('') + TOOLTIP_ELLIPSIS;
}

/**
 * 按行自动换行：
 * - 纯中文：每行 >50 字换行（50×2=100 单位）
 * - 纯英文：每行 >100 字换行
 * - 混排：按 2:1 加权，单行超过 100 单位换行
 */
export function wrapLongTooltipText(text: string): string {
  const lines: string[] = [];
  const paragraphs = text.split(/\r\n|\r|\n/);

  for (const paragraph of paragraphs) {
    if (paragraph === '') {
      lines.push('');
      continue;
    }

    let current = '';
    let units = 0;
    for (const char of splitGraphemes(paragraph)) {
      const charUnits = getCharDisplayUnits(char);
      if (units + charUnits > LINE_DISPLAY_UNIT_LIMIT && current.length > 0) {
        lines.push(current);
        current = char;
        units = charUnits;
      } else {
        current += char;
        units += charUnits;
      }
    }
    if (current.length > 0) lines.push(current);
  }

  return lines.join('\n');
}

export function formatLongTextTooltip(text: string): string {
  return wrapLongTooltipText(truncateLongTooltipText(text));
}
