import { afterEach, describe, expect, it, vi } from 'vitest';
import { SSEClient } from './sse';

function streamFromText(text: string): ReadableStream<Uint8Array> {
  const encoder = new TextEncoder();
  return new ReadableStream<Uint8Array>({
    start(controller) {
      controller.enqueue(encoder.encode(text));
      controller.close();
    },
  });
}

describe('SSEClient', () => {
  afterEach(() => {
    vi.unstubAllGlobals();
  });

  it('parses CRLF-delimited SSE events', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn().mockResolvedValue({
        ok: true,
        body: streamFromText(
          'data: {"type":"tool_call","tool":"shell"}\r\n\r\n',
        ),
      }),
    );

    const client = new SSEClient({ autoReconnect: false });
    const events: Array<{ type: string; tool?: string }> = [];

    await new Promise<void>((resolve) => {
      client.onEvent = (event) => {
        events.push(event);
        resolve();
      };
      client.connect();
    });

    expect(events).toEqual([{ type: 'tool_call', tool: 'shell' }]);
  });
});
