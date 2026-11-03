import type {
  StatusResponse,
  ToolSpec,
  CronJob,
  CronRun,
  Integration,
  DiagResult,
  MemoryEntry,
  CostSummary,
  CliTool,
  HealthSnapshot,
  Session,
  ChannelDetail,
  AgentProfile,
  SessionMessagesResponse,
  Skill,
} from '../types/api';
import { clearToken, getToken, setToken } from './auth';
import { apiOrigin, basePath, isQrcMode } from './basePath';
import { bridgeFetch, bridgeIsGatewayReachable, bridgeStartGatewayDaemon } from './apiBridge';
import { parse as parseToml } from 'smol-toml';

function buildApiUrl(path: string): string {
  if (/^https?:\/\//.test(path)) {
    return path;
  }

  const normalizedPath = path.startsWith('/') ? path : `/${path}`;

  // qrc 模式下不需要构造完整 URL，直接返回路径
  if (isQrcMode) {
    return normalizedPath;
  }

  const prefix = `${apiOrigin}${basePath}`;

  if (prefix) {
    if (/^https?:\/\//.test(prefix)) {
      return new URL(normalizedPath, `${prefix.replace(/\/+$/, '')}/`).toString();
    }
    return `${prefix}${normalizedPath}`;
  }

  return normalizedPath;
}

/**
 * qrc 兼容的 fetch 封装。qrc 模式下走 QWebChannel 桥接，
 * 非 qrc 模式下使用原生 fetch。
 */
async function compatFetch(url: string, init?: RequestInit): Promise<Response> {
  if (!isQrcMode) {
    return fetch(url, init);
  }

  // qrc 模式：通过桥接
  const headers: Record<string, string> = {};
  if (init?.headers) {
    if (init.headers instanceof Headers) {
      init.headers.forEach((v, k) => { headers[k] = v; });
    } else if (Array.isArray(init.headers)) {
      for (const [k, v] of init.headers) { headers[k] = v; }
    } else {
      Object.assign(headers, init.headers);
    }
  }

  // 处理 FormData（qrc 模式下 bridge 只支持 string body）
  let bodyStr: string | undefined;
  if (init?.body instanceof FormData) {
    const boundary = `----WebKitFormBoundary${Math.random().toString(36).substring(2)}`;
    const encoder = new TextEncoder();
    const byteParts: Uint8Array[] = [];

    for (const [key, value] of init.body.entries()) {
      if (typeof value === 'string') {
        const part = `--${boundary}\r\nContent-Disposition: form-data; name="${key}"\r\n\r\n${value}\r\n`;
        byteParts.push(encoder.encode(part));
      } else if (value instanceof File) {
        console.log('[compatFetch] File entry:', key, value.name, value.size, value.type, 'has arrayBuffer:', typeof value.arrayBuffer);
        let fileBytes: Uint8Array;
        if (typeof value.arrayBuffer === 'function') {
          fileBytes = new Uint8Array(await value.arrayBuffer());
        } else {
          // Qt WebEngine 可能不支持 File.arrayBuffer()，使用 FileReader 降级
          fileBytes = await new Promise<Uint8Array>((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = () => resolve(new Uint8Array(reader.result as ArrayBuffer));
            reader.onerror = () => reject(new Error('FileReader error: ' + reader.error));
            reader.readAsArrayBuffer(value);
          });
        }
        console.log('[compatFetch] fileBytes length:', fileBytes.length);
        const header = `--${boundary}\r\nContent-Disposition: form-data; name="${key}"; filename="${value.name}"\r\n`;
        const ct = value.type ? `Content-Type: ${value.type}\r\n` : '';
        const headerBytes = encoder.encode(header + ct + '\r\n');
        const footerBytes = encoder.encode('\r\n');
        const combined = new Uint8Array(headerBytes.length + fileBytes.length + footerBytes.length);
        combined.set(headerBytes, 0);
        combined.set(fileBytes, headerBytes.length);
        combined.set(footerBytes, headerBytes.length + fileBytes.length);
        byteParts.push(combined);
      }
    }
    byteParts.push(encoder.encode(`--${boundary}--\r\n`));

    const totalLen = byteParts.reduce((sum, p) => sum + p.length, 0);
    console.log('[compatFetch] total multipart body length:', totalLen);
    const fullBody = new Uint8Array(totalLen);
    let offset = 0;
    for (const p of byteParts) {
      fullBody.set(p, offset);
      offset += p.length;
    }

    let binary = '';
    const chunkSize = 8192;
    for (let i = 0; i < fullBody.length; i += chunkSize) {
      binary += String.fromCharCode(...fullBody.subarray(i, i + chunkSize));
    }
    bodyStr = btoa(binary);
    console.log('[compatFetch] base64 body length:', bodyStr.length);
    console.log('[compatFetch] boundary:', boundary);

    headers['Content-Type'] = `multipart/form-data; boundary=${boundary}`;
    headers['X-Body-Encoding'] = 'base64';
  } else if (typeof init?.body === 'string') {
    bodyStr = init.body;
  }

  const path = url.startsWith('/') ? url : `/${url}`;
  const resp = await bridgeFetch(path, {
    method: init?.method,
    headers,
    body: bodyStr,
  });

  // 构造一个类 Response 对象供调用方使用
  return {
    ok: resp.status >= 200 && resp.status < 300,
    status: resp.status,
    statusText: '',
    json: () => Promise.resolve(JSON.parse(resp.body || '{}')),
    text: () => Promise.resolve(resp.body || ''),
  } as Response;
}

// ---------------------------------------------------------------------------
// Base fetch wrapper
// ---------------------------------------------------------------------------

export class UnauthorizedError extends Error {
  constructor() {
    super('Unauthorized');
    this.name = 'UnauthorizedError';
  }
}

/**
 * Thrown when the gateway returns a structured `ConfigApiError` response body.
 * Carries the parsed envelope directly so callers can dispatch on `.code`
 * instead of regex-matching the message string.
 */
