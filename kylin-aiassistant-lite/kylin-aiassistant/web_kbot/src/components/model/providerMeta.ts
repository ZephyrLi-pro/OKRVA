// Provider metadata for the model configuration UI & model list rows.

import { t } from '@/lib/i18n';

export type PresetProviderKey =
  | 'qianfan'
  | 'deepseek'
  | 'doubao'
  | 'minimax'
  | 'moonshot'
  | 'ollama'
  | 'qwen'
  | 'glm'
  | 'custom';

export interface ProviderMeta {
  key: PresetProviderKey;
  label: string;
  displayTag: string;
  /** Config / runtime key under `providers.models.<key>` */
  storageProvider: string;
  isCustom: boolean;
  iconColor: string;
  iconBg: string;
  glyph: string;
  iconAsset?: string;
}

/** Picker order — matches add-model UI spec（含图标名） */
export const PRESET_PROVIDERS: ProviderMeta[] = [
  {
    key: 'qianfan',
    label: '百度千帆',
    displayTag: '百度千帆',
    storageProvider: 'qianfan',
    isCustom: false,
    iconColor: '#ffffff',
    iconBg: '#5B54E6',
    glyph: '千',
    iconAsset: 'qianfan.svg',
  },
  {
    key: 'deepseek',
    label: 'DeepSeek',
    displayTag: 'DeepSeek',
    storageProvider: 'deepseek',
    isCustom: false,
    iconColor: '#ffffff',
    iconBg: '#4D6BFE',
    glyph: 'D',
    iconAsset: 'deepseek.svg',
  },
  {
    key: 'doubao',
    label: '火山引擎',
    displayTag: '火山引擎',
    storageProvider: 'doubao',
    isCustom: false,
    iconColor: '#ffffff',
    iconBg: '#3B7BF5',
    glyph: '火',
    iconAsset: 'doubao.svg',
  },
  {
    key: 'minimax',
    label: 'MiniMax',
    displayTag: 'MiniMax',
    storageProvider: 'minimax-cn',
    isCustom: false,
    iconColor: '#ffffff',
    iconBg: '#E14B99',
    glyph: 'M',
    iconAsset: 'minimax.svg',
  },
  {
    key: 'moonshot',
    label: 'Moonshot',
    displayTag: 'Moonshot',
    storageProvider: 'moonshot',
    isCustom: false,
    iconColor: '#ffffff',
    iconBg: '#1C1C1E',
    glyph: 'M',
    iconAsset: 'moonshot.svg',
  },
  {
    key: 'ollama',
    label: 'Ollama',
    displayTag: 'Ollama',
    storageProvider: 'ollama',
    isCustom: false,
    iconColor: '#ffffff',
    iconBg: '#1C6B3A',
    glyph: 'O',
    iconAsset: 'ollama.svg',
  },
  {
    key: 'qwen',
    label: 'Qwen',
    displayTag: 'Qwen',
    storageProvider: 'qwen',
    isCustom: false,
    iconColor: '#ffffff',
    iconBg: '#5946F5',
    glyph: 'Q',
    iconAsset: 'qwen.svg',
  },
  {
    key: 'glm',
    label: 'Zhipu AI',
    displayTag: 'Zhipu AI',
    storageProvider: 'glm-cn',
    isCustom: false,
    iconColor: '#ffffff',
    iconBg: '#3B7BF5',
    glyph: 'Z',
    iconAsset: 'zhipu.svg',
  },
  {
    key: 'custom',
    label: '自定义',
    displayTag: '自定义',
    storageProvider: 'custom',
    isCustom: true,
    iconColor: '#ffffff',
    iconBg: '#3790FA',
    glyph: '+',
    iconAsset: 'custom.svg',
  },
];

/** 「添加模型」弹窗初次打开时的默认提供商（与 `PRESET_PROVIDERS[0]` 一致，现为百度千帆）。 */
export const ADD_MODEL_DEFAULT_PROVIDER_KEY: PresetProviderKey =
  PRESET_PROVIDERS[0]!.key;

const CUSTOM_META = PRESET_PROVIDERS.find((p) => p.key === 'custom')!;

const ALIAS_MAP: Record<string, PresetProviderKey> = {
  qianfan: 'qianfan',
  baidu: 'qianfan',

  deepseek: 'deepseek',

  doubao: 'doubao',
  doubao_cn: 'doubao',
  'doubao-cn': 'doubao',
  volcengine: 'doubao',
  ark: 'doubao',

  minimax: 'minimax',
  'minimax-cn': 'minimax',
  'minimax-intl': 'minimax',
  minimaxi: 'minimax',

  moonshot: 'moonshot',
  kimi: 'moonshot',
  'moonshot-cn': 'moonshot',
  'kimi-cn': 'moonshot',
  'moonshot-intl': 'moonshot',
  'kimi-intl': 'moonshot',
  'moonshot-global': 'moonshot',
  ollama: 'ollama',

  qwen: 'qwen',
  dashscope: 'qwen',
  'qwen-intl': 'qwen',
  'qwen-intl-dashscope': 'qwen',
  'dashscope-intl': 'qwen',
  'qwen-us': 'qwen',

  glm: 'glm',
  zhipu: 'glm',
  'glm-cn': 'glm',
  'zhipu-cn': 'glm',
  'glm-global': 'glm',
  'zhipu-global': 'glm',

  custom: 'custom',
};

export function resolveProviderMeta(rawProvider: string | undefined): {
  meta: ProviderMeta;
  baseUrl?: string;
  iconAsset?: string;
} {
  if (!rawProvider) return { meta: CUSTOM_META };
  const trimmed = rawProvider.trim();
  if (trimmed.startsWith('custom:')) {
    return {
      meta: CUSTOM_META,
      baseUrl: trimmed.slice('custom:'.length),
      iconAsset: CUSTOM_META.iconAsset,
    };
  }
  if (trimmed === 'custom')
    return { meta: CUSTOM_META, iconAsset: CUSTOM_META.iconAsset };
  const key = ALIAS_MAP[trimmed.toLowerCase()];
  if (key) {
    const found = PRESET_PROVIDERS.find((p) => p.key === key);
    if (found) return { meta: found, iconAsset: found.iconAsset };
  }
  return { meta: CUSTOM_META };
}

/** Map UI preset key → stored profile key (`resolveProviderMeta` normalization). */
export function findPresetByKey(key: PresetProviderKey): ProviderMeta {
  return PRESET_PROVIDERS.find((p) => p.key === key) ?? CUSTOM_META;
}

export function getProviderLabel(meta: Pick<ProviderMeta, 'key' | 'label'>): string {
  const translated = t(`provider.${meta.key}`);
  return translated === `provider.${meta.key}` ? meta.label : translated;
}
