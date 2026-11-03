import { useCallback, useEffect, useState, type CSSProperties } from 'react';
import { getCost } from '@/lib/api';
import type { CostSummary, ModelStats } from '@/types/api';
import { COST_SUMMARY_STALE_EVENT } from '@/lib/costSummaryEvents';
import { aggregateConfiguredUsageTotals } from '@/lib/costStats';
import { formatCNY, formatInt, formatPercent } from '@/lib/format';
import { TruncatedTextTooltip } from '@/components/model/modelTooltips';
import { tf, t } from '@/lib/i18n';

const TEXT_PRIMARY = 'var(--model-text-primary)';
const ROW_BG = '#F6F6F6';
const BAR_TRACK = '#E6E6E6';
const BAR_FILL = '#3790FA';
const CARD_MAX_WIDTH = 242;
const GRID_COLS =
  'minmax(0, 2fr) minmax(0, 1fr) minmax(0, 1fr) minmax(0, 1fr) minmax(0, 1fr)';
const CELL_TEXT_STYLE: CSSProperties = {
  fontFamily: 'var(--model-font-family)',
  fontSize: 'var(--model-font-size)',
  color: TEXT_PRIMARY,
  textAlign: 'left',
};

interface UsageStatsPanelProps {
  /**
   * The configured models in the page — maps cost API `model` id to optional
   * display label (`label || model value`) used for the「模型名称」column.
   */
  modelTagLookup: Map<string, { displayLabel: string; providerKey: string }>;
}

