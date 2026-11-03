import { describe, expect, it } from 'vitest';
import { aggregateConfiguredUsageTotals } from '@/lib/costStats';
import type { CostSummary } from '@/types/api';

describe('aggregateConfiguredUsageTotals', () => {
  it('ignores by_model rows that are not in the configured model list', () => {
    const cost: CostSummary = {
      session_cost_usd: 0.3,
      daily_cost_usd: 0.3,
      monthly_cost_usd: 0.3,
      request_count: 5,
      total_tokens: 500,
      by_model: {
        'deepseek-v3.2': {
          model: 'deepseek-v3.2',
          total_tokens: 200,
          request_count: 2,
          cost_usd: 0.2,
        },
        'deleted-model': {
          model: 'deleted-model',
          total_tokens: 300,
          request_count: 3,
          cost_usd: 0.1,
        },
      },
    };
    const lookup = new Map([
      ['deepseek-v3.2', { displayLabel: 'deepseek-v3.2', providerKey: 'deepseek' }],
    ]);

    const totals = aggregateConfiguredUsageTotals(cost, lookup);
    expect(totals.request_count).toBe(2);
    expect(totals.total_tokens).toBe(200);
    expect(totals.models).toHaveLength(1);
  });
});
