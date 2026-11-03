import { useState, useEffect, useRef, useCallback } from 'react';
import QRCodeLib from 'qrcode';
import { getWechatQrCode, pollWechatStatus } from '@/lib/api';
import { assetBasePath } from '@/lib/basePath';
import { useLocale } from '@/lib/i18n';
import {
  ChannelQrExpiredText,
  ChannelQrPanel,
  ChannelQrScannedBadge,
} from './channel/ChannelQrBox';
import { ChannelBindHeader, ChannelErrorAlert } from './channel/ChannelBindUi';
import { getChannelBindContentStyle } from './channel/channelBindLayout';
import { scheduleEmbeddedCompositorSafeWork } from '@/lib/embeddedQtWebEngine';

type BindPhase = 'loading' | 'waiting_scan' | 'scanned' | 'confirmed' | 'expired' | 'error';

interface QrData {
  qrcode: string;
  qrcode_img_url: string;
}

interface StatusData {
  status: string;
  bot_token?: string;
  ilink_bot_id?: string;
  ilink_user_id?: string;
}

const POLL_INTERVAL_MS = 2000;

export default function WeChatBind({
  onBindSuccess,
  contentPaddingTop,
}: {
  onBindSuccess?: () => void;
  contentPaddingTop?: number;
}) {
  const { t } = useLocale();
  const [phase, setPhase] = useState<BindPhase>('loading');
  const [qrData, setQrData] = useState<QrData | null>(null);
  const [statusData, setStatusData] = useState<StatusData | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [qrFetchFailed, setQrFetchFailed] = useState(false);
  const [qrRendered, setQrRendered] = useState(false);

  const canvasRef = useRef<HTMLCanvasElement>(null);
  const pollTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const pollingQrRef = useRef<string>('');

  const stopPolling = useCallback(() => {
    if (pollTimerRef.current) { clearInterval(pollTimerRef.current); pollTimerRef.current = null; }
    pollingQrRef.current = '';
  }, []);

  const renderQrOnCanvas = useCallback(async (payload: string) => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    setQrRendered(false);
    try {
      await QRCodeLib.toCanvas(canvas, payload, {
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
    if (!qrData || !(phase === 'waiting_scan' || phase === 'scanned')) return;
    const payload = qrData.qrcode_img_url || qrData.qrcode;
    const timerId = scheduleEmbeddedCompositorSafeWork(() => {
      void renderQrOnCanvas(payload);
    });
    return () => window.clearTimeout(timerId);
  }, [qrData, phase, renderQrOnCanvas]);

  const startPolling = useCallback((qrcode: string) => {
    stopPolling();
    pollingQrRef.current = qrcode;
    pollTimerRef.current = setInterval(async () => {
      if (pollingQrRef.current !== qrcode) return;
      try {
        const data = await pollWechatStatus(qrcode);
        if (pollingQrRef.current !== qrcode) return;
        switch (data.status) {
          case 'scaned':
            setPhase((prev) => prev === 'scanned' ? prev : 'scanned');
            break;
          case 'confirmed':
            stopPolling();
            setStatusData(data);
            setPhase('confirmed');
            onBindSuccess?.();
            break;
          case 'expired':
            stopPolling();
            setPhase('expired');
            break;
          default:
            setPhase((prev) => prev === 'scanned' || prev === 'waiting_scan' ? prev : 'waiting_scan');
        }
      } catch (err) {
        console.warn('WeChat status poll error:', err);
      }
    }, POLL_INTERVAL_MS);
  }, [stopPolling]);

  const handleFetchQrCode = useCallback(async () => {
    stopPolling();
    setPhase('loading');
    setError(null);
    setQrFetchFailed(false);
    setStatusData(null);
    setQrRendered(false);
    try {
      const data = await getWechatQrCode();
      setQrData(data);
      setPhase('waiting_scan');
      startPolling(data.qrcode);
    } catch (err) {
      setPhase('error');
      setQrFetchFailed(true);
      setError(err instanceof Error ? err.message : String(err));
    }
  }, [stopPolling, startPolling]);

  useEffect(() => {
    handleFetchQrCode();
    return () => { stopPolling(); };
  }, []);

  const isQrFetchError = phase === 'error' && qrFetchFailed;

  return (
    <div style={getChannelBindContentStyle(contentPaddingTop)}>
      <ChannelBindHeader
        icon="wechat.svg"
        alt="WeChat"
        title={t('channel.wechat')}
        description={t('channel.wechat.desc')}
      />

      <ChannelErrorAlert message={error} visible={!isQrFetchError} />

      {(phase === 'loading' || phase === 'waiting_scan' || phase === 'scanned' || phase === 'expired' || phase === 'confirmed' || isQrFetchError) && (
        <>
          <p style={{
            fontFamily: "'Noto Sans CJK SC', sans-serif",
            fontSize: 14,
            fontWeight: 700,
            color: 'rgba(0, 0, 0, 0.85)',
            marginTop: 0,
            marginBottom: 16,
          }}>
            {t('channel.scan_tip')}
          </p>

          <ChannelQrPanel
            canvasRef={canvasRef}
            placeholderSrc={`${assetBasePath}/images/channel-qr-placeholder.png`}
            phase={phase}
            qrRendered={qrRendered}
            isQrFetchError={isQrFetchError}
            brandColor="#1890ff"
            onRefresh={handleFetchQrCode}
            refreshLabel={t('channel.refresh_qr')}
            fetchFailedLabel={t('channel.qr_fetch_failed')}
            bindSuccessLabel={t('channel.bind_success')}
            confirmedFontSize={10}
            statusContent={phase === 'scanned' ? (
                <ChannelQrScannedBadge label={t('channel.scanned_confirm')} />
              ) : phase === 'expired' ? (
                <ChannelQrExpiredText label={t('channel.qr_expired')} />
              ) : phase === 'confirmed' ? (
                <span style={{ fontSize: 14, color: 'rgba(0,0,0,0.45)' }}>Bot ID: {statusData?.ilink_bot_id || '-'}</span>
              ) : null}
          />
        </>
      )}

    </div>
  );
}
