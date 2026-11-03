import { useState, useEffect, useRef, useCallback, useLayoutEffect, useMemo } from 'react';
import { Send, Bot, User, AlertCircle, Copy, Check, ChevronDown, ChevronRight, Paperclip, X, Square } from 'lucide-react';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import { useNavigate, useOutletContext } from 'react-router-dom';
import type { WsMessage } from '@/types/api';
import { WebSocketClient, getOrCreateSessionId, replaceSessionId } from '@/lib/ws';
import type { WsOutboundAttachment } from '@/types/api';
import { generateUUID } from '@/lib/uuid';
import { useDraft } from '@/hooks/useDraft';
import { useTheme } from '@/hooks/useTheme';
import { t } from '@/lib/i18n';
import { getSessionMessages, getStatus } from '@/lib/api';
import ToolCallCard from '@/components/ToolCallCard';
import type { ToolCallInfo } from '@/components/ToolCallCard';
import {
  MODEL_CONFIG_UPDATED_EVENT,
  loadConfiguredModels,
  setDefaultModel,
  type ModelConfigUpdatedDetail,
  type ModelInfo,
} from '@/lib/models';
import { dispatchCostSummaryStale } from '@/lib/costSummaryEvents';
import {
  loadChatHistory,
  mapServerMessagesToPersisted,
  persistedToUiMessages,
  saveChatHistory,
  uiMessagesToPersisted,
} from '@/lib/chatHistoryStorage';
import type { AgentPageOutletContext } from '@/components/layout/Layout';
import { useAgentNewSession } from '@/contexts/AgentNewSessionContext';
import { assetBasePath } from '@/lib/basePath';
import { IdentitySetupModal } from '@/components/IdentitySetupModal';

interface ChatMessage {
  id: string;
  role: 'user' | 'agent';
  kind?: 'text' | 'event' | 'no-model-error';
  content?: string;
  thinking?: string;
  markdown?: boolean;
  toolCall?: ToolCallInfo;
  eventTitle?: string;
  eventSummary?: string;
  eventPayload?: WsMessage;
  attachments?: ChatAttachment[];
  timestamp: Date;
}

type ChatAttachmentKind = 'image' | 'audio' | 'video' | 'file';

interface ChatAttachment {
  id: string;
  name: string;
  mimeType?: string;
  kind: ChatAttachmentKind;
  dataBase64: string;
  previewUrl?: string;
}

interface PendingApproval {
  requestId: string;
  toolName: string;
  argumentsSummary: string;
  expiresAt: number | null;
  choices: Array<'yes' | 'no' | 'always'>;
  submitting: boolean;
}

const DRAFT_KEY = 'agent-chat';
const CHAT_STORAGE_PREFIX = 'kylinbot_agent_chat_';
const MODEL_SELECTION_STORAGE_KEY = 'kylinbot_agent_selected_model_v1';
const MAX_VISIBLE_MESSAGES = 100;
const TIME_DIVIDER_GAP_MS = 5 * 60 * 1000;
const MAX_SINGLE_ATTACHMENT_BYTES = 15 * 1024 * 1024;
const MAX_TOTAL_ATTACHMENT_BYTES = 40 * 1024 * 1024;
const FILE_PICKER_ACCEPT = 'image/*,audio/*,video/*,*/*';

const LIGHT_SUGGESTION_GROUPS = [
  ['agent.light_suggestion_one', 'agent.light_suggestion_two', 'agent.light_suggestion_three'],
  ['agent.light_suggestion_four', 'agent.light_suggestion_five', 'agent.light_suggestion_six'],
  ['agent.light_suggestion_seven', 'agent.light_suggestion_eight', 'agent.light_suggestion_nine'],
] as const;

function readPersistedModelSelection(): { model?: string; provider?: string } {
  try {
    const raw = localStorage.getItem(MODEL_SELECTION_STORAGE_KEY);
    if (!raw) return {};
    const parsed = JSON.parse(raw) as { model?: string; provider?: string };
    return {
      model: parsed.model?.trim() || undefined,
      provider: parsed.provider?.trim() || undefined,
    };
  } catch {
    return {};
  }
}

function writePersistedModelSelection(selection: { model?: string; provider?: string }) {
  try {
    if (!selection.model || !selection.provider) {
      localStorage.removeItem(MODEL_SELECTION_STORAGE_KEY);
      return;
    }
    localStorage.setItem(MODEL_SELECTION_STORAGE_KEY, JSON.stringify(selection));
  } catch {
    // ignore storage errors
  }
}

function sameProviderModel(
  left: { provider?: string; model?: string },
  right: { provider?: string; model?: string },
): boolean {
  const leftProvider = left.provider?.trim().toLowerCase();
  const leftModel = left.model?.trim().toLowerCase();
  const rightProvider = right.provider?.trim().toLowerCase();
  const rightModel = right.model?.trim().toLowerCase();
  return Boolean(leftProvider && leftModel && leftProvider === rightProvider && leftModel === rightModel);
}

function lightThemeIconPath(fileName: string): string {
  return `${assetBasePath}/icons/${encodeURIComponent(fileName)}`;
}

function LightThemeAssetIcon({
  fileName,
  alt,
  className,
  style = {},
}: {
  fileName: string;
  alt: string;
  className: string;
  style?: React.CSSProperties;
}) {
  // 为发送图标添加旋转纠正
  const isSendIcon = fileName === "send_text_white.svg";
  const iconStyle = isSendIcon
    ? { ...style, transform: "rotate(0deg)" }
    : style;

  return <img src={lightThemeIconPath(fileName)} alt={alt} className={className} style={iconStyle} />;
}

const RECOMMENDED_ASK_COLORS = {
  default: 'rgba(0, 0, 0, 0.6)',
  hover: '#3790FA',
  pressed: '#2C73C8',
} as const;

function TruncatableSuggestionText({
  text,
  className,
  style,
}: {
  text: string;
  className?: string;
  style?: React.CSSProperties;
}) {
  const ref = useRef<HTMLSpanElement>(null);
  const [truncated, setTruncated] = useState(false);

  const measure = useCallback(() => {
    const el = ref.current;
    if (!el) return;
    setTruncated(el.scrollWidth > el.clientWidth);
  }, [text]);

  useLayoutEffect(() => {
    measure();
    const el = ref.current;
    if (!el || typeof ResizeObserver === 'undefined') return;
    const ro = new ResizeObserver(() => measure());
    ro.observe(el);
    return () => ro.disconnect();
  }, [measure, text]);

  return (
    <span ref={ref} className={className} style={style} title={truncated ? text : undefined}>
      {text}
    </span>
  );
}

function LightRecommendedQuestionsCard({
  prompts,
  suggestionGroupIndex,
  disabled,
  onSelect,
}: {
  prompts: string[];
  suggestionGroupIndex: number;
  disabled: boolean;
  onSelect: (prompt: string) => void;
}) {
  const [cardInside, setCardInside] = useState(false);
  const [rowHover, setRowHover] = useState<number | null>(null);
  const [rowPress, setRowPress] = useState<number | null>(null);

  useEffect(() => {
    setRowHover(null);
    setRowPress(null);
  }, [suggestionGroupIndex]);

  const showBorder = !disabled && (cardInside || rowHover !== null || rowPress !== null);

  return (
    <div
      className="w-full min-h-[140px] rounded-lg"
      style={{
        boxSizing: 'border-box',
        borderWidth: 1,
        borderStyle: 'solid',
        borderColor: showBorder ? 'rgba(0, 0, 0, 0.1)' : 'transparent',
        background: 'transparent',
      }}
      onMouseEnter={() => {
        if (!disabled) setCardInside(true);
      }}
      onMouseLeave={() => {
        setCardInside(false);
        setRowHover(null);
        setRowPress(null);
      }}
    >
      <div className="relative min-h-[138px] w-full overflow-hidden rounded-[8px]">
        <div className="pointer-events-none absolute inset-0 z-0 rounded-[8px] bg-white" />
        <div
          className="pointer-events-none absolute inset-0 z-[1] rounded-[8px]"
          style={{
            opacity: 0.05,
            background: 'linear-gradient(90deg, #6135FF 0%, #355DFF 100%)',
          }}
        />
        <div className="relative z-[2] pt-4">
          <div className="flex h-4 items-center gap-2 px-4">
            <img src={lightThemeIconPath('ask.svg')} alt="" className="h-4 w-4 shrink-0" />
            <span className="text-[14px] font-semibold leading-4" style={{ color: '#1A1A1A' }}>
              {t('agent.light_card_recommended')}
            </span>
          </div>
          <div className="flex flex-col gap-2 pb-4 pl-5 pr-4 pt-2">
            {prompts.map((prompt, index) => {
              const hovered = rowHover === index;
              const pressed = rowPress === index;
              const tone = disabled
                ? RECOMMENDED_ASK_COLORS.default
                : pressed
                  ? RECOMMENDED_ASK_COLORS.pressed
                  : hovered
                    ? RECOMMENDED_ASK_COLORS.hover
                    : RECOMMENDED_ASK_COLORS.default;

              return (
                <div key={`${suggestionGroupIndex}-${index}-${prompt}`} className="flex h-5 items-center">
                  <button
                    type="button"
                    disabled={disabled}
                    className="flex h-5 w-full min-w-0 items-center gap-2 bg-transparent text-left disabled:cursor-not-allowed disabled:opacity-50"
                    onMouseEnter={() => {
                      if (!disabled) setRowHover(index);
                    }}
                    onMouseLeave={() => {
                      setRowHover((h) => (h === index ? null : h));
                      setRowPress((p) => (p === index ? null : p));
                    }}
                    onMouseDown={() => {
                      if (!disabled) setRowPress(index);
                    }}
                    onMouseUp={() => {
                      setRowPress((p) => (p === index ? null : p));
                    }}
                    onClick={() => onSelect(prompt)}
                  >
                    <span
                      aria-hidden="true"
                      className="shrink-0 rounded-[2px]"
                      style={{
                        width: 4,
                        height: 4,
                        backgroundColor: tone,
                      }}
                    />
                    <TruncatableSuggestionText
                      text={prompt}
                      className="min-w-0 flex-1 truncate text-[13px] leading-5"
                      style={{ color: tone }}
                    />
                  </button>
                </div>
              );
            })}
          </div>
        </div>
      </div>
    </div>
  );
}

