// Model configuration storage layer
// =================================
//
// Backed by the 0.7.5 schema-driven config API (`/api/config/list`,
// `/api/config`, `/api/config/prop`, `/api/onboard/sections/...`). The legacy
// `GET/PUT /api/config` endpoints used by the previous implementation were
// removed upstream and now return `405 Method Not Allowed`; everything here
// goes through PATCH ops and the onboarding map-key affordance instead.
//
// Storage shape (per configured model):
//   [providers.models.<key>]
//     name      = "deepseek" | "glm" | "minimax" | "moonshot" (preset id; omitted for custom)
//     model     = "deepseek-v3.2"
//     api_key   = "sk-..."  (secret — never round-tripped back from the server)
//     base_url  = "https://..."  (only for preset providers that need an override)
//
// `<key>` is the preset id (`deepseek`, `glm`, `minimax`, `moonshot`) for
// presets, or `custom:<baseUrl>` for custom OpenAI-compatible endpoints. The
// runtime uses the key directly as the provider type (see
// `kylinbot-providers::create_provider_with_url_and_options`), so the key
// shape matters — keep it stable.
//
// `providers.fallback` points at the default model's key. Each preset can
// hold at most one entry; that mirrors the runtime constraint where the key
// drives adapter selection.

import {
  getProp,
  listProps,
  patchConfig,
  purgeModelCostStats,
  selectSectionItem,
  ApiError,
  type PatchOp,
  type PropResponse,
} from '@/lib/api';
import { dispatchCostSummaryStale } from '@/lib/costSummaryEvents';

/** Per-million-token pricing in CNY (元); maps to `providers.models.<key>.pricing`. */
export interface ModelPricingFields {
  input: number;
  output: number;
}

export interface ModelInfo {
  label: string;
  value: string;
  /** Stable storage identifier: preset key like `deepseek`, or `custom:<url>`. */
  provider: string;
  description?: string;
  baseUrl?: string;
  apiKey?: string;
}

export const MODEL_CONFIG_UPDATED_EVENT = 'kylinbot:model-config-updated';

export type ModelConfigUpdatedDetail = {
  action: 'add' | 'update' | 'delete';
  original?: { provider: string; model: string };
  /** Non-delete: active model pair. Delete: promoted default (`providers.fallback`) when one remains. */
  current?: { provider: string; model: string };
  /** True when this save wrote a new API key or base URL (triggers WS reconnect). */
  credentialsChanged?: boolean;
};

// Preset provider keys recognised by the upstream provider factory. The
// dropdown maps user picks to one of these (or builds a `custom:<url>` key).
const PRESET_KEYS = new Set([
  'deepseek',
  'glm',
  'minimax',
  'moonshot',
  'ollama',
  'qianfan',
  'qwen',
  'doubao',
]);

// LocalStorage cache used only to remember API keys for the edit dialog,
// since the config API hides secret values when listing. Reads here are
// best-effort; writes overwrite when a fresh value arrives.
const LOCAL_API_KEY_CACHE = 'kylinbot_model_api_keys_v2';
// `/model` 列表顺序：`rememberModelAdded` / `rememberModelUpdated` + 每次 `loadConfiguredModels`
// 会把「服务端有而本地队列缺」的新 key **插到队列前部**（后添加的更靠上），再按队列下标渲染。
const LOCAL_MODEL_ORDER_CACHE = 'kylinbot_model_order_v1';

type ApiKeyCache = Record<string, string>;

function readApiKeyCache(): ApiKeyCache {
  try {
    if (typeof localStorage === 'undefined') return {};
    const raw = localStorage.getItem(LOCAL_API_KEY_CACHE);
    if (!raw) return {};
    const parsed = JSON.parse(raw);
    return parsed && typeof parsed === 'object' ? (parsed as ApiKeyCache) : {};
  } catch {
    return {};
  }
}

