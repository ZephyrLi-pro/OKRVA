import { useState, useEffect, useRef, useCallback } from 'react';
import QRCodeLib from 'qrcode';
import { fetchWecomQrCode, pollWecomStatus, saveWecomBotConfig } from '@/lib/api';
import { assetBasePath } from '@/lib/basePath';
import { useLocale } from '@/lib/i18n';
import { ChannelQrExpiredText, ChannelQrPanel } from './channel/ChannelQrBox';
import {
  ChannelBindHeader,
  ChannelBindSuccess,
  ChannelErrorAlert,
  ChannelManualConfigForm,
  ChannelModeSelector,
} from './channel/ChannelBindUi';
import { getChannelBindContentStyle } from './channel/channelBindLayout';
import { scheduleEmbeddedCompositorSafeWork } from '@/lib/embeddedQtWebEngine';

type BindPhase = 'loading' | 'waiting_scan' | 'confirmed' | 'expired' | 'error';

type ReceiveMode = 'qrcode' | 'manual';

const POLL_INTERVAL_MS = 3000;
// 企业微信 query_result 接口在「无人扫码」场景下不会返回 expired —— 实测
// 10 分钟轮询期间状态仅为 init(0-105s) → pending(105s+)，从不变为 expired。
// 因此必须在前端做兜底过期计时。120s 为经验值（企微 AI Bot 二维码典型 TTL ≈ 2 分钟），
// 到点把 QR 标记为 expired 并停止轮询，用户点击二维码可重新拉取。
const QR_TTL_MS = 120_000;

