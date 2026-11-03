import { useCallback, useEffect, useRef, useState } from 'react';
import { checkGatewayAvailable, startGatewayDaemon } from '@/lib/api';

const START_WAIT_MS = 500;
const START_TIMEOUT_MS = 30_000;
/** 启动后稍等 gateway 完成初始化，避免子页面首屏 API 失败。 */
const SETTLE_AFTER_START_MS = 800;

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

export function useKybotService(routeKey: string) {
  const [available, setAvailable] = useState<boolean | null>(null);
  const [starting, setStarting] = useState(false);
  /** 服务从不可用变为可用时递增，供 Gate 强制 remount 子页面。 */
  const [contentEpoch, setContentEpoch] = useState(0);
  const startingRef = useRef(false);

  const markAvailable = useCallback((ok: boolean) => {
    setAvailable((prev) => {
      if (prev === ok) return prev;
      if (ok && prev === false) {
        setContentEpoch((epoch) => epoch + 1);
      }
      return ok;
    });
  }, []);

  const check = useCallback(async (): Promise<boolean> => {
    const ok = await checkGatewayAvailable();
    markAvailable(ok);
    return ok;
  }, [markAvailable]);

  // 仅在进入页面（切换 Tab / 路由变化）时检测一次，不做后台定时轮询。
  useEffect(() => {
    let cancelled = false;
    setAvailable(null);

    void checkGatewayAvailable().then((ok) => {
      if (!cancelled && !startingRef.current) markAvailable(ok);
    });

    return () => {
      cancelled = true;
    };
  }, [routeKey, markAvailable]);

  const start = useCallback(async () => {
    if (startingRef.current) return;
    startingRef.current = true;
    setStarting(true);
    try {
      const launched = await startGatewayDaemon();
      if (!launched) {
        console.error('[useKybotService] failed to launch kylin-bot daemon');
      }

      const deadline = Date.now() + START_TIMEOUT_MS;
      while (Date.now() < deadline) {
        await sleep(START_WAIT_MS);
        if (!(await checkGatewayAvailable())) continue;

        await sleep(SETTLE_AFTER_START_MS);
        if (!(await checkGatewayAvailable())) continue;

        startingRef.current = false;
        setStarting(false);
        markAvailable(true);
        return;
      }
    } finally {
      startingRef.current = false;
      setStarting(false);
    }
  }, [markAvailable]);

  return {
    available,
    starting,
    start,
    recheck: check,
    contentEpoch,
  };
}