export class ApiError extends Error {
  constructor(
    public readonly status: number,
    public readonly envelope: {
      code: string;
      message: string;
      path?: string;
      op_index?: number;
    },
  ) {
    super(`[${envelope.code}] ${envelope.message}`);
    this.name = 'ApiError';
  }
}

export async function apiFetch<T = unknown>(
  path: string,
  options: RequestInit = {},
): Promise<T> {
  const token = getToken();

  // qrc 模式：通过 QWebChannel 桥接走 Unix Socket
  if (isQrcMode) {
    const headers: Record<string, string> = {};
    if (token) {
      headers['Authorization'] = `Bearer ${token}`;
    }
    if (
      options.body &&
      typeof options.body === 'string' &&
      !headers['Content-Type']
    ) {
      headers['Content-Type'] = 'application/json';
    }

    // 将 RequestInit 的 headers 合并进来
    if (options.headers) {
      if (options.headers instanceof Headers) {
        options.headers.forEach((v, k) => { headers[k] = v; });
      } else if (Array.isArray(options.headers)) {
        for (const [k, v] of options.headers) { headers[k] = v; }
      } else {
        Object.assign(headers, options.headers);
      }
    }

    const normalizedPath = path.startsWith('/') ? path : `/${path}`;
    const resp = await bridgeFetch(normalizedPath, {
      method: options.method,
      headers,
      body: typeof options.body === 'string' ? options.body : undefined,
    });

    if (resp.status === 401) {
      clearToken();
      window.dispatchEvent(new Event('kylinbot-unauthorized'));
      throw new UnauthorizedError();
    }

    if (resp.status < 200 || resp.status >= 300) {
      if (resp.body) {
        try {
          const parsed = JSON.parse(resp.body);
          if (parsed && typeof parsed === 'object' && typeof parsed.code === 'string' && typeof parsed.message === 'string') {
            throw new ApiError(resp.status, parsed);
          }
        } catch (e) {
          if (e instanceof ApiError) throw e;
        }
      }
      throw new Error(`API ${resp.status}: ${resp.body || 'Unknown error'}`);
    }

    if (resp.status === 204 || !resp.body) {
      return undefined as unknown as T;
    }

    return JSON.parse(resp.body) as T;
  }

  // 非 qrc 模式：标准 HTTP fetch
  const headers = new Headers(options.headers);

  if (token) {
    headers.set('Authorization', `Bearer ${token}`);
  }

  if (
    options.body &&
    typeof options.body === 'string' &&
    !headers.has('Content-Type')
  ) {
    headers.set('Content-Type', 'application/json');
  }

  // For FormData, pass headers as plain object so browser auto-sets
  // Content-Type with correct multipart boundary
  const fetchOptions: RequestInit =
    options.body instanceof FormData
      ? {
          ...options,
          headers: token ? { Authorization: `Bearer ${token}` } : {},
        }
      : { ...options, headers };

  const response = await fetch(buildApiUrl(path), fetchOptions);

  if (response.status === 401) {
    clearToken();
    window.dispatchEvent(new Event('kylinbot-unauthorized'));
    throw new UnauthorizedError();
  }

  if (!response.ok) {
    // Try to parse a structured ConfigApiError envelope.
    const text = await response.text().catch(() => '');
    if (text) {
      try {
        const parsed = JSON.parse(text);
        if (
          parsed &&
          typeof parsed === 'object' &&
          typeof parsed.code === 'string' &&
          typeof parsed.message === 'string'
        ) {
          throw new ApiError(response.status, parsed);
        }
      } catch (e) {
        if (e instanceof ApiError) throw e;
      }
    }
    throw new Error(`API ${response.status}: ${text || response.statusText}`);
  }

  // Some endpoints may return 204 No Content
  if (response.status === 204) {
    return undefined as unknown as T;
  }

  return response.json() as Promise<T>;
}

function unwrapField<T>(value: T | Record<string, T>, key: string): T {
  if (value !== null && typeof value === 'object' && !Array.isArray(value) && key in value) {
    const unwrapped = (value as Record<string, T | undefined>)[key];
    if (unwrapped !== undefined) {
      return unwrapped;
    }
  }
  return value as T;
}

// ---------------------------------------------------------------------------
// Pairing
// ---------------------------------------------------------------------------

export async function pair(code: string): Promise<{ token: string }> {
  const response = await compatFetch(buildApiUrl('/pair'), {
    method: 'POST',
    headers: { 'X-Pairing-Code': code },
  });

  if (!response.ok) {
    const text = await response.text().catch(() => '');
    throw new Error(`Pairing failed (${response.status}): ${text || response.statusText}`);
  }

  const data = (await response.json()) as { token: string };
  setToken(data.token);
  return data;
}

export async function getAdminPairCode(): Promise<{ pairing_code: string | null; pairing_required: boolean }> {
  // Use the public /pair/code endpoint which works in Docker and remote environments
  // (no localhost restriction). Falls back to the admin endpoint for backward compat.
  const publicResp = await compatFetch(buildApiUrl('/pair/code'));
  if (publicResp.ok) {
    return publicResp.json() as Promise<{ pairing_code: string | null; pairing_required: boolean }>;
  }

  const response = await compatFetch(buildApiUrl('/admin/paircode'));
  if (!response.ok) {
    throw new Error(`Failed to fetch pairing code (${response.status})`);
  }
  return response.json() as Promise<{ pairing_code: string | null; pairing_required: boolean }>;
}

// ---------------------------------------------------------------------------
// Public health (no auth required)
// ---------------------------------------------------------------------------

export async function getPublicHealth(): Promise<{ require_pairing: boolean; paired: boolean }> {
  const response = await compatFetch(buildApiUrl('/health'));
  if (!response.ok) {
    throw new Error(`Health check failed (${response.status})`);
  }
  return response.json() as Promise<{ require_pairing: boolean; paired: boolean }>;
}

function isHealthyStatus(status: number): boolean {
  return status >= 200 && status < 300;
}

async function probeGatewayHealth(): Promise<boolean> {
  if (isQrcMode) {
    const resp = await bridgeFetch('/health');
    return isHealthyStatus(resp.status);
  }

  const response = await compatFetch(buildApiUrl('/health'), { cache: 'no-store' });
  return response.ok;
}

