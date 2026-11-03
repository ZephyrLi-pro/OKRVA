import { render, screen, waitFor, within } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { setLocale } from '@/lib/i18n';
import Model from './Model';

beforeEach(() => {
  setLocale('zh');
});
import { DeleteConfirmDialog } from '@/components/model/DeleteConfirmDialog';
import { UsageStatsPanel } from '@/components/model/UsageStatsPanel';

type MockEntry = {
  path: string;
  value?: string | { input: number; output: number };
  populated: boolean;
  is_secret?: boolean;
};

const apiMockState = vi.hoisted(() => ({
  entries: [] as MockEntry[],
  fallback: 'deepseek',
  cost: {
    session_cost_usd: 0,
    daily_cost_usd: 0,
    monthly_cost_usd: 0,
    request_count: 0,
    total_tokens: 0,
    by_model: {} as Record<string, {
      model: string;
      input_tokens: number;
      output_tokens: number;
      total_tokens: number;
      request_count: number;
      cost_usd: number;
    }>,
  },
  selectSectionItem: vi.fn(),
  patchConfig: vi.fn(),
  purgeModelCostStats: vi.fn(() => Promise.resolve({ removed_records: 0 })),
}));

vi.mock('@/lib/api', () => ({
  listProps: vi.fn(() =>
    Promise.resolve({
      entries: apiMockState.entries.map((entry) => ({
        category: 'providers',
        kind: 'string',
        type_hint: 'String',
        ...entry,
      })),
    }),
  ),
  getProp: vi.fn((path: string) => {
    if (path === 'providers.fallback') {
      return Promise.resolve({
        path,
        value: apiMockState.fallback,
        populated: Boolean(apiMockState.fallback),
      });
    }
    const entry = apiMockState.entries.find((item) => item.path === path);
    const rawValue = entry?.value;
    const wireValue =
      rawValue != null && typeof rawValue === 'object'
        ? JSON.stringify(rawValue)
        : (rawValue ?? '');
    return Promise.resolve({
      path,
      value: wireValue,
      populated: Boolean(entry?.populated),
    });
  }),
  patchConfig: apiMockState.patchConfig,
  selectSectionItem: apiMockState.selectSectionItem,
  getCost: vi.fn(() => Promise.resolve(apiMockState.cost)),
  purgeModelCostStats: apiMockState.purgeModelCostStats,
  ApiError: class ApiError extends Error {
    public readonly status: number;
    constructor(status: number, envelope: { message: string }) {
      super(envelope.message);
      this.status = status;
    }
  },
}));

function setConfiguredModels(
  models: Array<{
    provider: string;
    model: string;
    apiKey?: boolean;
    baseUrl?: string;
    pricing?: { input: number; output: number };
  }>,
) {
  apiMockState.entries = models.flatMap((model) => {
    const entries: MockEntry[] = [
      {
        path: `providers.models.${model.provider}.model`,
        value: model.model,
        populated: true,
      },
    ];
    if (model.apiKey) {
      entries.push({
        path: `providers.models.${model.provider}.api-key`,
        populated: true,
        is_secret: true,
      });
    }
    if (model.baseUrl) {
      entries.push({
        path: `providers.models.${model.provider}.base-url`,
        value: model.baseUrl,
        populated: true,
      });
    }
    if (model.pricing) {
      entries.push({
        path: `providers.models.${model.provider}.pricing`,
        value: model.pricing,
        populated: true,
      });
    }
    return entries;
  });
}

async function selectProviderInAddDialog(
  user: ReturnType<typeof userEvent.setup>,
  addDialog: HTMLElement,
  providerName: string,
) {
  await user.click(within(addDialog).getByRole('button', { name: /百度千帆/ }));
  await user.click(await screen.findByRole('option', { name: providerName }));
}

