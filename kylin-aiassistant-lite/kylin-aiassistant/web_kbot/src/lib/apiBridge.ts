/**
 * QWebChannel 桥接层：qrc 模式下替代 fetch()，将 HTTP 请求通过
 * Qt WebChannel 转发给 C++ WebApiBridge，由 C++ 经 Unix Socket 完成请求。
 *
 * 非 qrc 模式下不会加载此模块。
 */

/* eslint-disable @typescript-eslint/no-explicit-any */

declare global {
  interface Window {
    qt?: { webChannelTransport?: unknown };
    QWebChannel?: new (
      transport: unknown,
      callback: (channel: { objects: Record<string, any> }) => void,
    ) => void;
  }
}

interface BridgeObject {
  httpRequest: (
    requestId: string,
    method: string,
    path: string,
    body: string,
    headersJson: string,
  ) => void;
  httpResponse: {
    connect: (
      handler: (requestId: string, statusCode: number, body: string) => void,
    ) => void;
  };
  isGatewayReachable: () => boolean;
  startGatewayDaemon: () => boolean;
}

let bridge: BridgeObject | null = null;
let bridgeReady: Promise<BridgeObject> | null = null;
const pendingRequests = new Map<
  string,
  { resolve: (value: { status: number; body: string }) => void; reject: (err: Error) => void }
>();
let requestCounter = 0;

/** Align with gateway LONG_RUNNING_REQUEST_TIMEOUT_SECS for manual cron runs. */
function bridgeRequestTimeoutMs(method: string, path: string): number {
  const normalized = path.split('?')[0] ?? path;
  if (
    method.toUpperCase() === 'POST' &&
    /^\/api\/cron\/[^/]+\/run$/.test(normalized)
  ) {
    return 600_000;
  }
  return 30_000;
}

/**
 * 初始化 QWebChannel 连接，获取 C++ 端 webApiBridge 对象。
 * 仅在首次调用时初始化，后续复用同一个连接。
 */
function initBridge(): Promise<BridgeObject> {
  if (bridgeReady) return bridgeReady;

  bridgeReady = new Promise<BridgeObject>((resolve, reject) => {
    const QWebChannel = window.QWebChannel;
    const transport = window.qt?.webChannelTransport;

    console.log('[apiBridge] init: QWebChannel=', !!QWebChannel, 'transport=', !!transport);

    if (!QWebChannel || !transport) {
      const err = new Error('QWebChannel not available: QWebChannel=' + !!QWebChannel + ' transport=' + !!transport);
      console.error('[apiBridge]', err.message);
      reject(err);
      return;
    }

    new QWebChannel(transport, (channel) => {
      console.log('[apiBridge] channel connected, objects:', Object.keys(channel.objects));
      const obj = channel.objects.webApiBridge as BridgeObject | undefined;
      if (!obj) {
        const err = new Error('webApiBridge not found in WebChannel objects: ' + Object.keys(channel.objects).join(', '));
        console.error('[apiBridge]', err.message);
        reject(err);
        return;
      }

      // 监听 C++ 端异步返回的响应信号
      obj.httpResponse.connect(
        (requestId: string, statusCode: number, body: string) => {
          console.log('[apiBridge] response:', requestId, statusCode, body.substring(0, 100));
          const pending = pendingRequests.get(requestId);
          if (pending) {
            pendingRequests.delete(requestId);
            pending.resolve({ status: statusCode, body });
          }
        },
      );

      bridge = obj;
      console.log('[apiBridge] bridge ready');
      resolve(obj);
    });
  });

  return bridgeReady;
}

/**
 * 通过 QWebChannel 桥接发送 HTTP 请求。
 * 替代 qrc 模式下的 fetch()。
 */
export async function bridgeFetch(
  path: string,
  options: {
    method?: string;
    headers?: Record<string, string>;
    body?: string;
  } = {},
): Promise<{ status: number; body: string }> {
  const b = bridge ?? (await initBridge());

  const requestId = `req_${++requestCounter}_${Date.now()}`;
  const method = (options.method ?? 'GET').toUpperCase();
  const body = options.body ?? '';
  const headersJson = options.headers ? JSON.stringify(options.headers) : '';

  console.log('[apiBridge] request:', requestId, method, path, 'body.length:', body.length, 'has headers:', !!options.headers);

  return new Promise<{ status: number; body: string }>((resolve, reject) => {
    const timeoutMs = bridgeRequestTimeoutMs(method, path);
    const timeout = setTimeout(() => {
      pendingRequests.delete(requestId);
      reject(new Error(`Bridge request timeout: ${method} ${path}`));
    }, timeoutMs);

    pendingRequests.set(requestId, {
      resolve: (value) => {
        clearTimeout(timeout);
        resolve(value);
      },
      reject: (err) => {
        clearTimeout(timeout);
        reject(err);
      },
    });

    b.httpRequest(requestId, method, path, body, headersJson);
  });
}

/** Check gateway reachability via C++ bridge (qrc mode only). */
export async function bridgeIsGatewayReachable(): Promise<boolean> {
  const b = bridge ?? (await initBridge());
  if (typeof b.isGatewayReachable !== 'function') {
    throw new Error('webApiBridge.isGatewayReachable is not available');
  }
  return b.isGatewayReachable();
}

/** Start kylin-bot daemon via C++ bridge (qrc mode only). */
export async function bridgeStartGatewayDaemon(): Promise<boolean> {
  const b = bridge ?? (await initBridge());
  if (typeof b.startGatewayDaemon !== 'function') {
    throw new Error('webApiBridge.startGatewayDaemon is not available — rebuild kylin-aiassistant');
  }
  return b.startGatewayDaemon();
}