/** True when kylin-bot gateway /health responds successfully. */
export async function checkGatewayAvailable(): Promise<boolean> {
  // qrc 嵌入：WebChannel 偶发晚于 React 就绪，短重试避免误判为未启动。
  const maxAttempts = isQrcMode ? 4 : 1;
  for (let attempt = 0; attempt < maxAttempts; attempt++) {
    try {
      return await probeGatewayHealth();
    } catch (err) {
      if (attempt + 1 >= maxAttempts) {
        // 桥接 HTTP 失败时再尝试 C++ 同步探测（若已编译进 WebApiBridge）。
        if (isQrcMode) {
          try {
            return await bridgeIsGatewayReachable();
          } catch {
            return false;
          }
        }
        return false;
      }
      await new Promise((resolve) => setTimeout(resolve, 250 * (attempt + 1)));
    }
  }
  return false;
}

/** Start kylin-bot daemon; returns whether launch was attempted. */
export async function startGatewayDaemon(): Promise<boolean> {
  if (isQrcMode) {
    try {
      return await bridgeStartGatewayDaemon();
    } catch (err) {
      console.error('[checkGateway] startGatewayDaemon failed:', err);
      return false;
    }
  }

  return false;
}

// ---------------------------------------------------------------------------
// Status / Health
// ---------------------------------------------------------------------------

export function getStatus(): Promise<StatusResponse> {
  return apiFetch<StatusResponse>('/api/status');
}

