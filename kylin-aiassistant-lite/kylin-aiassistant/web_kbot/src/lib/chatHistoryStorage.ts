import type { SessionMessageRow, Session } from '@/types/api';
import { generateUUID } from '@/lib/uuid';
import { getSessions } from '@/lib/api';

const MAX_MESSAGES = 100;
const PREFIX = 'kylinbot_chat_history_v1:';

export interface ChatSessionSummary {
  sessionId: string;
  title: string;
  preview: string;
  updatedAt: string;
  messageCount: number;
}

export interface PersistedChatBubble {
  id: string;
  role: 'user' | 'agent';
  content: string;
  thinking?: string;
  markdown?: boolean;
  attachments?: PersistedAttachment[];
  timestamp: string;
}

export interface PersistedAttachment {
  id: string;
  name: string;
  mimeType?: string;
  kind: 'image' | 'audio' | 'video' | 'file';
  dataBase64: string;
}

function storageKey(sessionId: string): string {
  return `${PREFIX}${sessionId}`;
}

export function loadChatHistory(sessionId: string): PersistedChatBubble[] {
  try {
    const raw = localStorage.getItem(storageKey(sessionId));
    if (!raw) return [];
    const parsed = JSON.parse(raw) as { messages?: PersistedChatBubble[] };
    if (!parsed.messages?.length) return [];
    return parsed.messages;
  } catch {
    return [];
  }
}

export function saveChatHistory(sessionId: string, messages: PersistedChatBubble[]): void {
  try {
    const slice = messages.slice(-MAX_MESSAGES);
    localStorage.setItem(storageKey(sessionId), JSON.stringify({ messages: slice }));
  } catch {
    // QuotaExceeded or private mode
  }
}

let cachedSessions: ChatSessionSummary[] | null = null;

export async function listChatSessions(): Promise<ChatSessionSummary[]> {
  if (cachedSessions) {
    return cachedSessions;
  }

  try {
    const sessions = await getSessions();
    cachedSessions = sessions.map((session: Session) => ({
      sessionId: session.id,
      title: session.channel || '未命名会话',
      preview: '',
      updatedAt: session.last_activity,
      messageCount: session.message_count,
    }));
    return cachedSessions;
  } catch {
    return [];
  }
}

export function clearCachedSessions(): void {
  cachedSessions = null;
}

/**
 * 从 content 中提取 [IMAGE:data:<mime>;base64,<data>] 标记，
 * 返回清理后的纯文本和提取出的图片 attachments。
 * 这是 voice-assistant 屏幕共享截图的存储格式。
 */
function extractImageAttachmentsFromContent(content: string): {
  cleanContent: string;
  attachments: PersistedAttachment[];
} {
  if (!content) {
    return { cleanContent: '', attachments: [] };
  }

  const attachments: PersistedAttachment[] = [];
  // 匹配 [IMAGE:data:image/png;base64,xxxx] 等格式
  // 允许前后可选空格，base64 字符集包含 A-Z a-z 0-9 + / =
  const regex = /\[IMAGE:(data:([^;]+);base64,([A-Za-z0-9+/=]+))\]/gi;

  const cleanContent = content.replace(regex, (_match, _dataUri, mimeType: string, base64Data: string) => {
    const id = generateUUID();
    const ext = mimeType.split('/')[1] || 'png';
    const name = `screen-capture.${ext}`;

    attachments.push({
      id,
      name,
      mimeType,
      kind: 'image',
      dataBase64: base64Data,
    });

    return '';
  });

  // 清理移除标记后可能留下的多余空白
  const trimmed = cleanContent.replace(/\s{2,}/g, ' ').trim();

  return { cleanContent: trimmed, attachments };
}

/** Map server-persisted rows into UI messages (timestamps are synthetic for ordering). */
export function mapServerMessagesToPersisted(rows: SessionMessageRow[]): PersistedChatBubble[] {
  const base = Date.now() - rows.length * 1000;
  const out: PersistedChatBubble[] = [];
  let idx = 0;
  for (const row of rows) {
    if (row.role === 'system') continue;
    const ts = new Date(base + idx * 1000).toISOString();
    idx += 1;

    // 解析 content 中可能存在的 [IMAGE:...] 截图标记（voice-assistant 屏幕共享）
    const { cleanContent, attachments } = extractImageAttachmentsFromContent(row.content ?? '');

    if (row.role === 'user') {
      out.push({
        id: generateUUID(),
        role: 'user',
        content: cleanContent,
        attachments: attachments.length > 0 ? attachments : undefined,
        timestamp: ts,
      });
    } else if (row.role === 'assistant') {
      out.push({
        id: generateUUID(),
        role: 'agent',
        content: cleanContent,
        markdown: true,
        attachments: attachments.length > 0 ? attachments : undefined,
        timestamp: ts,
      });
    } else {
      out.push({
        id: generateUUID(),
        role: 'agent',
        content: cleanContent,
        markdown: false,
        attachments: attachments.length > 0 ? attachments : undefined,
        timestamp: ts,
      });
    }
  }
  return out;
}

export function persistedToUiMessages(
  rows: PersistedChatBubble[],
): Array<{
  id: string;
  role: 'user' | 'agent';
  content: string;
  thinking?: string;
  markdown?: boolean;
  attachments?: PersistedAttachment[];
  timestamp: Date;
}> {
  return rows.map((m) => ({
    id: m.id,
    role: m.role,
    content: m.content,
    thinking: m.thinking,
    markdown: m.markdown,
    attachments: m.attachments,
    timestamp: new Date(m.timestamp),
  }));
}

export function uiMessagesToPersisted(
  messages: Array<{
    id: string;
    role: 'user' | 'agent';
    content: string;
    thinking?: string;
    markdown?: boolean;
    attachments?: PersistedAttachment[];
    timestamp: Date;
  }>,
): PersistedChatBubble[] {
  return messages.map((m) => ({
    id: m.id,
    role: m.role,
    content: m.content,
    thinking: m.thinking,
    markdown: m.markdown,
    attachments: m.attachments,
    timestamp: m.timestamp.toISOString(),
  }));
}