describe('Model page overwrite flow', () => {
  beforeEach(() => {
    localStorage.clear();
    apiMockState.fallback = 'deepseek';
    apiMockState.selectSectionItem.mockReset();
    apiMockState.selectSectionItem.mockResolvedValue({
      fields_prefix: 'providers.models.deepseek',
      created: false,
    });
    apiMockState.patchConfig.mockReset();
    apiMockState.patchConfig.mockResolvedValue({ saved: true, results: [] });
    apiMockState.purgeModelCostStats.mockReset();
    apiMockState.purgeModelCostStats.mockResolvedValue({ removed_records: 0 });
    setConfiguredModels([{ provider: 'deepseek', model: 'deepseek-v3.2' }]);
    apiMockState.cost = {
      session_cost_usd: 0,
      daily_cost_usd: 0,
      monthly_cost_usd: 0,
      request_count: 0,
      total_tokens: 0,
      by_model: {},
    };
  });

  it('shows overwrite confirmation when adding an existing preset provider', async () => {
    const user = userEvent.setup();
    render(<Model />);

    await screen.findByText('deepseek-v3.2');
    await user.click(screen.getByRole('button', { name: '添加' }));
    const addDialog = await screen.findByRole('dialog', { name: '添加模型' });
    await selectProviderInAddDialog(user, addDialog, 'DeepSeek');
    await user.type(screen.getByPlaceholderText('必填（例如：deepseek-v3.2）'), 'deepseek-r1');
    await user.click(within(addDialog).getByRole('button', { name: '添加' }));

    const overwriteDialog = await screen.findByRole('dialog', {
      name: '确定覆盖现有模型？',
    });
    expect(within(addDialog).getByRole('button', { name: /DeepSeek/ })).toBeInTheDocument();
    expect(overwriteDialog).toHaveTextContent(
      '当前模型列表中已存在\u201CDeepSeek\u201D模型提供商，是否进行覆盖？',
    );
    expect(apiMockState.patchConfig).not.toHaveBeenCalled();

    await user.click(within(overwriteDialog).getByRole('button', { name: '覆盖' }));

    await waitFor(() => {
      expect(apiMockState.selectSectionItem).toHaveBeenCalledWith('providers', 'deepseek');
      expect(apiMockState.patchConfig).toHaveBeenCalledWith(
        expect.arrayContaining([
          expect.objectContaining({
            path: 'providers.models.deepseek.model',
            value: 'deepseek-r1',
          }),
        ]),
      );
      expect(apiMockState.purgeModelCostStats).toHaveBeenCalledWith(['deepseek-v3.2']);
    });
  });

  it('closes only the overwrite layer on X while keeping the add dialog open', async () => {
    const user = userEvent.setup();
    render(<Model />);

    await screen.findByText('deepseek-v3.2');
    await user.click(screen.getByRole('button', { name: '添加' }));
    const addDialog = await screen.findByRole('dialog', { name: '添加模型' });
    await selectProviderInAddDialog(user, addDialog, 'DeepSeek');
    await user.type(screen.getByPlaceholderText('必填（例如：deepseek-v3.2）'), 'deepseek-r1');
    await user.click(within(addDialog).getByRole('button', { name: '添加' }));

    const overwriteDialog = await screen.findByRole('dialog', {
      name: '确定覆盖现有模型？',
    });
    await user.click(within(overwriteDialog).getByRole('button', { name: '关闭' }));

    await waitFor(() => {
      expect(
        screen.queryByRole('dialog', { name: '确定覆盖现有模型？' }),
      ).not.toBeInTheDocument();
    });
    expect(screen.getByRole('dialog', { name: '添加模型' })).toBeInTheDocument();
    expect(apiMockState.patchConfig).not.toHaveBeenCalled();
  });

  it('closes only the overwrite layer on cancel while keeping the add dialog open', async () => {
    const user = userEvent.setup();
    render(<Model />);

    await screen.findByText('deepseek-v3.2');
    await user.click(screen.getByRole('button', { name: '添加' }));
    const addDialog = await screen.findByRole('dialog', { name: '添加模型' });
    await selectProviderInAddDialog(user, addDialog, 'DeepSeek');
    await user.type(screen.getByPlaceholderText('必填（例如：deepseek-v3.2）'), 'deepseek-r1');
    await user.click(within(addDialog).getByRole('button', { name: '添加' }));

    const overwriteDialog = await screen.findByRole('dialog', {
      name: '确定覆盖现有模型？',
    });
    await user.click(within(overwriteDialog).getByRole('button', { name: '取消' }));

    await waitFor(() => {
      expect(
        screen.queryByRole('dialog', { name: '确定覆盖现有模型？' }),
      ).not.toBeInTheDocument();
    });
    expect(screen.getByRole('dialog', { name: '添加模型' })).toBeInTheDocument();
    expect(apiMockState.patchConfig).not.toHaveBeenCalled();
  });

  it('shows overwrite confirmation for a custom provider with the same request URL', async () => {
    const user = userEvent.setup();
    setConfiguredModels([
      {
        provider: 'custom:https://api.example.com/v1',
        model: 'custom-a',
      },
    ]);
    apiMockState.fallback = 'custom:https://api.example.com/v1';
    render(<Model />);

    await screen.findByText('custom-a');
    await user.click(screen.getByRole('button', { name: '添加' }));
    const addDialog = await screen.findByRole('dialog', { name: '添加模型' });
    await selectProviderInAddDialog(user, addDialog, '自定义');
    await user.type(
      within(addDialog).getByPlaceholderText('必填（例如：deepseek-v3.2）'),
      'custom-b',
    );
    await user.type(
      within(addDialog).getByPlaceholderText('必填（例如：https://api.example.com/v1）'),
      'https://api.example.com/v1',
    );
    await user.click(within(addDialog).getByRole('button', { name: '添加' }));

    const overwriteDialog = await screen.findByRole('dialog', { name: '确定覆盖现有模型？' });
    expect(overwriteDialog).toHaveTextContent(
      '当前模型列表中已存在\u201Chttps://api.example.com/v1\u201D请求地址，是否进行覆盖？',
    );
  });

  it('does not purge usage when renaming one of two rows with the same model name', async () => {
    const user = userEvent.setup();
    setConfiguredModels([
      { provider: 'deepseek', model: 'deepseek-v4-pro' },
      { provider: 'qianfan', model: 'deepseek-v4-pro' },
    ]);
    render(<Model />);

    await screen.findAllByText('deepseek-v4-pro');
    await user.click(screen.getAllByLabelText('更多')[0]!);
    await user.click(screen.getByText('编辑'));
    const editDialog = await screen.findByRole('dialog', { name: '编辑模型' });
    const input = within(editDialog).getByDisplayValue('deepseek-v4-pro');
    await user.clear(input);
    await user.type(input, 'deepseek-v4-flash');
    await user.click(within(editDialog).getByRole('button', { name: '确定' }));

    await waitFor(() => {
      expect(apiMockState.patchConfig).toHaveBeenCalled();
    });
    expect(apiMockState.purgeModelCostStats).not.toHaveBeenCalled();
  });

  it('edits the current row directly without showing overwrite confirmation', async () => {
    const user = userEvent.setup();
    render(<Model />);

    await screen.findByText('deepseek-v3.2');
    await user.click(screen.getByLabelText('更多'));
    await user.click(screen.getByText('编辑'));
    const editDialog = await screen.findByRole('dialog', { name: '编辑模型' });
    const confirmBtn = within(editDialog).getByRole('button', { name: '确定' });
    expect(confirmBtn).toBeDisabled();

    const input = within(editDialog).getByDisplayValue('deepseek-v3.2');
    await user.clear(input);
    await user.type(input, 'deepseek-chat');
    expect(confirmBtn).toBeEnabled();

    await user.clear(input);
    await user.type(input, 'deepseek-v3.2');
    expect(confirmBtn).toBeDisabled();

    await user.clear(input);
    await user.type(input, 'deepseek-chat');
    await user.click(confirmBtn);

    expect(screen.queryByText('确定覆盖现有模型？')).not.toBeInTheDocument();
    await waitFor(() => {
      expect(apiMockState.patchConfig).toHaveBeenCalledWith(
        expect.arrayContaining([
          expect.objectContaining({
            path: 'providers.models.deepseek.model',
            value: 'deepseek-chat',
          }),
        ]),
      );
      expect(apiMockState.purgeModelCostStats).toHaveBeenCalledWith(['deepseek-v3.2']);
    });
  });

  it('enables edit confirm when APIKey or base URL changes', async () => {
    const user = userEvent.setup();
    setConfiguredModels([
      {
        provider: 'custom:https://api.example.com/v1',
        model: 'custom-a',
        apiKey: true,
      },
    ]);
    apiMockState.fallback = 'custom:https://api.example.com/v1';
    render(<Model />);

    await screen.findByText('custom-a');
    await user.click(screen.getByLabelText('更多'));
    await user.click(screen.getByText('编辑'));
    const editDialog = await screen.findByRole('dialog', { name: '编辑模型' });
    const confirmBtn = within(editDialog).getByRole('button', { name: '确定' });
    expect(confirmBtn).toBeDisabled();

    const apiKeyInput = within(editDialog).getByPlaceholderText('选填（云端服务必填）');
    await user.type(apiKeyInput, 'sk-test');
    expect(confirmBtn).toBeEnabled();

    await user.clear(apiKeyInput);
    expect(confirmBtn).toBeDisabled();

    const baseUrlInput = within(editDialog).getByDisplayValue('https://api.example.com/v1');
    await user.clear(baseUrlInput);
    await user.type(baseUrlInput, 'https://api.example.com/v2');
    expect(confirmBtn).toBeEnabled();

    await user.clear(baseUrlInput);
    await user.type(baseUrlInput, 'https://api.example.com/v1');
    expect(confirmBtn).toBeDisabled();
  });
});

