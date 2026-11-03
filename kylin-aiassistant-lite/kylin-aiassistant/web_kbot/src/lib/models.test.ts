import { beforeEach, describe, expect, it, vi } from 'vitest';
import {
  buildConfiguredModelsAfterEdit,
  buildConfiguredModelsAfterOverwrite,
  configuredModelNameSet,
  isRuntimeProviderModelAliasKey,
  purgeModelUsageStatsIfUnconfigured,
  type ModelInfo,
} from './models';

vi.mock('@/lib/api', () => ({
  purgeModelCostStats: vi.fn(() => Promise.resolve({ removed_records: 0 })),
}));

vi.mock('@/lib/costSummaryEvents', () => ({
  dispatchCostSummaryStale: vi.fn(),
}));

const { purgeModelCostStats } = await import('@/lib/api');

describe('purgeModelUsageStatsIfUnconfigured', () => {
  beforeEach(() => {
    vi.mocked(purgeModelCostStats).mockClear();
  });

  it('skips purge when another configured row still uses the model name', async () => {
    await purgeModelUsageStatsIfUnconfigured(['deepseek-v4-pro'], [
      { value: 'deepseek-v4-pro' },
      { value: 'deepseek-v4-flash' },
    ]);
    expect(purgeModelCostStats).not.toHaveBeenCalled();
  });

  it('purges when no configured row uses the model name', async () => {
    await purgeModelUsageStatsIfUnconfigured(['deepseek-v4-pro'], [
      { value: 'deepseek-v4-flash' },
    ]);
    expect(purgeModelCostStats).toHaveBeenCalledWith(['deepseek-v4-pro']);
  });
});

describe('configured model list helpers', () => {
  const rows: ModelInfo[] = [
    { label: 'A', value: 'deepseek-v4-pro', provider: 'deepseek' },
    { label: 'B', value: 'deepseek-v4-pro', provider: 'qianfan' },
  ];

  it('buildConfiguredModelsAfterOverwrite updates only the target provider', () => {
    const after = buildConfiguredModelsAfterOverwrite(
      rows,
      'deepseek',
      'deepseek-v4-flash',
    );
    expect(configuredModelNameSet(after)).toEqual(
      new Set(['deepseek-v4-flash', 'deepseek-v4-pro']),
    );
  });

  it('buildConfiguredModelsAfterEdit replaces the edited provider row', () => {
    const after = buildConfiguredModelsAfterEdit(rows, rows[0]!, {
      storageProvider: 'deepseek',
      modelName: 'deepseek-v4-flash',
    });
    expect(configuredModelNameSet(after)).toEqual(
      new Set(['deepseek-v4-flash', 'deepseek-v4-pro']),
    );
  });
});

describe('isRuntimeProviderModelAliasKey', () => {
  it('hides custom: mirrors when a named provider owns the same base URL', () => {
    const byKey = {
      ollama: { baseUrl: 'http://localhost:11434', model: 'qwen2.5:1.5b' },
      'custom:http://localhost:11434': { model: 'qwen2.5:1.5b' },
    };
    expect(isRuntimeProviderModelAliasKey('custom:http://localhost:11434', byKey)).toBe(true);
    expect(isRuntimeProviderModelAliasKey('ollama', byKey)).toBe(false);
  });

  it('keeps standalone custom providers', () => {
    const byKey = {
      'custom:http://localhost:11434': {
        baseUrl: 'http://localhost:11434',
        model: 'qwen2.5:1.5b',
      },
    };
    expect(isRuntimeProviderModelAliasKey('custom:http://localhost:11434', byKey)).toBe(false);
  });
});