export function getHealth(): Promise<HealthSnapshot> {
  return apiFetch<HealthSnapshot | { health: HealthSnapshot }>('/api/health').then((data) =>
    unwrapField(data, 'health'),
  );
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

export function getConfig(): Promise<string> {
  return apiFetch<string | { format?: string; content: string }>('/api/config').then((data) =>
    typeof data === 'string' ? data : data.content,
  );
}

export function putConfig(toml: string): Promise<void> {
  return apiFetch<void>('/api/config', {
    method: 'PUT',
    headers: { 'Content-Type': 'application/toml' },
    body: toml,
  });
}

// ---------------------------------------------------------------------------
// Tools
// ---------------------------------------------------------------------------

export function getTools(): Promise<ToolSpec[]> {
  return apiFetch<ToolSpec[] | { tools: ToolSpec[] }>('/api/tools').then((data) =>
    unwrapField(data, 'tools'),
  );
}

// ---------------------------------------------------------------------------
// Skills
// ---------------------------------------------------------------------------

export function getSkills(): Promise<Skill[]> {
  return apiFetch<Skill[] | { skills: Skill[] }>('/api/skills').then((data) =>
    unwrapField(data, 'skills'),
  );
}

export function toggleSkill(name: string, enabled: boolean): Promise<{ status: string; enabled: boolean }> {
  return apiFetch<{ status: string; enabled: boolean }>(`/api/skills/${encodeURIComponent(name)}/toggle`, {
    method: 'PUT',
    body: JSON.stringify({ enabled }),
  });
}

export function openSkillFolder(name: string): Promise<{ status: string; message?: string; error?: string }> {
  return apiFetch(`/api/skills/${encodeURIComponent(name)}/open-folder`, {
    method: 'POST',
  });
}

export function openSkillhub(): Promise<{ status: string; message?: string; error?: string }> {
  return apiFetch<{ status: string; message?: string; error?: string }>('/api/skills/open-skillhub', {
    method: 'POST',
    body: JSON.stringify({}),
  });
}

export function createSkill(name: string, body: {
  description?: string;
  author?: string;
  prompts?: string[];
}): Promise<{ status: string; name: string; message: string }> {
  return apiFetch(`/api/skills/${encodeURIComponent(name)}`, {
    method: 'PUT',
    body: JSON.stringify(body),
  });
}

export function deleteSkill(name: string): Promise<{ status: string; name: string; message: string }> {
  return apiFetch(`/api/skills/${encodeURIComponent(name)}`, {
    method: 'DELETE',
  });
}

export function importSkill(file: File): Promise<{ status: string; name: string; message: string }> {
  const formData = new FormData();
  formData.append('file', file);
  const token = getToken();
  return compatFetch(buildApiUrl('/api/skills/import'), {
    method: 'POST',
    body: formData,
    headers: token ? { Authorization: `Bearer ${token}` } : {},
  }).then(async (r) => {
    if (!r.ok) {
      const text = await r.text().catch(() => '');
      let parsed: Record<string, unknown> | null = null;
      try { parsed = JSON.parse(text); } catch {}
      if (parsed && parsed.duplicate && r.status === 409) {
        const dupErr = new Error(`API ${r.status}: ${text}`);
        (dupErr as Error & { duplicate?: boolean; skillName?: string }).duplicate = true;
        (dupErr as Error & { skillName?: string }).skillName = (parsed.name as string) || '';
        throw dupErr;
      }
      throw new Error(`API ${r.status}: ${text || r.statusText}`);
    }
    return r.json();
  });
}

export function importSkillFolder(files: File[]): Promise<{ status: string; name: string; message: string }> {
  const formData = new FormData();
  for (const file of files) {
    const relativePath = (file as File & { webkitRelativePath?: string }).webkitRelativePath || file.name;
    formData.append('files', file, relativePath);
  }
  const token = getToken();
  const url = buildApiUrl('/api/skills/import-folder');
  return compatFetch(url, {
    method: 'POST',
    body: formData,
    headers: token ? { Authorization: `Bearer ${token}` } : {},
  }).then(async (r) => {
    if (!r.ok) {
      const text = await r.text().catch(() => '');
      throw new Error(`API ${r.status}: ${text || r.statusText}`);
    }
    return r.json();
  });
}

// ---------------------------------------------------------------------------
// Cron
// ---------------------------------------------------------------------------

export function getCronJobs(): Promise<CronJob[]> {
  return apiFetch<CronJob[] | { jobs: CronJob[] }>('/api/cron').then((data) =>
    unwrapField(data, 'jobs'),
  );
}

export function addCronJob(body: {
  name?: string;
  command?: string;
  prompt?: string;
  job_type?: 'shell' | 'agent';
  schedule: string | { kind: 'cron'; expr: string; tz?: string } | { kind: 'at'; at: string } | { kind: 'every'; every_ms: number };
  enabled?: boolean;
  delivery?: { mode: string; channel?: string; to?: string; best_effort?: boolean };
  session_target?: 'isolated' | 'main';
  model?: string;
  allowed_tools?: string[];
  delete_after_run?: boolean;
}): Promise<CronJob> {
  return apiFetch<CronJob | { status: string; job: CronJob }>('/api/cron', {
    method: 'POST',
    body: JSON.stringify(body),
  }).then((data) => (typeof (data as { job?: CronJob }).job === 'object' ? (data as { job: CronJob }).job : (data as CronJob)));
}

export function deleteCronJob(id: string): Promise<void> {
  return apiFetch<void>(`/api/cron/${encodeURIComponent(id)}`, {
    method: 'DELETE',
  });
}
export function patchCronJob(
  id: string,
  patch: {
    name?: string;
    schedule?: string | { kind: 'cron'; expr: string; tz?: string } | { kind: 'at'; at: string } | { kind: 'every'; every_ms: number };
    command?: string;
    prompt?: string;
    enabled?: boolean;
    delivery?: { mode: string; channel?: string; to?: string; best_effort?: boolean };
    model?: string;
    session_target?: 'isolated' | 'main';
    delete_after_run?: boolean;
    allowed_tools?: string[];
  },
): Promise<CronJob> {
  return apiFetch<CronJob | { status: string; job: CronJob }>(
    `/api/cron/${encodeURIComponent(id)}`,
    {
      method: 'PATCH',
      body: JSON.stringify(patch),
    },
  ).then((data) => (typeof (data as { job?: CronJob }).job === 'object' ? (data as { job: CronJob }).job : (data as CronJob)));
}

export function runCronJobNow(id: string, approved: boolean = false): Promise<{ ok: boolean; status: string; output: string; duration_ms: number; job_id: string }> {
  return apiFetch<{ ok: boolean; status: string; output: string; duration_ms: number; job_id: string }>(
    `/api/cron/${encodeURIComponent(id)}/run`,
    {
      method: 'POST',
      body: JSON.stringify({ approved }),
    },
  );
}

export function pauseCronJob(id: string): Promise<CronJob> {
  return apiFetch<CronJob | { status: string; job: CronJob }>(
    `/api/cron/${encodeURIComponent(id)}/pause`,
    { method: 'POST' },
  ).then((data) => (typeof (data as { job?: CronJob }).job === 'object' ? (data as { job: CronJob }).job : (data as CronJob)));
}

export function resumeCronJob(id: string): Promise<CronJob> {
  return apiFetch<CronJob | { status: string; job: CronJob }>(
    `/api/cron/${encodeURIComponent(id)}/resume`,
    { method: 'POST' },
  ).then((data) => (typeof (data as { job?: CronJob }).job === 'object' ? (data as { job: CronJob }).job : (data as CronJob)));
}


export function getCronRuns(
  jobId: string,
  limit: number = 20,
): Promise<CronRun[]> {
  const params = new URLSearchParams({ limit: String(limit) });
  return apiFetch<CronRun[] | { runs: CronRun[] }>(
    `/api/cron/${encodeURIComponent(jobId)}/runs?${params}`,
  ).then((data) => unwrapField(data, 'runs'));
}

export interface CronSettings {
  enabled: boolean;
  catch_up_on_startup: boolean;
  max_run_history: number;
}

export function getCronSettings(): Promise<CronSettings> {
  return apiFetch<CronSettings>('/api/cron/settings');
}

export function patchCronSettings(
  patch: Partial<CronSettings>,
): Promise<CronSettings> {
  return apiFetch<CronSettings & { status: string }>('/api/cron/settings', {
    method: 'PATCH',
    body: JSON.stringify(patch),
  });
}

// ---------------------------------------------------------------------------
// Integrations
// ---------------------------------------------------------------------------

export function getIntegrations(): Promise<Integration[]> {
  return apiFetch<Integration[] | { integrations: Integration[] }>('/api/integrations').then(
    (data) => unwrapField(data, 'integrations'),
  );
}

// ---------------------------------------------------------------------------
// Doctor / Diagnostics
// ---------------------------------------------------------------------------

export function runDoctor(): Promise<DiagResult[]> {
  return apiFetch<DiagResult[] | { results: DiagResult[]; summary?: unknown }>('/api/doctor', {
    method: 'POST',
    body: JSON.stringify({}),
  }).then((data) => (Array.isArray(data) ? data : data.results));
}

// ---------------------------------------------------------------------------
// Memory
// ---------------------------------------------------------------------------

export function getMemory(
  query?: string,
  category?: string,
): Promise<MemoryEntry[]> {
  const params = new URLSearchParams();
  if (query) params.set('query', query);
  if (category) params.set('category', category);
  const qs = params.toString();
  return apiFetch<MemoryEntry[] | { entries: MemoryEntry[] }>(`/api/memory${qs ? `?${qs}` : ''}`).then(
    (data) => unwrapField(data, 'entries'),
  );
}

export function storeMemory(
  key: string,
  content: string,
  category?: string,
): Promise<void> {
  return apiFetch<unknown>('/api/memory', {
    method: 'POST',
    body: JSON.stringify({ key, content, category }),
  }).then(() => undefined);
}

export function deleteMemory(key: string): Promise<void> {
  return apiFetch<void>(`/api/memory/${encodeURIComponent(key)}`, {
    method: 'DELETE',
  });
}

// ---------------------------------------------------------------------------
// Cost
// ---------------------------------------------------------------------------

export function getCost(): Promise<CostSummary> {
  return apiFetch<CostSummary | { cost: CostSummary }>('/api/cost').then((data) =>
    unwrapField(data, 'cost'),
  );
}

export function purgeModelCostStats(
  models: string[],
): Promise<{ removed_records: number }> {
  return apiFetch<{ removed_records: number } | { status: string; removed_records: number }>(
    '/api/cost/purge',
    {
      method: 'POST',
      body: JSON.stringify({ models }),
    },
  ).then((data) => ({
    removed_records: data.removed_records ?? 0,
  }));
}

// ---------------------------------------------------------------------------
// Sessions
// ---------------------------------------------------------------------------

export function getSessions(): Promise<Session[]> {
  return apiFetch<Session[] | { sessions: Session[] }>('/api/sessions').then((data) =>
    unwrapField(data, 'sessions'),
  );
}

export function getSession(id: string): Promise<Session> {
  return apiFetch<Session>(`/api/sessions/${encodeURIComponent(id)}`);
}

/** Load persisted gateway WebSocket chat transcript for the dashboard Agent Chat. */
export function getSessionMessages(id: string): Promise<SessionMessagesResponse> {
  return apiFetch<SessionMessagesResponse>(
    `/api/sessions/${encodeURIComponent(id)}/messages`,
  );
}

// ---------------------------------------------------------------------------
// Channels (detailed)
// ---------------------------------------------------------------------------

export function getChannels(): Promise<ChannelDetail[]> {
  return apiFetch<ChannelDetail[] | { channels: ChannelDetail[] }>('/api/channels').then((data) =>
    unwrapField(data, 'channels'),
  );
}

export function getAgents(): Promise<AgentProfile[]> {
  return apiFetch<AgentProfile[] | { agents: AgentProfile[] }>('/api/agents').then((data) =>
    unwrapField(data, 'agents'),
  );
}

// ---------------------------------------------------------------------------
// CLI Tools
// ---------------------------------------------------------------------------

export function getCliTools(): Promise<CliTool[]> {
  return apiFetch<CliTool[] | { cli_tools: CliTool[] }>('/api/cli-tools').then((data) =>
    unwrapField(data, 'cli_tools'),
  );
}

// ---------------------------------------------------------------------------
// WeChat Bind
// ---------------------------------------------------------------------------

export interface WechatQrResponse {
  qrcode: string;
  qrcode_img_url: string;
  poll_url: string;
  expires_in: number;
}

export interface WechatStatusResponse {
  status: 'wait' | 'scaned' | 'expired' | 'confirmed';
  bot_token?: string;
  ilink_bot_id?: string;
  ilink_user_id?: string;
}

export function getWechatQrCode(): Promise<WechatQrResponse> {
  return compatFetch(buildApiUrl('/api/wechat/qrcode')).then((res) => {
    if (!res.ok) {
      return res.json().then((err) => Promise.reject(new Error(err.error || `QR code request failed (${res.status})`)));
    }
    return res.json() as Promise<WechatQrResponse>;
  });
}

export function pollWechatStatus(qrcode: string): Promise<WechatStatusResponse> {
  const params = new URLSearchParams({ qrcode });
  return compatFetch(buildApiUrl(`/api/wechat/status?${params}`)).then((res) => {
    if (!res.ok) {
      return res.json().then((err) => Promise.reject(new Error(err.error || `Status poll failed (${res.status})`)));
    }
    return res.json() as Promise<WechatStatusResponse>;
  });
}

// ---------------------------------------------------------------------------
// Feishu Bind (RFC 8628 Device Authorization Grant)
// ---------------------------------------------------------------------------

export interface FeishuBeginResponse {
  device_code: string;
  verification_uri_complete: string;
  expires_in: number;
  interval: number;
}

export interface FeishuUserInfo {
  open_id?: string;
  tenant_brand?: string;
}

export interface FeishuPollResponse {
  client_id?: string;
  client_secret?: string;
  user_info?: FeishuUserInfo;
  error?: string;
  error_description?: string;
}

export function beginFeishuRegistration(): Promise<FeishuBeginResponse> {
  return compatFetch(buildApiUrl('/api/feishu/register/begin'), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
  }).then((res) => {
    if (!res.ok) {
      return res.json().then((err) =>
        Promise.reject(new Error(err.error || `Feishu registration begin failed (${res.status})`))
      );
    }
    return res.json() as Promise<FeishuBeginResponse>;
  });
}