function writeApiKeyCache(cache: ApiKeyCache): void {
  try {
    if (typeof localStorage === 'undefined') return;
    localStorage.setItem(LOCAL_API_KEY_CACHE, JSON.stringify(cache));
  } catch {
    // Ignore — quota errors here shouldn't break the save path.
  }
}

function rememberApiKey(key: string, apiKey: string | undefined): void {
  if (!apiKey) return;
  const cache = readApiKeyCache();
  cache[key] = apiKey;
  writeApiKeyCache(cache);
}

function forgetApiKey(key: string): void {
  const cache = readApiKeyCache();
  if (key in cache) {
    delete cache[key];
    writeApiKeyCache(cache);
  }
}

function readModelOrder(): string[] {
  try {
    if (typeof localStorage === 'undefined') return [];
    const raw = localStorage.getItem(LOCAL_MODEL_ORDER_CACHE);
    if (!raw) return [];
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed)
      ? parsed.filter((v): v is string => typeof v === 'string')
      : [];
  } catch {
    return [];
  }
}

function writeModelOrder(order: string[]): void {
  try {
    if (typeof localStorage === 'undefined') return;
    localStorage.setItem(LOCAL_MODEL_ORDER_CACHE, JSON.stringify(order));
  } catch {
    // Ordering is a UI convenience; persistence failures should not block saves.
  }
}

function rememberModelAdded(key: string): void {
  const order = readModelOrder();
  if (!order.includes(key)) {
    // Later add → smaller index → closer to top of `/model`.
    writeModelOrder([key, ...order]);
  }
}

function rememberModelUpdated(originalKey: string, newKey: string): void {
  if (originalKey === newKey) {
    rememberModelAdded(newKey);
    return;
  }
  const order = readModelOrder();
  const originalIndex = order.indexOf(originalKey);
  const newIndex = order.indexOf(newKey);
  const withoutChangedKeys = order.filter(
    (key) => key !== originalKey && key !== newKey,
  );
  if (originalIndex >= 0) {
    const insertAt = Math.min(originalIndex, withoutChangedKeys.length);
    const next = [...withoutChangedKeys];
    next.splice(insertAt, 0, newKey);
    writeModelOrder(next);
  } else if (newIndex < 0) {
    writeModelOrder([newKey, ...order]);
  }
}

function rememberModelDeleted(key: string): void {
  const order = readModelOrder();
  if (order.includes(key)) {
    writeModelOrder(order.filter((existing) => existing !== key));
  }
}

/**
 * Persisted order ⊂ configured keys would leave newcomers at sort-rank ∞ and
 * stuck at the bottom. Merge unknown keys ahead of known tails so restores /
 * synced configs still honour “later key nearer top”.
 */
function mergePresentIntoModelOrder(
  existing: string[],
  providersInStableOrder: string[],
): string[] {
  const presentSet = new Set(providersInStableOrder);
  const trimmed = existing.filter((k) => presentSet.has(k));
  const missing = providersInStableOrder.filter((k) => !trimmed.includes(k));
  return [...missing, ...trimmed];
}

function emitModelConfigUpdated(detail: ModelConfigUpdatedDetail) {
  if (typeof window === 'undefined') return;
  window.dispatchEvent(
    new CustomEvent<ModelConfigUpdatedDetail>(MODEL_CONFIG_UPDATED_EVENT, { detail }),
  );
}

/** Model names still present in the configured list after add/edit/delete. */
export function configuredModelNameSet(
  entries: Array<{ value: string }>,
): Set<string> {
  return new Set(
    entries.map((entry) => entry.value.trim()).filter((name) => name.length > 0),
  );
}

async function purgeModelUsageStats(modelNames: string[]): Promise<void> {
  const unique = [...new Set(modelNames.map((name) => name.trim()).filter(Boolean))];
  if (unique.length === 0) return;

  try {
    await purgeModelCostStats(unique);
    dispatchCostSummaryStale();
  } catch (err) {
    console.warn('[models] failed to purge model usage stats', err);
  }
}

/**
 * Physically remove usage only for model names that no longer appear in the
 * configured list (e.g. two providers sharing `deepseek-v4-pro` keep stats
 * until the last list entry with that name is removed or renamed).
 */
