import React, { useState, useEffect, useLayoutEffect, useRef, useMemo, useCallback } from 'react';
import { createPortal } from 'react-dom';
import {
  AlarmClock,
  Plus,
  ChevronDown,
  ChevronUp,
  ChevronLeft,
  ChevronRight,
  ChevronsLeft,
  ChevronsRight,
} from 'lucide-react';
import type { CronJob } from '@/types/api';
import {
  getCronJobs,
  addCronJob,
  deleteCronJob,
  patchCronJob,
  runCronJobNow,
  pauseCronJob,
  resumeCronJob,
} from '@/lib/api';
import { assetBasePath } from '@/lib/basePath';
import { t } from '@/lib/i18n';
import { ModelDropdownOption } from '@/components/model/ModelDropdownOption';
import { ModelCloseButton, TruncatedTextTooltip, useModelMoreTooltip } from '@/components/model/modelTooltips';
import { PageLoadingState } from '@/components/ui/PageLoadingSpinner';

/** 与 ModelDialog 弹窗底部按钮一致 */
const K_ALPHA_NORMAL = 'rgba(0, 0, 0, 0.1)';
const K_ALPHA_HOVER = 'rgba(0, 0, 0, 0.15)';
const K_ALPHA_PRESS = 'rgba(0, 0, 0, 0.2)';

const PRIMARY_BTN = '#3790FA';
const PRIMARY_HOVER = '#3489ee';
const PRIMARY_PRESS = '#2c73c8';

const TEXT_MAIN = 'rgba(0, 0, 0, 0.85)';

const TASK_NAME_MAX_LEN = 32;
const TASK_PROMPT_MAX_LEN = 1000;

function countChars(s: string): number {
  return [...s].length;
}

function clampToMaxChars(s: string, max: number): string {
  const chars = [...s];
  if (chars.length <= max) return s;
  return chars.slice(0, max).join('');
}

/** Word 等多行粘贴到任务名称时合并为单行 */
function normalizeTaskNameInput(s: string): string {
  return s.replace(/\r\n|\r|\n/g, '');
}

function applyCharLimit(
  raw: string,
  max: number,
): { value: string; overflow: boolean } {
  const overflow = countChars(raw) > max;
  return { value: clampToMaxChars(raw, max), overflow };
}

function formatDate(iso: string | null): string {
  if (!iso) return '-';
  const d = new Date(iso);
  return `${d.getFullYear()}/${d.getMonth() + 1}/${d.getDate()} ${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}:${String(d.getSeconds()).padStart(2, '0')}`;
}

/** 单次任务执行后（成功或失败）不再显示下次执行时间 */
function shouldShowNextRun(job: CronJob): boolean {
  if (job.schedule.kind === 'at' && job.last_status != null) {
    return false;
  }
  return job.enabled;
}

// ---------- Toggle Switch ----------
function Toggle({ checked, onChange, disabled }: { checked: boolean; onChange: () => void; disabled?: boolean }) {
  return (
    <button
      onClick={onChange}
      disabled={disabled}
      style={{
        position: 'relative',
        display: 'inline-flex',
        alignItems: 'center',
        width: 40,
        height: 22,
        borderRadius: 11,
        background: checked ? '#1890ff' : '#d9d9d9',
        border: 'none',
        cursor: disabled ? 'not-allowed' : 'pointer',
        transition: 'background 0.25s',
        flexShrink: 0,
        opacity: disabled ? 0.5 : 1,
        padding: 0,
      }}
    >
      <span style={{
        position: 'absolute',
        left: checked ? 20 : 2,
        width: 18,
        height: 18,
        borderRadius: '50%',
        background: '#fff',
        boxShadow: '0 1px 4px rgba(0,0,0,0.2)',
        transition: 'left 0.25s',
      }} />
    </button>
  );
}

const STATUS_BADGE_FONT =
  "'Noto Sans CJK SC', 'Source Han Sans SC', 'PingFang SC', 'Microsoft YaHei', sans-serif";

// ---------- Status Badge ----------
function StatusBadge({ status }: { status: string | null }) {
  if (!status) return <span style={{ color: '#bbb', fontSize: 'var(--model-font-size)' }}>-</span>;
  const s = status.toLowerCase();
  const ok = s === 'ok' || s === 'success';
  const color = ok ? '#00cc66' : '#ff4d4f';
  const bgColor = ok ? 'rgba(0, 204, 102, 0.1)' : 'rgba(255, 77, 79, 0.1)';
  const label = ok ? t('cron.status_success') : t('cron.status_failed');

  return (
    <span
      style={{
        display: 'inline-flex',
        alignItems: 'center',
        boxSizing: 'border-box',
        height: 24,
        padding: '0 4px',
        color,
        background: bgColor,
        borderRadius: 12,
        whiteSpace: 'nowrap',
      }}
    >
      <svg
        width="16"
        height="16"
        viewBox="0 0 14 14"
        fill="none"
        xmlns="http://www.w3.org/2000/svg"
        aria-hidden
        style={{ display: 'block', flexShrink: 0, marginRight: 4 }}
      >
        <circle cx="7" cy="7" r="7" fill={color} />
        {ok ? (
          <path d="M4.5 7.5L6 9L9.5 5" stroke="#fff" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" />
        ) : (
          <path d="M4.5 4.5L9.5 9.5M9.5 4.5L4.5 9.5" stroke="#fff" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" />
        )}
      </svg>
      <span
        style={{
          display: 'inline-flex',
          alignItems: 'center',
          height: 24,
          fontFamily: STATUS_BADGE_FONT,
          fontSize: 12,
          fontWeight: 500,
          lineHeight: '24px',
        }}
      >
        {label}
      </span>
    </span>
  );
}

const MODEL_MORE_MENU_WIDTH = 180;
/** 与 .model-more-menu-panel（3×36 + gap + padding）一致，用于 flip 与定位 */
const MODEL_MORE_MENU_EST_HEIGHT = 124;
/** 菜单相对按钮右对齐基准再向右偏移（设计稿） */
const MODEL_MORE_MENU_OFFSET_X = 92;

// ---------- Row Dropdown Menu（样式对齐 ModelRow「更多」，Portal 避免表格 overflow 裁剪） ----------
function RowMenu({ onEdit, onDelete, onRunNow }: {
  onEdit: () => void;
  onDelete: () => void;
  onRunNow: () => void;
}) {
  const [menuOpen, setMenuOpen] = useState(false);
  const [moreHover, setMoreHover] = useState(false);
  const [morePressed, setMorePressed] = useState(false);
  const [menuFixedStyle, setMenuFixedStyle] = useState<React.CSSProperties>({});
  const menuRef = useRef<HTMLDivElement | null>(null);
  const moreBtnRef = useRef<HTMLButtonElement | null>(null);
  const { onMouseEnter: onMoreMouseEnter, onMouseMove: onMoreMouseMove, onMouseLeave: onMoreMouseLeave, tooltip: moreTooltip } =
    useModelMoreTooltip(menuOpen);

  const updateMenuPosition = useCallback(() => {
    const btn = moreBtnRef.current;
    if (!btn) return;
    const rect = btn.getBoundingClientRect();
    const dropUp = rect.bottom + MODEL_MORE_MENU_EST_HEIGHT > window.innerHeight;
    const top = dropUp ? rect.top - MODEL_MORE_MENU_EST_HEIGHT : rect.bottom;
    const left = Math.max(8, rect.right - MODEL_MORE_MENU_WIDTH + MODEL_MORE_MENU_OFFSET_X);
    setMenuFixedStyle({
      position: 'fixed',
      top,
      left,
      zIndex: 10000,
    });
  }, []);

  const toggleMenu = useCallback(() => {
    setMenuOpen((open) => {
      if (!open) {
        updateMenuPosition();
      }
      return !open;
    });
  }, [updateMenuPosition]);

  useLayoutEffect(() => {
    if (!menuOpen) return;
    updateMenuPosition();
    const onScrollOrResize = () => setMenuOpen(false);
    window.addEventListener('resize', onScrollOrResize);
    window.addEventListener('scroll', onScrollOrResize, true);
    return () => {
      window.removeEventListener('resize', onScrollOrResize);
      window.removeEventListener('scroll', onScrollOrResize, true);
    };
  }, [menuOpen, updateMenuPosition]);

  useEffect(() => {
    if (!menuOpen) return;
    const handler = (e: MouseEvent) => {
      const target = e.target as Node | null;
      if (
        menuRef.current &&
        !menuRef.current.contains(target) &&
        moreBtnRef.current &&
        !moreBtnRef.current.contains(target)
      ) {
        setMenuOpen(false);
      }
    };
    const closeOnBlur = () => setMenuOpen(false);
    document.addEventListener('mousedown', handler);
    window.addEventListener('blur', closeOnBlur);
    return () => {
      document.removeEventListener('mousedown', handler);
      window.removeEventListener('blur', closeOnBlur);
    };
  }, [menuOpen]);

  const moreBackground = (() => {
    if (menuOpen) return 'var(--model-alpha-hover)';
    if (morePressed) return 'var(--model-alpha-active)';
    if (moreHover) return 'var(--model-alpha-hover)';
    return 'transparent';
  })();

  return (
    <div className="relative inline-flex shrink-0">
      <button
        ref={moreBtnRef}
        type="button"
        aria-expanded={menuOpen}
        aria-label={t('cron.more')}
        className="inline-flex shrink-0 cursor-default items-center justify-center transition-colors border-none"
        style={{
          width: 36,
          height: 36,
          borderRadius: 6,
          background: moreBackground,
        }}
        onMouseEnter={(e) => {
          setMoreHover(true);
          onMoreMouseEnter(e);
        }}
        onMouseMove={onMoreMouseMove}
        onMouseLeave={() => {
          setMoreHover(false);
          setMorePressed(false);
          onMoreMouseLeave();
        }}
        onMouseDown={() => setMorePressed(true)}
        onMouseUp={() => setMorePressed(false)}
        onClick={toggleMenu}
      >
        <img
          src={`${assetBasePath}/icons/view-more-horizontal-symbolic.svg`}
          alt=""
          width={16}
          height={16}
          className="pointer-events-none object-contain select-none"
          draggable={false}
        />
      </button>

      {menuOpen && createPortal(
        <div
          ref={menuRef}
          className="model-more-menu-panel shrink-0"
          style={menuFixedStyle}
          onMouseDown={e => e.stopPropagation()}
          onClick={e => e.stopPropagation()}
        >
          <button
            type="button"
            className="skill-uninstall-btn"
            style={{ width: 164, height: 36 }}
            onMouseDown={e => e.stopPropagation()}
            onClick={() => {
              setMenuOpen(false);
              onRunNow();
            }}
          >
            {t('cron.run_now')}
          </button>
          <button
            type="button"
            className="skill-uninstall-btn"
            style={{ width: 164, height: 36 }}
            onMouseDown={e => e.stopPropagation()}
            onClick={() => {
              setMenuOpen(false);
              onEdit();
            }}
          >
            {t('cron.edit')}
          </button>
          <button
            type="button"
            className="skill-uninstall-btn"
            style={{ width: 164, height: 36 }}
            onMouseDown={e => e.stopPropagation()}
            onClick={() => {
              setMenuOpen(false);
              onDelete();
            }}
          >
            {t('cron.delete')}
          </button>
        </div>,
        document.body,
      )}

      {moreTooltip}
    </div>
  );
}

// ---------- Schedule helpers ----------
type CronScheduleInput = Parameters<typeof addCronJob>[0]['schedule'];

type IntervalScheduleUiState = {
  value: string;
  unit: 'minute' | 'hour';
  weekdays: number[];
  windowStart: string;
  windowEnd: string;
};

type CronIntervalUiMeta = IntervalScheduleUiState & {
  scheduleSignature: string;
};

const CRON_INTERVAL_UI_META_STORAGE_KEY = 'kylinbot_cron_interval_ui_meta_v1';

function getCronCurrentTimeHHMM(): string {
  const now = new Date();
  return `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}`;
}

function formatLocalYmd(d: Date): string {
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, '0');
  const day = String(d.getDate()).padStart(2, '0');
  return `${y}-${m}-${day}`;
}