function chatStorageKey(): string {
  return `${CHAT_STORAGE_PREFIX}${getOrCreateSessionId()}`;
}

function shouldShowTimeDivider(current: Date, previous?: Date): boolean {
  if (!previous) {
    return true;
  }
  return current.getTime() - previous.getTime() >= TIME_DIVIDER_GAP_MS;
}

function formatConversationTime(timestamp: Date, now = new Date()): string {
  const sameYear = timestamp.getFullYear() === now.getFullYear();
  const todayStart = new Date(now.getFullYear(), now.getMonth(), now.getDate());
  const yesterdayStart = new Date(todayStart);
  yesterdayStart.setDate(todayStart.getDate() - 1);
  const weekStart = new Date(todayStart);
  weekStart.setDate(todayStart.getDate() - 6);

  const hhmm = timestamp.toLocaleTimeString('zh-CN', {
    hour: '2-digit',
    minute: '2-digit',
    hour12: false,
  });

  if (timestamp >= todayStart) {
    return hhmm;
  }

  if (timestamp >= yesterdayStart) {
    return `${t('chat.yesterday')} ${hhmm}`;
  }

  if (timestamp >= weekStart) {
    const weekday = [t('chat.weekday.sun'), t('chat.weekday.mon'), t('chat.weekday.tue'), t('chat.weekday.wed'), t('chat.weekday.thu'), t('chat.weekday.fri'), t('chat.weekday.sat')][timestamp.getDay()];
    return `${weekday} ${hhmm}`;
  }

  const month = timestamp.getMonth() + 1;
  const day = timestamp.getDate();
  const year = timestamp.getFullYear();
  const dateStr = sameYear
    ? t('chat.date.month_day').replace('{m}', String(month)).replace('{d}', String(day))
    : t('chat.date.year_month_day').replace('{y}', String(year)).replace('{m}', String(month)).replace('{d}', String(day));
  return `${dateStr} ${hhmm}`;
}

function capMessages(messages: ChatMessage[]): ChatMessage[] {
  if (messages.length <= MAX_VISIBLE_MESSAGES) {
    return messages;
  }
  return messages.slice(messages.length - MAX_VISIBLE_MESSAGES);
}

function getAttachmentKind(mimeType: string | undefined): ChatAttachmentKind {
  if (!mimeType) {
    return 'file';
  }
  if (mimeType.startsWith('image/')) {
    return 'image';
  }
  if (mimeType.startsWith('audio/')) {
    return 'audio';
  }
  if (mimeType.startsWith('video/')) {
    return 'video';
  }
  return 'file';
}

function readFileAsDataUrl(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => {
      if (typeof reader.result === 'string') {
        resolve(reader.result);
      } else {
        reject(new Error('Failed to read file'));
      }
    };
    reader.onerror = () => reject(reader.error ?? new Error('Failed to read file'));
    reader.readAsDataURL(file);
  });
}

function loadPersistedMessages(): ChatMessage[] {
  try {
    const raw = sessionStorage.getItem(chatStorageKey());
    if (!raw) {
      return [];
    }
    const parsed = JSON.parse(raw) as Array<
      Omit<ChatMessage, 'timestamp'> & { timestamp: string }
    >;
    return capMessages(
      parsed.map((message) => ({
        ...message,
        kind: message.kind ?? 'text',
        timestamp: new Date(message.timestamp),
      })),
    );
  } catch {
    return [];
  }
}

function persistMessages(messages: ChatMessage[]): void {
  try {
    sessionStorage.setItem(chatStorageKey(), JSON.stringify(capMessages(messages)));
  } catch {
    // Ignore storage failures and keep the in-memory chat usable.
  }
}

function persistableMessages(
  messages: ChatMessage[],
): Array<{
  id: string;
  role: 'user' | 'agent';
  content: string;
  thinking?: string;
  markdown?: boolean;
  attachments?: ChatAttachment[];
  timestamp: Date;
}> {
  return messages.filter(
    (
      message,
    ): message is {
      id: string;
      role: 'user' | 'agent';
      content: string;
      thinking?: string;
      markdown?: boolean;
      attachments?: ChatAttachment[];
      timestamp: Date;
    } => message.kind !== 'event' && typeof message.content === 'string',
  );
}