export async function purgeModelUsageStatsIfUnconfigured(
  candidateNames: string[],
  configuredAfter: Array<{ value: string }>,
): Promise<void> {
  const stillUsed = configuredModelNameSet(configuredAfter);
  const orphaned = [
    ...new Set(
      candidateNames
        .map((name) => name.trim())
        .filter((name) => name.length > 0 && !stillUsed.has(name)),
    ),
  ];
  await purgeModelUsageStats(orphaned);
}

/** Config list after editing one row (provider key may change). */
export function buildConfiguredModelsAfterEdit(
  models: ModelInfo[],
  original: ModelInfo,
  next: { storageProvider: string; modelName: string; label?: string },
): ModelInfo[] {
  return models
    .filter((row) => row.provider !== original.provider)
    .concat([
      {
        ...original,
        provider: next.storageProvider,
        value: next.modelName,
        label: next.label?.trim() || next.modelName,
      },
    ]);
}

/** Config list after overwriting a provider slot with a new model name. */
export function buildConfiguredModelsAfterOverwrite(
  models: ModelInfo[],
  storageProvider: string,
  modelName: string,
): ModelInfo[] {
  return models.map((row) =>
    row.provider === storageProvider ? { ...row, value: modelName } : row,
  );
}

/**
 * Resolve a (preset key or 'custom') + optional baseUrl to the persistence
 * key written into `[providers.models.<key>]`. Custom providers carry the
 * URL in the key itself so the runtime's `custom:` prefix routing picks
 * them up without extra wiring.
 */
function resolveProviderKey(provider: string, baseUrl?: string): string {
  const normalized = provider.trim();
  const url = baseUrl?.trim();
  if (normalized === 'custom') {
    if (!url) {
      throw new Error('自定义模型需要填写请求地址（例如 https://api.openai.com/v1）');
    }
    return `custom:${url}`;
  }
  if (normalized.startsWith('custom:')) {
    return normalized;
  }
  return normalized;
}

// ── Config-backed model list ──────────────────────────────────────────────

type EntryFields = {
  name?: string;
  baseUrl?: string;
  model?: string;
  apiKeyPopulated?: boolean;
};

/** Config list API uses `<unset>` and `populated: false` for cleared Option / string fields — still round-trips a display string. */
function cfgStringMeaningful(s: string | undefined): boolean {
  if (!s) return false;
  const t = s.trim();
  return t.length > 0 && t !== '<unset>';
}

function parseEntryPath(path: string): { key: string; field: string } | null {
  const prefix = 'providers.models.';
  if (!path.startsWith(prefix)) return null;
  const rest = path.slice(prefix.length);
  const lastDot = rest.lastIndexOf('.');
  if (lastDot <= 0) return null;
  return {
    key: rest.slice(0, lastDot),
    field: rest.slice(lastDot + 1),
  };
}

function entryHasAnyContent(fields: EntryFields): boolean {
  return Boolean(
    cfgStringMeaningful(fields.model) ||
      cfgStringMeaningful(fields.baseUrl) ||
      cfgStringMeaningful(fields.name) ||
      fields.apiKeyPopulated,
  );
}

function normalizeProviderBaseUrl(url: string): string {
  return url.trim().replace(/\/+$/, '');
}

/** Hide runtime-only provider mirrors (e.g. `custom:http://localhost:11434` cloned from `ollama`). */
export function isRuntimeProviderModelAliasKey(
  key: string,
  byKey: Record<string, EntryFields>,
): boolean {
  const trimmed = key.trim();
  if (!trimmed) return false;

  if (trimmed.startsWith('custom:')) {
    const url = normalizeProviderBaseUrl(trimmed.slice('custom:'.length));
    if (!url) return false;
    return Object.entries(byKey).some(([otherKey, fields]) => {
      if (otherKey === trimmed || otherKey.startsWith('custom:')) return false;
      const otherUrl = fields.baseUrl?.trim();
      return Boolean(otherUrl && normalizeProviderBaseUrl(otherUrl) === url);
    });
  }

  return false;
}

