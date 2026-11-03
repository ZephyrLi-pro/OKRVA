import { useState, useEffect, useRef, useCallback } from 'react';
import QRCodeLib from 'qrcode';
import { beginFeishuRegistration, pollFeishuRegistration, saveFeishuWebSocketConfig } from '@/lib/api';
import { assetBasePath } from '@/lib/basePath';
import { useLocale } from '@/lib/i18n';
import {
  ChannelQrExpiredText,
  ChannelQrPanel,
  ChannelQrScannedBadge,
} from './channel/ChannelQrBox';
import {
  ChannelBindHeader,
  ChannelBindSuccess,
  ChannelErrorAlert,
  ChannelManualConfigForm,
  ChannelModeSelector,
} from './channel/ChannelBindUi';
import { getChannelBindContentStyle } from './channel/channelBindLayout';
import { scheduleEmbeddedCompositorSafeWork } from '@/lib/embeddedQtWebEngine';

type BindPhase = 'loading' | 'waiting_scan' | 'scanned' | 'confirmed' | 'expired' | 'error' | 'ws_success';

type ReceiveMode = 'qrcode' | 'websocket';

const POLL_INTERVAL_MS = 2000;

export default function FeishuBind({
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

  const [appId, setAppId] = useState('');
  const [appSecret, setAppSecret] = useState('');
  const [showSecret, setShowSecret] = useState(false);
  const [submitting, setSubmitting] = useState(false);

  const canvasRef = useRef<HTMLCanvasElement>(null);
  const pollTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const pollingDeviceCodeRef = useRef<string>('');

  const stopPolling = useCallback(() => {
    if (pollTimerRef.current) {
      clearInterval(pollTimerRef.current);
      pollTimerRef.current = null;
    }
    pollingDeviceCodeRef.current = '';
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
    if (!qrUrl || !(phase === 'waiting_scan' || phase === 'scanned')) return;
    const timerId = scheduleEmbeddedCompositorSafeWork(() => {
      void renderQrOnCanvas(qrUrl);
    });
    return () => window.clearTimeout(timerId);
  }, [qrUrl, phase, renderQrOnCanvas]);

  const startPolling = useCallback((code: string) => {
    stopPolling();
    pollingDeviceCodeRef.current = code;
    pollTimerRef.current = setInterval(async () => {
      if (pollingDeviceCodeRef.current !== code) return;
      try {
        const data = await pollFeishuRegistration(code);
        if (pollingDeviceCodeRef.current !== code) return;

        if (data.client_id && data.client_secret) {
          stopPolling();
          setPhase('confirmed');
          onBindSuccess?.();
          return;
        }

        switch (data.error) {
          case 'authorization_pending':
          case undefined:
            break;
          case 'slow_down':
            console.log('Server requested slower polling');
            break;
          case 'access_denied':
            stopPolling();
            setQrFetchFailed(false);
            setError(data.error_description || t('channel.feishu.access_denied'));
            setPhase('error');
            break;
          case 'expired_token':
            stopPolling();
            setPhase('expired');
            break;
          default:
            if (data.error) {
              console.warn('Feishu poll error:', data.error, data.error_description);
            }
        }
      } catch (err) {
        console.warn('Feishu status poll error:', err);
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
      const data = await beginFeishuRegistration();
      setQrUrl(data.verification_uri_complete);
      setPhase('waiting_scan');
      startPolling(data.device_code);
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
    if (newMode === 'websocket') {
      stopPolling();
      setPhase('loading');
      setError(null);
      setQrFetchFailed(false);
      setAppId('');
      setAppSecret('');
      setTimeout(() => setPhase('ws_success'), 100);
    } else {
      handleFetchQrCode();
    }
  };

  const handleWsConnect = async () => {
    if (!appId.trim() || !appSecret.trim()) {
      setQrFetchFailed(false);
      setError(t('channel.feishu.fill_required'));
      return;
    }

    setSubmitting(true);
    setError(null);
    setQrFetchFailed(false);
    try {
      await saveFeishuWebSocketConfig(appId.trim(), appSecret.trim());
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
    setAppId('');
    setAppSecret('');
    setShowSecret(false);
    setError(null);
    setQrFetchFailed(false);
    onClose?.();
  };

  const isQrFetchError = phase === 'error' && qrFetchFailed;

  return (
    <div style={getChannelBindContentStyle(contentPaddingTop)}>
      <ChannelBindHeader
        icon="feishu.svg"
        alt="Feishu"
        title={t('channel.feishu')}
        description={t('channel.feishu.desc')}
      />

      <ChannelModeSelector
        value={mode}
        options={[
          { value: 'qrcode', label: t('channel.scan_connect') },
          { value: 'websocket', label: t('channel.websocket_connect') },
        ]}
        onChange={(value) => handleModeChange(value as ReceiveMode)}
        visible={phase !== 'error' || mode === 'websocket' || isQrFetchError}
      />

      <ChannelErrorAlert message={error} visible={!isQrFetchError} />

      {mode === 'websocket' && (
        <>
          {phase === 'confirmed' ? (
            <ChannelBindSuccess label={t('channel.bind_success')} />
          ) : (
            <ChannelManualConfigForm
              primary={{
                label: 'App ID',
                value: appId,
                onChange: setAppId,
                placeholder: t('channel.feishu.app_id_placeholder'),
              }}
              secret={{
                label: 'App Secret',
                value: appSecret,
                onChange: setAppSecret,
                placeholder: t('channel.feishu.app_secret_placeholder'),
              }}
              showSecret={showSecret}
              onToggleSecret={() => setShowSecret(!showSecret)}
              cancelLabel={t('channel.cancel')}
              submitLabel={t('channel.connect')}
              onCancel={handleCancel}
              onSubmit={handleWsConnect}
              submitting={submitting}
              canSubmit={Boolean(!submitting && appId.trim() && appSecret.trim())}
            />
          )}
        </>
      )}

      {mode === 'qrcode' && (
        <>
          {(phase === 'loading' || phase === 'waiting_scan' || phase === 'scanned' || phase === 'expired' || phase === 'confirmed' || isQrFetchError) && (
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
              confirmedFontSize={10}
              statusContent={phase === 'scanned' ? (
                <ChannelQrScannedBadge label={t('channel.scanned_confirm')} />
              ) : phase === 'expired' ? (
                <ChannelQrExpiredText label={t('channel.qr_expired')} />
              ) : null}
            />
          )}
        </>
      )}

    </div>
  );
}