function parseOnceScheduleDateTime(date: string, time: string): Date | null {
  const ymd = date.trim();
  const hhmm = time.trim();
  if (!/^\d{4}-\d{2}-\d{2}$/.test(ymd) || !/^\d{2}:\d{2}$/.test(hhmm)) return null;
  const atLocal = new Date(`${ymd}T${hhmm}:00`);
  if (Number.isNaN(atLocal.getTime())) return null;
  return atLocal;
}

function isOnceScheduleInPast(date: string, time: string, now = new Date()): boolean {
  const atLocal = parseOnceScheduleDateTime(date, time);
  if (!atLocal) return false;
  return atLocal.getTime() < now.getTime();
}


/** True when expr matches the fixed-shape cron produced by the daily/custom time UI (`m h * * dow`). */
function isSimpleDailyCronExpr(expr: string): boolean {
  const parts = (expr ?? '').trim().split(/\s+/);
  if (parts.length !== 5) return false;
  const [min, hr, dom, mon, dow] = parts;
  if (min === undefined || hr === undefined || dom === undefined || mon === undefined || dow === undefined)
    return false;
  if (dom !== '*' || mon !== '*') return false;
  if (!/^\d{1,2}$/.test(min) || !/^\d{1,2}$/.test(hr)) return false;
  if (dow !== '*' && !/^[\d,\s]+$/.test(dow)) return false;
  return true;
}

function parseCronExprToUI(expr: string): { time: string; weekdays: number[] } {
  const parts = (expr ?? '').trim().split(/\s+/);
  if (parts.length >= 5) {
    const minutePart = parts[0] ?? '0';
    const hourPart = parts[1] ?? '0';
    const wdStr = parts[4] ?? '*';
    const time = `${hourPart.padStart(2, '0')}:${minutePart.padStart(2, '0')}`;
    const weekdays =
      wdStr === '*'
        ? [1, 2, 3, 4, 5, 6, 7]
        : wdStr
          .split(',')
          .map(n => {
            const d = parseInt(n, 10);
            return d === 0 ? 7 : d;
          })
          .filter(d => d >= 1 && d <= 7);
    return { time, weekdays };
  }
  return { time: getCronCurrentTimeHHMM(), weekdays: [1, 2, 3, 4, 5, 6, 7] };
}

function formatCronHhmm(hour: number, minute: number): string {
  return `${String(hour).padStart(2, '0')}:${String(minute).padStart(2, '0')}`;
}

/** True when expr was produced by the interval UI (minute or hour step within an hour range). */
function isIntervalCronExpr(expr: string): boolean {
  const parts = (expr ?? '').trim().split(/\s+/);
  if (parts.length !== 5) return false;
  const [min, hr] = parts;
  if (min?.startsWith('*/')) return true;
  if (min?.includes('/')) return true;
  if (min?.includes('-')) return true;
  if (hr?.includes('-')) return true;
  return false;
}

function parseIntervalCronExpr(
  expr: string,
): IntervalScheduleUiState | null {
  const parts = (expr ?? '').trim().split(/\s+/);
  if (parts.length !== 5) return null;
  const [min, hr, dom, mon, dow] = parts;
  if (dom !== '*' || mon !== '*') return null;

  const weekdays =
    dow === '*'
      ? [1, 2, 3, 4, 5, 6, 7]
      : dow!
          .split(',')
          .map(n => {
            const d = parseInt(n, 10);
            return d === 0 ? 7 : d;
          })
          .filter(d => d >= 1 && d <= 7);

  const minuteList = min!.match(/^\d{1,2}(?:,\d{1,2})+$/);
  if (minuteList) {
    const minutes = min!
      .split(',')
      .map(part => parseInt(part, 10));
    if (minutes.some((minute, index) =>
      !Number.isInteger(minute) ||
      minute < 0 ||
      minute > 59 ||
      (index > 0 && minute <= (minutes[index - 1] ?? -1)),
    )) {
      return null;
    }
    const step = (minutes[1] ?? 0) - (minutes[0] ?? 0);
    if (step < 1 || minutes.some((minute, index) => index > 0 && minute - (minutes[index - 1] ?? minute) !== step)) {
      return null;
    }
    const m = hr!.match(/^(\d{1,2})-(\d{1,2})$/);
    if (!m) return null;
    const sh = parseInt(m[1]!, 10);
    const eh = parseInt(m[2]!, 10);
    return {
      value: String(step),
      unit: 'minute',
      weekdays,
      windowStart: formatCronHhmm(sh, minutes[0] ?? 0),
      windowEnd: formatCronHhmm(eh, minutes[minutes.length - 1] ?? 59),
    };
  }

  const minuteStep = min!.match(/^(\d{1,2})\/(\d{1,2})$/);
  if (minuteStep) {
    const sm = parseInt(minuteStep[1]!, 10);
    const val = minuteStep[2]!;
    const m = hr!.match(/^(\d{1,2})-(\d{1,2})$/);
    if (!m) return null;
    const sh = parseInt(m[1]!, 10);
    const eh = parseInt(m[2]!, 10);
    return {
      value: val,
      unit: 'minute',
      weekdays,
      windowStart: formatCronHhmm(sh, sm),
      windowEnd: sh === eh ? formatCronHhmm(eh, sm) : formatCronHhmm(eh, 59),
    };
  }

  if (min!.startsWith('*/')) {
    const val = min!.slice(2);
    const m = hr!.match(/^(\d{1,2})-(\d{1,2})$/);
    if (!m) return null;
    const sh = parseInt(m[1]!, 10);
    const eh = parseInt(m[2]!, 10);
    return {
      value: val,
      unit: 'minute',
      weekdays,
      windowStart: formatCronHhmm(sh, 0),
      windowEnd: formatCronHhmm(eh, 59),
    };
  }

  const minFixedOrRange = min!.match(/^(\d{1,2})(?:-(\d{1,2}))?$/);
  const hourRangeWithFixedMinute = hr!.match(/^(\d{1,2})-(\d{1,2})(?:\/(\d{1,2}))?$/);
  if (hourRangeWithFixedMinute && minFixedOrRange) {
    const sm = parseInt(minFixedOrRange[1]!, 10);
    const em = minFixedOrRange[2] ? parseInt(minFixedOrRange[2]!, 10) : sm;
    const sh = parseInt(hourRangeWithFixedMinute[1]!, 10);
    const eh = parseInt(hourRangeWithFixedMinute[2]!, 10);
    const val = hourRangeWithFixedMinute[3] ?? '1';
    return {
      value: val,
      unit: 'hour',
      weekdays,
      windowStart: formatCronHhmm(sh, sm),
      windowEnd: formatCronHhmm(eh, em),
    };
  }

  if (min === '0' && hr!.includes('-')) {
    const m = hr!.match(/^(\d{1,2})-(\d{1,2})(?:\/(\d{1,2}))?$/);
    if (!m) return null;
    const sh = parseInt(m[1]!, 10);
    const eh = parseInt(m[2]!, 10);
    const val = m[3] ?? '1';
    return {
      value: val,
      unit: 'hour',
      weekdays,
      windowStart: formatCronHhmm(sh, 0),
      windowEnd: formatCronHhmm(eh, 59),
    };
  }

  return null;
}

function hhmmToMinutes(hhmm: string): number | null {
  const m = hhmm.trim().match(/^(\d{1,2}):(\d{2})$/);
  if (!m) return null;
  const h = parseInt(m[1] ?? '', 10);
  const mi = parseInt(m[2] ?? '', 10);
  if (
    !Number.isFinite(h) ||
    !Number.isFinite(mi) ||
    h < 0 ||
    h > 23 ||
    mi < 0 ||
    mi > 59
  )
    return null;
  return h * 60 + mi;
}

function buildMinuteIntervalList(startMinute: number, intervalMinutes: number): string | null {
  if (
    !Number.isInteger(startMinute) ||
    !Number.isInteger(intervalMinutes) ||
    startMinute < 0 ||
    startMinute > 59 ||
    intervalMinutes < 1 ||
    intervalMinutes > 59
  ) {
    return null;
  }
  const minutes: number[] = [];
  for (let minute = startMinute; minute <= 59; minute += intervalMinutes) {
    minutes.push(minute);
  }
  return minutes.join(',');
}

function jobScheduleSignature(job: Pick<CronJob, 'schedule' | 'expression'>): string {
  if (job.schedule.kind === 'cron') {
    return `cron:${job.schedule.expr ?? job.expression ?? ''}:${job.schedule.tz ?? ''}`;
  }
  if (job.schedule.kind === 'every') {
    return `every:${job.schedule.every_ms ?? ''}`;
  }
  return `at:${job.schedule.at ?? ''}`;
}

function normalizeWeekdays(weekdays: unknown): number[] | null {
  if (!Array.isArray(weekdays)) return null;
  const normalized = [...new Set(
    weekdays
      .filter((day): day is number => typeof day === 'number' && Number.isInteger(day) && day >= 1 && day <= 7),
  )].sort((a, b) => a - b);
  return normalized.length > 0 ? normalized : null;
}

function isObjectRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function readCronIntervalUiMetaStore(): Record<string, unknown> {
  if (typeof localStorage === 'undefined') return {};
  try {
    const raw = localStorage.getItem(CRON_INTERVAL_UI_META_STORAGE_KEY);
    if (!raw) return {};
    const parsed: unknown = JSON.parse(raw);
    return isObjectRecord(parsed) ? parsed : {};
  } catch {
    return {};
  }
}

function writeCronIntervalUiMetaStore(store: Record<string, unknown>): void {
  if (typeof localStorage === 'undefined') return;
  try {
    localStorage.setItem(CRON_INTERVAL_UI_META_STORAGE_KEY, JSON.stringify(store));
  } catch {
    // localStorage may be unavailable or full in restricted WebView contexts.
  }
}

export function readCronIntervalUiMeta(job: CronJob): IntervalScheduleUiState | null {
  const storedMeta = readCronIntervalUiMetaStore()[job.id];
  if (!isObjectRecord(storedMeta)) return null;
  const meta = storedMeta as Partial<CronIntervalUiMeta>;
  if (!meta || meta.scheduleSignature !== jobScheduleSignature(job)) return null;
  if (meta.unit !== 'minute' && meta.unit !== 'hour') return null;
  if (typeof meta.value !== 'string' || !/^\d+$/.test(meta.value)) return null;
  if (typeof meta.windowStart !== 'string' || typeof meta.windowEnd !== 'string') return null;
  const startMin = hhmmToMinutes(meta.windowStart);
  const endMin = hhmmToMinutes(meta.windowEnd);
  if (startMin === null || endMin === null || endMin < startMin) return null;
  const weekdays = normalizeWeekdays(meta.weekdays);
  if (!weekdays) return null;
  return {
    value: meta.value,
    unit: meta.unit,
    weekdays,
    windowStart: meta.windowStart,
    windowEnd: meta.windowEnd,
  };
}

export function writeCronIntervalUiMeta(job: CronJob, uiState: IntervalScheduleUiState | null): void {
  const store = readCronIntervalUiMetaStore();
  if (!uiState) {
    delete store[job.id];
    writeCronIntervalUiMetaStore(store);
    return;
  }
  const weekdays = normalizeWeekdays(uiState.weekdays);
  const startMin = hhmmToMinutes(uiState.windowStart);
  const endMin = hhmmToMinutes(uiState.windowEnd);
  if (!weekdays || startMin === null || endMin === null || endMin < startMin) return;
  store[job.id] = {
    scheduleSignature: jobScheduleSignature(job),
    value: uiState.value,
    unit: uiState.unit,
    weekdays,
    windowStart: uiState.windowStart,
    windowEnd: uiState.windowEnd,
  };
  writeCronIntervalUiMetaStore(store);
}

function removeCronIntervalUiMeta(jobId: string): void {
  const store = readCronIntervalUiMetaStore();
  delete store[jobId];
  writeCronIntervalUiMetaStore(store);
}