export function pollFeishuRegistration(deviceCode: string): Promise<FeishuPollResponse> {
  return compatFetch(buildApiUrl('/api/feishu/register/poll'), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ device_code: deviceCode }),
  }).then((res) => {
    if (!res.ok) {
      return res.json().then((err) =>
        Promise.reject(new Error(err.error || `Feishu registration poll failed (${res.status})`))
      );
    }
    return res.json() as Promise<FeishuPollResponse>;
  });
}

// ---------------------------------------------------------------------------
// Feishu WebSocket manual bind
// ---------------------------------------------------------------------------

export interface FeishuWebSocketConfigRequest {
  app_id: string;
  app_secret: string;
}

export function saveFeishuWebSocketConfig(appId: string, appSecret: string): Promise<void> {
  return apiFetch<void>('/api/feishu/config', {
    method: 'POST',
    body: JSON.stringify({
      app_id: appId,
      app_secret: appSecret,
    } satisfies FeishuWebSocketConfigRequest),
  });
}

// ---------------------------------------------------------------------------
// DingTalk Device Flow (scan QR code to register app)
// ---------------------------------------------------------------------------

export interface DingtalkBeginResponse {
  device_code: string;
  verification_uri_complete: string;
  expires_in: number;
  interval: number;
}

export interface DingtalkPollResponse {
  status?: string;
  client_id?: string;
  client_secret?: string;
  fail_reason?: string;
  error?: string;
}

export function beginDingtalkRegistration(): Promise<DingtalkBeginResponse> {
  return compatFetch(buildApiUrl('/api/dingtalk/register/begin'), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
  }).then((res) => {
    if (!res.ok) {
      return res.json().then((err) =>
        Promise.reject(new Error(err.error || `DingTalk registration begin failed (${res.status})`))
      );
    }
    return res.json() as Promise<DingtalkBeginResponse>;
  });
}

