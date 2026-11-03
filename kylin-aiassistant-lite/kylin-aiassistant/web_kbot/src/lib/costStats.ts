import type { CostSummary, ModelStats } from '@/types/api';

/** Per-model rows that still exist in the configured model list. */
export function visibleModelStats(
  cost: CostSummary,
  modelTagLookup: Map<string, unknown>,
): ModelStats[] {
  return Object.values(cost.by_model).filter((m) => modelTagLookup.has(m.model));
}

/** Request/token totals scoped to configured models (excludes purged or deleted ids). */
export function aggregateConfiguredUsageTotals(
  cost: CostSummary,
  modelTagLookup: Map<string, unknown>,
): { models: ModelStats[]; request_count: number; total_tokens: number } {
  const models = visibleModelStats(cost, modelTagLookup);
  return {
    models,
    request_count: models.reduce((sum, m) => sum + m.request_count, 0),
    total_tokens: models.reduce((sum, m) => sum + m.total_tokens, 0),
  };
}