/** Interval UI → API schedule. Uses `every` only for all days + full-day window; else cron (weekdays + hour span). */
export function buildIntervalSchedule(
  val: number,
  unit: 'minute' | 'hour',
  weekdays: number[],
  winStart: string,
  winEnd: string,
): CronScheduleInput | null {
  const max = unit === 'hour' ? 23 : 59;
  if (!Number.isFinite(val) || val < 1 || val > max) return null;
  if (weekdays.length === 0) return null;
  const startMin = hhmmToMinutes(winStart);
  const endMin = hhmmToMinutes(winEnd);
  if (startMin === null || endMin === null) return null;
  if (endMin < startMin) return null;

  const fullDay = winStart === '00:00' && winEnd === '23:59';
  const fullWeek = weekdays.length === 7;
  if (fullDay && fullWeek) {
    const unitMs = unit === 'minute' ? 60000 : 3600000;
    return { kind: 'every', every_ms: val * unitMs };
  }

  const sm = startMin % 60;
  const em = endMin % 60;
  const sh = Math.floor(startMin / 60);
  const eh = Math.floor(endMin / 60);
  if (eh < sh) return null;
  const dow = weekdays
    .map(d => (d === 7 ? 0 : d))
    .sort((a, b) => a - b)
    .join(',');

  if (unit === 'minute') {
    const minField = buildMinuteIntervalList(sm, val);
    if (!minField) return null;
    const expr = `${minField} ${sh}-${eh} * * ${dow}`;
    return { kind: 'cron', expr };
  }
  const hourField = val === 1 ? `${sh}-${eh}` : `${sh}-${eh}/${val}`;
  const minField = sm === em ? String(sm) : `${sm}-${em}`;
  const expr = `${minField} ${hourField} * * ${dow}`;
  return { kind: 'cron', expr };
}

function buildFormSchedule(
  formScheduleKind: 'daily' | 'interval' | 'once' | 'custom',
  formDailyTime: string,
  formCustomTime: string,
  formCustomWeekdays: number[],
  formIntervalValue: string,
  formIntervalUnit: 'minute' | 'hour',
  formIntervalWeekdays: number[],
  formIntervalWindowStart: string,
  formIntervalWindowEnd: string,
  formOnceTime: string,
  formOnceDate: string,
): CronScheduleInput | null {
  if (formScheduleKind === 'daily') {
    const [hStr = '0', mStr = '0'] = formDailyTime.split(':');
    const h = parseInt(hStr, 10);
    const m = parseInt(mStr, 10);
    return { kind: 'cron', expr: `${m} ${h} * * *` };
  }
  if (formScheduleKind === 'custom') {
    if (formCustomWeekdays.length === 0) return null;
    const [hStr = '0', mStr = '0'] = formCustomTime.split(':');
    const h = parseInt(hStr, 10);
    const m = parseInt(mStr, 10);
    const days =
      formCustomWeekdays.length === 7
        ? '*'
        : formCustomWeekdays.map(d => (d === 7 ? 0 : d)).join(',');
    return { kind: 'cron', expr: `${m} ${h} * * ${days}` };
  }
  if (formScheduleKind === 'interval') {
    const val = parseInt(formIntervalValue, 10);
    return buildIntervalSchedule(
      val,
      formIntervalUnit,
      formIntervalWeekdays,
      formIntervalWindowStart,
      formIntervalWindowEnd,
    );
  }
  const atLocal = parseOnceScheduleDateTime(formOnceDate, formOnceTime);
  if (!atLocal) return null;
  return { kind: 'at', at: atLocal.toISOString() };
}

// ---------- Weekday Chips ----------
function WeekdayChips({
  weekdays,
  onChange,
}: {
  weekdays: number[];
  onChange: (next: number[]) => void;
}) {
  const days = [1, 2, 3, 4, 5, 6, 7];
  const keys: Record<number, string> = {
    1: 'cron.weekday_1',
    2: 'cron.weekday_2',
    3: 'cron.weekday_3',
    4: 'cron.weekday_4',
    5: 'cron.weekday_5',
    6: 'cron.weekday_6',
    7: 'cron.weekday_7',
  };
  return (
    <div style={{ display: 'flex', flexWrap: 'wrap', marginTop: 8 }}>
      {days.map((day, idx) => {
        const selected = weekdays.includes(day);
        const isLastSelected = selected && weekdays.length === 1;
        const dayKey = keys[day] ?? 'cron.weekday_1';
        const bgDefault = selected ? 'rgba(55, 144, 250, 0.1)' : '#e6e6e6';
        const bgHover = selected ? 'rgba(55, 144, 250, 0.15)' : '#dcdcdc';
        const bgActive = selected ? 'rgba(55, 144, 250, 0.2)' : '#b9b9b9';
        const txtColor = selected ? '#3790fa' : 'rgba(0, 0, 0, 0.85)';
        return (
          <React.Fragment key={day}>
            <button
              type="button"
              onClick={() => {
                if (selected) {
                  if (weekdays.length <= 1) return;
                  onChange(weekdays.filter(d => d !== day));
                  return;
                }
                onChange([...weekdays, day].sort((a, b) => a - b));
              }}
              onMouseEnter={e => (e.currentTarget.style.background = bgHover)}
              onMouseLeave={e => (e.currentTarget.style.background = bgDefault)}
              onMouseDown={e => (e.currentTarget.style.background = bgActive)}
              onMouseUp={e => (e.currentTarget.style.background = bgHover)}
              style={{
                boxSizing: 'border-box',
                display: 'inline-flex',
                alignItems: 'center',
                justifyContent: 'center',
                height: 36,
                padding: '0 8px',
                width: 'auto',
                borderRadius: 6,
                border: 'none',
                background: bgDefault,
                color: txtColor,
                fontSize: 14,
                fontWeight: 400,
                cursor: isLastSelected ? 'not-allowed' : 'pointer',
                transition: 'background 0.15s',
              }}
            >
              {t(dayKey)}
            </button>
            {idx < 6 && <span style={{ width: '8px', flexShrink: 0, display: 'inline-block' }} aria-hidden />}
          </React.Fragment>
        );
      })}
    </div>
  );
}

// ---------- Execution-time schedule kind (custom dropdown per design spec) ----------
const SCHEDULE_KIND_TEXT = 'rgba(0, 0, 0, 0.85)';
const CRON_FIELD_TEXT = 'rgba(0, 0, 0, 0.85)';

type CronFieldSurfaceState = {
  focused?: boolean;
  hovered?: boolean;
};

/** Default / hover / focus surface for cron modal task name & prompt inputs */
function cronFieldSurfaceStyle(
  state: CronFieldSurfaceState,
  extra?: React.CSSProperties,
): React.CSSProperties {
  const isActive = Boolean(state.focused);
  const isHovered = Boolean(state.hovered && !isActive);
  return {
    boxSizing: 'border-box',
    borderRadius: 6,
    border: isActive ? '2px solid #3790FA' : '1px solid transparent',
    background: isActive ? '#FFFFFF' : isHovered ? '#DCDCDC' : '#E6E6E6',
    outline: 'none',
    color: CRON_FIELD_TEXT,
    ...extra,
  };
}

const CRON_MODAL_LIST_ROW_H = 36;
const CRON_MODAL_LIST_PANEL_PAD = 16;

/** 弹窗内下拉：Portal + 空间不足时向上展开，避免被 overflow 裁剪 */
function CronModalListSelect({
  value,
  onChange,
  options,
  triggerStyle,
  containerStyle,
}: {
  value: string;
  onChange: (next: string) => void;
  options: ReadonlyArray<{ value: string; label: string }>;
  triggerStyle: React.CSSProperties;
  containerStyle?: React.CSSProperties;
}) {
  const [open, setOpen] = useState(false);
  const [dropUp, setDropUp] = useState(false);
  const [panelStyle, setPanelStyle] = useState<React.CSSProperties>({});
  const wrapRef = useRef<HTMLDivElement>(null);
  const triggerRef = useRef<HTMLButtonElement>(null);
  const panelRef = useRef<HTMLDivElement>(null);

  const panelEstHeight = options.length * CRON_MODAL_LIST_ROW_H + CRON_MODAL_LIST_PANEL_PAD;

  const updatePanelPosition = useCallback(() => {
    const trigger = triggerRef.current;
    if (!trigger) return;
    const rect = trigger.getBoundingClientRect();
    const gap = 4;
    const spaceBelow = window.innerHeight - rect.bottom - gap;
    const spaceAbove = rect.top - gap;
    const shouldDropUp = spaceBelow < panelEstHeight && spaceAbove >= spaceBelow;
    setDropUp(shouldDropUp);
    const top = shouldDropUp
      ? Math.max(8, rect.top - panelEstHeight - gap)
      : rect.bottom + gap;
    setPanelStyle({
      position: 'fixed',
      top,
      left: rect.left,
      width: rect.width,
      zIndex: 10100,
    });
  }, [panelEstHeight]);

  const toggleOpen = useCallback(() => {
    setOpen((wasOpen) => {
      if (!wasOpen) updatePanelPosition();
      return !wasOpen;
    });
  }, [updatePanelPosition]);

  useLayoutEffect(() => {
    if (!open) return;
    updatePanelPosition();
    const close = () => setOpen(false);
    window.addEventListener('resize', close);
    window.addEventListener('scroll', close, true);
    return () => {
      window.removeEventListener('resize', close);
      window.removeEventListener('scroll', close, true);
    };
  }, [open, updatePanelPosition]);

  useEffect(() => {
    if (!open) return;
    const handler = (e: MouseEvent) => {
      const target = e.target as Node | null;
      if (
        panelRef.current?.contains(target) ||
        triggerRef.current?.contains(target) ||
        wrapRef.current?.contains(target)
      ) {
        return;
      }
      setOpen(false);
    };
    const closeOnBlur = () => setOpen(false);
    document.addEventListener('mousedown', handler);
    window.addEventListener('blur', closeOnBlur);
    return () => {
      document.removeEventListener('mousedown', handler);
      window.removeEventListener('blur', closeOnBlur);
    };
  }, [open]);

  const activeLabel = options.find(o => o.value === value)?.label ?? (value !== '' ? value : '');

  const triggerMerged: React.CSSProperties = {
    ...triggerStyle,
    display: 'flex',
    alignItems: 'center',
    gap: 8,
    width: '100%',
    justifyContent: 'space-between',
    textAlign: 'left',
    cursor: 'pointer',
    fontFamily: 'inherit',
  };

  const wrapOuter: React.CSSProperties = {
    position: 'relative',
    width: '100%',
    ...containerStyle,
  };

  const listPanel = (
    <div
      ref={panelRef}
      className="model-dialog-dropdown cron-modal-list-dropdown box-border flex flex-col overflow-hidden"
      style={{
        ...panelStyle,
        padding: '8px 6px 8px 8px',
        borderRadius: 8,
        border: '1px solid #E5E7EB',
        background: '#FFFFFF',
        boxSizing: 'border-box',
      }}
      onMouseDown={e => e.stopPropagation()}
    >
      <div
        role="listbox"
        className="flex w-full flex-col"
        style={{ width: '100%' }}
        onMouseDown={e => e.stopPropagation()}
      >
        {options.map((opt, i) => {
          const selected = value === opt.value;
          return (
            <ModelDropdownOption
              key={`${opt.value}:${i}:${opt.label}`}
              selected={selected}
              reserveCheckmarkSlot
              fullWidth
              onPick={() => {
                onChange(opt.value);
                setOpen(false);
              }}
            >
              {opt.label}
            </ModelDropdownOption>
          );
        })}
      </div>
    </div>
  );

  return (
    <div ref={wrapRef} style={wrapOuter}>
      <button
        ref={triggerRef}
        type="button"
        aria-expanded={open}
        aria-haspopup="listbox"
        onClick={toggleOpen}
        style={triggerMerged}
      >
        <span
          style={{
            overflow: 'hidden',
            textOverflow: 'ellipsis',
            whiteSpace: 'nowrap',
            flex: 1,
            minWidth: 0,
            color: SCHEDULE_KIND_TEXT,
          }}
        >
          {activeLabel}
        </span>
        {open && dropUp ? (
          <ChevronUp size={16} style={{ flexShrink: 0, color: SCHEDULE_KIND_TEXT }} aria-hidden />
        ) : (
          <ChevronDown size={16} style={{ flexShrink: 0, color: SCHEDULE_KIND_TEXT }} aria-hidden />
        )}
      </button>

      {open && createPortal(listPanel, document.body)}
    </div>
  );
}

