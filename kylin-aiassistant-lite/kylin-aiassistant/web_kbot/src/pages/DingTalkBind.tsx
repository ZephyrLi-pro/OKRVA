import { useState, useEffect, useRef, useCallback } from 'react';
import QRCodeLib from 'qrcode';
import { beginDingtalkRegistration, pollDingtalkRegistration, saveDingtalkStreamConfig } from '@/lib/api';
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

type BindPhase = 'loading' | 'waiting_scan' | 'confirmed' | 'expired' | 'error' | 'ws_success';

type ReceiveMode = 'qrcode' | 'stream';

const POLL_INTERVAL_MS = 2000;

export default function DingTalkBind({
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

  const [clientId, setClientId] = useState('');
  const [clientSecret, setClientSecret] = useState('');
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
    if (!qrUrl || !(phase === 'waiting_scan' || phase === 'confirmed')) return;
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
        const data = await pollDingtalkRegistration(code);
        if (pollingDeviceCodeRef.current !== code) return;

        if (data.client_id && data.client_secret) {
          stopPolling();
          setPhase('confirmed');
          onBindSuccess?.();
          return;
        }

        switch (data.status) {
          case 'WAITING':
            break;
          case 'FAIL':
            stopPolling();
            setQrFetchFailed(false);
            setError(data.fail_reason || t('channel.dingtalk.auth_failed'));
            setPhase('error');
            break;
          case 'EXPIRED':
            stopPolling();
            setPhase('expired');
            break;
          default:
            if (data.error) {
              console.warn('DingTalk poll error:', data.error);
            }
        }
      } catch (err) {
        console.warn('DingTalk status poll error:', err);
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
      const data = await beginDingtalkRegistration();
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
    if (newMode === 'stream') {
      stopPolling();
      setPhase('loading');
      setError(null);
      setQrFetchFailed(false);
      setClientId('');
      setClientSecret('');
      setTimeout(() => setPhase('ws_success'), 100);
    } else {
      handleFetchQrCode();
    }
  };

  const handleStreamConnect = async () => {
    if (!clientId.trim() || !clientSecret.trim()) {
      setQrFetchFailed(false);
      setError(t('channel.dingtalk.fill_required'));
      return;
    }

    setSubmitting(true);
    setError(null);
    setQrFetchFailed(false);
    try {
      await saveDingtalkStreamConfig(clientId.trim(), clientSecret.trim());
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
    setClientId('');
    setClientSecret('');
    setShowSecret(false);
    setError(null);
    setQrFetchFailed(false);
    onClose?.();
  };

  const isQrFetchError = phase === 'error' && qrFetchFailed;

  return (
    <div style={getChannelBindContentStyle(contentPaddingTop)}>
      <ChannelBindHeader
        icon="dingtalk.svg"
        alt="DingTalk"
        title={t('channel.dingtalk')}
        description={t('channel.dingtalk.desc')}
      />

      <ChannelModeSelector
        value={mode}
        options={[
          { value: 'qrcode', label: t('channel.scan_connect') },
          { value: 'stream', label: t('channel.stream_connect') },
        ]}
        onChange={(value) => handleModeChange(value as ReceiveMode)}
        visible={phase !== 'error' || mode === 'stream' || isQrFetchError}
      />

      <ChannelErrorAlert message={error} visible={!isQrFetchError} />

      {mode === 'stream' && (
        <>
          {phase === 'confirmed' ? (
            <ChannelBindSuccess label={t('channel.bind_success')} />
          ) : (
            <ChannelManualConfigForm
              primary={{
                label: 'Client ID',
                value: clientId,
                onChange: setClientId,
                placeholder: t('channel.dingtalk.client_id_placeholder'),
              }}
              secret={{
                label: 'Client Secret',
                value: clientSecret,
                onChange: setClientSecret,
                placeholder: t('channel.dingtalk.client_secret_placeholder'),
              }}
              showSecret={showSecret}
              onToggleSecret={() => setShowSecret(!showSecret)}
              cancelLabel={t('channel.cancel')}
              submitLabel={t('channel.connect')}
              onCancel={handleCancel}
              onSubmit={handleStreamConnect}
              submitting={submitting}
              canSubmit={Boolean(!submitting && clientId.trim() && clientSecret.trim())}
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