export function pollDingtalkRegistration(deviceCode: string): Promise<DingtalkPollResponse> {
  return compatFetch(buildApiUrl('/api/dingtalk/register/poll'), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ device_code: deviceCode }),
  }).then((res) => {
    if (!res.ok) {
      return res.json().then((err) =>
        Promise.reject(new Error(err.error || `DingTalk registration poll failed (${res.status})`))
      );
    }
    return res.json() as Promise<DingtalkPollResponse>;
  });
}

// ---------------------------------------------------------------------------
// DingTalk Stream Mode manual bind
// ---------------------------------------------------------------------------

export interface DingtalkStreamConfigRequest {
  client_id: string;
  client_secret: string;
}

export function saveDingtalkStreamConfig(clientId: string, clientSecret: string): Promise<void> {
  return apiFetch<void>('/api/dingtalk/config', {
    method: 'POST',
    body: JSON.stringify({
      client_id: clientId,
      client_secret: clientSecret,
    } satisfies DingtalkStreamConfigRequest),
  });
}

// ---------------------------------------------------------------------------
// WeCom AI Bot QR Code Binding
// ---------------------------------------------------------------------------

export interface WecomQrResponse {
  scode: string;
  auth_url: string;
}

export interface WecomStatusResponse {
  status: string;
  bot_id?: string;
  secret?: string;
}

export function fetchWecomQrCode(): Promise<WecomQrResponse> {
  return compatFetch(buildApiUrl('/api/wecom/qrcode'), {
    method: 'GET',
    headers: { 'Content-Type': 'application/json' },
  }).then((res) => {
    if (!res.ok) {
      return res.json().then((err) =>
        Promise.reject(new Error(err.error || `WeCom QR code request failed (${res.status})`))
      );
    }
    return res.json() as Promise<WecomQrResponse>;
  });
}

export function pollWecomStatus(scode: string): Promise<WecomStatusResponse> {
  return compatFetch(buildApiUrl(`/api/wecom/status?scode=${encodeURIComponent(scode)}`), {
    method: 'GET',
    headers: { 'Content-Type': 'application/json' },
  }).then((res) => {
    if (!res.ok) {
      return res.json().then((err) =>
        Promise.reject(new Error(err.error || `WeCom status check failed (${res.status})`))
      );
    }
    return res.json() as Promise<WecomStatusResponse>;
  });
}

export interface WecomBotConfigRequest {
  bot_id: string;
  secret: string;
}

export function saveWecomBotConfig(botId: string, secret: string): Promise<void> {
  return apiFetch<void>('/api/wecom/config', {
    method: 'POST',
    body: JSON.stringify({
      bot_id: botId,
      secret,
    } satisfies WecomBotConfigRequest),
  });
}

// ---------------------------------------------------------------------------
// Lansenger manual bind
// ---------------------------------------------------------------------------

export const LANSENGER_DEFAULT_API_GATEWAY_URL = 'https://apigw-cec.cec.com.cn/open/apigw';

export interface LansengerConfigRequest {
  app_id: string;
  app_secret: string;
  api_gateway_url?: string;
}

export interface LansengerProbeResponse {
  ok: boolean;
  error?: string;
}

function buildLansengerRequest(
  appId: string,
  appSecret: string,
  apiGatewayUrl?: string,
): LansengerConfigRequest {
  const gatewayUrl = apiGatewayUrl?.trim();
  return {
    app_id: appId,
    app_secret: appSecret,
    ...(gatewayUrl ? { api_gateway_url: gatewayUrl } : {}),
  };
}

export function probeLansengerConfig(
  appId: string,
  appSecret: string,
  apiGatewayUrl?: string,
): Promise<LansengerProbeResponse> {
  return apiFetch<LansengerProbeResponse>('/api/lansenger/probe', {
    method: 'POST',
    body: JSON.stringify(buildLansengerRequest(appId, appSecret, apiGatewayUrl)),
  });
}

export function saveLansengerConfig(
  appId: string,
  appSecret: string,
  apiGatewayUrl?: string,
): Promise<void> {
  return apiFetch<void>('/api/lansenger/config', {
    method: 'POST',
    body: JSON.stringify(buildLansengerRequest(appId, appSecret, apiGatewayUrl)),
  });
}

export type ChannelKey = 'wechat' | 'wecom' | 'dingtalk' | 'feishu' | 'lansenger';

export interface ChannelIdentifier {
  label: string;
  value: string;
}

export interface WechatAccountResponse {
  account_id?: string;
  token?: string;
  base_url?: string;
  user_id?: string;
  saved_at?: string;
}

export function getWechatAccount(): Promise<WechatAccountResponse> {
  return apiFetch<WechatAccountResponse>('/api/wechat/account');
}

const CHANNEL_LABELS: Record<ChannelKey, string> = {
  wechat: 'Bot ID',
  wecom: 'Bot ID',
  dingtalk: 'Client ID',
  feishu: 'App ID',
  lansenger: 'App ID',
};

export async function getChannelIdentifier(channelKey: ChannelKey): Promise<ChannelIdentifier> {
  const detail = (await getChannels()).find((channel) => {
    const type = channel.type.toLowerCase();
    return type === channelKey || (channelKey === 'wechat' && type === 'weixin');
  });
  if (detail?.identifier?.value) {
    return { label: CHANNEL_LABELS[channelKey], value: detail.identifier.value };
  }

  switch (channelKey) {
    case 'wechat': {
      try {
        const account = await getWechatAccount();
        return { label: CHANNEL_LABELS[channelKey], value: account.account_id?.split('@')[0] || '—' };
      } catch {
        return { label: CHANNEL_LABELS[channelKey], value: '—' };
      }
    }
    case 'wecom':
    case 'dingtalk':
    case 'feishu':
    case 'lansenger': {
      const toml = await getConfig();
      const config = parseToml(toml) as Record<string, unknown>;
      const channels = config.channels as Record<string, Record<string, unknown>> | undefined;
      const section = channels?.[channelKey];
      switch (channelKey) {
        case 'wecom':
          return { label: CHANNEL_LABELS[channelKey], value: String(section?.bot_id ?? '—') };
        case 'dingtalk':
          return { label: CHANNEL_LABELS[channelKey], value: String(section?.client_id ?? '—') };
        case 'feishu':
          return { label: CHANNEL_LABELS[channelKey], value: String(section?.app_id ?? '—') };
        case 'lansenger':
          return { label: CHANNEL_LABELS[channelKey], value: String(section?.app_id ?? '—') };
        default:
          return { label: 'ID', value: '—' };
      }
    }
  }
}

