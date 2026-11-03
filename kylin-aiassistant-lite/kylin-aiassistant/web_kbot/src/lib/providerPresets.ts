/**
 * Provider IDs aligned with `src/onboard/wizard.rs` (setup_provider).
 * Used for the dashboard “add model route” helper; full keys live in config / docs.
 */
export type ProviderPreset = {
  id: string;
  labelZh: string;
  notesZh?: string;
};

export type ProviderTier = {
  tierZh: string;
  providers: ProviderPreset[];
};

export const PROVIDER_PRESETS: ProviderTier[] = [
  {
    tierZh: '推荐（聚合与直连）',
    providers: [
      { id: 'openrouter', labelZh: 'OpenRouter', notesZh: '多模型路由，OpenAI兼容 Chat Completions' },
      { id: 'venice', labelZh: 'Venice AI' },
      { id: 'anthropic', labelZh: 'Anthropic（Claude 直连）', notesZh: 'Messages API；需在配置中设置对应密钥' },
      { id: 'openai', labelZh: 'OpenAI', notesZh: 'Chat Completions / Responses 视 wire_api' },
      { id: 'openai-codex', labelZh: 'OpenAI Codex（OAuth）' },
      { id: 'deepseek', labelZh: 'DeepSeek' },
      { id: 'mistral', labelZh: 'Mistral' },
      { id: 'xai', labelZh: 'xAI（Grok）' },
      { id: 'perplexity', labelZh: 'Perplexity' },
      { id: 'gemini', labelZh: 'Google Gemini' },
    ],
  },
  {
    tierZh: '快速推理',
    providers: [
      { id: 'groq', labelZh: 'Groq' },
      { id: 'fireworks', labelZh: 'Fireworks AI' },
      { id: 'novita', labelZh: 'Novita AI' },
      { id: 'together-ai', labelZh: 'Together AI' },
      { id: 'nvidia', labelZh: 'NVIDIA NIM' },
    ],
  },
  {
    tierZh: '网关 / 云厂商',
    providers: [
      { id: 'vercel', labelZh: 'Vercel AI Gateway' },
      { id: 'cloudflare', labelZh: 'Cloudflare AI Gateway' },
      { id: 'astrai', labelZh: 'Astrai' },
      { id: 'avian', labelZh: 'Avian' },
      { id: 'bedrock', labelZh: 'Amazon Bedrock' },
    ],
  },
  {
    tierZh: '国内与专项',
    providers: [
      { id: 'kimi-code', labelZh: 'Kimi Code' },
      { id: 'qwen-code', labelZh: 'Qwen Code' },
      { id: 'moonshot', labelZh: 'Moonshot（国内）' },
      { id: 'moonshot-intl', labelZh: 'Moonshot（国际）' },
      { id: 'glm', labelZh: 'GLM /智谱（国际）' },
      { id: 'glm-cn', labelZh: 'GLM / 智谱（国内）' },
      { id: 'minimax', labelZh: 'MiniMax（国际）' },
      { id: 'minimax-cn', labelZh: 'MiniMax（国内）' },
      { id: 'qwen', labelZh: '通义 Qwen（国内）' },
      { id: 'qwen-intl', labelZh: '通义 Qwen（国际）' },
      { id: 'qwen-us', labelZh: '通义 Qwen（美区）' },
      { id: 'qianfan', labelZh: '百度千帆' },
      { id: 'zai', labelZh: 'Z.AI（国际）' },
      { id: 'zai-cn', labelZh: 'Z.AI（国内）' },
      { id: 'synthetic', labelZh: 'Synthetic' },
      { id: 'opencode', labelZh: 'OpenCode Zen' },
      { id: 'opencode-go', labelZh: 'OpenCode Go' },
      { id: 'cohere', labelZh: 'Cohere' },
    ],
  },
  {
    tierZh: '本地 / 自定义 OpenAI 兼容',
    providers: [
      { id: 'ollama', labelZh: 'Ollama', notesZh: '可配 api_url 指向本机或远程' },
      { id: 'llamacpp', labelZh: 'llama.cpp server' },
      { id: 'vllm', labelZh: 'vLLM' },
      { id: 'custom', labelZh: '自定义（model_providers + base_url）', notesZh: '供应商 ID 形如 custom:https://host，需在 config.toml 配置 [model_providers.xxx]' },
    ],
  },
];