export async function loadConfiguredModels(): Promise<ModelInfo[]> {
  let entries;
  try {
    const list = await listProps('providers.models');
    entries = list.entries;
  } catch {
    return [];
  }

  const byKey: Record<string, EntryFields> = {};
  for (const e of entries) {
    const parsed = parseEntryPath(e.path);
    if (!parsed) continue;
    const bucket = byKey[parsed.key] ?? (byKey[parsed.key] = {});
    const field = parsed.field;
    // The Configurable derive serialises field names in kebab-case
    // (`api-key`, `base-url`); guard against the snake_case form too in
    // case a future server change normalises on the wire.
    if (field === 'api-key' || field === 'api_key') {
      if (e.is_secret) bucket.apiKeyPopulated = e.populated;
      else if (typeof e.value === 'string') {
        bucket.apiKeyPopulated = e.value.length > 0;
      }
    } else if (field === 'base-url' || field === 'base_url') {
      if (typeof e.value === 'string' && e.populated && cfgStringMeaningful(e.value)) {
        bucket.baseUrl = e.value.trim();
      }
    } else if (field === 'model' && typeof e.value === 'string') {
      if (e.populated && cfgStringMeaningful(e.value)) {
        bucket.model = e.value.trim();
      }
    } else if (field === 'name' && typeof e.value === 'string') {
      if (e.populated && cfgStringMeaningful(e.value)) {
        bucket.name = e.value.trim();
      }
    }
  }

  const cache = readApiKeyCache();
  const result: ModelInfo[] = [];
  for (const [key, fields] of Object.entries(byKey)) {
    if (isRuntimeProviderModelAliasKey(key, byKey)) continue;
    if (!entryHasAnyContent(fields)) continue;
    let provider: string;
    let baseUrl: string | undefined;
    if (key.startsWith('custom:')) {
      provider = key;
      baseUrl = key.slice('custom:'.length);
    } else {
      provider = key;
      baseUrl = fields.baseUrl;
    }
    const model = cfgStringMeaningful(fields.model) ? fields.model!.trim() : key;
    result.push({
      label: model,
      value: model,
      provider,
      description: '已配置模型',
      baseUrl,
      apiKey: cache[key],
    });
  }
  const providersInStableOrder = result.map((m) => m.provider);
  const mergedOrder = mergePresentIntoModelOrder(readModelOrder(), providersInStableOrder);
  if (providersInStableOrder.length > 0 && mergedOrder.length > 0) {
    try {
      const prev = typeof localStorage !== 'undefined'
        ? localStorage.getItem(LOCAL_MODEL_ORDER_CACHE)
        : null;
      const next = JSON.stringify(mergedOrder);
      if (prev !== next) writeModelOrder(mergedOrder);
    } catch {
      writeModelOrder(mergedOrder);
    }
    const orderIndex = new Map(mergedOrder.map((key, index) => [key, index]));
    // Add-order only (see LOCAL_MODEL_ORDER_CACHE).
    result.sort((a, b) => {
      const ai = orderIndex.get(a.provider) ?? mergedOrder.length;
      const bi = orderIndex.get(b.provider) ?? mergedOrder.length;
      if (ai !== bi) return ai - bi;
      return 0;
    });
  }
  return result;
}

// ── Mutations ────────────────────────────────────────────────────────────