function ScheduleKindSelect({
  value,
  onChange,
  triggerStyle,
}: {
  value: 'daily' | 'interval' | 'once' | 'custom';
  onChange: (next: 'daily' | 'interval' | 'once' | 'custom') => void;
  triggerStyle: React.CSSProperties;
}) {
  return (
    <CronModalListSelect
      value={value}
      onChange={next =>
        next === 'daily' || next === 'interval' || next === 'once' || next === 'custom'
          ? onChange(next)
          : undefined
      }
      triggerStyle={triggerStyle}
      options={[
        { value: 'daily', label: t('cron.schedule_daily_label') },
        { value: 'interval', label: t('cron.schedule_interval_label') },
        { value: 'once', label: t('cron.schedule_once_label') },
        { value: 'custom', label: t('cron.schedule_custom_label') },
      ]}
    />
  );
}

/** Minute / hour picker for interval mode — execution-time dropdown visuals */
function IntervalUnitSelect({
  value,
  onChange,
  triggerStyle,
}: {
  value: 'minute' | 'hour';
  onChange: (next: 'minute' | 'hour') => void;
  triggerStyle: React.CSSProperties;
}) {
  return (
    <CronModalListSelect
      value={value}
      containerStyle={{ flex: 1, minWidth: 0 }}
      onChange={next => (next === 'minute' || next === 'hour' ? onChange(next) : undefined)}
      triggerStyle={triggerStyle}
      options={[
        { value: 'minute', label: t('cron.interval_unit_minute') },
        { value: 'hour', label: t('cron.interval_unit_hour') },
      ]}
    />
  );
}

// ---------- Time Spinner (QTimeEdit style, HH:MM only) ----------
function TimeSpinner({
  value,
  onChange,
  style,
}: {
  value: string;
  onChange: (v: string) => void;
  style?: React.CSSProperties;
}) {
  const [focused, setFocused] = useState(false);
  const [segment, setSegment] = useState<'h' | 'm'>('h');
  const [pendingDigit, setPendingDigit] = useState<string | null>(null);
  const containerRef = useRef<HTMLDivElement>(null);

  const parts = value.split(':');
  const hVal = Math.min(23, Math.max(0, parseInt(parts[0] ?? '0', 10) || 0));
  const mVal = Math.min(59, Math.max(0, parseInt(parts[1] ?? '0', 10) || 0));
  const hh = String(hVal).padStart(2, '0');
  const mm = String(mVal).padStart(2, '0');

  const emit = (h: number, m: number) =>
    onChange(
      `${String(((h % 24) + 24) % 24).padStart(2, '0')}:${String(((m % 60) + 60) % 60).padStart(2, '0')}`,
    );

  const bump = (delta: number) => {
    if (segment === 'h') emit(hVal + delta, mVal);
    else emit(hVal, mVal + delta);
  };

  const handleDigit = (digit: string) => {
    const d = parseInt(digit, 10);
    if (segment === 'h') {
      if (pendingDigit === null) {
        emit(d, mVal);
        if (d >= 3) { setPendingDigit(null); setSegment('m'); }
        else setPendingDigit(digit);
      } else {
        const combined = parseInt(pendingDigit, 10) * 10 + d;
        emit(combined <= 23 ? combined : d, mVal);
        setPendingDigit(null);
        setSegment('m');
      }
    } else {
      if (pendingDigit === null) {
        emit(hVal, d);
        if (d >= 6) setPendingDigit(null);
        else setPendingDigit(digit);
      } else {
        const combined = parseInt(pendingDigit, 10) * 10 + d;
        emit(hVal, combined <= 59 ? combined : d);
        setPendingDigit(null);
      }
    }
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    switch (e.key) {
      case 'ArrowUp': e.preventDefault(); bump(1); break;
      case 'ArrowDown': e.preventDefault(); bump(-1); break;
      case 'ArrowLeft': e.preventDefault(); setSegment('h'); setPendingDigit(null); break;
      case 'ArrowRight': e.preventDefault(); setSegment('m'); setPendingDigit(null); break;
      default:
        if (/^\d$/.test(e.key)) { e.preventDefault(); handleDigit(e.key); }
    }
  };

  const segStyle = (active: boolean): React.CSSProperties => ({
    padding: '0 3px',
    borderRadius: 2,
    background: focused && active ? '#3790FA' : 'transparent',
    color: focused && active ? '#fff' : 'rgba(0,0,0,0.85)',
    fontSize: 13,
    minWidth: 22,
    textAlign: 'center' as const,
    display: 'inline-block',
    lineHeight: '22px',
    cursor: 'default',
  });

  return (
    <div
      ref={containerRef}
      tabIndex={0}
      onFocus={() => setFocused(true)}
      onBlur={e => {
        if (!containerRef.current?.contains(e.relatedTarget as Node)) {
          setFocused(false);
          setPendingDigit(null);
        }
      }}
      onKeyDown={handleKeyDown}
      style={{
        ...style,
        display: 'flex',
        alignItems: 'center',
        paddingLeft: 12,
        paddingRight: 36,
        position: 'relative',
        cursor: 'default',
        outline: 'none',
        border: focused ? '1px solid #3790FA' : '1px solid transparent',
        borderRadius: 6,
        background: '#E6E6E6',
        height: 36,
        boxSizing: 'border-box',
        userSelect: 'none',
      }}
    >
      <span style={segStyle(segment === 'h')} onClick={() => { setSegment('h'); containerRef.current?.focus(); }}>
        {hh}
      </span>
      <span style={{ fontSize: 13, color: 'rgba(0,0,0,0.85)', padding: '0 1px' }}>:</span>
      <span style={segStyle(segment === 'm')} onClick={() => { setSegment('m'); containerRef.current?.focus(); }}>
        {mm}
      </span>
      <div
        style={{
          position: 'absolute',
          right: 8,
          top: 0,
          bottom: 0,
          width: 16,
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
        }}
      >
        <button
          type="button"
          tabIndex={-1}
          onClick={e => { e.preventDefault(); bump(1); containerRef.current?.focus(); }}
          aria-label={t('cron.daily_time_step_up')}
          style={{ padding: 0, margin: 0, border: 'none', background: 'transparent', cursor: 'pointer', height: 18, width: 16, display: 'flex', alignItems: 'center', justifyContent: 'center', flexShrink: 0, color: SCHEDULE_KIND_TEXT }}
        >
          <ChevronUp size={16} strokeWidth={2} aria-hidden />
        </button>
        <button
          type="button"
          tabIndex={-1}
          onClick={e => { e.preventDefault(); bump(-1); containerRef.current?.focus(); }}
          aria-label={t('cron.daily_time_step_down')}
          style={{ padding: 0, margin: 0, border: 'none', background: 'transparent', cursor: 'pointer', height: 18, width: 16, display: 'flex', alignItems: 'center', justifyContent: 'center', flexShrink: 0, color: SCHEDULE_KIND_TEXT }}
        >
          <ChevronDown size={16} strokeWidth={2} aria-hidden />
        </button>
      </div>
    </div>
  );
}

/** Single-run date field: `YYYY/MM/DD` display, click opens month grid (Mon-first). Value remains `YYYY-MM-DD`. */
function OnceDatePicker({
  value,
  onChange,
  style,
}: {
  value: string;
  onChange: (v: string) => void;
  style?: React.CSSProperties;
}) {
  const mt = /^(\d{4})-(\d{2})-(\d{2})$/.exec(value.trim());
  const selY = mt ? parseInt(mt[1]!, 10) : new Date().getFullYear();
  const selMo = mt ? parseInt(mt[2]!, 10) : new Date().getMonth() + 1;
  const selD = mt ? parseInt(mt[3]!, 10) : new Date().getDate();

  const wrapRef = useRef<HTMLDivElement>(null);
  const panelRef = useRef<HTMLDivElement>(null);
  const [open, setOpen] = useState(false);
  const [popoverStyle, setPopoverStyle] = useState<React.CSSProperties>({});
  const [viewY, setViewY] = useState(selY);
  const [viewM, setViewM] = useState(selMo);

  const display = `${String(selY).padStart(4, '0')}/${String(selMo).padStart(2, '0')}/${String(selD).padStart(2, '0')}`;

  useEffect(() => {
    if (open) {
      setViewY(selY);
      setViewM(selMo);
    }
  }, [open, selY, selMo]);

  const updatePopoverPosition = useCallback(() => {
    const r = wrapRef.current?.getBoundingClientRect();
    if (!r) return;
    const gap = 4;
    setPopoverStyle({
      position: 'fixed',
      right: window.innerWidth - r.right,
      bottom: window.innerHeight - r.top + gap,
      width: 'min(300px, calc(100vw - 32px))',
      zIndex: 10100,
      background: '#fff',
      borderRadius: 12,
      boxShadow: '0 8px 28px rgba(0,0,0,0.14)',
      padding: '12px 12px 14px',
      boxSizing: 'border-box',
    });
  }, []);

  useLayoutEffect(() => {
    if (!open) return;
    updatePopoverPosition();
  }, [open, updatePopoverPosition]);

  useEffect(() => {
    if (!open) return;
    window.addEventListener('resize', updatePopoverPosition);
    window.addEventListener('scroll', updatePopoverPosition, true);
    return () => {
      window.removeEventListener('resize', updatePopoverPosition);
      window.removeEventListener('scroll', updatePopoverPosition, true);
    };
  }, [open, updatePopoverPosition]);

  useEffect(() => {
    if (!open) return;
    const onDoc = (e: MouseEvent) => {
      const target = e.target as Node;
      if (wrapRef.current?.contains(target) || panelRef.current?.contains(target)) return;
      setOpen(false);
    };
    const closeOnBlur = () => setOpen(false);
    document.addEventListener('mousedown', onDoc);
    window.addEventListener('blur', closeOnBlur);
    return () => {
      document.removeEventListener('mousedown', onDoc);
      window.removeEventListener('blur', closeOnBlur);
    };
  }, [open]);

  const cells = useMemo(() => {
    const first = new Date(viewY, viewM - 1, 1);
    const startPad = (first.getDay() + 6) % 7;
    const daysInMonth = new Date(viewY, viewM, 0).getDate();
    const out: { date: Date; inMonth: boolean }[] = [];
    for (let i = startPad; i > 0; i--) {
      out.push({ date: new Date(viewY, viewM - 1, 1 - i), inMonth: false });
    }
    for (let d = 1; d <= daysInMonth; d++) {
      out.push({ date: new Date(viewY, viewM - 1, d), inMonth: true });
    }
    let cur = new Date(viewY, viewM - 1, daysInMonth);
    while (out.length % 7 !== 0) {
      cur.setDate(cur.getDate() + 1);
      out.push({ date: new Date(cur), inMonth: false });
    }
    while (out.length < 42) {
      cur.setDate(cur.getDate() + 1);
      out.push({ date: new Date(cur), inMonth: false });
    }
    return out;
  }, [viewY, viewM]);

  const shiftMonth = (delta: number) => {
    const d = new Date(viewY, viewM - 1 + delta, 1);
    setViewY(d.getFullYear());
    setViewM(d.getMonth() + 1);
  };

  const shiftYear = (delta: number) => {
    setViewY(y => y + delta);
  };

  const headerCenter = `${viewY}/${viewM}/${selD}`;

  const isSameDay = (a: Date, y: number, m: number, day: number) =>
    a.getFullYear() === y && a.getMonth() + 1 === m && a.getDate() === day;

  const navBtn: React.CSSProperties = {
    padding: 4,
    border: 'none',
    background: 'transparent',
    cursor: 'pointer',
    borderRadius: 4,
    color: SCHEDULE_KIND_TEXT,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
  };

  return (
    <div ref={wrapRef} style={{ ...style, position: 'relative', minWidth: 0 }}>
      <button
        type="button"
        onClick={() => {
          setOpen(o => {
            if (!o) updatePopoverPosition();
            return !o;
          });
        }}
        aria-label={t('cron.once_open_calendar')}
        aria-expanded={open}
        style={{
          width: '100%',
          height: 36,
          boxSizing: 'border-box',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          paddingLeft: 12,
          paddingRight: 10,
          border: open ? '1px solid #3790FA' : '1px solid transparent',
          borderRadius: 6,
          background: '#E6E6E6',
          cursor: 'pointer',
          fontSize: 13,
          color: 'rgba(0,0,0,0.85)',
          outline: 'none',
          textAlign: 'left',
          fontFamily: 'inherit',
        }}
      >
        <span>{display}</span>
        <img
          src={`${assetBasePath}/icons/ukui-datetime-symbolic.svg`}
          width={18}
          height={18}
          alt=""
          aria-hidden
          style={{ flexShrink: 0, display: 'block', opacity: 0.45, objectFit: 'contain' }}
        />
      </button>

      {open &&
        createPortal(
          <div ref={panelRef} style={popoverStyle} onMouseDown={e => e.stopPropagation()}>
          <div
            style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              marginBottom: 10,
              gap: 4,
            }}
          >
            <button type="button" style={navBtn} aria-label={t('cron.cal_prev_year')} onClick={() => shiftYear(-1)}>
              <ChevronsLeft size={18} strokeWidth={2} />
            </button>
            <button type="button" style={navBtn} aria-label={t('cron.cal_prev_month')} onClick={() => shiftMonth(-1)}>
              <ChevronLeft size={18} strokeWidth={2} />
            </button>
            <span style={{ fontSize: 14, color: 'rgba(0,0,0,0.85)', fontWeight: 500, flex: 1, textAlign: 'center' }}>
              {headerCenter}
            </span>
            <button type="button" style={navBtn} aria-label={t('cron.cal_next_month')} onClick={() => shiftMonth(1)}>
              <ChevronRight size={18} strokeWidth={2} />
            </button>
            <button type="button" style={navBtn} aria-label={t('cron.cal_next_year')} onClick={() => shiftYear(1)}>
              <ChevronsRight size={18} strokeWidth={2} />
            </button>
          </div>

          <div
            style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(7, 1fr)',
              gap: 2,
              textAlign: 'center',
              fontSize: 12,
              color: 'rgba(0,0,0,0.45)',
              marginBottom: 6,
            }}
          >
            {([1, 2, 3, 4, 5, 6, 7] as const).map(d => (
              <span key={d}>{t(`cron.cal_head_${d}` as 'cron.cal_head_1')}</span>
            ))}
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(7, 1fr)', gap: 4 }}>
            {cells.map(({ date, inMonth }, idx) => {
              const d = date.getDate();
              const selected = isSameDay(date, selY, selMo, selD);
              return (
                <button
                  key={`${idx}-${date.getTime()}`}
                  type="button"
                  onClick={() => {
                    onChange(formatLocalYmd(date));
                    setOpen(false);
                  }}
                  style={{
                    height: 32,
                    border: 'none',
                    borderRadius: 6,
                    cursor: 'pointer',
                    fontSize: 13,
                    fontWeight: selected ? 600 : 400,
                    background: selected ? '#3790FA' : 'transparent',
                    color: selected ? '#fff' : inMonth ? 'rgba(0,0,0,0.85)' : 'rgba(0,0,0,0.28)',
                  }}
                >
                  {d}
                </button>
              );
            })}
          </div>
          </div>,
          document.body,
        )}
    </div>
  );
}