export function deleteWechatAccount(): Promise<void> {
  return apiFetch<void>('/api/wechat/account', { method: 'DELETE' });
}

export async function disconnectChannel(channelKey: ChannelKey): Promise<void> {
  await apiFetch<void>(`/api/channels/${encodeURIComponent(channelKey)}`, {
    method: 'DELETE',
  });
}

// ---------------------------------------------------------------------------
// Config — per-property CRUD (schema-driven, from zeroclaw 0.7.x)
// ---------------------------------------------------------------------------

export interface ValidationWarning {
  code: string;
  message: string;
  path: string;
}

export interface PropResponse {
  path: string;
  value?: unknown;
  populated?: boolean;
  warnings?: ValidationWarning[];
}

export interface ListResponseEntry {
  path: string;
  category: string;
  kind: string;
  type_hint: string;
  value?: unknown;
  populated: boolean;
  is_secret: boolean;
  enum_variants?: string[];
  onboard_section?: string;
}

export interface DriftEntry {
  path: string;
  secret?: boolean;
  drifted: boolean;
  in_memory_value?: unknown;
  on_disk_value?: unknown;
}

export interface ListResponse {
  entries: ListResponseEntry[];
  drifted?: DriftEntry[];
}

export interface PatchOp {
  op: 'add' | 'replace' | 'remove' | 'test';
  path: string;
  value?: unknown;
  comment?: string;
}

export interface PatchOpResult {
  op: string;
  path: string;
  value?: unknown;
  populated?: boolean;
  comment?: string;
}

export interface PatchResponse {
  saved: boolean;
  results: PatchOpResult[];
  warnings?: ValidationWarning[];
}

export interface ConfigApiError {
  code: string;
  message: string;
  path?: string;
  op_index?: number;
}

export function getProp(path: string): Promise<PropResponse> {
  return apiFetch<PropResponse>(`/api/config/prop?path=${encodeURIComponent(path)}`);
}

export function putProp(path: string, value: unknown, comment?: string): Promise<PropResponse> {
  return apiFetch<PropResponse>('/api/config/prop', {
    method: 'PUT',
    body: JSON.stringify({ path, value, comment }),
  });
}

export function deleteProp(path: string): Promise<PropResponse> {
  return apiFetch<PropResponse>(`/api/config/prop?path=${encodeURIComponent(path)}`, {
    method: 'DELETE',
  });
}

export function listProps(prefix?: string): Promise<ListResponse> {
  const q = prefix ? `?prefix=${encodeURIComponent(prefix)}` : '';
  return apiFetch<ListResponse>(`/api/config/list${q}`);
}

export function patchConfig(ops: PatchOp[]): Promise<PatchResponse> {
  return apiFetch<PatchResponse>('/api/config', {
    method: 'PATCH',
    body: JSON.stringify(ops),
  });
}

export function getDrift(): Promise<{ drifted: DriftEntry[] }> {
  return apiFetch<{ drifted: DriftEntry[] }>('/api/config/drift');
}

// ── Config schema descriptions ──────────────────────────────────────────────

type JsonSchema = Record<string, unknown> | undefined;

let configSchemaCache: Promise<JsonSchema> | null = null;

export function fetchConfigSchema(): Promise<JsonSchema> {
  if (!configSchemaCache) {
    configSchemaCache = apiFetch<JsonSchema>('/api/config', { method: 'OPTIONS' })
      .catch(() => undefined);
  }
  return configSchemaCache;
}

function resolveRef(node: unknown, root: unknown): unknown {
  if (!node || typeof node !== 'object') return node;
  const ref = (node as { $ref?: unknown }).$ref;
  if (typeof ref !== 'string' || !ref.startsWith('#/')) return node;
  let target: unknown = root;
  for (const seg of ref.slice(2).split('/')) {
    if (target && typeof target === 'object') target = (target as Record<string, unknown>)[seg];
    else return node;
  }
  return target ?? node;
}

function unwrapOptional(node: unknown): unknown {
  if (!node || typeof node !== 'object') return node;
  const anyOf = (node as { anyOf?: unknown[] }).anyOf;
  if (!Array.isArray(anyOf)) return node;
  const nonNull = anyOf.find((b) => {
    if (!b || typeof b !== 'object') return false;
    const t = (b as { type?: unknown }).type;
    return t !== 'null' && !(Array.isArray(t) && t.includes('null') && t.length === 1);
  });
  return nonNull ?? node;
}

function resolveAndUnwrap(node: unknown, root: unknown): unknown {
  let cur = node;
  for (let i = 0; i < 8; i++) {
    const next = unwrapOptional(resolveRef(cur, root));
    if (next === cur) return cur;
    cur = next;
  }
  return cur;
}

export interface ObjectArrayPropMeta {
  key: string;
  label: string;
  kind: string;
  description?: string;
  enumVariants?: string[];
  optional: boolean;
}