export default function WeComBind({
  onBindSuccess,
  onClose,
  contentPaddingTop,
}: {
  onBindSuccess?: () => void;
  onClose?: () => void;
  contentPaddingTop?: number;
}) {
  const { t } = useLocale();
  const [mode, setMode] = useState<ReceiveMode>('qrcode');
  const [phase, setPhase] = useState<BindPhase>('loading');
  const [qrUrl, setQrUrl] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [qrFetchFailed, setQrFetchFailed] = useState(false);
  const [qrRendered, setQrRendered] = useState(false);

  const [botId, setBotId] = useState('');
  const [secret, setSecret] = useState('');
  const [showSecret, setShowSecret] = useState(false);
  const [submitting, setSubmitting] = useState(false);

  const canvasRef = useRef<HTMLCanvasElement>(null);
  const pollTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const pollingScodeRef = useRef<string>('');
  const expireTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  const stopPolling = useCallback(() => {
    if (pollTimerRef.current) {
      clearInterval(pollTimerRef.current);
      pollTimerRef.current = null;
    }
    if (expireTimerRef.current) {
      clearTimeout(expireTimerRef.current);
      expireTimerRef.current = null;
    }
    pollingScodeRef.current = '';
  }, []);

  const renderQrOnCanvas = useCallback(async (url: string) => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    setQrRendered(false);
    try {
      await QRCodeLib.toCanvas(canvas, url, {
        width: 160,
        margin: 2,
        color: { dark: '#000000', light: '#ffffff' },
      });
      setQrRendered(true);
    } catch (err) {
      console.error('Failed to render QR code:', err);
    }
  }, []);

  useEffect(() => {
    if (!qrUrl || !(phase === 'waiting_scan' || phase === 'confirmed')) return;
    const timerId = scheduleEmbeddedCompositorSafeWork(() => {
      void renderQrOnCanvas(qrUrl);
    });
    return () => window.clearTimeout(timerId);
  }, [qrUrl, phase, renderQrOnCanvas]);

  const startPolling = useCallback((scode: string) => {
    stopPolling();
    pollingScodeRef.current = scode;
    pollTimerRef.current = setInterval(async () => {
      if (pollingScodeRef.current !== scode) return;
      try {
        const data = await pollWecomStatus(scode);
        if (pollingScodeRef.current !== scode) return;

        if (data.status === 'success' && data.bot_id && data.secret) {
          stopPolling();
          setPhase('confirmed');
          onBindSuccess?.();
          return;
        }

        if (data.status === 'expired') {
          stopPolling();
          setPhase('expired');
        }
      } catch (err) {
        console.warn('WeCom status poll error:', err);
      }
    }, POLL_INTERVAL_MS);
  }, [stopPolling]);

  const handleFetchQrCode = useCallback(async () => {
    stopPolling();
    setPhase('loading');
    setError(null);
    setQrFetchFailed(false);
    setQrRendered(false);
    try {
      const data = await fetchWecomQrCode();
      setQrUrl(data.auth_url);
      setPhase('waiting_scan');
      startPolling(data.scode);
      // 客户端兜底过期计时：企微 query_result 在无人扫码时不会返 expired，
      // 必须靠前端到点把 phase 翻成 expired，否则用户面前的 QR 永远显示"等待扫描"。
      expireTimerRef.current = setTimeout(() => {
        if (pollingScodeRef.current === data.scode) {
          stopPolling();
          setPhase('expired');
        }
      }, QR_TTL_MS);
    } catch (err) {
      setPhase('error');
      setQrFetchFailed(true);
      setError(err instanceof Error ? err.message : String(err));
    }
  }, [stopPolling, startPolling]);

  useEffect(() => {
    if (mode === 'qrcode') {
      handleFetchQrCode();
    }
    return () => {
      stopPolling();
    };
  }, [mode]);

  const handleModeChange = (newMode: ReceiveMode) => {
    setMode(newMode);
    if (newMode === 'manual') {
      stopPolling();
      setPhase('loading');
      setError(null);
      setQrFetchFailed(false);
      setBotId('');
      setSecret('');
      setTimeout(() => setPhase('loading'), 100);
    } else {
      handleFetchQrCode();
    }
  };

  const handleManualConnect = async () => {
    if (!botId.trim() || !secret.trim()) {
      setQrFetchFailed(false);
      setError(t('channel.wecom.fill_required'));
      return;
    }

    setSubmitting(true);
    setError(null);
    setQrFetchFailed(false);
    try {
      await saveWecomBotConfig(botId.trim(), secret.trim());
      setPhase('confirmed');
      onBindSuccess?.();
    } catch (err) {
      setPhase('error');
      setError(err instanceof Error ? err.message : String(err));
    } finally {
      setSubmitting(false);
    }
  };

  const handleCancel = () => {
    setBotId('');
    setSecret('');
    setShowSecret(false);
    setError(null);
    setQrFetchFailed(false);
    onClose?.();
  };

  const isQrFetchError = phase === 'error' && qrFetchFailed;

  return (
    <div style={getChannelBindContentStyle(contentPaddingTop)}>
      <ChannelBindHeader
        icon="wecom.svg"
        alt="WeCom"
        title={t('channel.wecom')}
        description={t('channel.wecom.desc')}
      />

      <ChannelModeSelector
        value={mode}
        options={[
          { value: 'qrcode', label: t('channel.scan_connect') },
          { value: 'manual', label: t('channel.websocket_connect') },
        ]}
        onChange={(value) => handleModeChange(value as ReceiveMode)}
        visible={phase !== 'error' || mode === 'manual' || isQrFetchError}
      />

      <ChannelErrorAlert message={error} visible={!isQrFetchError} />

      {mode === 'manual' && (
        <>
          {phase === 'confirmed' ? (
            <ChannelBindSuccess label={t('channel.bind_success')} />
          ) : (
            <ChannelManualConfigForm
              primary={{
                label: 'Bot ID',
                value: botId,
                onChange: setBotId,
                placeholder: t('channel.wecom.bot_id_placeholder'),
              }}
              secret={{
                label: 'Secret',
                value: secret,
                onChange: setSecret,
                placeholder: t('channel.wecom.secret_placeholder'),
              }}
              showSecret={showSecret}
              onToggleSecret={() => setShowSecret(!showSecret)}
              cancelLabel={t('channel.cancel')}
              submitLabel={t('channel.connect')}
              onCancel={handleCancel}
              onSubmit={handleManualConnect}
              submitting={submitting}
              canSubmit={Boolean(!submitting && botId.trim() && secret.trim())}
            />
          )}
        </>
      )}

      {mode === 'qrcode' && (
        <>
          {(phase === 'loading' || phase === 'waiting_scan' || phase === 'expired' || phase === 'confirmed' || isQrFetchError) && (
            <ChannelQrPanel
              canvasRef={canvasRef}
              phase={phase}
              qrRendered={qrRendered}
              isQrFetchError={isQrFetchError}
              placeholderSrc={`${assetBasePath}/images/channel-qr-placeholder.png`}
              brandColor="#1890ff"
              onRefresh={handleFetchQrCode}
              refreshLabel={t('channel.refresh_qr')}
              fetchFailedLabel={t('channel.qr_fetch_failed')}
              bindSuccessLabel={t('channel.bind_success')}
              statusContent={phase === 'expired' ? (
                <ChannelQrExpiredText label={t('channel.qr_expired')} />
              ) : null}
            />
          )}
        </>
      )}

    </div>
  );
}