function extractToolName(msg: WsMessage): string {
  if (msg.name && msg.name.trim()) {
    return msg.name.trim();
  }

  const content = msg.content?.trim();
  if (!content) {
    return 'tool';
  }

  const quotedName = content.match(/`([^`]+)`/);
  if (quotedName?.[1]) {
    return quotedName[1];
  }

  const plainName = content.match(/^Running\s+([A-Za-z0-9_.-]+)/i);
  if (plainName?.[1]) {
    return plainName[1];
  }

  return 'tool';
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return value && typeof value === 'object' && !Array.isArray(value)
    ? (value as Record<string, unknown>)
    : undefined;
}

function asString(value: unknown): string | undefined {
  return typeof value === 'string' && value.trim() ? value : undefined;
}

function normalizeIncomingMessage(msg: WsMessage): WsMessage {
  const eventType = msg.event_type?.trim().toLowerCase();
  const payload = asRecord(msg.payload);

  if (eventType === 'tool_call_start') {
    return {
      ...msg,
      type: 'tool_call',
      name: msg.name ?? msg.tool ?? asString(payload?.tool) ?? asString(payload?.name),
      args: msg.args ?? payload?.arguments ?? payload?.args,
    };
  }

  if (eventType === 'tool_call_result') {
    return {
      ...msg,
      type: 'tool_result',
      name: msg.name ?? msg.tool ?? asString(payload?.tool) ?? asString(payload?.name),
      output: msg.output ?? asString(payload?.output) ?? asString(payload?.content),
    };
  }

  return msg;
}

function titleCaseToken(token: string): string {
  if (token.toLowerCase() === 'llm') {
    return 'LLM';
  }
  return token.charAt(0).toUpperCase() + token.slice(1).toLowerCase();
}

function beautifyEventTitle(raw: string): string {
  const normalized = raw.trim().toLowerCase();
  if (!normalized) {
    return 'Event';
  }

  if (normalized === 'tool_call' || normalized === 'tool_call_start' || normalized === 'tool_result' || normalized === 'tool_call_result') {
    return 'Tool';
  }

  if (normalized === 'llm_request' || normalized === 'llm_response') {
    return 'LLM';
  }

  const simplified = normalized
    .replace(/^(web_|ws_)/, '')
    .replace(/_(start|end|request|response|call|result|event)$/, '');

  return simplified
    .split(/[_\-.]+/)
    .filter(Boolean)
    .map(titleCaseToken)
    .join(' ');
}

function eventTitleForMessage(msg: WsMessage): string {
  const source = msg.event_type ?? msg.type ?? msg.name ?? extractToolName(msg);
  return beautifyEventTitle(source);
}

function stringifyEventPayload(payload: WsMessage): string {
  return JSON.stringify(payload, null, 2);
}

function eventSummaryForMessage(msg: WsMessage): string | undefined {
  switch (msg.type) {
    case 'tool_call':
    case 'tool_result':
      return msg.name ?? extractToolName(msg);
    case 'approval_request':
    case 'approval_restored':
      return msg.tool_name ?? 'tool';
    case 'approval_resolved':
      return msg.decision ? `decision: ${msg.decision}` : undefined;
    case 'approval_expired':
      return msg.request_id ?? msg.message_id ?? undefined;
    default:
      return undefined;
  }
}

function shouldRenderEventMessage(msg: WsMessage): boolean {
  switch (msg.type) {
    case 'approval_request':
    case 'approval_restored':
    case 'approval_resolved':
    case 'approval_expired':
      return true;
    default:
      return false;
  }
}

export default function AgentChat() {
  const sessionIdRef = useRef(getOrCreateSessionId());
  const { registerNewSessionHandler } = useAgentNewSession();
  const outletContext = useOutletContext<AgentPageOutletContext | undefined>();
  const { draft, saveDraft, clearDraft } = useDraft(DRAFT_KEY);
  const { resolvedTheme } = useTheme();
  const isLight = resolvedTheme === 'light';
  const [messages, setMessages] = useState<ChatMessage[]>(() => loadPersistedMessages());
  const messagesRef = useRef(messages);
  messagesRef.current = messages;
  const [sessionEpoch, setSessionEpoch] = useState(0);
  const [historyReady, setHistoryReady] = useState(false);
  const [input, setInput] = useState(draft);
  const [typing, setTyping] = useState(false);
  const [connected, setConnected] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [pendingApproval, setPendingApproval] = useState<PendingApproval | null>(null);
  const [pendingAttachments, setPendingAttachments] = useState<ChatAttachment[]>([]);
  const [currentModel, setCurrentModel] = useState<string | undefined>();
  const [currentProvider, setCurrentProvider] = useState<string | undefined>();
  const [expandedEventIds, setExpandedEventIds] = useState<Set<string>>(new Set());
  const [suggestionGroupIndex, setSuggestionGroupIndex] = useState(0);
  const suggestionKeys =
    LIGHT_SUGGESTION_GROUPS[suggestionGroupIndex % LIGHT_SUGGESTION_GROUPS.length] ?? LIGHT_SUGGESTION_GROUPS[0]!;
  const suggestionPrompts = suggestionKeys.map((key) => t(key));

  const wsRef = useRef<WebSocketClient | null>(null);
  const pendingWsReconnectRef = useRef(false);
  /** Defer syncing `/model` edits to WS while the assistant reply is streaming. */
  const pendingModelConfigDetailRef = useRef<ModelConfigUpdatedDetail | null>(null);
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLTextAreaElement>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const [copiedId, setCopiedId] = useState<string | null>(null);
  const pendingContentRef = useRef('');
  const pendingThinkingRef = useRef('');
  const pendingAttachmentsRef = useRef<ChatAttachment[]>([]);
  // Snapshot of thinking captured at chunk_reset, so it survives the reset.
  const capturedThinkingRef = useRef('');
  const [streamingContent, setStreamingContent] = useState('');
  const [streamingThinking, setStreamingThinking] = useState('');
  const [showIdentitySetup, setShowIdentitySetup] = useState(false);
  const [identitySetupDone, setIdentitySetupDone] = useState(() => {
    try {
      return localStorage.getItem('kylinbot_identity_setup_done') === 'true';
    } catch {
      return false;
    }
  });
  const [configuredModelCount, setConfiguredModelCount] = useState<number | null>(
    null,
  );

  const composerMenuRef = useRef<HTMLDivElement>(null);
  const [composerModelsOpen, setComposerModelsOpen] = useState(false);
  const [composerModelsList, setComposerModelsList] = useState<ModelInfo[]>([]);
  const [composerModelsLoading, setComposerModelsLoading] = useState(false);
  const [composerPressedItem, setComposerPressedItem] = useState<string | null>(null);
  const [composerHoveredItem, setComposerHoveredItem] = useState<string | null>(null);
  const [modelSwitchLoading, setModelSwitchLoading] = useState(false);

  const reconnectWebSocketAfterConfigChange = useCallback(() => {
    if (typing && wsRef.current?.connected) {
      try {
        wsRef.current.cancel();
      } catch {
        /* ignore */
      }
    }
    pendingContentRef.current = '';
    pendingThinkingRef.current = '';
    capturedThinkingRef.current = '';
    setStreamingContent('');
    setStreamingThinking('');
    setTyping(false);
    pendingWsReconnectRef.current = true;
    setSessionEpoch((e) => e + 1);
  }, [typing]);

  const loadComposerModelList = useCallback(async () => {
    setComposerModelsLoading(true);
    try {
      const list = await loadConfiguredModels();
      setComposerModelsList(
        list.map((m) => ({
          ...m,
          description: m.description ?? t('agent.model_selector_configured'),
        })),
      );
    } finally {
      setComposerModelsLoading(false);
    }
  }, []);

  const handleComposerModelPick = useCallback(
    async (model: ModelInfo) => {
      if (modelSwitchLoading || !connected) return;
      setComposerModelsOpen(false);
      setModelSwitchLoading(true);
      try {
        const normalizedProvider =
          model.provider === 'custom' && model.baseUrl?.trim()
            ? `custom:${model.baseUrl.trim()}`
            : model.provider;
        await setDefaultModel({ provider: normalizedProvider, model: model.value });
        setCurrentProvider(normalizedProvider);
        setCurrentModel(model.value);
        reconnectWebSocketAfterConfigChange();
      } catch {
        pendingWsReconnectRef.current = false;
        setModelSwitchLoading(false);
        setError(t('agent.send_error'));
      }
    },
    [
      connected,
      modelSwitchLoading,
      reconnectWebSocketAfterConfigChange,
    ],
  );

  const toggleComposerModelsMenu = useCallback(async () => {
    if (composerModelsOpen) {
      setComposerModelsOpen(false);
      return;
    }
    setComposerModelsOpen(true);
    await loadComposerModelList();
  }, [composerModelsOpen, loadComposerModelList]);

  const composerItemSelected = useCallback(
    (model: ModelInfo) => {
      if (!currentModel) return false;
      if (
        currentProvider
        && currentModel === model.value
        && currentProvider === model.provider
      )
        return true;
      const sameVal = composerModelsList.filter((item) => item.value === currentModel);
      return sameVal.length === 1 && sameVal[0]?.provider === model.provider;
    },
    [composerModelsList, currentModel, currentProvider],
  );

  const composerCurrentLabel = useMemo(() => {
    const found =
      composerModelsList.find(
        (m) =>
          m.value === currentModel
          && (currentProvider ? m.provider === currentProvider : true),
      )?.label ?? null;
    if (found) return found;
    if (composerModelsList[0]?.label) return composerModelsList[0].label;
    if (currentModel) return currentModel;
    return t('agent.model_selector_title');
  }, [composerModelsList, currentModel, currentProvider]);

  const navigate = useNavigate();
  const setAgentGenerating = outletContext?.setAgentGenerating;

  useEffect(() => {
    let cancelled = false;
    const refresh = () => {
      void loadConfiguredModels()
        .then((m) => {
          if (cancelled) return;
          setConfiguredModelCount(m.length);
        })
        .catch(() => {
          if (cancelled) return;
          setConfiguredModelCount(null);
        });
    };
    refresh();
    const handler = () => refresh();
    window.addEventListener(MODEL_CONFIG_UPDATED_EVENT, handler);
    return () => {
      cancelled = true;
      window.removeEventListener(MODEL_CONFIG_UPDATED_EVENT, handler);
    };
  }, []);

  // Persist draft to in-memory store so it survives route changes
  useEffect(() => {
    saveDraft(input);
  }, [input, saveDraft]);

  useEffect(() => {
    setAgentGenerating?.(typing);
  }, [typing, setAgentGenerating]);

  useEffect(() => {
    return () => {
      setAgentGenerating?.(false);
    };
  }, [setAgentGenerating]);

  useEffect(() => {
    const persisted = readPersistedModelSelection();
    if (persisted.model && persisted.provider) {
      setCurrentModel(persisted.model);
      setCurrentProvider(persisted.provider);
    }

    getStatus()
      .then((status) => {
        if (!persisted.model && status.model) {
          setCurrentModel(status.model);
        }
        if (!persisted.provider && status.provider) {
          setCurrentProvider(status.provider);
        }
      })
      .catch(() => {
        // Ignore status fetch errors and keep selector usable.
      });
  }, []);

  useEffect(() => {
    writePersistedModelSelection({ model: currentModel, provider: currentProvider });
  }, [currentModel, currentProvider]);

  const applyModelConfigUpdateFromDetail = useCallback(
    (detail: ModelConfigUpdatedDetail) => {
      const selected = { provider: currentProvider, model: currentModel };

      // Delete clears or repoints persisted default; remap local selection only
      // when the user was aligned with the removed row (`current` carries the
      // promoted default when applicable).
      if (detail.action === 'delete' && detail.original) {
        const hadDeletedSelection = sameProviderModel(selected, detail.original);
        if (!hadDeletedSelection) return;

        if (detail.current) {
          setCurrentProvider(detail.current.provider);
          setCurrentModel(detail.current.model);
        } else {
          setCurrentProvider(undefined);
          setCurrentModel(undefined);
        }
        if (connected) {
          reconnectWebSocketAfterConfigChange();
        }
        return;
      }

      const selectedMatchesOriginal = detail.original
        ? sameProviderModel(selected, {
            provider: detail.original.provider,
            model: detail.original.model,
          })
        : false;
      const selectedMatchesCurrent = detail.current
        ? sameProviderModel(selected, {
            provider: detail.current.provider,
            model: detail.current.model,
          })
        : false;
      if (!selectedMatchesOriginal && !selectedMatchesCurrent) {
        return;
      }

      if (detail.action === 'update' && detail.current) {
        if (
          !sameProviderModel(selected, {
            provider: detail.current.provider,
            model: detail.current.model,
          })
        ) {
          setCurrentProvider(detail.current.provider);
          setCurrentModel(detail.current.model);
        }
      }

      if (connected) {
        reconnectWebSocketAfterConfigChange();
      }
    },
    [connected, currentModel, currentProvider, reconnectWebSocketAfterConfigChange],
  );

  useEffect(() => {
    const handler = (event: Event) => {
      const customEvent = event as CustomEvent<ModelConfigUpdatedDetail>;
      const detail = customEvent.detail;
      if (!detail) {
        return;
      }
      if (typing) {
        pendingModelConfigDetailRef.current = detail;
        return;
      }
      applyModelConfigUpdateFromDetail(detail);
    };

    window.addEventListener(MODEL_CONFIG_UPDATED_EVENT, handler as EventListener);
    return () =>
      window.removeEventListener(MODEL_CONFIG_UPDATED_EVENT, handler as EventListener);
  }, [typing, applyModelConfigUpdateFromDetail]);

  useEffect(() => {
    if (typing) return;
    const pending = pendingModelConfigDetailRef.current;
    if (!pending) return;
    pendingModelConfigDetailRef.current = null;
    applyModelConfigUpdateFromDetail(pending);
  }, [typing, applyModelConfigUpdateFromDetail]);

  useEffect(() => {
    if (!composerModelsOpen) return;
    const handler = (e: MouseEvent) => {
      if (composerMenuRef.current && !composerMenuRef.current.contains(e.target as Node)) {
        setComposerModelsOpen(false);
      }
    };
    document.addEventListener('mousedown', handler);
    return () => document.removeEventListener('mousedown', handler);
  }, [composerModelsOpen]);

  useEffect(() => {
    const handler = () => {
      const oldSid = sessionIdRef.current;
      saveChatHistory(oldSid, uiMessagesToPersisted(persistableMessages(messagesRef.current)));
      try {
        sessionStorage.removeItem(`${CHAT_STORAGE_PREFIX}${oldSid}`);
      } catch {
        // ignore
      }
      sessionIdRef.current = replaceSessionId();
      setMessages([]);
      setInput('');
      clearDraft();
      setError(null);
      setPendingApproval(null);
      setTyping(false);
      pendingContentRef.current = '';
      pendingThinkingRef.current = '';
      capturedThinkingRef.current = '';
      setStreamingContent('');
      setStreamingThinking('');
      setExpandedEventIds(new Set());
      setCopiedId(null);
      setHistoryReady(false);
      setSessionEpoch((e) => e + 1);
    };
    registerNewSessionHandler(handler);
    return () => registerNewSessionHandler(null);
  }, [registerNewSessionHandler, clearDraft]);

  // Hydrate chat from server (preferred) or localStorage fallback
  useEffect(() => {
    const sid = sessionIdRef.current;
    let cancelled = false;

    (async () => {
      try {
        const res = await getSessionMessages(sid);
        if (cancelled) return;
        if (res.session_persistence && res.messages.length > 0) {
          setMessages((prev) =>
            prev.length > 0 ? prev : persistedToUiMessages(mapServerMessagesToPersisted(res.messages)),
          );
        } else if (!res.session_persistence) {
          setMessages((prev) => {
            if (prev.length > 0) return prev;
            const ls = loadChatHistory(sid);
            return ls.length ? persistedToUiMessages(ls) : prev;
          });
        }
      } catch {
        if (!cancelled) {
          setMessages((prev) => {
            if (prev.length > 0) return prev;
            const ls = loadChatHistory(sid);
            return ls.length ? persistedToUiMessages(ls) : prev;
          });
        }
      } finally {
        if (!cancelled) setHistoryReady(true);
      }
    })();

    return () => {
      cancelled = true;
    };
  }, [sessionEpoch]);

  // Mirror transcript to localStorage (bounded); server remains source of truth when persistence is on
  useEffect(() => {
    if (!historyReady) return;
    saveChatHistory(sessionIdRef.current, uiMessagesToPersisted(persistableMessages(messages)));
  }, [messages, historyReady]);

  useEffect(() => {
    const ws = new WebSocketClient();

    const appendEventMessage = (msg: WsMessage) => {
      if (!shouldRenderEventMessage(msg)) {
        return;
      }
      setMessages((prev) =>
        capMessages([
          ...prev,
          {
            id: generateUUID(),
            role: 'agent',
            kind: 'event',
            eventTitle: eventTitleForMessage(msg),
            eventSummary: eventSummaryForMessage(msg),
            eventPayload: msg,
            timestamp: new Date(),
          },
        ]),
      );
    };

    ws.onOpen = () => {
      setConnected(true);
      setError(null);
      if (pendingWsReconnectRef.current) {
        pendingWsReconnectRef.current = false;
        setModelSwitchLoading(false);
      }
    };

    ws.onClose = (ev: CloseEvent) => {
      setConnected(false);
      if (ev.code !== 1000 && ev.code !== 1001) {
        setError(`Connection closed unexpectedly (code: ${ev.code}). Please check your configuration.`);
      }
    };

    ws.onError = () => {
      setError(t('agent.connection_error'));
    };

    ws.onMessage = (msg: WsMessage) => {
      const normalizedMsg = normalizeIncomingMessage(msg);
      switch (normalizedMsg.type) {
        case 'connected':
          break;

        case 'thinking':
          setTyping(true);
          pendingThinkingRef.current += normalizedMsg.content ?? '';
          setStreamingThinking(pendingThinkingRef.current);
          break;

        case 'chunk':
          setTyping(true);
          pendingContentRef.current += normalizedMsg.content ?? '';
          setStreamingContent(pendingContentRef.current);
          break;

        case 'chunk_reset':
          // Server signals that the authoritative done message follows.
          // Snapshot thinking before clearing display state.
          capturedThinkingRef.current = pendingThinkingRef.current;
          pendingContentRef.current = '';
          pendingThinkingRef.current = '';
          setStreamingContent('');
          setStreamingThinking('');
          break;

        case 'message':
        case 'done': {
          const content = normalizedMsg.full_response ?? normalizedMsg.content ?? pendingContentRef.current;
          const thinking = capturedThinkingRef.current || pendingThinkingRef.current || undefined;
          if (content) {
            setMessages((prev) =>
              capMessages([
                ...prev,
                {
                  id: generateUUID(),
                  role: 'agent',
                  content,
                  thinking,
                  markdown: true,
                  timestamp: new Date(),
                },
              ]),
            );
          }
          pendingContentRef.current = '';
          pendingThinkingRef.current = '';
          capturedThinkingRef.current = '';
          setStreamingContent('');
          setStreamingThinking('');
          setTyping(false);
          dispatchCostSummaryStale();
          break;
        }

        case 'tool_call': {
          const toolName = normalizedMsg.name ?? 'unknown';
          const toolArgs = normalizedMsg.args;
          setMessages((prev) => {
            // Dedup: backend streaming may re-send tool_call events before execution.
            // Skip if an unresolved card with the same name+args already exists.
            const argsKey = JSON.stringify(toolArgs ?? {});
            const isDuplicate = prev.some(
              (m) => m.toolCall
                && m.toolCall.output === undefined
                && m.toolCall.name === toolName
                && JSON.stringify(m.toolCall.args ?? {}) === argsKey,
            );
            if (isDuplicate) return prev;

            return [
              ...prev,
              {
                id: generateUUID(),
                role: 'agent' as const,
                content: `${t('agent.tool_call_prefix')} ${toolName}(${argsKey})`,
                toolCall: { name: toolName, args: toolArgs },
                timestamp: new Date(),
              },
            ];
          });
          break;
        }

        case 'approval_request':
        case 'approval_restored':
          appendEventMessage(normalizedMsg);
          setPendingApproval({
            requestId: normalizedMsg.request_id ?? normalizedMsg.message_id ?? generateUUID(),
            toolName: normalizedMsg.tool_name ?? 'tool',
            argumentsSummary: normalizedMsg.arguments_summary ?? '',
            expiresAt: typeof normalizedMsg.expires_at === 'number' ? normalizedMsg.expires_at : null,
            choices: normalizedMsg.choices ?? ['yes', 'no', 'always'],
            submitting: false,
          });
          setTyping(false);
          break;

        case 'approval_resolved':
          appendEventMessage(normalizedMsg);
          setPendingApproval(null);
          break;

        case 'approval_expired':
          appendEventMessage(normalizedMsg);
          setPendingApproval(null);
          break;

        case 'message_cancelled':
          setTyping(false);
          pendingContentRef.current = '';
          pendingThinkingRef.current = '';
          setStreamingContent('');
          setStreamingThinking('');
          break;

        case 'tool_result': {
          setMessages((prev) => {
            // Forward scan: find the FIRST unresolved toolCall (order-guaranteed by backend)
            const idx = prev.findIndex((m) => m.toolCall && m.toolCall.output === undefined);
            if (idx !== -1) {
              const updated = [...prev];
              const existing = prev[idx]!;
              updated[idx] = {
                ...existing,
                toolCall: { ...existing.toolCall!, output: normalizedMsg.output ?? '' },
              };
              return updated;
            }
            // Fallback: no unresolved call found — append standalone card
            return [
              ...prev,
              {
                id: generateUUID(),
                role: 'agent' as const,
                content: `${t('agent.tool_result_prefix')} ${normalizedMsg.output ?? ''}`,
                toolCall: { name: normalizedMsg.name ?? 'unknown', output: normalizedMsg.output ?? '' },
                timestamp: new Date(),
              },
            ];
          });
          break;
        }

        case 'error':
          setMessages((prev) =>
            capMessages([
              ...prev,
              {
                id: generateUUID(),
                role: 'agent',
                content: `${t('agent.error_prefix')} ${normalizedMsg.message ?? t('agent.unknown_error')}`,
                timestamp: new Date(),
              },
            ]),
          );
          if (normalizedMsg.code === 'AGENT_INIT_FAILED' || normalizedMsg.code === 'AUTH_ERROR' || normalizedMsg.code === 'PROVIDER_ERROR') {
            setError(`Configuration error: ${normalizedMsg.message}. Please check your provider settings (API key, model, etc.).`);
          } else if (normalizedMsg.code === 'INVALID_JSON' || normalizedMsg.code === 'UNKNOWN_MESSAGE_TYPE' || normalizedMsg.code === 'EMPTY_CONTENT') {
            setError(`Message error: ${normalizedMsg.message}`);
          }
          setTyping(false);
          pendingContentRef.current = '';
          pendingThinkingRef.current = '';
          setStreamingContent('');
          setStreamingThinking('');
          break;
      }
    };

    ws.connect();
    wsRef.current = ws;

    return () => {
      ws.disconnect();
    };
  }, [sessionEpoch]);

  useEffect(() => {
    persistMessages(messages);
  }, [messages]);

  useEffect(() => {
    pendingAttachmentsRef.current = pendingAttachments;
  }, [pendingAttachments]);

  useEffect(() => {
    return () => {
      pendingAttachmentsRef.current.forEach((attachment) => {
        if (attachment.previewUrl) {
          URL.revokeObjectURL(attachment.previewUrl);
        }
      });
    };
  }, []);

  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages, typing, streamingContent]);

  const sendUserMessage = (trimmed: string, sourceAttachments: ChatAttachment[]): boolean => {
    if ((!trimmed && sourceAttachments.length === 0) || !wsRef.current?.connected || pendingApproval) {
      return false;
    }

    const outboundAttachments: WsOutboundAttachment[] = sourceAttachments.map((attachment) => ({
      file_name: attachment.name,
      data_base64: attachment.dataBase64,
      mime_type: attachment.mimeType,
    }));

    const bubbleAttachments = sourceAttachments.map((attachment) => ({
      ...attachment,
      // Bubble attachments should not rely on ObjectURL because it gets revoked after send.
      previewUrl: undefined,
    }));

    setMessages((prev) =>
      capMessages([
        ...prev,
        {
          id: generateUUID(),
          role: 'user',
          content: trimmed,
          attachments: bubbleAttachments,
          timestamp: new Date(),
        },
      ]),
    );

    try {
      wsRef.current.sendMessage(trimmed, outboundAttachments);
      setTyping(true);
      pendingContentRef.current = '';
      pendingThinkingRef.current = '';
      sourceAttachments.forEach((attachment) => {
        if (attachment.previewUrl) {
          URL.revokeObjectURL(attachment.previewUrl);
        }
      });
      if (sourceAttachments.length > 0) {
        setPendingAttachments([]);
      }
      setError(null);
    } catch {
      setError(t('agent.send_error'));
      return false;
    }

    return true;
  };

  const handleSend = () => {
    const trimmed = input.trim();
    const isIdentityQuestion = /^(你是谁|who.*are.*you|你是干什么的|你是什么|what.*are.*you)/i.test(trimmed);
    if (isIdentityQuestion && messages.length === 0 && !identitySetupDone) {
      setShowIdentitySetup(true);
      return;
    }
    // Gate the send on at least one configured model. We use the cached count
    // populated by the MODEL_CONFIG_UPDATED_EVENT listener so the check runs
    // synchronously. `null` means the lookup is still pending — fall through
    // and let the existing flow handle errors.
    if (configuredModelCount === 0 && (trimmed || pendingAttachments.length > 0)) {
      // Echo the user message so the conversation stays consistent, then
      // surface the gating error as a system-style bubble.
      setMessages((prev) =>
        capMessages([
          ...prev,
          {
            id: generateUUID(),
            role: 'user',
            content: trimmed,
            attachments: pendingAttachments.map((a) => ({ ...a, previewUrl: undefined })),
            timestamp: new Date(),
          },
          {
            id: generateUUID(),
            role: 'agent',
            kind: 'no-model-error',
            timestamp: new Date(),
          },
        ]),
      );
      pendingAttachments.forEach((a) => {
        if (a.previewUrl) URL.revokeObjectURL(a.previewUrl);
      });
      setPendingAttachments([]);
      setInput('');
      clearDraft();
      if (inputRef.current) {
        inputRef.current.style.height = 'auto';
        inputRef.current.focus();
      }
      return;
    }
    if (!sendUserMessage(trimmed, pendingAttachments)) return;

    setInput('');
    clearDraft();
    if (inputRef.current) {
      inputRef.current.style.height = 'auto';
      inputRef.current.focus();
    }
  };

  const handleStop = () => {
    if (!wsRef.current?.connected) return;
    try {
      wsRef.current.cancel();
    } catch {
      setError(t('agent.send_error'));
    }
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key !== 'Enter' || e.nativeEvent.isComposing) return;
    if (e.shiftKey || e.ctrlKey || e.metaKey) return;
    e.preventDefault();
    handleSend();
  };

  const handleTextareaChange = (e: React.ChangeEvent<HTMLTextAreaElement>) => {
    setInput(e.target.value);
    e.target.style.height = 'auto';
    e.target.style.height = `${Math.min(e.target.scrollHeight, 200)}px`;
  };

  const handlePickAttachments = () => {
    if (!connected || pendingApproval) return;
    fileInputRef.current?.click();
  };

  const handleRemovePendingAttachment = (attachmentId: string) => {
    setPendingAttachments((prev) => {
      const target = prev.find((item) => item.id === attachmentId);
      if (target?.previewUrl) {
        URL.revokeObjectURL(target.previewUrl);
      }
      return prev.filter((item) => item.id !== attachmentId);
    });
  };

  const handleFileSelect = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = Array.from(e.target.files ?? []);
    e.target.value = '';
    if (files.length === 0) return;

    const currentSize = pendingAttachments.reduce((sum, item) => {
      const bytes = Math.floor((item.dataBase64.length * 3) / 4);
      return sum + bytes;
    }, 0);
    const selectedTotal = files.reduce((sum, file) => sum + file.size, 0);

    if (currentSize + selectedTotal > MAX_TOTAL_ATTACHMENT_BYTES) {
      setError(t('chat.attachment_too_large'));
      return;
    }

    for (const file of files) {
      if (file.size > MAX_SINGLE_ATTACHMENT_BYTES) {
        setError(t('chat.file_too_large').replace('{name}', file.name));
        return;
      }
    }

    try {
      const nextAttachments = await Promise.all(
        files.map(async (file): Promise<ChatAttachment> => {
          const dataUrl = await readFileAsDataUrl(file);
          const base64Part = dataUrl.split(',')[1] ?? '';
          const mimeType = file.type || undefined;
          const kind = getAttachmentKind(mimeType);
          return {
            id: generateUUID(),
            name: file.name,
            mimeType,
            kind,
            dataBase64: base64Part,
            previewUrl: kind === 'image' ? URL.createObjectURL(file) : undefined,
          };
        }),
      );
      setPendingAttachments((prev) => [...prev, ...nextAttachments]);
      setError(null);
    } catch {
      setError(t('chat.read_attachment_failed'));
    }
  };

  const sendSuggestedPrompt = (prompt: string) => {
    const trimmed = prompt.trim();
    if (!sendUserMessage(trimmed, [])) return;

    setInput('');
    clearDraft();
    if (inputRef.current) {
      inputRef.current.style.height = 'auto';
      inputRef.current.focus();
    }
  };

  const rotateSuggestionGroup = () => {
    setSuggestionGroupIndex((i) => (i + 1) % LIGHT_SUGGESTION_GROUPS.length);
  };

  const handleIdentitySetupConfirm = (name: string, role: string) => {
    if (!wsRef.current?.connected) return;
    try {
      wsRef.current.sendJson({
        type: 'set_identity',
        name: name || undefined,
        role: role || undefined,
      });
      setShowIdentitySetup(false);
      setIdentitySetupDone(true);
      try {
        localStorage.setItem('kylinbot_identity_setup_done', 'true');
      } catch { }
    } catch {
      setError(t('agent.send_error'));
    }
  };

  const handleApproval = (decision: 'yes' | 'no' | 'always') => {
    if (!pendingApproval || !wsRef.current?.connected) return;
    setPendingApproval((current) =>
      current ? { ...current, submitting: true } : current,
    );
    try {
      wsRef.current.sendApprovalResponse(pendingApproval.requestId, decision);
    } catch {
      setPendingApproval((current) =>
        current ? { ...current, submitting: false } : current,
      );
      setError(t('agent.send_error'));
    }
  };

  const handleCopy = useCallback((msgId: string, content: string) => {
    const onSuccess = () => {
      setCopiedId(msgId);
      setTimeout(() => setCopiedId((prev) => (prev === msgId ? null : prev)), 2000);
    };

    if (navigator.clipboard?.writeText) {
      navigator.clipboard.writeText(content).then(onSuccess).catch(() => {
        // Fallback for insecure contexts (HTTP)
        fallbackCopy(content) && onSuccess();
      });
    } else {
      fallbackCopy(content) && onSuccess();
    }
  }, []);

  const toggleEventExpanded = useCallback((msgId: string) => {
    setExpandedEventIds((prev) => {
      const next = new Set(prev);
      if (next.has(msgId)) {
        next.delete(msgId);
      } else {
        next.add(msgId);
      }
      return next;
    });
  }, []);

  /**
   * Fallback copy using a temporary textarea for HTTP contexts
   * where navigator.clipboard is unavailable.
   */
  function fallbackCopy(text: string): boolean {
    const textarea = document.createElement('textarea');
    textarea.value = text;
    textarea.style.position = 'fixed';
    textarea.style.opacity = '0';
    document.body.appendChild(textarea);
    textarea.select();
    try {
      document.execCommand('copy');
      return true;
    } catch {
      return false;
    } finally {
      document.body.removeChild(textarea);
    }
  }

  return (
    <div className="flex flex-col h-[calc(100vh-3.5rem)] overflow-hidden">
      {/* Connection status bar */}
      {error && (
        <div className="px-4 py-2 border-b flex items-center gap-2 text-sm animate-fade-in" style={{ background: 'rgba(239, 68, 68, 0.08)', borderColor: 'rgba(239, 68, 68, 0.2)', color: '#f87171', }}>
          <AlertCircle className="h-4 w-4 shrink-0" />
          {error}
        </div>
      )}

      {/* Messages area */}
      <div className={`flex-1 overflow-y-auto space-y-4 ${isLight ? 'px-8 pb-8 pt-10 md:px-12' : 'p-4'}`}>
        {messages.length === 0 && (
          isLight ? (
            <div
              data-testid="agent-light-welcome"
              className="mx-auto flex w-full max-w-5xl flex-col items-center text-center animate-fade-in"
              style={{ color: 'var(--pc-text-muted)' }}
            >
              <div className="mt-2 flex h-12 w-12 shrink-0 items-center justify-center overflow-hidden rounded-full leading-none">
                <img
                  src={`${assetBasePath}/kylin-ai-assistant.png`}
                  alt="KylinBot"
                  className="block h-full w-full object-contain"
                  onError={(e) => {
                    e.currentTarget.style.display = 'none';
                  }}
                />
              </div>
              <p className="mt-3 text-[20px] font-semibold tracking-[-0.02em]" style={{ color: 'var(--pc-text-primary)' }}>
                {t('agent.light_greeting_title')}
              </p>
              <p className="mt-2 max-w-3xl text-[13px] leading-[1.95]" style={{ color: 'var(--pc-text-muted)' }}>
                {t('agent.light_greeting_subtitle')}
              </p>

              <div className="mt-6 flex w-full items-center justify-between px-1 text-[13px]" style={{ color: 'var(--pc-text-secondary)' }}>
                <span>{t('agent.light_try_label')}</span>
                <button
                  type="button"
                  className="text-[12px] font-medium"
                  style={{ color: '#3790FA' }}
                  onClick={rotateSuggestionGroup}
                >
                  {t('agent.light_refresh_label')}
                </button>
              </div>

              <div data-testid="agent-light-suggestions" className="mt-3 w-full">
                <LightRecommendedQuestionsCard
                  prompts={suggestionPrompts}
                  suggestionGroupIndex={suggestionGroupIndex}
                  disabled={!connected || Boolean(pendingApproval)}
                  onSelect={sendSuggestedPrompt}
                />
              </div>
            </div>
          ) : (
            <div className="flex flex-col items-center justify-center h-full text-center animate-fade-in" style={{ color: 'var(--pc-text-muted)' }}>
              <div className="h-16 w-16 rounded-3xl flex items-center justify-center mb-4 animate-float" style={{ background: 'var(--pc-accent-glow)' }}>
                <Bot className="h-8 w-8" style={{ color: 'var(--pc-accent)' }} />
              </div>
              <p className="text-lg font-semibold mb-1" style={{ color: 'var(--pc-text-primary)' }}>KylinBot Agent</p>
              <p className="text-sm" style={{ color: 'var(--pc-text-muted)' }}>{t('agent.start_conversation')}</p>
            </div>
          )
        )}

        {messages.map((msg, idx) => {
          const previousTimestamp = idx > 0 ? messages[idx - 1]?.timestamp : undefined;

          return (
            <div key={msg.id}>
              {shouldShowTimeDivider(msg.timestamp, previousTimestamp) && (
                <div
                  className="flex justify-center py-2"
                  style={{ color: isLight ? '#b0b4bc' : 'var(--pc-text-faint)' }}
                >
                  <span className={`${isLight ? 'text-[12px]' : 'text-[11px]'}`}>
                    {formatConversationTime(msg.timestamp)}
                  </span>
                </div>
              )}

              <div
                className={`group flex items-start gap-3 ${msg.role === 'user' ? 'flex-row-reverse animate-slide-in-right' : 'animate-slide-in-left'
                  }`}
                style={{ animationDelay: `${Math.min(idx * 30, 200)}ms` }}
              >
                <div
                  className={`flex-shrink-0 flex items-center justify-center border ${isLight ? 'w-8 h-8 rounded-[12px]' : 'w-9 h-9 rounded-2xl'}`}
                  style={{
                    background: msg.role === 'user'
                      ? (isLight ? '#ffffff' : 'var(--pc-accent)')
                      : 'var(--pc-bg-elevated)',
                    borderColor: msg.role === 'user'
                      ? (isLight ? '#d9dde5' : 'var(--pc-accent)')
                      : 'var(--pc-border)',
                  }}
                >
                  {msg.role === 'user' ? (
                    <User className="h-4 w-4" style={{ color: isLight ? '#9097a3' : '#ffffff' }} />
                  ) : (
                    <Bot className="h-4 w-4" style={{ color: 'var(--pc-accent)' }} />
                  )}
                </div>
                <div className={`relative ${isLight ? 'max-w-[78%]' : 'max-w-[75%]'}`}>
                  <div
                    className={`${isLight ? 'rounded-[8px] px-[14px] py-[11px]' : 'rounded-2xl px-4 py-3'} border`}
                    style={
                      msg.role === 'user'
                        ? {
                          background: isLight ? '#cfeeff' : 'var(--pc-accent-glow)',
                          borderColor: isLight ? '#c7e7fb' : 'var(--pc-accent-dim)',
                          color: isLight ? '#1f2d3d' : 'var(--pc-text-primary)',
                        }
                        : {
                          background: isLight ? '#ffffff' : 'var(--pc-bg-elevated)',
                          borderColor: isLight ? '#ececf1' : 'var(--pc-border)',
                          color: 'var(--pc-text-primary)',
                          boxShadow: isLight ? '0 1px 2px rgba(39, 47, 61, 0.05)' : 'none',
                        }
                    }
                  >
                    {msg.role === 'agent' && msg.kind !== 'event' && msg.thinking && (
                      <details className="mb-2">
                        <summary className="text-xs select-none" style={{ color: 'var(--pc-text-muted)' }}>Thinking</summary>
                        <pre className="text-xs mt-1 whitespace-pre-wrap break-words leading-relaxed overflow-auto max-h-60 p-2 rounded-lg" style={{ color: 'var(--pc-text-muted)', background: 'var(--pc-bg-surface)' }}>{msg.thinking}</pre>
                      </details>
                    )}
                    {msg.toolCall ? (
                      <ToolCallCard toolCall={msg.toolCall} />
                    ) : msg.role === 'agent' && msg.kind === 'no-model-error' ? (
                      <div className="text-sm space-y-2">
                        <p
                          className="text-sm leading-relaxed"
                          style={{ color: 'var(--color-status-error)' }}
                        >
                          {t('model.no_models_chat_prompt')}
                        </p>
                        <div className="flex justify-end">
                          <button
                            type="button"
                            onClick={() => navigate('/model')}
                            className="text-xs"
                            style={{
                              color: '#3B82F6',
                              background: 'transparent',
                              border: 'none',
                              cursor: 'default',
                              padding: 0,
                              textDecoration: 'underline',
                            }}
                          >
                            {t('model.no_models_chat_action')}
                          </button>
                        </div>
                        <div
                          className="text-[10px] text-right"
                          style={{ color: 'var(--pc-text-faint)' }}
                        >
                          {t('agent.generated_by')}
                        </div>
                      </div>
                    ) : msg.role === 'agent' && msg.kind === 'event' && msg.eventPayload ? (
                      <div className="text-sm">
                        <div className="flex flex-wrap items-center gap-2">
                          <button
                            type="button"
                            onClick={() => toggleEventExpanded(msg.id)}
                            aria-label={expandedEventIds.has(msg.id) ? t('agent.hide_json') : t('agent.show_json')}
                            className="inline-flex h-6 w-6 flex-shrink-0 items-center justify-center rounded-lg border transition-colors"
                            style={{ borderColor: 'var(--pc-border)', background: 'var(--pc-bg)', color: 'var(--pc-text-muted)' }}
                            onMouseEnter={(e) => { e.currentTarget.style.borderColor = 'var(--pc-accent-dim)'; e.currentTarget.style.color = 'var(--pc-text-primary)'; }}
                            onMouseLeave={(e) => { e.currentTarget.style.borderColor = 'var(--pc-border)'; e.currentTarget.style.color = 'var(--pc-text-muted)'; }}
                          >
                            {expandedEventIds.has(msg.id) ? (
                              <ChevronDown className="h-3.5 w-3.5" />
                            ) : (
                              <ChevronRight className="h-3.5 w-3.5" />
                            )}
                          </button>
                          <p className="break-words text-[13px] font-semibold" style={{ color: 'var(--pc-text-primary)' }}>
                            {msg.eventTitle ?? 'Event'}
                          </p>
                          {msg.eventSummary && (
                            <p
                              className="break-words rounded-md border px-2 py-0.5 text-[11px]"
                              style={{
                                color: 'var(--pc-text-muted)',
                                borderColor: 'var(--pc-border)',
                                background: 'var(--pc-bg)',
                              }}
                            >
                              {msg.eventSummary}
                            </p>
                          )}
                        </div>
                        {expandedEventIds.has(msg.id) && (
                          <pre
                            className="mt-3 overflow-x-auto rounded-xl border p-3 text-[12px] leading-6"
                            style={{ borderColor: 'var(--pc-border)', background: 'var(--pc-bg)', color: 'var(--pc-text-secondary)' }}
                          >
                            {stringifyEventPayload(msg.eventPayload)}
                          </pre>
                        )}
                      </div>
                    ) : msg.role === 'agent' && msg.markdown ? (
                      <div
                        data-testid="chat-markdown"
                        className={`${isLight ? 'text-[14px] leading-[1.45]' : 'text-sm leading-relaxed'} break-words chat-markdown`}
                      >
                        <ReactMarkdown remarkPlugins={[remarkGfm]}>{msg.content ?? ''}</ReactMarkdown>
                      </div>
                    ) : msg.role === 'agent' ? (
                      <p className={`${isLight ? 'text-[14px] leading-[1.45]' : 'text-sm leading-relaxed'} whitespace-pre-wrap break-words`}>{msg.content ?? ''}</p>
                    ) : (
                      <div className="space-y-2">
                        {msg.content && (
                          <p className={`${isLight ? 'text-[14px] leading-[1.45]' : 'text-sm leading-relaxed'} whitespace-pre-wrap break-words`}>{msg.content}</p>
                        )}
                        {msg.attachments && msg.attachments.length > 0 && (
                          <div className="flex flex-wrap gap-2">
                            {msg.attachments.map((attachment) => (
                              attachment.kind === 'image' ? (
                                <img
                                  key={attachment.id}
                                  src={attachment.previewUrl ?? `data:${attachment.mimeType ?? 'image/*'};base64,${attachment.dataBase64}`}
                                  alt={attachment.name}
                                  className="h-[88px] w-[88px] rounded-[10px] border object-cover"
                                  style={{ borderColor: 'rgba(80, 104, 127, 0.2)' }}
                                />
                              ) : (
                                <div
                                  key={attachment.id}
                                  className="max-w-[240px] rounded-[10px] border px-2 py-1 text-[12px] truncate"
                                  style={{ borderColor: 'rgba(80, 104, 127, 0.2)', background: 'rgba(255, 255, 255, 0.45)' }}
                                  title={attachment.name}
                                >
                                  {attachment.name}
                                </div>
                              )
                            ))}
                          </div>
                        )}
                      </div>
                    )}
                  </div>
                  <button
                    onClick={() =>
                      handleCopy(
                        msg.id,
                        msg.kind === 'event' && msg.eventPayload
                          ? stringifyEventPayload(msg.eventPayload)
                          : (msg.content ?? ''),
                      )
                    }
                    aria-label={t('agent.copy_message')}
                    className="absolute top-1 right-1 opacity-0 group-hover:opacity-100 transition-all p-1.5 rounded-xl"
                    style={{ background: 'var(--pc-bg-elevated)', border: '1px solid var(--pc-border)', color: 'var(--pc-text-muted)', }}
                    onMouseEnter={(e) => { e.currentTarget.style.color = 'var(--pc-text-primary)'; e.currentTarget.style.borderColor = 'var(--pc-accent-dim)'; }}
                    onMouseLeave={(e) => { e.currentTarget.style.color = 'var(--pc-text-muted)'; e.currentTarget.style.borderColor = 'var(--pc-border)'; }}
                  >
                    {copiedId === msg.id ? (
                      <Check className="h-3 w-3" style={{ color: '#34d399' }} />
                    ) : (
                      <Copy className="h-3 w-3" />
                    )}
                  </button>
                </div>
              </div>
            </div>
          );
        })}

        {typing && (
          <div className="flex items-start gap-3 animate-fade-in">
            <div className="flex-shrink-0 w-9 h-9 rounded-2xl flex items-center justify-center border" style={{ background: 'var(--pc-bg-elevated)', borderColor: 'var(--pc-border)' }}>
              <Bot className="h-4 w-4" style={{ color: 'var(--pc-accent)' }} />
            </div>
            {streamingContent || streamingThinking ? (
              <div className="rounded-2xl px-4 py-3 border max-w-[75%]" style={{ background: 'var(--pc-bg-elevated)', borderColor: 'var(--pc-border)', color: 'var(--pc-text-primary)' }}>
                {streamingThinking && (
                  <details className="mb-2" open={!streamingContent}>
                    <summary className="text-xs select-none" style={{ color: 'var(--pc-text-muted)' }}>Thinking{!streamingContent && '...'}</summary>
                    <pre className="text-xs mt-1 whitespace-pre-wrap break-words leading-relaxed overflow-auto max-h-60 p-2 rounded-lg" style={{ color: 'var(--pc-text-muted)', background: 'var(--pc-bg-surface)' }}>{streamingThinking}</pre>
                  </details>
                )}
                {streamingContent && (
                  <div className="text-sm break-words chat-markdown leading-relaxed">
                    <ReactMarkdown remarkPlugins={[remarkGfm]}>{streamingContent}</ReactMarkdown>
                  </div>
                )}
              </div>
            ) : (
              <div className="rounded-2xl px-4 py-3 border flex items-center gap-1.5" style={{ background: 'var(--pc-bg-elevated)', borderColor: 'var(--pc-border)' }}>
                <span className="bounce-dot w-1.5 h-1.5 rounded-full" style={{ background: 'var(--pc-accent)' }} />
                <span className="bounce-dot w-1.5 h-1.5 rounded-full" style={{ background: 'var(--pc-accent)' }} />
                <span className="bounce-dot w-1.5 h-1.5 rounded-full" style={{ background: 'var(--pc-accent)' }} />
              </div>
            )}
          </div>
        )}

        {pendingApproval && (
          <div className="rounded-2xl border p-4 max-w-3xl mx-auto animate-fade-in" style={{ background: 'var(--pc-bg-elevated)', borderColor: 'var(--pc-accent-dim)' }}>
            <div className="flex items-start justify-between gap-4">
              <div>
                <p className="text-xs uppercase tracking-[0.18em]" style={{ color: 'var(--pc-accent)' }}>
                  Approval Required
                </p>
                <p className="mt-1 text-sm font-semibold" style={{ color: 'var(--pc-text-primary)' }}>
                  {pendingApproval.toolName}
                </p>
                {pendingApproval.argumentsSummary && (
                  <p className="mt-2 text-sm leading-relaxed" style={{ color: 'var(--pc-text-muted)' }}>
                    {pendingApproval.argumentsSummary}
                  </p>
                )}
                {pendingApproval.expiresAt && (
                  <p className="mt-2 text-[11px]" style={{ color: 'var(--pc-text-faint)' }}>
                    Expires at {new Date(pendingApproval.expiresAt * 1000).toLocaleTimeString()}
                  </p>
                )}
              </div>
              <div className="flex flex-col gap-2 min-w-[150px]">
                {pendingApproval.choices.includes('yes') && (
                  <button
                    type="button"
                    onClick={() => handleApproval('yes')}
                    disabled={pendingApproval.submitting}
                    className="btn-electric rounded-xl px-3 py-2 text-sm"
                    style={{ color: 'white' }}
                  >
                    Allow Once
                  </button>
                )}
                {pendingApproval.choices.includes('always') && (
                  <button
                    type="button"
                    onClick={() => handleApproval('always')}
                    disabled={pendingApproval.submitting}
                    className="rounded-xl px-3 py-2 text-sm border"
                    style={{ borderColor: 'var(--pc-accent-dim)', color: 'var(--pc-text-primary)', background: 'var(--pc-bg)' }}
                  >
                    Always This Session
                  </button>
                )}
                {pendingApproval.choices.includes('no') && (
                  <button
                    type="button"
                    onClick={() => handleApproval('no')}
                    disabled={pendingApproval.submitting}
                    className="rounded-xl px-3 py-2 text-sm border"
                    style={{ borderColor: 'rgba(239, 68, 68, 0.3)', color: '#fca5a5', background: 'rgba(239, 68, 68, 0.08)' }}
                  >
                    Deny
                  </button>
                )}
              </div>
            </div>
          </div>
        )}

        <div ref={messagesEndRef} />
      </div>

      {/* Input area */}
      <div
        className={isLight ? 'px-4 pb-1 pt-2 md:px-6' : 'border-t p-4'}
        style={isLight ? undefined : { borderColor: 'var(--pc-border)', background: 'var(--pc-bg-surface)' }}
      >
        <div
          data-testid={isLight ? 'agent-light-composer-shell' : undefined}
          className={`mx-auto ${isLight ? 'agent-light-composer-shell max-w-5xl rounded-[14px] px-[10px] pb-[6px] pt-[8px]' : 'max-w-4xl'}`}
          style={isLight ? { background: '#F6F6F6' } : undefined}
        >
          <div className={isLight ? 'flex flex-col gap-2' : 'flex items-center gap-3'}>
            <input
              ref={fileInputRef}
              type="file"
              multiple
              accept={FILE_PICKER_ACCEPT}
              className="hidden"
              onChange={handleFileSelect}
            />
            {isLight ? (
              <div className="relative">
                <textarea
                  ref={inputRef}
                  rows={1}
                  value={input}
                  onChange={handleTextareaChange}
                  onKeyDown={handleKeyDown}
                  placeholder={connected ? t('agent.light_type_message') : t('agent.connecting')}
                  disabled={!connected || !!pendingApproval}
                  className={`w-full resize-none border-0 px-1 pr-10 text-[14px] outline-none disabled:opacity-40`}
                  style={{
                    minHeight: '44px',
                    maxHeight: '200px',
                    paddingTop: '1px',
                    paddingBottom: '0',
                    color: 'var(--pc-text-primary)',
                    lineHeight: '1.55',
                    background: 'transparent',
                  }}
                />
              </div>
            ) : (
              <textarea
                ref={inputRef}
                rows={1}
                value={input}
                onChange={handleTextareaChange}
                onKeyDown={handleKeyDown}
                placeholder={connected ? t('agent.type_message') : t('agent.connecting')}
                disabled={!connected || !!pendingApproval}
                className="input-electric flex-1 px-4 text-sm resize-none disabled:opacity-40"
                style={{ minHeight: '44px', maxHeight: '200px', paddingTop: '10px', paddingBottom: '10px' }}
              />
            )}

            {pendingAttachments.length > 0 && (
              <div className="flex flex-wrap items-center gap-2 px-1">
                {pendingAttachments.map((attachment) => (
                  <div
                    key={attachment.id}
                    className={`group relative border ${attachment.kind === 'image' ? 'h-[54px] w-[54px] overflow-hidden rounded-[10px]' : 'flex max-w-[260px] items-center gap-2 rounded-[10px] px-2 py-1.5'}`}
                    style={{ borderColor: '#d8dde6', background: '#f7f9fc' }}
                  >
                    {attachment.kind === 'image' ? (
                      <img
                        src={attachment.previewUrl}
                        alt={attachment.name}
                        className="h-full w-full object-cover"
                      />
                    ) : (
                      <span className="truncate text-[12px] leading-4" style={{ color: '#4d5057' }}>
                        {attachment.name}
                      </span>
                    )}
                    <button
                      type="button"
                      onClick={() => handleRemovePendingAttachment(attachment.id)}
                      className="absolute right-1 top-1 inline-flex h-4 w-4 items-center justify-center rounded-full border bg-white"
                      style={{ borderColor: '#c8cfda', color: '#586171' }}
                      aria-label={t('chat.remove_attachment').replace('{name}', attachment.name)}
                    >
                      <X className="h-3 w-3" />
                    </button>
                  </div>
                ))}
              </div>
            )}

            {isLight ? (
              <div className="flex items-center justify-between gap-4 pt-[9px]">
                <div className="flex flex-wrap items-center gap-[14px] text-[12px]" style={{ color: '#4d5057' }}>
                  <div className="relative" ref={composerMenuRef}>
                    <button
                      type="button"
                      onClick={() => void toggleComposerModelsMenu()}
                      disabled={!connected || modelSwitchLoading}
                      className="inline-flex items-center gap-1 text-[12px] leading-none transition-all disabled:opacity-40 h-[36px] w-auto px-2 rounded-lg"
                      style={{
                        color: '#262626',
                        background: composerModelsOpen
                          ? 'rgba(0, 0, 0, 0.08)'
                          : 'rgba(0, 0, 0, 0.03)',
                      }}
                      onMouseEnter={(e) => {
                        if (!composerModelsOpen) {
                          e.currentTarget.style.background = 'rgba(0, 0, 0, 0.05)';
                        }
                      }}
                      onMouseLeave={(e) => {
                        if (!composerModelsOpen) {
                          e.currentTarget.style.background = 'rgba(0, 0, 0, 0.03)';
                        }
                      }}
                    >
                      <Bot className="h-4 w-4 opacity-[0.86] flex-shrink-0" />
                      <span className="max-w-[208px] truncate">{composerCurrentLabel}</span>
                      <ChevronDown
                        className="h-4 w-4 opacity-60 transition-transform flex-shrink-0"
                        style={{ transform: composerModelsOpen ? 'rotate(180deg)' : 'none' }}
                      />
                    </button>

                    {composerModelsOpen && (
                      <div
                        className="absolute bottom-full left-0 mb-2 rounded-lg overflow-hidden animate-fade-in"
                        style={{
                          width: '290px',
                          background: '#FFFFFF',
                          border: '1px solid rgba(0, 0, 0, 0.1)',
                          borderRadius: '8px',
                          boxShadow: '0 4px 20px rgba(0, 0, 0, 0.2)',
                          zIndex: 9999,
                          maxHeight: '378px',
                        }}
                      >
                        <div
                          className="overflow-y-auto"
                          style={{
                            maxHeight: '376px',
                          }}
                        >
                          <div className="py-[7px] px-1">
                            {composerModelsList.length > 0 && (
                              <div className="space-y-[2px]">
                                <div
                                  className="py-1.5 text-[10px] font-semibold uppercase tracking-wider"
                                  style={{
                                    color: 'rgba(0, 0, 0, 0.35)',
                                    paddingLeft: '32px',
                                  }}
                                >
                                  {t('agent.model_selector_configured')}
                                </div>
                                {composerModelsList.map((model) => {
                                  const isSelected = composerItemSelected(model);
                                  const configuredItemKey = `configured-${model.provider}-${model.value}`;
                                  const isPressed = composerPressedItem === configuredItemKey;
                                  const isHovered = composerHoveredItem === configuredItemKey;
                                  const showWhite = isPressed || isHovered;
                                  return (
                                    <button
                                      key={configuredItemKey}
                                      type="button"
                                      onClick={() => void handleComposerModelPick(model)}
                                      onMouseDown={() => setComposerPressedItem(configuredItemKey)}
                                      onMouseUp={() => setComposerPressedItem(null)}
                                      onMouseEnter={() => setComposerHoveredItem(configuredItemKey)}
                                      onMouseLeave={() => {
                                        setComposerHoveredItem(null);
                                        setComposerPressedItem(null);
                                      }}
                                      className="w-full text-left flex items-center gap-2 transition-colors"
                                      style={{
                                        height: '36px',
                                        borderRadius: '6px',
                                        padding: '0 8px',
                                        background: isPressed
                                          ? 'rgba(44, 115, 200, 1)'
                                          : isHovered
                                            ? 'rgba(55, 144, 250, 1)'
                                            : 'transparent',
                                      }}
                                    >
                                      <div
                                        className="w-4 h-4 flex-shrink-0 flex items-center justify-center"
                                        style={{ opacity: isSelected ? 1 : 0 }}
                                      >
                                        <Check
                                          className="h-4 w-4"
                                          style={{
                                            color: showWhite ? '#FFFFFF' : 'currentColor',
                                          }}
                                        />
                                      </div>
                                      <Bot
                                        className="h-4 w-4 flex-shrink-0"
                                        style={{
                                          color: showWhite ? '#FFFFFF' : 'currentColor',
                                        }}
                                      />
                                      <span
                                        className="model-label text-[13px] font-medium truncate flex-1"
                                        style={{
                                          color: showWhite
                                            ? '#FFFFFF'
                                            : 'rgba(0, 0, 0, 0.85)',
                                        }}
                                      >
                                        {model.label}
                                      </span>
                                    </button>
                                  );
                                })}
                              </div>
                            )}
                            {composerModelsLoading && composerModelsList.length === 0 && (
                              <div
                                className="px-4 py-3 text-[12px]"
                                style={{ color: 'rgba(0, 0, 0, 0.35)' }}
                              >
                                {t('common.loading')}
                              </div>
                            )}
                            {!composerModelsLoading && composerModelsList.length === 0 && (
                              <div
                                className="px-4 py-3 text-[12px]"
                                style={{ color: 'rgba(0, 0, 0, 0.35)' }}
                              >
                                {t('agent.model_selector_no_configured')}
                              </div>
                            )}
                          </div>
                        </div>
                      </div>
                    )}
                  </div>
                </div>
                <div className="flex items-center gap-[10px]">
                  <div
                    className='flex items-center justify-center'
                    style={{
                      width: '32px',
                      height: '32px',
                      borderRadius: '6px',
                      background: 'transparent',
                      border: 'none',
                      cursor: 'default',
                    }}
                    onClick={handlePickAttachments}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.background = '#DCDCDC';
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.background = 'transparent';
                    }}
                    onMouseDown={(e) => {
                      e.currentTarget.style.background = '#B9B9B9';
                    }}
                    onMouseUp={(e) => {
                      e.currentTarget.style.background = '#DCDCDC';
                    }}
                  >
                    <Paperclip className='h-4 w-4' style={{ color: '#5c6067' }} strokeWidth={1.9} />
                  </div>
                  {typing ? (
                    <button
                      type="button"
                      onClick={handleStop}
                      disabled={!connected}
                      className="flex h-[32px] w-[32px] items-center justify-center transition-colors"
                      style={{
                        borderRadius: '6px',
                        background: !connected ? 'rgba(55, 144, 250, 0.45)' : '#3790FA',
                      }}
                      aria-label="Stop generating"
                      onMouseEnter={(e) => {
                        if (connected) {
                          e.currentTarget.style.background = '#3489EE';
                        }
                      }}
                      onMouseLeave={(e) => {
                        e.currentTarget.style.background = !connected ? 'rgba(55, 144, 250, 0.45)' : '#3790FA';
                      }}
                      onMouseDown={(e) => {
                        if (connected) {
                          e.currentTarget.style.background = '#2C73C8';
                        }
                      }}
                      onMouseUp={(e) => {
                        if (connected) {
                          e.currentTarget.style.background = '#3790FA';
                        }
                      }}
                    >
                      <Square className="h-3.5 w-3.5 fill-white text-white" />
                    </button>
                  ) : (
                    <button
                      type="button"
                      onClick={handleSend}
                      disabled={
                        !connected ||
                        !!pendingApproval ||
                        (!input.trim() && pendingAttachments.length === 0)
                      }
                      className="flex h-[32px] w-[32px] items-center justify-center transition-colors"
                      style={{
                        borderRadius: '6px',
                        background:
                          !connected ||
                            !!pendingApproval ||
                            (!input.trim() && pendingAttachments.length === 0)
                            ? 'rgba(55, 144, 250, 0.45)'
                            : '#3790FA',
                      }}
                      aria-label="Send message"
                      onMouseEnter={(e) => {
                        if (
                          connected &&
                          !pendingApproval &&
                          (input.trim() || pendingAttachments.length > 0)
                        ) {
                          e.currentTarget.style.background = '#3489EE';
                        }
                      }}
                      onMouseLeave={(e) => {
                        if (
                          connected &&
                          !pendingApproval &&
                          (input.trim() || pendingAttachments.length > 0)
                        ) {
                          e.currentTarget.style.background = '#3790FA';
                        } else {
                          e.currentTarget.style.background = 'rgba(55, 144, 250, 0.45)';
                        }
                      }}
                      onMouseDown={(e) => {
                        if (
                          connected &&
                          !pendingApproval &&
                          (input.trim() || pendingAttachments.length > 0)
                        ) {
                          e.currentTarget.style.background = '#2C73C8';
                        }
                      }}
                      onMouseUp={(e) => {
                        if (
                          connected &&
                          !pendingApproval &&
                          (input.trim() || pendingAttachments.length > 0)
                        ) {
                          e.currentTarget.style.background = '#3790FA';
                        }
                      }}
                    >
                      <LightThemeAssetIcon
                        fileName="send_text_white.svg"
                        alt="Send message"
                        className="h-4 w-4 opacity-[0.84]"
                        style={{ transform: "rotate(0deg)" }}
                      />
                    </button>
                  )}
                </div>
              </div>
            ) : typing ? (
              <>
                <button
                  type='button'
                  onClick={handleStop}
                  disabled={!connected}
                  className="btn-electric flex-shrink-0 flex items-center justify-center"
                  style={{ color: 'white', width: '32px', height: '32px', borderRadius: '6px', background: '#3790FA' }}
                >
                  <Square className="h-4 w-4 fill-white" />
                </button>
              </>
            ) : (
              <>
                <button
                  type='button'
                  onClick={handleSend}
                  disabled={!connected || !input.trim() || !!pendingApproval}
                  className="btn-electric flex-shrink-0 flex items-center justify-center"
                  style={{ color: 'white', width: '32px', height: '32px', borderRadius: '6px' }}
                >
                  <Send className="h-4 w-4" />
                </button>
              </>
            )}
          </div>

          {!isLight && (
            <div className="flex items-center justify-center mt-2 gap-2">
              <span
                className="status-dot"
                style={connected
                  ? { background: 'var(--color-status-success)', boxShadow: '0 0 6px var(--color-status-success)' }
                  : { background: 'var(--color-status-error)', boxShadow: '0 0 6px var(--color-status-error)' }
                }
              />
              <span className="text-[10px]" style={{ color: 'var(--pc-text-faint)' }}>
                {connected ? t('agent.connected_status') : t('agent.disconnected_status')}
              </span>
            </div>
          )}
        </div>
      </div>

      <IdentitySetupModal
        open={showIdentitySetup}
        onClose={() => setShowIdentitySetup(false)}
        onConfirm={handleIdentitySetupConfirm}
      />
    </div>
  );
}