// ---------- Add/Edit Modal ----------
function CronModal({
  job,
  onClose,
  onSaved,
}: {
  job: CronJob | 'add';
  onClose: () => void;
  onSaved: (updated: CronJob) => void;
}) {
  const isEdit = job !== 'add';
  const existing = isEdit ? (job as CronJob) : null;

  // Derive initial values from existing job
  const initKind = (): 'daily' | 'interval' | 'once' | 'custom' => {
    if (!existing) return 'daily';
    if (existing.schedule.kind === 'cron') {
      const ex = existing.schedule.expr ?? '';
      if (isIntervalCronExpr(ex)) return 'interval';
      const hasTz = Boolean(existing.schedule.tz && String(existing.schedule.tz).trim());
      if (!isSimpleDailyCronExpr(ex) || hasTz) return 'custom';
      const parts = ex.trim().split(/\s+/);
      const dow = parts[4] ?? '*';
      return dow === '*' ? 'daily' : 'custom';
    }
    if (existing.schedule.kind === 'every') return 'interval';
    return 'once';
  };

  // Daily: time only (`m h * * *`). Custom: time + weekdays (same as pre-change「每天」).
  const cronParsed =
    existing?.schedule.kind === 'cron' && existing.schedule.expr
      ? parseCronExprToUI(existing.schedule.expr)
      : null;

  const intervalParsedFromSchedule =
    existing?.schedule.kind === 'cron' && existing.schedule.expr && isIntervalCronExpr(existing.schedule.expr)
      ? parseIntervalCronExpr(existing.schedule.expr)
      : null;
  const intervalParsed = (existing ? readCronIntervalUiMeta(existing) : null) ?? intervalParsedFromSchedule;

  const initIntervalValue = (): string => {
    if (existing?.schedule.kind === 'every' && existing.schedule.every_ms) {
      const ms = existing.schedule.every_ms;
      if (ms % 3600000 === 0) return String(ms / 3600000);
      if (ms % 60000 === 0) return String(ms / 60000);
      return String(Math.max(1, Math.round(ms / 60000)));
    }
    if (intervalParsed) return intervalParsed.value;
    return '5';
  };

  const initIntervalUnit = (): 'minute' | 'hour' => {
    if (existing?.schedule.kind === 'every' && existing.schedule.every_ms) {
      const ms = existing.schedule.every_ms;
      if (ms % 3600000 === 0) return 'hour';
      return 'minute';
    }
    if (intervalParsed) return intervalParsed.unit;
    return 'minute';
  };

  // Form state
  const [formName, setFormName] = useState(() =>
    clampToMaxChars(normalizeTaskNameInput(existing?.name ?? ''), TASK_NAME_MAX_LEN),
  );
  const [formPrompt, setFormPrompt] = useState(() =>
    clampToMaxChars(existing?.prompt ?? existing?.command ?? '', TASK_PROMPT_MAX_LEN),
  );
  const [nameOverflow, setNameOverflow] = useState(false);
  const [promptOverflow, setPromptOverflow] = useState(false);
  const [formScheduleKind, setFormScheduleKind] = useState<
    'daily' | 'interval' | 'once' | 'custom'
  >(initKind());

  const [formDailyTime, setFormDailyTime] = useState(
    cronParsed?.time ?? getCronCurrentTimeHHMM(),
  );
  const [formCustomTime, setFormCustomTime] = useState(
    cronParsed?.time ?? getCronCurrentTimeHHMM(),
  );
  const [formCustomWeekdays, setFormCustomWeekdays] = useState<number[]>(
    cronParsed?.weekdays ?? [1, 2, 3, 4, 5, 6, 7],
  );
  // Interval
  const [formIntervalValue, setFormIntervalValue] = useState(initIntervalValue());
  const [formIntervalUnit, setFormIntervalUnit] = useState<'minute' | 'hour'>(initIntervalUnit());
  const [formIntervalWeekdays, setFormIntervalWeekdays] = useState<number[]>(
    intervalParsed?.weekdays ?? [1, 2, 3, 4, 5, 6, 7],
  );
  const [formIntervalWindowStart, setFormIntervalWindowStart] = useState(
    intervalParsed?.windowStart ?? '00:00',
  );
  const [formIntervalWindowEnd, setFormIntervalWindowEnd] = useState(
    intervalParsed?.windowEnd ?? '23:59',
  );
  const initOnceTime = (): string => {
    if (existing?.schedule.kind === 'at' && existing.schedule.at) {
      const d = new Date(existing.schedule.at);
      return `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`;
    }
    return getCronCurrentTimeHHMM();
  };

  const initOnceDate = (): string => {
    if (existing?.schedule.kind === 'at' && existing.schedule.at) {
      return formatLocalYmd(new Date(existing.schedule.at));
    }
    return formatLocalYmd(new Date());
  };

  // Once — separate time + date (defaults: current local time / date)
  const [formOnceTime, setFormOnceTime] = useState(initOnceTime());
  const [formOnceDate, setFormOnceDate] = useState(initOnceDate());
  const [formError, setFormError] = useState<string | null>(null);
  const [submitting, setSubmitting] = useState(false);
  const [nameFocused, setNameFocused] = useState(false);
  const [nameHovered, setNameHovered] = useState(false);
  const [promptFocused, setPromptFocused] = useState(false);
  const [promptHovered, setPromptHovered] = useState(false);

  // Dirty detection: capture initial snapshot for edit mode
  const initialSnapshot = useRef({
    name: existing?.name ?? '',
    prompt: existing?.prompt ?? existing?.command ?? '',
    scheduleKind: initKind(),
    dailyTime: cronParsed?.time ?? getCronCurrentTimeHHMM(),
    customTime: cronParsed?.time ?? getCronCurrentTimeHHMM(),
    customWeekdays: [...(cronParsed?.weekdays ?? [1, 2, 3, 4, 5, 6, 7])],
    intervalValue: initIntervalValue(),
    intervalUnit: initIntervalUnit(),
    intervalWeekdays: intervalParsed?.weekdays ?? [1, 2, 3, 4, 5, 6, 7],
    intervalWindowStart: intervalParsed?.windowStart ?? '00:00',
    intervalWindowEnd: intervalParsed?.windowEnd ?? '23:59',
    onceTime: initOnceTime(),
    onceDate: initOnceDate(),
  });

  const onceTimePast = useMemo(
    () =>
      formScheduleKind === 'once' &&
      isOnceScheduleInPast(formOnceDate, formOnceTime),
    [formScheduleKind, formOnceDate, formOnceTime],
  );

  const isScheduleValid = useMemo(
    () =>
      buildFormSchedule(
        formScheduleKind,
        formDailyTime,
        formCustomTime,
        formCustomWeekdays,
        formIntervalValue,
        formIntervalUnit,
        formIntervalWeekdays,
        formIntervalWindowStart,
        formIntervalWindowEnd,
        formOnceTime,
        formOnceDate,
      ) !== null,
    [
      formScheduleKind,
      formDailyTime,
      formCustomTime,
      formCustomWeekdays,
      formIntervalValue,
      formIntervalUnit,
      formIntervalWeekdays,
      formIntervalWindowStart,
      formIntervalWindowEnd,
      formOnceTime,
      formOnceDate,
    ],
  );

  const canSubmit = useMemo(() => {
    if (!isScheduleValid) return false;
    if (onceTimePast) return false;
    if (!isEdit) {
      // Add mode: enabled only when name and prompt are both non-empty
      return formName.trim().length > 0 && formPrompt.trim().length > 0;
    }
    const s = initialSnapshot.current;
    return (
      formName !== s.name ||
      formPrompt !== s.prompt ||
      formScheduleKind !== s.scheduleKind ||
      formDailyTime !== s.dailyTime ||
      formCustomTime !== s.customTime ||
      formCustomWeekdays.length !== s.customWeekdays.length ||
      formCustomWeekdays.some((v, i) => v !== s.customWeekdays[i]) ||
      formIntervalValue !== s.intervalValue ||
      formIntervalUnit !== s.intervalUnit ||
      formIntervalWeekdays.length !== s.intervalWeekdays.length ||
      formIntervalWeekdays.some((v, i) => v !== s.intervalWeekdays[i]) ||
      formIntervalWindowStart !== s.intervalWindowStart ||
      formIntervalWindowEnd !== s.intervalWindowEnd ||
      formOnceTime !== s.onceTime ||
      formOnceDate !== s.onceDate
    );
  }, [
    isScheduleValid,
    onceTimePast,
    isEdit,
    formName,
    formPrompt,
    formScheduleKind,
    formDailyTime,
    formCustomTime,
    formCustomWeekdays,
    formIntervalValue,
    formIntervalUnit,
    formIntervalWeekdays,
    formIntervalWindowStart,
    formIntervalWindowEnd,
    formOnceTime,
    formOnceDate,
  ]);


  const handleSubmit = async () => {
    if (!formPrompt.trim()) {
      setFormError(t('cron.validation_error'));
      return;
    }
    const schedule = buildFormSchedule(
      formScheduleKind,
      formDailyTime,
      formCustomTime,
      formCustomWeekdays,
      formIntervalValue,
      formIntervalUnit,
      formIntervalWeekdays,
      formIntervalWindowStart,
      formIntervalWindowEnd,
      formOnceTime,
      formOnceDate,
    );
    if (!schedule) return;

    setSubmitting(true);
    setFormError(null);
    try {
      const payload = {
        name: formName.trim() || undefined,
        schedule,
        prompt: formPrompt.trim(),
        job_type: 'agent' as const,
        session_target: 'main' as const,
        // 新建任务默认开启；编辑时保留原有 enabled 状态，不随保存被改写
        ...(isEdit ? {} : { enabled: true as const }),
      };
      let result: CronJob;
      if (isEdit) {
        result = await patchCronJob((existing as CronJob).id, payload);
      } else {
        result = await addCronJob(payload);
      }
      writeCronIntervalUiMeta(
        result,
        formScheduleKind === 'interval'
          ? {
              value: formIntervalValue,
              unit: formIntervalUnit,
              weekdays: formIntervalWeekdays,
              windowStart: formIntervalWindowStart,
              windowEnd: formIntervalWindowEnd,
            }
          : null,
      );
      onSaved(result);
    } catch (err) {
      setFormError(
        err instanceof Error ? err.message : t(isEdit ? 'cron.edit_error' : 'cron.add_error'),
      );
    } finally {
      setSubmitting(false);
    }
  };

  // Shared styles
  const fieldLabel: React.CSSProperties = {
    display: 'block',
    fontSize: 14,
    color: 'rgba(0,0,0,0.85)',
    marginBottom: 6,
    fontWeight: 400,
  };
  const fieldErrorStyle: React.CSSProperties = {
    marginTop: 4,
    fontSize: 12,
    fontWeight: 400,
    color: '#F53F3F',
    lineHeight: '16px',
    minHeight: 16,
  };
  const fieldInputStyle = (
    focused: boolean,
    hovered: boolean,
  ): React.CSSProperties => ({
    ...cronFieldSurfaceStyle({ focused, hovered }),
    display: 'block',
    width: '100%',
    height: 36,
    padding: focused ? '0 11px' : '0 12px',
    fontSize: 13,
  });
  const fieldTextareaStyle = (focused: boolean, hovered: boolean): React.CSSProperties => ({
    ...cronFieldSurfaceStyle({ focused, hovered }),
    display: 'block',
    width: '100%',
    height: 120,
    padding: focused ? '7px 11px' : '8px 12px',
    resize: 'none',
    lineHeight: '1.5',
    fontSize: 13,
  });
  const inputBase: React.CSSProperties = {
    display: 'block',
    width: '100%',
    boxSizing: 'border-box',
    height: 36,
    padding: '0 12px',
    border: 'none',
    borderRadius: 6,
    fontSize: 13,
    color: 'rgba(0,0,0,0.85)',
    background: '#E6E6E6',
    outline: 'none',
  };
  const selStyle: React.CSSProperties = {
    ...inputBase,
    padding: '0 8px 0 12px',
    cursor: 'pointer',
    appearance: 'none',
  };

  const intervalValueMin = 1;
  const intervalValueMax = formIntervalUnit === 'hour' ? 23 : 59;
  const intervalValueNum = parseInt(formIntervalValue, 10);
  const intervalAtMin =
    formIntervalValue === '' || !Number.isFinite(intervalValueNum) || intervalValueNum <= intervalValueMin;
  const intervalAtMax = Number.isFinite(intervalValueNum) && intervalValueNum >= intervalValueMax;
  const bumpIntervalValue = (delta: number) => {
    const raw = parseInt(formIntervalValue, 10);
    const base = Number.isFinite(raw) && raw >= intervalValueMin ? raw : intervalValueMin;
    setFormIntervalValue(
      String(Math.min(intervalValueMax, Math.max(intervalValueMin, base + delta))),
    );
  };

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        width: '100vw',
        height: '100vh',
        background: 'transparent',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 200,
      }}
    >
      <style>{`
        .cron-add-task-modal input::-webkit-input-placeholder,
        .cron-add-task-modal textarea::-webkit-input-placeholder {
          color: rgba(0, 0, 0, 0.35) !important;
          -webkit-text-fill-color: rgba(0, 0, 0, 0.35) !important;
          opacity: 1 !important;
        }
        .cron-add-task-modal input::-moz-placeholder,
        .cron-add-task-modal textarea::-moz-placeholder {
          color: rgba(0, 0, 0, 0.35) !important;
          opacity: 1 !important;
        }
        .cron-add-task-modal input:-ms-input-placeholder,
        .cron-add-task-modal textarea:-ms-input-placeholder {
          color: rgba(0, 0, 0, 0.35) !important;
        }
        .cron-add-task-modal input::placeholder,
        .cron-add-task-modal textarea::placeholder {
          color: rgba(0, 0, 0, 0.35) !important;
          opacity: 1 !important;
        }
        .cron-add-task-modal input.cron-interval-number-input::-webkit-outer-spin-button,
        .cron-add-task-modal input.cron-interval-number-input::-webkit-inner-spin-button {
          -webkit-appearance: none;
          margin: 0;
        }
        .cron-add-task-modal input.cron-interval-number-input {
          appearance: none;
          -moz-appearance: textfield;
        }
      `}</style>
      <div
        className="cron-add-task-modal"
        style={{
          background: '#fff',
          borderRadius: 14,
          width: 424,
          maxHeight: 520,
          display: 'flex',
          flexDirection: 'column',
          boxShadow: '0 8px 32px rgba(0,0,0,0.18)',
          transform: 'translate(-28px, -40px)',
        }}
      >
        {/* Header — layout aligned with ModelDialog title bar */}
        <div
          style={{
            height: 40,
            flexShrink: 0,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            paddingLeft: 12,
            paddingRight: 8,
            boxSizing: 'border-box',
          }}
        >
          <div
            style={{
              display: 'flex',
              flexDirection: 'row',
              alignItems: 'center',
              alignSelf: 'stretch',
              flex: '1 1 auto',
              minWidth: 0,
              lineHeight: 0,
            }}
          >
            <span
              style={{
                display: 'inline-flex',
                flexShrink: 0,
                alignItems: 'center',
                justifyContent: 'center',
                alignSelf: 'center',
              }}
            >
              <img
                src={`${assetBasePath}/icons/logo.png`}
                alt=""
                width={24}
                height={24}
                style={{ width: 24, height: 24, objectFit: 'contain', display: 'block' }}
                onError={(e) => { (e.target as HTMLImageElement).style.display = 'none'; }}
              />
            </span>
            <span
              style={{
                display: 'inline-flex',
                alignItems: 'center',
                alignSelf: 'center',
                marginLeft: 8,
                lineHeight: '20px',
                fontSize: 14,
                fontWeight: 400,
                color: 'rgba(0,0,0,0.85)',
              }}
            >
              {isEdit ? t('cron.edit_modal_title') : t('cron.add_modal_title')}
            </span>
          </div>
          <ModelCloseButton onClick={onClose} />
        </div>

        {/* Body */}
        <div style={{ padding: '0 24px 4px', overflowY: 'auto', flex: 1 }}>
          {formError && (
            <div
              style={{
                background: '#fff2f0',
                border: '1px solid #ffa39e',
                borderRadius: 6,
                padding: '8px 12px',
                marginBottom: 12,
                fontSize: 13,
                color: '#cf1322',
              }}
            >
              {formError}
            </div>
          )}
          <div style={{ display: 'flex', flexDirection: 'column', gap: 16, paddingBottom: 8 }}>
            {/* 任务名称 */}
            <div>
              <label style={fieldLabel}>{t('cron.task_name')}</label>
              <input
                type="text"
                value={formName}
                onChange={(e) => {
                  const normalized = normalizeTaskNameInput(e.target.value);
                  const { value, overflow } = applyCharLimit(normalized, TASK_NAME_MAX_LEN);
                  setNameOverflow(overflow);
                  setFormName(value);
                }}
                onFocus={() => setNameFocused(true)}
                onBlur={() => setNameFocused(false)}
                onMouseEnter={() => setNameHovered(true)}
                onMouseLeave={() => setNameHovered(false)}
                placeholder={t('cron.task_name_placeholder')}
                style={fieldInputStyle(nameFocused, nameHovered)}
              />
              <div style={fieldErrorStyle}>
                {nameOverflow ? t('cron.name_max_len') : '\u00A0'}
              </div>
            </div>

            {/* 任务指令 */}
            <div>
              <label style={fieldLabel}>{t('cron.task_prompt_label')}</label>
              <textarea
                value={formPrompt}
                onChange={(e) => {
                  const { value, overflow } = applyCharLimit(e.target.value, TASK_PROMPT_MAX_LEN);
                  setPromptOverflow(overflow);
                  setFormPrompt(value);
                }}
                onFocus={() => setPromptFocused(true)}
                onBlur={() => setPromptFocused(false)}
                onMouseEnter={() => setPromptHovered(true)}
                onMouseLeave={() => setPromptHovered(false)}
                placeholder={t('cron.task_prompt_placeholder')}
                style={fieldTextareaStyle(promptFocused, promptHovered)}
              />
              <div style={fieldErrorStyle}>
                {promptOverflow ? t('cron.prompt_max_len') : '\u00A0'}
              </div>
            </div>

            {/* 执行时间 */}
            <div>
              <label style={fieldLabel}>{t('cron.exec_time')}</label>
              <ScheduleKindSelect
                value={formScheduleKind}
                onChange={setFormScheduleKind}
                triggerStyle={selStyle}
              />

              {/* Daily: time of day, every calendar day */}
              {formScheduleKind === 'daily' && (
                <div style={{ marginTop: 8 }}>
                  <TimeSpinner value={formDailyTime} onChange={setFormDailyTime} style={{ width: '100%' }} />
                </div>
              )}

              {/* Custom: time + weekdays (legacy「每天」controls) */}
              {formScheduleKind === 'custom' && (
                <div style={{ marginTop: 8 }}>
                  <TimeSpinner value={formCustomTime} onChange={setFormCustomTime} style={{ width: '100%' }} />
                  <WeekdayChips weekdays={formCustomWeekdays} onChange={setFormCustomWeekdays} />
                </div>
              )}

              {/* Interval: 每间隔 + stepped number + unit dropdown + weekdays */}
              {formScheduleKind === 'interval' && (
                <div style={{ marginTop: 8, display: 'flex', flexDirection: 'column' }}>
                  <div
                    style={{
                      display: 'flex',
                      alignItems: 'center',
                      width: '100%',
                      flexWrap: 'nowrap',
                    }}
                  >
                    <span
                      style={{
                        fontSize: 14,
                        color: 'rgba(0,0,0,0.85)',
                        whiteSpace: 'nowrap',
                        flexShrink: 0,
                      }}
                    >
                      {t('cron.interval_row_label')}
                    </span>
                    <span style={{ width: '8px', flexShrink: 0, display: 'inline-block' }} aria-hidden />
                    <div style={{ flex: 1, minWidth: 0, position: 'relative' }}>
                      <input
                        type="text"
                        inputMode="numeric"
                        className="cron-interval-number-input"
                        value={formIntervalValue}
                        onKeyDown={e => {
                          if (
                            e.key === 'Backspace' ||
                            e.key === 'Delete' ||
                            e.key === 'Tab' ||
                            e.key === 'Escape' ||
                            e.key === 'Enter' ||
                            e.key === 'ArrowLeft' ||
                            e.key === 'ArrowRight' ||
                            e.key === 'Home' ||
                            e.key === 'End' ||
                            e.ctrlKey ||
                            e.metaKey
                          ) {
                            return;
                          }
                          if (!/^\d$/.test(e.key)) e.preventDefault();
                        }}
                        onPaste={e => {
                          e.preventDefault();
                          const digits = e.clipboardData.getData('text').replace(/\D/g, '');
                          if (!digits) return;
                          const n = parseInt(digits, 10);
                          if (!Number.isFinite(n)) return;
                          setFormIntervalValue(
                            String(Math.min(intervalValueMax, Math.max(intervalValueMin, n))),
                          );
                        }}
                        onChange={e => {
                          const v = e.target.value;
                          if (v === '') {
                            setFormIntervalValue('');
                            return;
                          }
                          if (!/^\d+$/.test(v)) return;
                          const n = parseInt(v, 10);
                          if (!Number.isFinite(n) || n < intervalValueMin || n > intervalValueMax) return;
                          setFormIntervalValue(String(n));
                        }}
                        style={{
                          ...inputBase,
                          width: '100%',
                          paddingLeft: 12,
                          paddingRight: 36,
                          textAlign: 'left',
                        }}
                      />
                      <div
                        style={{
                          position: 'absolute',
                          right: 8,
                          top: 0,
                          bottom: 0,
                          width: 16,
                          display: 'flex',
                          flexDirection: 'column',
                          alignItems: 'center',
                          justifyContent: 'center',
                          gap: 0,
                        }}
                      >
                        <button
                          type="button"
                          disabled={intervalAtMax}
                          onClick={e => {
                            e.preventDefault();
                            bumpIntervalValue(1);
                          }}
                          aria-label={t('cron.interval_step_up')}
                          style={{
                            padding: 0,
                            margin: 0,
                            border: 'none',
                            background: 'transparent',
                            cursor: intervalAtMax ? 'not-allowed' : 'pointer',
                            height: 18,
                            width: 16,
                            display: 'flex',
                            alignItems: 'center',
                            justifyContent: 'center',
                            flexShrink: 0,
                            color: SCHEDULE_KIND_TEXT,
                            opacity: intervalAtMax ? 0.3 : 1,
                          }}
                        >
                          <ChevronUp size={16} strokeWidth={2} aria-hidden />
                        </button>
                        <button
                          type="button"
                          disabled={intervalAtMin}
                          onClick={e => {
                            e.preventDefault();
                            bumpIntervalValue(-1);
                          }}
                          aria-label={t('cron.interval_step_down')}
                          style={{
                            padding: 0,
                            margin: 0,
                            border: 'none',
                            background: 'transparent',
                            cursor: intervalAtMin ? 'not-allowed' : 'pointer',
                            height: 18,
                            width: 16,
                            display: 'flex',
                            alignItems: 'center',
                            justifyContent: 'center',
                            flexShrink: 0,
                            color: SCHEDULE_KIND_TEXT,
                            opacity: intervalAtMin ? 0.3 : 1,
                          }}
                        >
                          <ChevronDown size={16} strokeWidth={2} aria-hidden />
                        </button>
                      </div>
                    </div>
                    <span style={{ width: '8px', flexShrink: 0, display: 'inline-block' }} aria-hidden />
                    <IntervalUnitSelect
                      value={formIntervalUnit}
                      triggerStyle={selStyle}
                      onChange={next => {
                        setFormIntervalUnit(next);
                        const max = next === 'hour' ? 23 : 59;
                        setFormIntervalValue(prev => {
                          if (prev === '') return '';
                          const n = parseInt(prev, 10);
                          if (!Number.isFinite(n))
                            return String(Math.min(max, Math.max(1, 5)));
                          return String(Math.min(max, Math.max(1, n)));
                        });
                      }}
                    />
                  </div>
                  <div style={{ marginBottom: '8px' }}>
                    <WeekdayChips
                      weekdays={formIntervalWeekdays}
                      onChange={setFormIntervalWeekdays}
                    />
                  </div>
                  <div
                    style={{
                      marginTop: 8,
                      display: 'flex',
                      alignItems: 'center',
                      width: '100%',
                    }}
                  >
                    <span
                      style={{
                        fontSize: 14,
                        color: 'rgba(0,0,0,0.85)',
                        whiteSpace: 'nowrap',
                        flexShrink: 0,
                        fontWeight: 400,
                      }}
                    >
                      {t('cron.interval_effective_time')}
                    </span>
                    <span style={{ width: '8px', flexShrink: 0, display: 'inline-block' }} aria-hidden />
                    <TimeSpinner
                      value={formIntervalWindowStart}
                      onChange={setFormIntervalWindowStart}
                      style={{ flex: 1, minWidth: 0 }}
                    />
                    <span style={{ width: '8px', flexShrink: 0, display: 'inline-block' }} aria-hidden />
                    <TimeSpinner
                      value={formIntervalWindowEnd}
                      onChange={setFormIntervalWindowEnd}
                      style={{ flex: 1, minWidth: 0 }}
                    />
                  </div>
                </div>
              )}

               {/* Once: time + date row (defaults: current local time / date) */}
               {formScheduleKind === 'once' && (
                 <div style={{ marginTop: 8 }}>
                   <div
                     style={{
                       display: 'flex',
                       alignItems: 'stretch',
                       width: '100%',
                     }}
                   >
                     <TimeSpinner value={formOnceTime} onChange={setFormOnceTime} style={{ flex: 1, minWidth: 0 }} />
                     <span style={{ width: '8px', flexShrink: 0, display: 'inline-block' }} aria-hidden />
                     <OnceDatePicker value={formOnceDate} onChange={setFormOnceDate} style={{ flex: 1, minWidth: 0 }} />
                   </div>
                   <div style={fieldErrorStyle}>
                     {onceTimePast ? t('cron.once_time_past') : '\u00A0'}
                   </div>
                 </div>
               )}

            </div>

          </div>
        </div>

        {/* Footer — 与 ModelDialog 一致：不用 flex gap（Qt 5.12 WebEngine 不支持），用 marginLeft */}
        <div
          style={{
            display: 'flex',
            flexDirection: 'row',
            flexWrap: 'nowrap',
            justifyContent: 'flex-end',
            alignItems: 'center',
            boxSizing: 'border-box',
            padding: '16px 24px 24px',
          }}
        >
          <button
            type="button"
            onClick={onClose}
            style={{
              height: 36,
              paddingLeft: 16,
              paddingRight: 16,
              borderRadius: 6,
              background: K_ALPHA_NORMAL,
              color: TEXT_MAIN,
              border: 'none',
              cursor: 'pointer',
              fontSize: 14,
              fontWeight: 400,
              boxSizing: 'border-box',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.background = K_ALPHA_HOVER;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.background = K_ALPHA_NORMAL;
            }}
            onMouseDown={(e) => {
              e.currentTarget.style.background = K_ALPHA_PRESS;
            }}
            onMouseUp={(e) => {
              e.currentTarget.style.background = K_ALPHA_HOVER;
            }}
          >
            {t('cron.cancel')}
          </button>
          <button
            type="button"
            onClick={handleSubmit}
            disabled={submitting || !canSubmit}
            style={{
              marginLeft: 8,
              height: 36,
              paddingLeft: 16,
              paddingRight: 16,
              borderRadius: 6,
              background: submitting || !canSubmit ? 'rgba(55, 144, 250, 0.45)' : PRIMARY_BTN,
              color: '#FFFFFF',
              border: 'none',
              cursor: submitting || !canSubmit ? 'not-allowed' : 'pointer',
              fontSize: 14,
              fontWeight: 400,
              boxSizing: 'border-box',
            }}
            onMouseEnter={(e) => {
              if (submitting || !canSubmit) return;
              e.currentTarget.style.background = PRIMARY_HOVER;
            }}
            onMouseLeave={(e) => {
              if (submitting || !canSubmit) return;
              e.currentTarget.style.background = PRIMARY_BTN;
            }}
            onMouseDown={(e) => {
              if (submitting || !canSubmit) return;
              e.currentTarget.style.background = PRIMARY_PRESS;
            }}
            onMouseUp={(e) => {
              if (submitting || !canSubmit) return;
              e.currentTarget.style.background = PRIMARY_HOVER;
            }}
          >
            {submitting
              ? t(isEdit ? 'cron.saving' : 'cron.adding')
              : t(isEdit ? 'cron.confirm' : 'cron.modal_add')}
          </button>
        </div>
      </div>
    </div>
  );
}

