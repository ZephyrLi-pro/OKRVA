import { describe, expect, it } from 'vitest';
import {
  formatLongTextTooltip,
  getTextScriptProfile,
  TOOLTIP_ELLIPSIS,
  truncateLongTooltipText,
  wrapLongTooltipText,
} from './longTextTooltip';

describe('getTextScriptProfile', () => {
  it('detects chinese-only text', () => {
    expect(getTextScriptProfile('任务指令示例')).toBe('chinese');
  });

  it('detects english-only text', () => {
    expect(getTextScriptProfile('run backup daily')).toBe('english');
  });

  it('detects mixed text', () => {
    expect(getTextScriptProfile('每天 run backup')).toBe('mixed');
  });
});

describe('truncateLongTooltipText', () => {
  it('keeps chinese text within 100 chars', () => {
    const text = '中'.repeat(100);
    expect(truncateLongTooltipText(text)).toBe(text);
  });

  it('truncates chinese text to 97 chars plus ellipsis', () => {
    const text = '中'.repeat(120);
    const result = truncateLongTooltipText(text);
    expect(result).toBe('中'.repeat(97) + TOOLTIP_ELLIPSIS);
    expect([...result].length).toBe(100);
  });

  it('keeps english text below 200 chars', () => {
    const text = 'a'.repeat(199);
    expect(truncateLongTooltipText(text)).toBe(text);
  });

  it('truncates english text to 196 chars plus ellipsis', () => {
    const text = 'a'.repeat(250);
    const result = truncateLongTooltipText(text);
    expect(result).toBe('a'.repeat(196) + TOOLTIP_ELLIPSIS);
    expect([...result].length).toBe(199);
  });

  it('truncates mixed text to 100 chars total including ellipsis', () => {
    const text = '每'.repeat(80) + 'a'.repeat(80);
    const result = truncateLongTooltipText(text);
    expect([...result].length).toBe(100);
    expect(result.endsWith(TOOLTIP_ELLIPSIS)).toBe(true);
  });
});

describe('wrapLongTooltipText', () => {
  it('wraps chinese text after 50 characters per line', () => {
    const text = '中'.repeat(51);
    expect(wrapLongTooltipText(text)).toBe('中'.repeat(50) + '\n' + '中');
  });

  it('wraps english text after 100 characters per line', () => {
    const text = 'a'.repeat(101);
    expect(wrapLongTooltipText(text)).toBe('a'.repeat(100) + '\n' + 'a');
  });

  it('wraps mixed text using 2:1 weighted units', () => {
    const text = '中'.repeat(40) + 'a'.repeat(25);
    expect(wrapLongTooltipText(text)).toBe('中'.repeat(40) + 'a'.repeat(20) + '\n' + 'a'.repeat(5));
  });

  it('preserves explicit newlines', () => {
    expect(wrapLongTooltipText('第一行\n第二行')).toBe('第一行\n第二行');
  });
});

describe('formatLongTextTooltip', () => {
  it('truncates then wraps long chinese tooltip text', () => {
    const text = '中'.repeat(120);
    const result = formatLongTextTooltip(text);
    expect(result.endsWith(TOOLTIP_ELLIPSIS)).toBe(true);
    expect(result.split('\n').every(line => [...line].length <= 50)).toBe(true);
  });
});