function buildEntryOps(
  key: string,
  fields: {
    model: string;
    apiKey?: string;
    baseUrl?: string;
    pricing?: ModelPricingFields | null;
  },
): PatchOp[] {
  const ops: PatchOp[] = [];
  ops.push({ op: 'replace', path: `providers.models.${key}.model`, value: fields.model });
  // For preset keys, sync the `name` field so downstream tooling that
  // inspects display labels still gets a sane value. Custom keys carry the
  // URL inline already, so `name` would be redundant noise.
  if (PRESET_KEYS.has(key)) {
    ops.push({ op: 'replace', path: `providers.models.${key}.name`, value: key });
  }
  if (fields.apiKey && fields.apiKey.length > 0) {
    ops.push({ op: 'replace', path: `providers.models.${key}.api-key`, value: fields.apiKey });
  }
  // `base-url` only matters when the provider key itself doesn't already
  // encode the endpoint (i.e. non-custom presets pointing at a self-hosted
  // gateway). For `custom:<url>` keys the runtime parses the URL from the
  // prefix, so we leave `base-url` untouched.
  if (!key.startsWith('custom:')) {
    if (fields.baseUrl && fields.baseUrl.length > 0) {
      ops.push({
        op: 'replace',
        path: `providers.models.${key}.base-url`,
        value: fields.baseUrl,
      });
    }
  }
  if (fields.pricing) {
    ops.push({
      op: 'replace',
      path: `providers.models.${key}.pricing`,
      value: { input: fields.pricing.input, output: fields.pricing.output },
    });
  } else if (fields.pricing === null) {
    ops.push({ op: 'remove', path: `providers.models.${key}.pricing` });
  }
  return ops;
}

/**
 * Non-secret `GET /api/config/prop` responses only include `{ path, value }`
 * (`PropResponse`). Unlike `/api/config/list` rows, they do not carry
 * `populated`; checking `r.populated` here always failed and left
 * `providers.fallback` unread.
 */
function nonSecretPropString(r: PropResponse): string | null {
  const v = r.value;
  if (typeof v !== 'string') return null;
  const t = v.trim();
  if (t.length === 0 || t === '<unset>') return null;
  return t;
}

async function getCurrentFallback(): Promise<string | null> {
  try {
    const r = await getProp('providers.fallback');
    return nonSecretPropString(r);
  } catch {
    // Fallback unset or path missing — treat as no default.
  }
  return null;
}

async function setFallback(key: string): Promise<void> {
  await patchConfig([{ op: 'replace', path: 'providers.fallback', value: key }]);
}

async function clearProvidersFallback(): Promise<void> {
  try {
    await patchConfig([{ op: 'remove', path: 'providers.fallback' }]);
  } catch {
    // Some stores treat optional strings as absent only after remove retry paths.
    try {
      await patchConfig([
        {
          op: 'replace',
          path: 'providers.fallback',
          value: '',
        },
      ]);
    } catch {
      /* ignore — best-effort */
    }
  }
}

export async function saveModelConfiguration(input: {
  provider: string;
  model: string;
  apiKey?: string;
  baseUrl?: string;
  label?: string;
  pricing?: ModelPricingFields | null;
  /** When overwriting an existing provider entry, purge stats for the prior model name. */
  replacedModel?: string;
  /** Model list after save; used to skip purge when another row still uses the old name. */
  configuredAfter: ModelInfo[];
}): Promise<void> {
  const key = resolveProviderKey(input.provider, input.baseUrl);
  const model = input.model.trim();
  if (!key || !model) {
    throw new Error('provider 和 model 不能为空');
  }
  const apiKey = input.apiKey?.trim();

  if (input.replacedModel && input.replacedModel.trim() !== model) {
    await purgeModelUsageStatsIfUnconfigured(
      [input.replacedModel],
      input.configuredAfter,
    );
  }

  // `selectSectionItem` is idempotent (creates the map entry if missing)
  // but also rewrites `providers.fallback = <key>` as a side effect. We
  // capture the prior fallback up front so we can restore it after the
  // patch when the user is adding a secondary model rather than their
  // first one.
  const priorFallback = await getCurrentFallback();
  // Idempotent: creates `providers.models.<key>` if missing. As a side
  // effect the server rewrites `providers.fallback = key`; we may restore
  // the prior value below when this isn't the user's first add.
  await selectSectionItem('providers', key);

  const ops = buildEntryOps(key, {
    model,
    apiKey,
    baseUrl: input.baseUrl?.trim(),
    pricing: input.pricing,
  });
  await patchConfig(ops);

  // Preserve the previous default if it still resolves to a configured
  // model — otherwise let the just-added entry stand in (matches the
  // first-add semantics from the test plan).
  if (priorFallback && priorFallback !== key) {
    try {
      const r = await getProp(`providers.models.${priorFallback}.model`);
      if (nonSecretPropString(r)) {
        await setFallback(priorFallback);
      }
    } catch {
      // Prior fallback entry vanished — leave the new fallback in place.
    }
  }

  rememberApiKey(key, apiKey);
  rememberModelAdded(key);
  const baseUrl = input.baseUrl?.trim();
  emitModelConfigUpdated({
    action: 'add',
    current: { provider: key, model },
    credentialsChanged: Boolean(apiKey) || Boolean(baseUrl),
  });
}