// ---------- Confirm Delete Dialog ----------
function ConfirmDeleteModal({ onConfirm, onCancel }: { onConfirm: () => void; onCancel: () => void }) {
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        e.stopPropagation();
        onConfirm();
      }
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [onConfirm]);

  return (
    <div
      role="dialog"
      aria-modal="true"
      aria-label={t('cron.confirm_delete')}
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        width: '100vw',
        height: '100vh',
        background: 'transparent',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 300,
      }}
    >
      <div
        style={{
          background: '#fff',
          borderRadius: 8,
          width: 376,
          minHeight: 172,
          boxShadow: '0 6px 30px rgba(0, 0, 0, 0.12)',
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
          transform: 'translate(-28px, -40px)',
        }}
      >
        <div
          style={{
            width: 376,
            height: 40,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'flex-end',
            paddingRight: 8,
            boxSizing: 'border-box',
            flexShrink: 0,
          }}
        >
          <ModelCloseButton onClick={onCancel} />
        </div>

        <div
          style={{
            paddingLeft: 24,
            paddingRight: 24,
            paddingBottom: 24,
            flex: 1,
            display: 'flex',
            flexDirection: 'column',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'flex-start', paddingTop: 2 }}>
            <img
              src={`${assetBasePath}/icons/dialog-question.svg`}
              alt=""
              width={16}
              height={16}
              style={{ flexShrink: 0, marginTop: 2, display: 'block', objectFit: 'contain' }}
            />
            <span style={{ width: 8, flexShrink: 0 }} aria-hidden />
            <div className="min-w-0 flex-1">
              <p
                style={{
                  margin: 0,
                  fontSize: 14,
                  fontWeight: 500,
                  color: TEXT_MAIN,
                  lineHeight: 1.5,
                }}
              >
                {t('cron.confirm_delete')}
              </p>
              <p
                style={{
                  margin: '8px 0 0',
                  fontSize: 14,
                  color: 'rgba(0, 0, 0, 0.55)',
                  lineHeight: 1.5,
                }}
              >
                {t('cron.delete_desc')}
              </p>
            </div>
          </div>

          <div style={{ flex: 1 }} />

          <div style={{ display: 'flex', justifyContent: 'flex-end', marginTop: 16 }}>
            <button type="button" onClick={onCancel} className="skill-dialog-cancel-btn">
              {t('cron.cancel')}
            </button>
            <span style={{ width: 8, flexShrink: 0 }} aria-hidden />
            <button type="button" onClick={onConfirm} className="skill-dialog-uninstall-btn">
              {t('cron.delete')}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}