describe('Model page delete flow', () => {
  beforeEach(() => {
    localStorage.clear();
    apiMockState.fallback = 'deepseek';
    apiMockState.selectSectionItem.mockReset();
    apiMockState.selectSectionItem.mockResolvedValue({
      fields_prefix: 'providers.models.deepseek',
      created: false,
    });
    apiMockState.patchConfig.mockReset();
    apiMockState.patchConfig.mockResolvedValue({ saved: true, results: [] });
    apiMockState.purgeModelCostStats.mockReset();
    apiMockState.purgeModelCostStats.mockResolvedValue({ removed_records: 0 });
    setConfiguredModels([{ provider: 'deepseek', model: 'deepseek-v3.2' }]);
  });

  it('switches default model when clicking a non-default row', async () => {
    const user = userEvent.setup();
    setConfiguredModels([
      { provider: 'deepseek', model: 'deepseek-v3.2' },
      { provider: 'glm', model: 'glm-4' },
    ]);
    apiMockState.fallback = 'deepseek';
    render(<Model />);

    await screen.findByText('deepseek-v3.2');
    await screen.findByText('glm-4');

    await user.click(screen.getByText('glm-4'));

    await waitFor(() => {
      expect(apiMockState.patchConfig).toHaveBeenCalledWith(
        expect.arrayContaining([
          expect.objectContaining({
            op: 'replace',
            path: 'providers.fallback',
            value: 'glm',
          }),
        ]),
      );
    });
  });

  it('clears providers.fallback when deleting the only model', async () => {
    const user = userEvent.setup();
    render(<Model />);

    await screen.findByText('deepseek-v3.2');
    await user.click(screen.getByLabelText('更多'));
    await user.click(screen.getByText('删除'));
    const confirm = await screen.findByRole('dialog', {
      name: '删除后将无法恢复，是否确认删除该模型？',
    });
    await user.click(within(confirm).getByRole('button', { name: '删除' }));

    await waitFor(() => {
      const ops = apiMockState.patchConfig.mock.calls.flatMap((c) => c[0] as { op: string; path: string }[]);
      expect(ops.some((o) => o.path === 'providers.fallback')).toBe(true);
      expect(apiMockState.purgeModelCostStats).toHaveBeenCalledWith(['deepseek-v3.2']);
    });

    const ops = apiMockState.patchConfig.mock.calls.flatMap((c) => c[0] as { op: string; path: string }[]);
    expect(ops.filter((o) => o.path === 'providers.fallback')).toEqual(
      expect.arrayContaining([expect.objectContaining({ op: 'remove', path: 'providers.fallback' })]),
    );
  });

  it('does not purge usage when deleting one of two rows with the same model name', async () => {
    const user = userEvent.setup();
    setConfiguredModels([
      { provider: 'deepseek', model: 'deepseek-v4-pro' },
      { provider: 'qianfan', model: 'deepseek-v4-pro' },
    ]);
    apiMockState.fallback = 'deepseek';
    render(<Model />);

    await screen.findAllByText('deepseek-v4-pro');
    await user.click(screen.getAllByLabelText('更多')[0]!);
    await user.click(screen.getByText('删除'));
    const confirm = await screen.findByRole('dialog', {
      name: '删除后将无法恢复，是否确认删除该模型？',
    });
    await user.click(within(confirm).getByRole('button', { name: '删除' }));

    await waitFor(() => {
      expect(apiMockState.patchConfig).toHaveBeenCalled();
    });
    expect(apiMockState.purgeModelCostStats).not.toHaveBeenCalled();
  });

  it('repoints providers.fallback to the remaining top model when deleting the default', async () => {
    const user = userEvent.setup();
    setConfiguredModels([
      { provider: 'glm', model: 'glm-4' },
      { provider: 'deepseek', model: 'deepseek-v3.2' },
    ]);
    apiMockState.fallback = 'glm';

    render(<Model />);
    await screen.findByText('glm-4');

    await user.click(screen.getAllByLabelText('更多')[0]!);
    await user.click(screen.getByText('删除'));
    const confirm = await screen.findByRole('dialog', {
      name: '删除后将无法恢复，是否确认删除该模型？',
    });
    await user.click(within(confirm).getByRole('button', { name: '删除' }));

    await waitFor(() => {
      const ops = apiMockState.patchConfig.mock.calls.flatMap(
        (c) => c[0] as { op: string; path: string; value?: unknown }[],
      );
      expect(
        ops.some(
          (o) =>
            o.op === 'replace' &&
            o.path === 'providers.fallback' &&
            o.value === 'deepseek',
        ),
      ).toBe(true);
    });
  });
});