export async function updateModelConfiguration(
  original: { provider: string; model: string },
  input: {
    provider: string;
    model: string;
    apiKey?: string;
    baseUrl?: string;
    label?: string;
    pricing?: ModelPricingFields | null;
    /** Model list after update; drives orphan purge for renamed entries. */
    configuredAfter: ModelInfo[];
  },
): Promise<void> {
  const originalKey = resolveProviderKey(original.provider, undefined);
  const newKey = resolveProviderKey(input.provider, input.baseUrl);
  const model = input.model.trim();
  if (!newKey || !model) {
    throw new Error('provider 和 model 不能为空');
  }
  const apiKey = input.apiKey?.trim();

  if (original.model.trim() !== model) {
    await purgeModelUsageStatsIfUnconfigured(
      [original.model],
      input.configuredAfter,
    );
  }

  const priorFallback = await getCurrentFallback();

  if (originalKey === newKey) {
    // Same key, just rewrite the fields. No need to touch fallback.
    const ops = buildEntryOps(newKey, {
      model,
      apiKey,
      baseUrl: input.baseUrl?.trim(),
      pricing: input.pricing,
    });
    await patchConfig(ops);
    rememberApiKey(newKey, apiKey);
  } else {
    // Provider type / custom URL changed — recreate as a fresh entry and
    // clear the old one.
    await selectSectionItem('providers', newKey);
    await patchConfig(buildEntryOps(newKey, {
      model,
      apiKey,
      baseUrl: input.baseUrl?.trim(),
      pricing: input.pricing,
    }));
    await clearEntryFields(originalKey);
    forgetApiKey(originalKey);
    rememberApiKey(newKey, apiKey);

    // Move the default over when the edited entry was the previous default.
    if (priorFallback === originalKey) {
      await setFallback(newKey);
    } else if (priorFallback && priorFallback !== newKey) {
      // `selectSectionItem` rewrote the fallback to the new key; restore
      // the previous default since the user wasn't editing it.
      try {
        const r = await getProp(`providers.models.${priorFallback}.model`);
        if (nonSecretPropString(r)) {
          await setFallback(priorFallback);
        }
      } catch {
        // Stale fallback — fall through.
      }
    }
  }

  rememberModelUpdated(originalKey, newKey);
  const baseUrl = input.baseUrl?.trim();
  emitModelConfigUpdated({
    action: 'update',
    original: { provider: original.provider, model: original.model },
    current: { provider: newKey, model },
    credentialsChanged: Boolean(apiKey) || Boolean(baseUrl),
  });
}

const PROVIDERS_MODELS_LIST_PREFIX = 'providers.models.';

/**
 * Extract the HashMap key from a leaf path `providers.models.<hm_key>.<field>`.
 * Matches `route_hashmap_path` (split on the **last** `.` only) so keys with URL dots work.
 */
function providersModelsHashMapKeyFromPath(path: string): string | null {
  if (!path.startsWith(PROVIDERS_MODELS_LIST_PREFIX)) return null;
  const rest = path.slice(PROVIDERS_MODELS_LIST_PREFIX.length);
  const lastDot = rest.lastIndexOf('.');
  if (lastDot <= 0) return null;
  return rest.slice(0, lastDot);
}

