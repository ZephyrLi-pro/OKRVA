import { describe, expect, it } from 'vitest';
import { formatRuntimeDuration, formatSessionLabel, summarizeAgentStyle } from './Dashboard';

describe('formatSessionLabel', () => {
  it('returns a shortened id when session id exists', () => {
    expect(formatSessionLabel('123456789abc')).toBe('12345678...');
  });

  it('returns a fallback when session id is missing', () => {
    expect(formatSessionLabel(undefined)).toBe('-');
    expect(formatSessionLabel('')).toBe('-');
  });
});

describe('summarizeAgentStyle', () => {
  it('returns a localized fallback when system prompt is missing', () => {
    expect(summarizeAgentStyle(undefined)).toBe('General purpose');
  });

  it('uses the first sentence of the system prompt', () => {
    expect(
      summarizeAgentStyle(
        'You are a meticulous code reviewer. Focus on bugs first. Keep summaries short.',
      ),
    ).toBe('You are a meticulous code reviewer.');
  });
});

describe('formatRuntimeDuration', () => {
  it('returns a fallback when no runtime start exists', () => {
    expect(formatRuntimeDuration(undefined)).toBe('-');
  });
});