describe('Model dialog pricing', () => {
  beforeEach(() => {
    localStorage.clear();
    apiMockState.fallback = '';
    setConfiguredModels([]);
    apiMockState.selectSectionItem.mockReset();
    apiMockState.selectSectionItem.mockResolvedValue({
      fields_prefix: 'providers.models.deepseek',
      created: true,
    });
    apiMockState.patchConfig.mockReset();
    apiMockState.patchConfig.mockResolvedValue({ saved: true, results: [] });
  });

  it('patchConfig includes pricing when advanced settings are filled', async () => {
    const user = userEvent.setup();
    render(<Model />);

    await waitFor(() => {
      expect(screen.getByRole('button', { name: '添加' })).toBeInTheDocument();
    });
    await user.click(screen.getByRole('button', { name: '添加' }));
    const dialog = await screen.findByRole('dialog', { name: '添加模型' });
    await user.type(
      screen.getByPlaceholderText('必填（例如：deepseek-v3.2）'),
      'deepseek-v3.2',
    );
    await user.click(
      screen.getByRole('button', { name: '高级设置（可配置模型用量统计）' }),
    );
    await user.type(
      within(dialog).getByRole('textbox', { name: '模型输入 Tokens 单价' }),
      '0.5',
    );
    await user.type(
      within(dialog).getByRole('textbox', { name: '模型输出 Tokens 单价' }),
      '2',
    );
    await user.click(within(dialog).getByRole('button', { name: '添加' }));

    await waitFor(() => {
      expect(apiMockState.patchConfig).toHaveBeenCalledWith(
        expect.arrayContaining([
          expect.objectContaining({
            path: 'providers.models.qianfan.pricing',
            value: { input: 0.5, output: 2 },
          }),
        ]),
      );
    });
  });

  it('pricing fields show optional placeholder, accept decimals, and silently block text', async () => {
    const user = userEvent.setup();
    render(<Model />);

    await waitFor(() => {
      expect(screen.getByRole('button', { name: '添加' })).toBeInTheDocument();
    });
    await user.click(screen.getByRole('button', { name: '添加' }));
    const dialog = await screen.findByRole('dialog', { name: '添加模型' });
    await user.type(
      screen.getByPlaceholderText('必填（例如：deepseek-v3.2）'),
      'deepseek-v3.2',
    );
    await user.click(
      screen.getByRole('button', { name: '高级设置（可配置模型用量统计）' }),
    );

    const inputPrice = within(dialog).getByRole('textbox', { name: '模型输入 Tokens 单价' });
    const outputPrice = within(dialog).getByRole('textbox', { name: '模型输出 Tokens 单价' });
    expect(inputPrice).toHaveAttribute('placeholder', '选填');
    expect(outputPrice).toHaveAttribute('placeholder', '选填');

    await user.type(inputPrice, '0.75');
    expect(inputPrice).toHaveValue('0.75');
    await user.type(outputPrice, '2.5');
    expect(outputPrice).toHaveValue('2.5');

    await user.clear(inputPrice);
    await user.type(inputPrice, 'abc!@#，。');
    expect(inputPrice).toHaveValue('');
    expect(within(dialog).queryByText('模型输入 Tokens 单价需为非负数字')).not.toBeInTheDocument();
  });

  it('keeps advanced settings collapsed on edit until user expands', async () => {
    const user = userEvent.setup();
    setConfiguredModels([
      {
        provider: 'deepseek',
        model: 'deepseek-v3.2',
        pricing: { input: 0.5, output: 2 },
      },
    ]);
    apiMockState.fallback = 'deepseek';
    render(<Model />);

    await screen.findByText('deepseek-v3.2');
    await user.click(screen.getByLabelText('更多'));
    await user.click(screen.getByText('编辑'));
    const dialog = await screen.findByRole('dialog', { name: '编辑模型' });
    expect(
      within(dialog).queryByRole('textbox', { name: '模型输入 Tokens 单价' }),
    ).not.toBeInTheDocument();
    await user.click(
      within(dialog).getByRole('button', { name: '高级设置（可配置模型用量统计）' }),
    );
    await waitFor(() => {
      expect(within(dialog).getByDisplayValue('0.5')).toBeInTheDocument();
      expect(within(dialog).getByDisplayValue('2')).toBeInTheDocument();
    });
  });

  it('loads pricing on edit when getProp returns JSON string wire form', async () => {
    const user = userEvent.setup();
    setConfiguredModels([
      {
        provider: 'minimax-cn',
        model: 'MiniMax-M2.7',
        pricing: { input: 1.2, output: 3.4 },
      },
    ]);
    apiMockState.fallback = 'minimax-cn';
    render(<Model />);

    await screen.findByText('MiniMax-M2.7');
    await user.click(screen.getByLabelText('更多'));
    await user.click(screen.getByText('编辑'));
    const dialog = await screen.findByRole('dialog', { name: '编辑模型' });
    expect(
      within(dialog).queryByRole('textbox', { name: '模型输入 Tokens 单价' }),
    ).not.toBeInTheDocument();
    await user.click(
      within(dialog).getByRole('button', { name: '高级设置（可配置模型用量统计）' }),
    );
    await waitFor(() => {
      expect(within(dialog).getByDisplayValue('1.2')).toBeInTheDocument();
      expect(within(dialog).getByDisplayValue('3.4')).toBeInTheDocument();
    });
  });

  it('loads pricing on edit when getProp returns TOML display string', async () => {
    const user = userEvent.setup();
    setConfiguredModels([{ provider: 'deepseek', model: 'deepseek-v3.2' }]);
    apiMockState.entries.push({
      path: 'providers.models.deepseek.pricing',
      value: '{ input = 0.5, output = 2.0 }',
      populated: true,
    });
    apiMockState.fallback = 'deepseek';
    render(<Model />);

    await screen.findByText('deepseek-v3.2');
    await user.click(screen.getByLabelText('更多'));
    await user.click(screen.getByText('编辑'));
    const dialog = await screen.findByRole('dialog', { name: '编辑模型' });
    await user.click(
      within(dialog).getByRole('button', { name: '高级设置（可配置模型用量统计）' }),
    );
    await waitFor(() => {
      expect(within(dialog).getByDisplayValue('0.5')).toBeInTheDocument();
      expect(within(dialog).getByDisplayValue('2')).toBeInTheDocument();
    });
  });

  it('removes pricing on edit when advanced fields are cleared', async () => {
    const user = userEvent.setup();
    setConfiguredModels([
      {
        provider: 'deepseek',
        model: 'deepseek-v3.2',
        pricing: { input: 0.5, output: 2 },
      },
    ]);
    apiMockState.fallback = 'deepseek';
    render(<Model />);

    await screen.findByText('deepseek-v3.2');
    await user.click(screen.getByLabelText('更多'));
    await user.click(screen.getByText('编辑'));
    const dialog = await screen.findByRole('dialog', { name: '编辑模型' });
    await user.click(
      within(dialog).getByRole('button', { name: '高级设置（可配置模型用量统计）' }),
    );
    await waitFor(() => {
      expect(within(dialog).getByDisplayValue('0.5')).toBeInTheDocument();
    });
    await user.clear(within(dialog).getByDisplayValue('0.5'));
    await user.clear(within(dialog).getByDisplayValue('2'));
    await user.click(within(dialog).getByRole('button', { name: '确定' }));

    await waitFor(() => {
      expect(apiMockState.patchConfig).toHaveBeenCalledWith(
        expect.arrayContaining([
          expect.objectContaining({
            op: 'remove',
            path: 'providers.models.deepseek.pricing',
          }),
        ]),
      );
    });
  });
});