function pathBelongsToProvidersModelKey(path: string, key: string): boolean {
  return providersModelsHashMapKeyFromPath(path) === key;
}

/**
 * Leaf paths for exactly this profile. Do not use `listProps('providers.models.' + key)`:
 * the gateway matches `starts_with`, so e.g. `custom:https://a/v1` also returns
 * `custom:https://a/v1-backup.*`; batch-removing those corrupts TOML on save.
 */
async function listConfiguredPathsUnderModelKey(key: string): Promise<string[]> {
  try {
    const { entries } = await listProps('providers.models');
    return entries
      .map((e) => e.path)
      .filter((path) => pathBelongsToProvidersModelKey(path, key));
  } catch {
    return [];
  }
}

async function clearEntryFields(key: string): Promise<void> {
  const discovered = await listConfiguredPathsUnderModelKey(key);
  const fieldPaths =
    discovered.length > 0
      ? discovered
      : [
          `providers.models.${key}.model`,
          `providers.models.${key}.name`,
          `providers.models.${key}.api-key`,
          `providers.models.${key}.base-url`,
        ];
  const uniq = [...new Set(fieldPaths)].sort((a, b) => {
    const da = a.split('.').length;
    const db = b.split('.').length;
    if (da !== db) return db - da;
    return b.localeCompare(a);
  });

  try {
    await patchConfig(uniq.map((path) => ({ op: 'remove', path })));
  } catch (err) {
    // Retry per path when the batch rejects (unsupported batch size, stale path…).
    if (err instanceof ApiError) {
      for (const path of uniq) {
        try {
          await patchConfig([{ op: 'remove', path }]);
        } catch {
          /* ignore — path may already be absent */
        }
      }
    } else {
      throw err;
    }
  }
}

export async function deleteModelConfiguration(input: {
  provider: string;
  model: string;
  /** Rows that stay after this delete (same order as the `/model` list). Drives `providers.fallback`. */
  remainingAfterDelete: ModelInfo[];
}): Promise<void> {
  const key = resolveProviderKey(input.provider, undefined);
  if (!key) return;

  await purgeModelUsageStatsIfUnconfigured(
    [input.model],
    input.remainingAfterDelete,
  );

  const priorFallback = await getCurrentFallback();
  await clearEntryFields(key);
  forgetApiKey(key);
  rememberModelDeleted(key);

  let promoted: { provider: string; model: string } | undefined;

  if (input.remainingAfterDelete.length === 0) {
    await clearProvidersFallback();
  } else if (priorFallback === key) {
    const next = input.remainingAfterDelete[0];
    if (next) {
      await setFallback(next.provider);
      promoted = { provider: next.provider, model: next.value };
    } else {
      await clearProvidersFallback();
    }
  }

  emitModelConfigUpdated({
    action: 'delete',
    original: { provider: input.provider, model: input.model },
    ...(promoted ? { current: promoted } : {}),
  });
}

/**
 * Resolve the current default model (`providers.fallback`) into an entry
 * the UI can match against its loaded list.
 */
export async function getDefaultModelKey(): Promise<{
  provider: string;
  model: string;
} | null> {
  const fallback = await getCurrentFallback();
  if (!fallback) return null;
  try {
    const r = await getProp(`providers.models.${fallback}.model`);
    const model = nonSecretPropString(r);
    if (model) {
      return { provider: fallback, model };
    }
    // Fallback configured but its entry has no model — treat as "no default".
    return null;
  } catch {
    return null;
  }
}

/**
 * Switch the default model to the given provider+model pair. The runtime
 * only consults `providers.fallback`, so we ignore the supplied `model`
 * after validating that the entry exists.
 */
export async function setDefaultModel(input: {
  provider: string;
  model: string;
}): Promise<void> {
  const key = resolveProviderKey(input.provider, undefined);
  if (!key) {
    throw new Error('provider 不能为空');
  }
  await setFallback(key);
  emitModelConfigUpdated({
    action: 'update',
    current: { provider: input.provider, model: input.model },
  });
}