// ---------- Main Page ----------
export default function Cron() {
  const [jobs, setJobs] = useState<CronJob[]>([]);
  const [loading, setLoading] = useState(true);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [actionError, setActionError] = useState<string | null>(null);
  const [modalJob, setModalJob] = useState<CronJob | 'add' | null>(null);
  const [confirmDeleteId, setConfirmDeleteId] = useState<string | null>(null);
  const [togglingId, setTogglingId] = useState<string | null>(null);
  const [runningId, setRunningId] = useState<string | null>(null);
  const fetchGenerationRef = useRef(0);

  const fetchJobs = useCallback((options?: { silent?: boolean }) => {
    const silent = options?.silent ?? false;
    const generation = ++fetchGenerationRef.current;
    if (!silent) {
      setLoading(true);
      setLoadError(null);
    }
    return getCronJobs()
      .then((next) => {
        if (generation !== fetchGenerationRef.current) return;
        setJobs(next);
        if (silent) {
          setActionError(null);
        }
      })
      .catch((err) => {
        if (generation !== fetchGenerationRef.current) return;
        const message = err instanceof Error ? err.message : String(err);
        if (silent) {
          setActionError(message);
        } else {
          setLoadError(message);
        }
      })
      .finally(() => {
        if (generation !== fetchGenerationRef.current) return;
        if (!silent) {
          setLoading(false);
        }
      });
  }, []);

  useEffect(() => { void fetchJobs(); }, [fetchJobs]);

  useEffect(() => {
    const timer = window.setInterval(() => {
      void fetchJobs({ silent: true });
    }, 5000);
    return () => window.clearInterval(timer);
  }, [fetchJobs]);

  const handleToggle = async (job: CronJob) => {
    setTogglingId(job.id);
    setActionError(null);
    // Invalidate in-flight list polls so they cannot overwrite toggle results.
    fetchGenerationRef.current += 1;
    try {
      const updated = job.enabled ? await pauseCronJob(job.id) : await resumeCronJob(job.id);
      fetchGenerationRef.current += 1;
      setJobs(prev => prev.map(j => j.id === updated.id ? updated : j));
    } catch (err) {
      setActionError(err instanceof Error ? err.message : t('cron.toggle_error'));
      void fetchJobs({ silent: true });
    } finally {
      setTogglingId(null);
    }
  };

  const handleDelete = async (id: string) => {
    setActionError(null);
    try {
      await deleteCronJob(id);
      removeCronIntervalUiMeta(id);
      setJobs(prev => prev.filter(j => j.id !== id));
    } catch (err) {
      setActionError(err instanceof Error ? err.message : t('cron.delete_error'));
    } finally {
      setConfirmDeleteId(null);
    }
  };

  const handleRunNow = async (job: CronJob) => {
    setRunningId(job.id);
    setActionError(null);
    try {
      await runCronJobNow(job.id);
      await fetchJobs({ silent: true });
    } catch (err) {
      setActionError(err instanceof Error ? err.message : t('cron.run_error'));
    } finally {
      setRunningId(null);
    }
  };

  const handleSaved = (updated: CronJob) => {
    setJobs(prev => {
      const idx = prev.findIndex(j => j.id === updated.id);
      if (idx >= 0) {
        const next = [...prev];
        next[idx] = updated;
        return next;
      }
      return [...prev, updated];
    });
    setModalJob(null);
  };

  const CRON_NAME_COL_W = 212;
  const CRON_NAME_TEXT_W = 164;
  const CRON_NEXT_RUN_COL_W = 144;
  const CRON_STATUS_COL_W = 84;
  const CRON_ACTIONS_COL_W = 120;
  const CRON_FIXED_COLS_W =
    CRON_NAME_COL_W + CRON_NEXT_RUN_COL_W + CRON_STATUS_COL_W + CRON_ACTIONS_COL_W;
  const CRON_LIST_RADIUS = 6;
  const CRON_ICON_TEXT_GAP = 8;

  const colNameStyle: React.CSSProperties = {
    width: CRON_NAME_COL_W,
    maxWidth: CRON_NAME_COL_W,
    minWidth: CRON_NAME_COL_W,
  };
  const colNextRunStyle: React.CSSProperties = {
    width: CRON_NEXT_RUN_COL_W,
    maxWidth: CRON_NEXT_RUN_COL_W,
    minWidth: CRON_NEXT_RUN_COL_W,
  };
  const colStatusStyle: React.CSSProperties = {
    width: CRON_STATUS_COL_W,
    maxWidth: CRON_STATUS_COL_W,
    minWidth: CRON_STATUS_COL_W,
  };
  const colActionsStyle: React.CSSProperties = {
    width: CRON_ACTIONS_COL_W,
    maxWidth: CRON_ACTIONS_COL_W,
    minWidth: CRON_ACTIONS_COL_W,
  };

  const pageShellStyle: React.CSSProperties = {
    marginLeft: 24,
    marginRight: 24,
    marginTop: 0,
  };

  const thTdBase: React.CSSProperties = {
    boxSizing: 'border-box',
    textAlign: 'left',
    fontSize: 'var(--model-font-size)',
    fontFamily: 'var(--model-font-family)',
    background: '#f5f5f5',
    borderBottom: '1px solid #fff',
  };

  const thStyle: React.CSSProperties = {
    ...thTdBase,
    height: 36,
    fontWeight: 500,
    color: 'rgba(0, 0, 0, 0.6)',
    whiteSpace: 'nowrap',
  };

  const tdStyle: React.CSSProperties = {
    ...thTdBase,
    height: 60,
    color: 'var(--model-text-primary)',
    verticalAlign: 'middle',
  };

  const thTdFirst: React.CSSProperties = { padding: '0 8px 0 16px' };
  const thTdMid: React.CSSProperties = { padding: '0 8px' };
  const thTdLast: React.CSSProperties = { padding: '0 16px 0 8px' };

  if (loadError && jobs.length === 0) {
    return (
      <div className="model-page box-border" style={pageShellStyle}>
        <div style={{ background: '#fff2f0', border: '1px solid #ffa39e', borderRadius: 8, padding: '12px 16px', color: '#cf1322', fontSize: 13 }}>
          {t('cron.load_error')}: {loadError}
        </div>
      </div>
    );
  }

  if (loading) {
    return <PageLoadingState />;
  }

  const addBtnDefaultBg = '#F6F6F6';
  const addBtnHoverBg = '#F2F2F2';
  const addBtnActiveBg = '#EEEEEE';
  const addBtnColor = 'rgba(0, 0, 0, 0.85)';

  return (
    <div className="model-page box-border" style={pageShellStyle}>
      {actionError && (
        <div style={{ background: '#fff2f0', border: '1px solid #ffa39e', borderRadius: 8, padding: '12px 16px', color: '#cf1322', fontSize: 13, marginBottom: 12 }}>
          {actionError}
        </div>
      )}
      <h2 className="model-section-title">
        {jobs.length > 0
          ? `${t('cron.task_list')}（${jobs.length}）`
          : t('cron.task_list')}
      </h2>

      {/* Content */}
      {jobs.length === 0 ? (
        /* Empty state */
        <div
          style={{
            border: '1px solid #f0f0f0',
            borderRadius: CRON_LIST_RADIUS,
            background: addBtnDefaultBg,
          }}
          onMouseEnter={e => (e.currentTarget.style.background = addBtnHoverBg)}
          onMouseLeave={e => (e.currentTarget.style.background = addBtnDefaultBg)}
          onMouseDown={e => (e.currentTarget.style.background = addBtnActiveBg)}
          onMouseUp={e => (e.currentTarget.style.background = addBtnHoverBg)}
        >
          <button
            onClick={() => setModalJob('add')}
            style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: 8,
              width: '100%',
              padding: '16px',
              background: 'transparent',
              border: 'none',
              cursor: 'pointer',
              fontFamily: 'var(--model-add-font-family)',
              fontSize: 'var(--model-font-size)',
              color: addBtnColor,
            }}
          >
            <Plus size={16} />{t('cron.add_job')}
          </button>
        </div>
      ) : (
        /* Table */
        <div style={{ borderRadius: CRON_LIST_RADIUS, background: '#fff', overflow: 'auto', flex: 1 }}>
          <table
            style={{
              width: '100%',
              minWidth: CRON_FIXED_COLS_W + 120,
              tableLayout: 'fixed',
              borderCollapse: 'separate',
              borderSpacing: 0,
            }}
          >
            <colgroup>
              <col style={{ width: `${CRON_NAME_COL_W}px` }} />
              <col style={{ width: `calc(100% - ${CRON_FIXED_COLS_W}px)` }} />
              <col style={{ width: `${CRON_NEXT_RUN_COL_W}px` }} />
              <col style={{ width: `${CRON_STATUS_COL_W}px` }} />
              <col style={{ width: `${CRON_ACTIONS_COL_W}px` }} />
            </colgroup>
            <thead>
              <tr>
                <th
                  style={{
                    ...thStyle,
                    ...thTdFirst,
                    ...colNameStyle,
                    borderRadius: `${CRON_LIST_RADIUS}px 0 0 0`,
                  }}
                >
                  {t('cron.name')}
                </th>
                <th style={{ ...thStyle, ...thTdMid, minWidth: 0 }}>{t('cron.prompt_col')}</th>
                <th
                  style={{
                    ...thStyle,
                    ...thTdMid,
                    ...colNextRunStyle,
                    whiteSpace: 'nowrap',
                  }}
                >
                  {t('cron.next_run')}
                </th>
                <th
                  style={{
                    ...thStyle,
                    ...thTdMid,
                    ...colStatusStyle,
                    whiteSpace: 'nowrap',
                  }}
                >
                  {t('cron.last_status')}
                </th>
                <th
                  style={{
                    ...thStyle,
                    ...thTdLast,
                    ...colActionsStyle,
                    whiteSpace: 'nowrap',
                    borderRadius: `0 ${CRON_LIST_RADIUS}px 0 0`,
                  }}
                  aria-label={t('cron.actions')}
                />
              </tr>
            </thead>
            <tbody>
              {jobs.map(job => (
                <tr key={job.id}>
                  {/* Name */}
                  <td style={{ ...tdStyle, ...thTdFirst, ...colNameStyle }}>
                    <div
                      style={{
                        display: 'flex',
                        alignItems: 'center',
                        minWidth: 0,
                        color: 'var(--model-text-primary)',
                        fontWeight: 400,
                      }}
                    >
                      <span
                        style={{
                          display: 'inline-flex',
                          flexShrink: 0,
                          width: 16,
                          height: 16,
                          marginRight: CRON_ICON_TEXT_GAP,
                          alignItems: 'center',
                          justifyContent: 'center',
                        }}
                      >
                        <AlarmClock size={16} style={{ color: '#262626', display: 'block' }} />
                      </span>
                      <TruncatedTextTooltip
                        text={job.name ?? job.id.slice(0, 8)}
                        style={{
                          width: CRON_NAME_TEXT_W,
                          maxWidth: CRON_NAME_TEXT_W,
                          flex: `0 0 ${CRON_NAME_TEXT_W}px`,
                          minWidth: 0,
                        }}
                      />
                    </div>
                  </td>
                  {/* Prompt / command */}
                  <td
                    style={{
                      ...tdStyle,
                      ...thTdMid,
                      minWidth: 0,
                      color: 'var(--model-text-secondary)',
                      overflow: 'hidden',
                    }}
                  >
                    <div style={{ minWidth: 0, overflow: 'hidden', width: '100%' }}>
                      <TruncatedTextTooltip
                        text={job.prompt ?? job.command ?? ''}
                        tooltipVariant="longText"
                        style={{ color: 'var(--model-text-secondary)', display: 'block', width: '100%' }}
                      />
                    </div>
                  </td>
                  {/* Next run */}
                  <td
                    style={{
                      ...tdStyle,
                      ...thTdMid,
                      ...colNextRunStyle,
                      color: 'var(--model-text-secondary)',
                      whiteSpace: 'nowrap',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                    }}
                  >
                    {shouldShowNextRun(job) ? formatDate(job.next_run) : null}
                  </td>
                  {/* Last status */}
                  <td
                    style={{
                      ...tdStyle,
                      ...thTdMid,
                      ...colStatusStyle,
                      overflow: 'hidden',
                    }}
                  >
                    <StatusBadge status={job.last_status} />
                  </td>
                  {/* Actions */}
                  <td style={{ ...tdStyle, ...thTdLast, ...colActionsStyle, overflow: 'visible' }}>
                    <div style={{ display: 'inline-flex', alignItems: 'center' }}>
                      <RowMenu
                        onEdit={() => setModalJob(job)}
                        onDelete={() => setConfirmDeleteId(job.id)}
                        onRunNow={() => handleRunNow(job)}
                      />
                      <div style={{ marginLeft: 16, flexShrink: 0 }}>
                        <Toggle
                          checked={job.enabled}
                          onChange={() => handleToggle(job)}
                          disabled={togglingId === job.id || runningId === job.id}
                        />
                      </div>
                    </div>
                  </td>
                </tr>
              ))}
              {/* Add task row */}
              <tr>
                <td
                  colSpan={5}
                  onClick={() => setModalJob('add')}
                  onMouseEnter={e => (e.currentTarget.style.background = addBtnHoverBg)}
                  onMouseLeave={e => (e.currentTarget.style.background = addBtnDefaultBg)}
                  onMouseDown={e => (e.currentTarget.style.background = addBtnActiveBg)}
                  onMouseUp={e => (e.currentTarget.style.background = addBtnHoverBg)}
                  style={{
                    ...tdStyle,
                    padding: '0 16px',
                    textAlign: 'center',
                    borderBottom: 'none',
                    borderRadius: `0 0 ${CRON_LIST_RADIUS}px ${CRON_LIST_RADIUS}px`,
                    cursor: 'pointer',
                    background: addBtnDefaultBg,
                    color: addBtnColor,
                  }}
                >
                  <span
                    style={{
                      display: 'inline-flex',
                      alignItems: 'center',
                      gap: 8,
                      fontFamily: 'var(--model-add-font-family)',
                      fontSize: 'var(--model-font-size)',
                      color: addBtnColor,
                    }}
                  >
                    <Plus size={16} />{t('cron.add_job')}
                  </span>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      )}

      {/* Add / Edit Modal */}
      {modalJob !== null && (
        <CronModal
          job={modalJob}
          onClose={() => setModalJob(null)}
          onSaved={handleSaved}
        />
      )}

      {/* Confirm Delete */}
      {confirmDeleteId !== null && (
        <ConfirmDeleteModal
          onConfirm={() => handleDelete(confirmDeleteId)}
          onCancel={() => setConfirmDeleteId(null)}
        />
      )}
    </div>
  );
}
