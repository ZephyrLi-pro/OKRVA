import { render, screen, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import AgentChat from './AgentChat';
import type { WsMessage } from '@/types/api';
import { Outlet, Route, Routes, MemoryRouter } from 'react-router-dom';
import type { AgentPageOutletContext } from '@/components/layout/Layout';
import { AgentNewSessionProvider } from '@/contexts/AgentNewSessionContext';
import { ThemeContext, type ThemeContextValue } from '@/contexts/ThemeContextDef';

const wsMockState = vi.hoisted(() => ({
  lastInstance: null as {
    onMessage: ((msg: WsMessage) => void) | null;
    sentMessages: string[];
  } | null,
  sessionSeq: 0,
}));

vi.mock('@/lib/api', () => ({
  getSessionMessages: vi.fn(() =>
    Promise.resolve({
      session_id: '',
      session_persistence: false,
      messages: [],
    }),
  ),
  getStatus: vi.fn(() =>
    Promise.resolve({
      provider: null,
      model: '',
      temperature: 0,
      uptime_seconds: 0,
      gateway_port: 0,
      locale: 'zh',
      memory_backend: '',
      paired: true,
      channels: {},
      health: { pid: 0, updated_at: '', uptime_seconds: 0, components: {} },
    }),
  ),
  getConfig: vi.fn(() => Promise.resolve('')),
  putConfig: vi.fn(() => Promise.resolve()),
  // Schema-driven config endpoints used by `loadConfiguredModels` so the
  // chat page's no-model send-gate sees a seeded entry instead of throwing.
  listProps: vi.fn(() =>
    Promise.resolve({
      entries: [
        {
          path: 'providers.models.deepseek.model',
          category: 'providers',
          kind: 'string',
          type_hint: 'String',
          value: 'deepseek-v3.2',
          populated: true,
          is_secret: false,
        },
        {
          path: 'providers.models.deepseek.api-key',
          category: 'providers',
          kind: 'string',
          type_hint: 'String',
          populated: true,
          is_secret: true,
        },
      ],
    }),
  ),
  getProp: vi.fn((path: string) => {
    if (path === 'providers.fallback') {
      return Promise.resolve({ path, value: 'deepseek', populated: true });
    }
    return Promise.resolve({ path, value: '', populated: false });
  }),
  patchConfig: vi.fn(() => Promise.resolve({ saved: true, results: [] })),
  selectSectionItem: vi.fn(() =>
    Promise.resolve({ fields_prefix: 'providers.models.deepseek', created: true }),
  ),
  // `ApiError` is referenced for `instanceof` checks; provide the real
  // class shape so production code keeps working under test.
  ApiError: class ApiError extends Error {
    public readonly status: number;
    public readonly envelope: { code: string; message: string };
    constructor(
      status: number,
      envelope: { code: string; message: string; path?: string; op_index?: number },
    ) {
      super(`[${envelope.code}] ${envelope.message}`);
      this.name = 'ApiError';
      this.status = status;
      this.envelope = envelope;
    }
  },
}));

vi.mock('@/lib/ws', () => ({
  WebSocketClient: class MockWebSocketClient {
    connected = true;
    onMessage: ((msg: WsMessage) => void) | null = null;
    onOpen: (() => void) | null = null;
    onClose: ((ev: CloseEvent) => void) | null = null;
    onError: ((ev: Event) => void) | null = null;
    sentMessages: string[] = [];

    constructor() {
      wsMockState.lastInstance = this;
    }

    connect(): void {
      this.onOpen?.();
    }

    disconnect(): void {}

    sendMessage(content: string): void {
      this.sentMessages.push(content);
    }

    sendApprovalResponse(): void {}
  },
  getOrCreateSessionId: () => {
    let id = sessionStorage.getItem('kylinbot_session_id');
    if (!id) {
      id = 'test-session';
      sessionStorage.setItem('kylinbot_session_id', id);
    }
    return id;
  },
  replaceSessionId: () => {
    wsMockState.sessionSeq += 1;
    const id = `test-session-${wsMockState.sessionSeq}`;
    sessionStorage.setItem('kylinbot_session_id', id);
    return id;
  },
}));

describe('AgentChat event rendering', () => {
  beforeEach(() => {
    sessionStorage.clear();
    const ls = localStorage;
    if (typeof ls.clear === 'function') {
      ls.clear();
    } else {
      const keys: string[] = [];
      for (let i = 0; i < ls.length; i += 1) {
        const k = ls.key(i);
        if (k) keys.push(k);
      }
      keys.forEach((k) => ls.removeItem(k));
    }
    wsMockState.lastInstance = null;
    wsMockState.sessionSeq = 0;
    // The schema-driven `listProps`/`getProp` mocks above already report a
    // configured DeepSeek model, so the chat page's no-model send-gate
    // sees `configuredModelCount > 0` and forwards messages normally.
  });

  function renderAgentChat(themeOverrides?: Partial<ThemeContextValue>) {
    const themeValue: ThemeContextValue = {
      theme: 'dark',
      accent: 'blue',
      colorTheme: 'default-dark',
      uiFont: 'system',
      monoFont: 'jetbrains',
      uiFontSize: 15,
      monoFontSize: 14,
      resolvedTheme: 'dark',
      setTheme: () => {},
      setAccent: () => {},
      setColorTheme: () => {},
      setUiFont: () => {},
      setMonoFont: () => {},
      setUiFontSize: () => {},
      setMonoFontSize: () => {},
      ...themeOverrides,
    };

    return render(
      <AgentNewSessionProvider>
        <ThemeContext.Provider value={themeValue}>
          <MemoryRouter initialEntries={['/agent']}>
            <Routes>
              <Route
                path="/agent"
                element={(
                  <Outlet
                    context={{ setAgentGenerating: () => {} } satisfies AgentPageOutletContext}
                  />
                )}
              >
                <Route index element={<AgentChat />} />
              </Route>
            </Routes>
          </MemoryRouter>
        </ThemeContext.Provider>
      </AgentNewSessionProvider>,
    );
  }

  it('renders the light-theme welcome shell with suggestion cards and a framed composer', async () => {
    renderAgentChat({ resolvedTheme: 'light' });

    await waitFor(() => expect(wsMockState.lastInstance).not.toBeNull());

    expect(screen.getByTestId('agent-light-welcome')).toBeInTheDocument();
    expect(screen.getByTestId('agent-light-suggestions')).toBeInTheDocument();
    expect(screen.getByTestId('agent-light-composer-shell')).toBeInTheDocument();
  });

  it('shows a tool call card with expandable args JSON', async () => {
    const user = userEvent.setup();
    const { container } = renderAgentChat();

    await waitFor(() => expect(wsMockState.lastInstance).not.toBeNull());

    wsMockState.lastInstance?.onMessage?.({
      type: 'tool_call',
      name: 'shell',
      args: { command: 'pwd' },
      message_id: 'msg-1',
      session_id: 'test-session',
    });

    expect(await screen.findByText('shell')).toBeInTheDocument();
    await user.click(screen.getByText('args'));

    expect(container.textContent ?? '').toContain('"command": "pwd"');
  });

  it('uses event_type as the display title when present', async () => {
    renderAgentChat();

    await waitFor(() => expect(wsMockState.lastInstance).not.toBeNull());

    wsMockState.lastInstance?.onMessage?.({
      type: 'tool_call',
      event_type: 'file_read',
      name: 'file_read',
      args: { path: 'Cargo.toml' },
      message_id: 'msg-2',
      session_id: 'test-session',
    });

    expect(await screen.findByText('file_read')).toBeInTheDocument();
    expect(screen.queryByText('Running file_read...')).not.toBeInTheDocument();
  });

  it('renders tool results as event cards but never shows session messages', async () => {
    renderAgentChat();

    await waitFor(() => expect(wsMockState.lastInstance).not.toBeNull());

    wsMockState.lastInstance?.onMessage?.({
      type: 'session_start',
      session_id: 'test-session',
      resumed: true,
      content: 'connected',
    } as WsMessage & { resumed: boolean });

    wsMockState.lastInstance?.onMessage?.({
      type: 'tool_result',
      name: 'shell',
      output: 'pwd output',
      message_id: 'msg-3',
      session_id: 'test-session',
    });

    expect(await screen.findByText('shell')).toBeInTheDocument();
    expect(screen.getByText('pwd output')).toBeInTheDocument();
    expect(screen.queryByText('Session')).not.toBeInTheDocument();
    expect(screen.queryByText('test-session · resumed')).not.toBeInTheDocument();
  });

  it('normalizes runtime trace tool_call_result events into resolved tool cards', async () => {
    renderAgentChat();

    await waitFor(() => expect(wsMockState.lastInstance).not.toBeNull());

    wsMockState.lastInstance?.onMessage?.({
      type: 'tool_call',
      name: 'weather',
      args: { location: '北京', days: 1 },
      session_id: 'test-session',
    });

    wsMockState.lastInstance?.onMessage?.({
      type: 'message',
      event_type: 'tool_call_result',
      payload: {
        tool: 'weather',
        output: 'Weather for Beijing',
      },
      session_id: 'test-session',
    } as WsMessage);

    await waitFor(() => expect(screen.getByText('Weather for Beijing')).toBeInTheDocument());
    expect(screen.getAllByText('weather')).toHaveLength(1);
    expect(screen.getByText('Weather for Beijing')).toBeInTheDocument();
    expect(screen.queryByText('unknown')).not.toBeInTheDocument();
  });

  it('adds approval lifecycle events to the chat stream', async () => {
    renderAgentChat();

    await waitFor(() => expect(wsMockState.lastInstance).not.toBeNull());

    wsMockState.lastInstance?.onMessage?.({
      type: 'approval_request',
      request_id: 'req-1',
      tool_name: 'shell',
      arguments_summary: 'command: pwd',
      arguments: { command: 'pwd' },
      expires_at: 1,
      choices: ['yes', 'no'],
      session_id: 'test-session',
    });

    wsMockState.lastInstance?.onMessage?.({
      type: 'approval_resolved',
      request_id: 'req-1',
      decision: 'yes',
      session_id: 'test-session',
    });

    expect(await screen.findByText('Approval')).toBeInTheDocument();
    expect(await screen.findByText('Approval Resolved')).toBeInTheDocument();
    expect(await screen.findByText('decision: yes')).toBeInTheDocument();
  });

  it('applies styled markdown table classes to agent responses', async () => {
    renderAgentChat();

    await waitFor(() => expect(wsMockState.lastInstance).not.toBeNull());

    wsMockState.lastInstance?.onMessage?.({
      type: 'done',
      full_response: '| Name | Value |\n| --- | --- |\n| shell | ok |',
      session_id: 'test-session',
      message_id: 'msg-5',
    });

    const markdownRoot = await screen.findByTestId('chat-markdown');
    const table = markdownRoot.querySelector('table');

    expect(markdownRoot).toBeInTheDocument();
    expect(table).not.toBeNull();
    expect(screen.getByText('Name').closest('th')).not.toBeNull();
    expect(screen.getByText('shell').closest('td')).not.toBeNull();
  });

  it('uses Enter to send by default', async () => {
    const user = userEvent.setup();
    renderAgentChat();

    await waitFor(() => expect(wsMockState.lastInstance).not.toBeNull());

    const textarea = screen.getByPlaceholderText('Type a message...');
    await user.type(textarea, 'hello');
    await user.keyboard('{Shift>}{Enter}{/Shift}');

    expect((textarea as HTMLTextAreaElement).value).toBe('hello\n');
    expect(wsMockState.lastInstance?.sentMessages).toEqual([]);

    await user.keyboard('{Enter}');

    expect(wsMockState.lastInstance?.sentMessages).toEqual(['hello']);
    expect((textarea as HTMLTextAreaElement).value).toBe('');
  });

  it('does not use Ctrl+Enter as a send shortcut', async () => {
    const user = userEvent.setup();
    renderAgentChat();

    await waitFor(() => expect(wsMockState.lastInstance).not.toBeNull());

    const textarea = screen.getByPlaceholderText('Type a message...');
    await user.type(textarea, 'line one');
    await user.keyboard('{Control>}{Enter}{/Control}');
    await user.type(textarea, 'line two');

    expect((textarea as HTMLTextAreaElement).value).toBe('line oneline two');
    expect(wsMockState.lastInstance?.sentMessages).toEqual([]);
  });
});