export function UsageStatsPanel({ modelTagLookup }: UsageStatsPanelProps) {
  const [cost, setCost] = useState<CostSummary | null>(null);
  const [error, setError] = useState<string | null>(null);

  const refresh = useCallback(() => {
    getCost()
      .then((data) => {
        setCost(data);
        setError(null);
      })
      .catch((err) => setError(err instanceof Error ? err.message : String(err)));
  }, []);

  useEffect(() => {
    refresh();
  }, [refresh]);

  useEffect(() => {
    let debounceTimer: number | undefined;
    const scheduleRefresh = () => {
      if (debounceTimer !== undefined) clearTimeout(debounceTimer);
      debounceTimer = window.setTimeout(() => {
        debounceTimer = undefined;
        refresh();
      }, 400);
    };
    const onVisibility = () => {
      if (document.visibilityState === 'visible') scheduleRefresh();
    };
    window.addEventListener(COST_SUMMARY_STALE_EVENT, scheduleRefresh);
    document.addEventListener('visibilitychange', onVisibility);
    return () => {
      window.removeEventListener(COST_SUMMARY_STALE_EVENT, scheduleRefresh);
      document.removeEventListener('visibilitychange', onVisibility);
      if (debounceTimer !== undefined) clearTimeout(debounceTimer);
    };
  }, [refresh]);

  if (error) {
    return (
      <div
        className="rounded-xl border p-3 text-sm"
        style={{
          background: 'rgba(255, 68, 102, 0.08)',
          borderColor: 'rgba(255, 68, 102, 0.2)',
          color: 'var(--color-status-error)',
        }}
      >
        {tf('model.usage_load_error', { error })}
      </div>
    );
  }

  const configuredUsage = cost
    ? aggregateConfiguredUsageTotals(cost, modelTagLookup)
    : { models: [] as ModelStats[], request_count: 0, total_tokens: 0 };
  const totalRequests = configuredUsage.request_count;
  const totalTokens = configuredUsage.total_tokens;
  const avgTokensPerRequest =
    totalRequests > 0 ? Math.floor(totalTokens / totalRequests) : 0;
  const costPer1k =
    totalTokens > 0 ? ((cost?.monthly_cost_usd ?? 0) / totalTokens) * 1000 : 0;

  const summaryCards = [
    { label: t('model.usage.session_cost'), value: formatCNY(cost?.session_cost_usd ?? 0) },
    { label: t('model.usage.daily_cost'), value: formatCNY(cost?.daily_cost_usd ?? 0) },
    { label: t('model.usage.monthly_cost'), value: formatCNY(cost?.monthly_cost_usd ?? 0) },
    { label: t('model.usage.total_requests'), value: formatInt(totalRequests) },
  ];

  const tokenCards = [
    { label: t('model.usage.total_tokens'), value: formatInt(totalTokens) },
    { label: t('model.usage.avg_tokens'), value: formatInt(avgTokensPerRequest) },
    { label: t('model.usage.cost_per_1k'), value: formatCNY(costPer1k) },
  ];

  const models = [...configuredUsage.models];
  models.sort((a, b) => b.cost_usd - a.cost_usd);
  const visibleRequestCount = models.reduce(
    (sum, model) => sum + model.request_count,
    0,
  );
  const hasDetailRows = models.length > 0;

  return (
    <div className="flex flex-col min-w-0 box-border w-full" style={{ gap: 0 }}>
      <h3 className="model-section-title">{t('model.usage_title')}</h3>

      <div
        className="flex min-w-0 flex-nowrap"
        style={{
          gap: 8,
          width: '100%',
          justifyContent: 'space-between',
          marginBottom: 8,
        }}
      >
        {summaryCards.map((c) => (
          <StatCard key={c.label} label={c.label} value={c.value} />
        ))}
      </div>

      <div
        className="flex min-h-0 min-w-0 shrink-0 flex-col box-border"
        style={{
          width: '100%',
          boxSizing: 'border-box',
          paddingLeft: 16,
          paddingRight: 16,
          paddingTop: 16,
          paddingBottom: 16,
          marginBottom: 8,
          background: ROW_BG,
          borderRadius: 6,
          gap: 0,
        }}
      >
        <div
          style={{
            flexShrink: 0,
            marginBottom: 8,
            fontFamily: 'var(--model-font-family)',
            fontSize: 'var(--model-font-size)',
            color: TEXT_PRIMARY,
            fontWeight: 'normal',
            lineHeight: 1,
          }}
        >
          {t('model.usage.token_stats')}
        </div>
        <div
          className="grid min-h-0 min-w-0 shrink-0 w-full"
          style={{
            gridTemplateColumns: 'repeat(3, minmax(0, 1fr))',
            columnGap: 8,
            alignItems: 'stretch',
          }}
        >
          {tokenCards.map((c) => (
            <div
              key={c.label}
              className="flex min-h-0 min-w-0 flex-col justify-between box-border"
              style={{
                height: 60,
                minHeight: 0,
                flexShrink: 0,
                boxSizing: 'border-box',
                padding: 8,
                background: '#FFFFFF',
                borderRadius: 6,
              }}
            >
              <span
                style={{
                  fontFamily: 'var(--model-font-family)',
                  fontSize: 'var(--model-font-size)',
                  color: TEXT_PRIMARY,
                  lineHeight: '20px',
                }}
              >
                {c.label}
              </span>
              <span
                style={{
                  fontFamily: 'var(--model-font-family)',
                  fontSize: 'var(--model-font-size)',
                  fontWeight: 'bold',
                  color: TEXT_PRIMARY,
                  lineHeight: '20px',
                }}
              >
                {c.value}
              </span>
            </div>
          ))}
        </div>
      </div>

      <div
        className="flex min-w-0 shrink-0 flex-col box-border"
        style={{
          width: '100%',
          background: '#ffffff',
        }}
      >
        <div style={{ marginBottom: hasDetailRows ? 1 : 0 }}>
          <div
            className="grid min-w-0 items-center"
            style={{
              height: 36,
              boxSizing: 'border-box',
              paddingLeft: 16,
              paddingRight: 16,
              background: ROW_BG,
              borderRadius: hasDetailRows ? '6px 6px 0 0' : 6,
              gridTemplateColumns: GRID_COLS,
              alignItems: 'center',
            }}
          >
            {[
              t('model.usage.col_name'),
              t('model.usage.col_cost'),
              t('model.usage.col_token'),
              t('model.usage.col_requests'),
              t('model.usage.col_share'),
            ].map((h) => (
              <span key={h} style={CELL_TEXT_STYLE}>
                {h}
              </span>
            ))}
          </div>
        </div>
        {models.map((m, index) => {
          const tag = modelTagLookup.get(m.model);
          const share =
            visibleRequestCount > 0
              ? (m.request_count / visibleRequestCount) * 100
              : 0;
          const isLast = index === models.length - 1;
          return (
            <div
              key={m.model}
              style={{
                marginBottom: isLast ? 0 : 1,
              }}
            >
              <div
                className="grid min-w-0 items-center"
                style={{
                  height: 36,
                  boxSizing: 'border-box',
                  paddingLeft: 16,
                  paddingRight: 16,
                  background: ROW_BG,
                  gridTemplateColumns: GRID_COLS,
                  borderRadius: isLast ? '0 0 6px 6px' : 0,
                }}
              >
                <TruncatedTextTooltip
                  className="min-w-0"
                  text={tag?.displayLabel ?? m.model}
                  style={CELL_TEXT_STYLE}
                />
                <span style={CELL_TEXT_STYLE}>{formatCNY(m.cost_usd)}</span>
                <span style={CELL_TEXT_STYLE}>{formatInt(m.total_tokens)}</span>
                <span style={CELL_TEXT_STYLE}>{formatInt(m.request_count)}</span>
                <ShareBar share={share} />
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}

function StatCard({ label, value }: { label: string; value: string }) {
  return (
    <div
      className="flex flex-col box-border justify-between min-w-0"
      style={{
        flex: '1 1 0%',
        maxWidth: CARD_MAX_WIDTH,
        height: 80,
        padding: 16,
        background: ROW_BG,
        borderRadius: 6,
      }}
    >
      <span
        style={{
          fontFamily: 'var(--model-font-family)',
          fontSize: 'var(--model-font-size)',
          color: TEXT_PRIMARY,
        }}
      >
        {label}
      </span>
      <span
        style={{
          fontFamily: 'var(--model-font-family)',
          fontSize: 'var(--model-font-size)',
          fontWeight: 'bold',
          color: TEXT_PRIMARY,
        }}
      >
        {value}
      </span>
    </div>
  );
}

function ShareBar({ share }: { share: number }) {
  const pct = Math.max(Math.min(share, 100), 0);
  return (
    <div
      className="flex min-w-0 items-center"
      style={{ gap: 8, justifyContent: 'flex-start' }}
    >
      <div
        className="min-h-0 min-w-0 flex-1 overflow-hidden"
        style={{ height: 6, borderRadius: 3, background: BAR_TRACK }}
      >
        <div
          style={{
            height: '100%',
            width: `${pct}%`,
            background: BAR_FILL,
            borderRadius: 3,
            transition: 'width 0.5s ease',
          }}
        />
      </div>
      <span
        className="flex-shrink-0 whitespace-nowrap"
        style={{
          fontFamily: 'var(--model-font-family)',
          fontSize: 'var(--model-font-size)',
          color: TEXT_PRIMARY,
        }}
      >
        {formatPercent(share)}
      </span>
    </div>
  );
}