describe('Model dialogs and usage stats', () => {
  it('does not close a confirmation dialog when clicking the blank overlay', async () => {
    const user = userEvent.setup();
    const onCancel = vi.fn();
    render(
      <DeleteConfirmDialog
        open
        onCancel={onCancel}
        onConfirm={() => {}}
      />,
    );

    await user.click(screen.getByRole('dialog'));

    expect(screen.getByText('删除后将无法恢复，是否确认删除该模型？')).toBeInTheDocument();
    expect(onCancel).not.toHaveBeenCalled();
  });

  it('filters model usage rows to currently configured model ids', async () => {
    apiMockState.cost = {
      session_cost_usd: 0.2,
      daily_cost_usd: 0.2,
      monthly_cost_usd: 0.2,
      request_count: 2,
      total_tokens: 200,
      by_model: {
        'deepseek-v3.2': {
          model: 'deepseek-v3.2',
          input_tokens: 100,
          output_tokens: 100,
          total_tokens: 200,
          request_count: 2,
          cost_usd: 0.2,
        },
      },
    };
    const lookup = new Map([
      ['deepseek-v3.2', { displayLabel: 'deepseek-v3.2', providerKey: 'deepseek' }],
    ]);

    render(<UsageStatsPanel modelTagLookup={lookup} />);

    expect(await screen.findByText('deepseek-v3.2')).toBeInTheDocument();
    expect(screen.queryByText('deleted')).not.toBeInTheDocument();

    const totalRequestsCard = screen.getByText('总请求数').closest('div');
    expect(within(totalRequestsCard!).getByText('2')).toBeInTheDocument();

    const totalTokensCard = screen.getByText('总Tokens').closest('div');
    expect(within(totalTokensCard!).getByText('200')).toBeInTheDocument();
  });

  it('excludes purged model rows from request and token totals', async () => {
    apiMockState.cost = {
      session_cost_usd: 0.3,
      daily_cost_usd: 0.3,
      monthly_cost_usd: 0.3,
      request_count: 5,
      total_tokens: 500,
      by_model: {
        'deepseek-v3.2': {
          model: 'deepseek-v3.2',
          input_tokens: 100,
          output_tokens: 100,
          total_tokens: 200,
          request_count: 2,
          cost_usd: 0.2,
        },
        'ghost-model': {
          model: 'ghost-model',
          input_tokens: 150,
          output_tokens: 150,
          total_tokens: 300,
          request_count: 3,
          cost_usd: 0.1,
        },
      },
    };
    const lookup = new Map([
      ['deepseek-v3.2', { displayLabel: 'deepseek-v3.2', providerKey: 'deepseek' }],
    ]);

    render(<UsageStatsPanel modelTagLookup={lookup} />);

    await screen.findByText('deepseek-v3.2');
    expect(screen.queryByText('ghost-model')).not.toBeInTheDocument();

    const totalRequestsCard = screen.getByText('总请求数').closest('div');
    expect(within(totalRequestsCard!).getByText('2')).toBeInTheDocument();

    const totalTokensCard = screen.getByText('总Tokens').closest('div');
    expect(within(totalTokensCard!).getByText('200')).toBeInTheDocument();

    expect(screen.getAllByText('¥0.2000').length).toBeGreaterThanOrEqual(1);
  });
});

describe('Model page runtime alias filtering', () => {
  beforeEach(() => {
    localStorage.clear();
    apiMockState.fallback = 'ollama';
    apiMockState.selectSectionItem.mockReset();
    apiMockState.patchConfig.mockReset();
    setConfiguredModels([
      {
        provider: 'ollama',
        model: 'qwen2.5:1.5b',
        baseUrl: 'http://localhost:11434',
      },
      {
        provider: 'custom:http://localhost:11434',
        model: 'qwen2.5:1.5b',
      },
    ]);
  });

  it('does not list custom mirrors when ollama already owns the base URL', async () => {
    render(<Model />);

    await screen.findByText('qwen2.5:1.5b');
    expect(screen.getAllByText('qwen2.5:1.5b')).toHaveLength(1);
    expect(screen.getByText('Ollama')).toBeInTheDocument();
    expect(screen.queryByText(/\|\s*自定义/)).not.toBeInTheDocument();
  });
});
