import { t } from '@/lib/i18n';

const ELLIPSIS = '\u2026';

function overwriteHead(kind: 'preset' | 'custom'): string {
  return t(kind === 'preset' ? 'model.overwrite_preset_prefix' : 'model.overwrite_custom_prefix');
}

function overwriteTail(kind: 'preset' | 'custom'): string {
  return t(kind === 'preset' ? 'model.overwrite_preset_suffix' : 'model.overwrite_custom_suffix');
}

const MIN_INNER_LEN = 4;

/**
 * 对话框灰字可用宽度 ≈376−48；14px 下两行：按「全角等价单位」粗略估算，
 * ASCII 字符按约 0.56 单位计，避免出现第三行后才依赖 CSS 硬性裁切。
 */
const TWO_LINE_UNIT_BUDGET = 42;

/** 公开的估算函数，可供单测校准预算 */
export function overwriteTextWidthUnits(s: string): number {
  let u = 0;
  for (let i = 0; i < s.length; i++) {
    const c = s.codePointAt(i)!;
    if (c > 0xffff) i++;
    u += c <= 0x7f ? 0.56 : 1;
  }
  return u;
}

function fitsTwoLines(concat: string): boolean {
  return overwriteTextWidthUnits(concat) <= TWO_LINE_UNIT_BUDGET;
}

function abbreviatedInner(kind: 'preset' | 'custom', quotedInner: string): string {
  const head = overwriteHead(kind);
  const tail = overwriteTail(kind);
  const rawTrim = quotedInner.trim();
  if (rawTrim.length === 0) return '';

  const full = `${head}${rawTrim}${tail}`;
  if (fitsTwoLines(full)) return rawTrim;

  let lo = MIN_INNER_LEN;
  let hi = rawTrim.length;
  let best = MIN_INNER_LEN;
  while (lo <= hi) {
    const mid = (lo + hi) >> 1;
    const piece =
      mid >= rawTrim.length ? rawTrim : `${rawTrim.slice(0, mid)}${ELLIPSIS}`;
    if (fitsTwoLines(`${head}${piece}${tail}`)) {
      best = mid;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }

  if (best >= rawTrim.length) return rawTrim;

  let k = best;
  let inner = `${rawTrim.slice(0, k)}${ELLIPSIS}`;
  while (k > MIN_INNER_LEN && !fitsTwoLines(`${head}${inner}${tail}`)) {
    k--;
    inner = `${rawTrim.slice(0, k)}${ELLIPSIS}`;
  }
  return inner;
}

export interface OverwriteMutedParts {
  prefix: string;
  inner: string;
  suffix: string;
}

/** 前缀 + 后缀字面完整保留；仅用「单位预算 + 后缀」压低引号内核。 */
export function splitOverwriteMutedBody(
  kind: 'preset' | 'custom',
  quotedInner: string,
): OverwriteMutedParts {
  const suffix = overwriteTail(kind);
  const inner = abbreviatedInner(kind, quotedInner);
  return { prefix: overwriteHead(kind), inner, suffix };
}

export function formatOverwriteSubtitle(
  kind: 'preset' | 'custom',
  quotedInner: string,
): string {
  const { prefix, inner, suffix } = splitOverwriteMutedBody(kind, quotedInner);
  return `${prefix}${inner}${suffix}`;
}
