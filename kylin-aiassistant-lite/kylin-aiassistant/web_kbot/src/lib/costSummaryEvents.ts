/** Dispatched after a WebSocket chat turn records usage so UIs can refetch `/api/cost`. */
export const COST_SUMMARY_STALE_EVENT = 'kylinbot-cost-summary-stale';

export function dispatchCostSummaryStale(): void {
  if (typeof window === 'undefined') return;
  window.dispatchEvent(new Event(COST_SUMMARY_STALE_EVENT));
}
