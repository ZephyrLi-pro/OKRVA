import { useState } from 'react';
import {
  LANSENGER_DEFAULT_API_GATEWAY_URL,
  saveLansengerConfig,
} from '@/lib/api';
import { useLocale } from '@/lib/i18n';
import {
  ChannelBindHeader,
  ChannelBindSuccess,
  ChannelErrorAlert,
  ChannelManualConfigForm,
} from './channel/ChannelBindUi';
import { getChannelBindContentStyle } from './channel/channelBindLayout';

type BindPhase = 'editing' | 'confirmed' | 'error';

export default function LansengerBind({
  onBindSuccess,
  onClose,
  contentPaddingTop,
}: {
  onBindSuccess?: () => void;
  onClose?: () => void;
  contentPaddingTop?: number;
}) {
  const { t } = useLocale();
  const [phase, setPhase] = useState<BindPhase>('editing');
  const [appId, setAppId] = useState('');
  const [appSecret, setAppSecret] = useState('');
  const [apiGatewayUrl] = useState(LANSENGER_DEFAULT_API_GATEWAY_URL);
  const [showSecret, setShowSecret] = useState(false);
  const [submitting, setSubmitting] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const handleConnect = async () => {
    if (!appId.trim() || !appSecret.trim()) {
      setError(t('channel.lansenger.fill_required'));
      setPhase('error');
      return;
    }

    setSubmitting(true);
    setError(null);
    try {
      await saveLansengerConfig(appId.trim(), appSecret.trim(), apiGatewayUrl.trim());
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
    setPhase('editing');
    onClose?.();
  };

  const canSubmit = Boolean(appId.trim() && appSecret.trim() && !submitting);

  return (
    <div style={getChannelBindContentStyle(contentPaddingTop)}>
      <ChannelBindHeader
        icon="lansenger.svg"
        alt="Lansenger"
        title={t('channel.lansenger')}
        description={t('channel.lansenger.desc')}
        descriptionMaxWidth={376}
      />

      <ChannelErrorAlert message={error} />

      {phase === 'confirmed' ? (
        <ChannelBindSuccess label={t('channel.bind_success')} />
      ) : (
        <ChannelManualConfigForm
          primary={{
            label: 'App ID',
            value: appId,
            onChange: (value) => {
              setAppId(value);
              setError(null);
              setPhase('editing');
            },
            placeholder: t('channel.lansenger.app_id_placeholder'),
          }}
          secret={{
            label: 'App Secret',
            value: appSecret,
            onChange: (value) => {
              setAppSecret(value);
              setError(null);
              setPhase('editing');
            },
            placeholder: t('channel.lansenger.app_secret_placeholder'),
          }}
          showSecret={showSecret}
          onToggleSecret={() => setShowSecret(!showSecret)}
          cancelLabel={t('channel.cancel')}
          submitLabel={t('channel.connect')}
          onCancel={handleCancel}
          onSubmit={handleConnect}
          submitting={submitting}
          canSubmit={canSubmit}
          compactActions
        />
      )}
    </div>
  );
}