export function objectArrayElementProps(
  schema: JsonSchema,
  kebabPath: string,
): ObjectArrayPropMeta[] | null {
  if (!schema) return null;
  let cur: unknown = schema;
  for (const seg of kebabPath.split('.')) {
    cur = unwrapOptional(resolveRef(cur, schema));
    if (!cur || typeof cur !== 'object') return null;
    const snake = seg.replace(/-/g, '_');
    const props = (cur as { properties?: Record<string, unknown> }).properties;
    const additional = (cur as { additionalProperties?: unknown }).additionalProperties;
    if (props && Object.prototype.hasOwnProperty.call(props, snake)) {
      cur = props[snake];
    } else if (additional && typeof additional === 'object') {
      cur = additional;
    } else {
      return null;
    }
  }
  cur = unwrapOptional(resolveRef(cur, schema));
  if (!cur || typeof cur !== 'object') return null;
  const items = (cur as { items?: unknown }).items;
  if (!items) return null;
  const elem = unwrapOptional(resolveRef(items, schema));
  if (!elem || typeof elem !== 'object') return null;
  const elemProps = (elem as { properties?: Record<string, unknown> }).properties;
  if (!elemProps) return null;
  const out: ObjectArrayPropMeta[] = [];
  for (const [snakeKey, raw] of Object.entries(elemProps)) {
    const wrapped = raw as { description?: unknown; type?: unknown; anyOf?: unknown[]; enum?: unknown[] } | null;
    const desc = typeof wrapped?.description === 'string' ? wrapped.description : undefined;
    const isOptional = Array.isArray(wrapped?.anyOf)
      || (Array.isArray(wrapped?.type) && (wrapped!.type as string[]).includes('null'));
    const resolved = unwrapOptional(resolveRef(wrapped, schema)) as Record<string, unknown> | null;
    const t = resolved?.type;
    const enumVariants = Array.isArray(resolved?.enum)
      ? (resolved!.enum as unknown[]).filter((v): v is string => typeof v === 'string')
      : undefined;
    let kind: string = 'unknown';
    if (enumVariants && enumVariants.length > 0) kind = 'enum';
    else if (t === 'boolean' || (Array.isArray(t) && t.includes('boolean'))) kind = 'bool';
    else if (t === 'integer' || (Array.isArray(t) && t.includes('integer'))) kind = 'integer';
    else if (t === 'number' || (Array.isArray(t) && t.includes('number'))) kind = 'float';
    else if (t === 'string' || (Array.isArray(t) && t.includes('string'))) kind = 'string';
    else if (t === 'array') {
      const arrItems = resolved?.items as { type?: unknown } | undefined;
      kind = arrItems?.type === 'string' ? 'string-array' : 'array';
    } else if (t === 'object') kind = 'object';
    out.push({ key: snakeKey, label: snakeKey.replace(/_/g, ' '), kind, description: desc, enumVariants, optional: isOptional });
  }
  return out;
}

export function descriptionForPath(schema: JsonSchema, kebabPath: string): string | null {
  if (!schema) return null;
  let cur: unknown = schema;
  let last: unknown = null;
  for (const seg of kebabPath.split('.')) {
    cur = resolveAndUnwrap(cur, schema);
    if (!cur || typeof cur !== 'object') return null;
    const snake = seg.replace(/-/g, '_');
    const props = (cur as { properties?: Record<string, unknown> }).properties;
    const additional = (cur as { additionalProperties?: unknown }).additionalProperties;
    if (props && Object.prototype.hasOwnProperty.call(props, snake)) {
      last = props[snake];
    } else if (additional && typeof additional === 'object') {
      last = additional;
    } else {
      return null;
    }
    cur = last;
  }
  const wrapDesc = (last as { description?: unknown } | null)?.description;
  if (typeof wrapDesc === 'string' && wrapDesc.length > 0) return wrapDesc;
  const resolved = resolveAndUnwrap(last, schema) as { description?: unknown } | null;
  const innerDesc = resolved?.description;
  return typeof innerDesc === 'string' && innerDesc.length > 0 ? innerDesc : null;
}

// ---------------------------------------------------------------------------
// Onboard sections + picker (schema-driven config, from zeroclaw 0.7.x)
// ---------------------------------------------------------------------------

export interface SectionInfo {
  key: string;
  label: string;
  help: string;
  has_picker: boolean;
  completed: boolean;
  group: string;
}

export interface SectionsResponse {
  sections: SectionInfo[];
}

export function getSections(): Promise<SectionsResponse> {
  return apiFetch<SectionsResponse>('/api/onboard/sections');
}

export interface PickerItem {
  key: string;
  label: string;
  description?: string;
  badge?: string;
}

export interface PickerResponse {
  section: string;
  items: PickerItem[];
  help: string;
}

export function getSectionPicker(section: string): Promise<PickerResponse> {
  return apiFetch<PickerResponse>(
    `/api/onboard/sections/${encodeURIComponent(section)}`,
  );
}

export interface SelectItemResponse {
  fields_prefix: string;
  created: boolean;
}

export function selectSectionItem(section: string, key: string): Promise<SelectItemResponse> {
  return apiFetch<SelectItemResponse>(
    `/api/onboard/sections/${encodeURIComponent(section)}/items/${encodeURIComponent(key)}`,
    { method: 'POST' },
  );
}

// ---------------------------------------------------------------------------
// Catalog (provider + model picker)
// ---------------------------------------------------------------------------

export interface CatalogProvider {
  name: string;
  display_name: string;
  local: boolean;
  aliases: string[];
}

export interface CatalogResponse {
  providers: CatalogProvider[];
}

export function getCatalog(): Promise<CatalogResponse> {
  return apiFetch<CatalogResponse>('/api/onboard/catalog');
}

export interface CatalogModelsResponse {
  provider: string;
  models: string[];
  live: boolean;
}

export function getCatalogModels(provider: string): Promise<CatalogModelsResponse> {
  return apiFetch<CatalogModelsResponse>(
    `/api/onboard/catalog/models?provider=${encodeURIComponent(provider)}`,
  );
}

export interface CatalogProviderDefaultsResponse {
  provider: string;
  /** Schema `base-url` onboarding prefill (unset fields omitted). */
  base_url?: string | null;
}

/// Trait-level defaults for a provider — same source as onboarding `apply_provider_trait_defaults`.
export function getCatalogProviderDefaults(
  provider: string,
): Promise<CatalogProviderDefaultsResponse> {
  return apiFetch<CatalogProviderDefaultsResponse>(
    `/api/onboard/catalog/provider-defaults?provider=${encodeURIComponent(provider)}`,
  );
}

// ---------------------------------------------------------------------------
// Daemon admin
// ---------------------------------------------------------------------------

export interface AdminResponse {
  success: boolean;
  message: string;
}

export function reloadDaemon(): Promise<AdminResponse> {
  return apiFetch<AdminResponse>('/admin/reload', { method: 'POST' });
}
