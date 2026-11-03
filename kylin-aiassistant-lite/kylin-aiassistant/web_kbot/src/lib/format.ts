/** Format a money value as ¥xx.xxxx (4 decimal places, design spec). */
export function formatCNY(value: number): string {
  if (!Number.isFinite(value)) return '¥0.0000';
  return `¥${value.toFixed(4)}`;
}

/** Format an integer with thousand separators using zh-CN locale. */
export function formatInt(value: number): string {
  if (!Number.isFinite(value)) return '0';
  return Math.trunc(value).toLocaleString('zh-CN');
}

/** Round to whole percent, e.g. 9.7 -> "10%". */
export function formatPercent(value: number): string {
  if (!Number.isFinite(value)) return '0%';
  return `${Math.round(value)}%`;
}
