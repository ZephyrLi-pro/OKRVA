import { assetBasePath } from '@/lib/basePath';
import { t } from '@/lib/i18n';
import {
  KYBOT_SERVICE_UNAVAILABLE_BUTTON,
  KYBOT_SERVICE_UNAVAILABLE_BUTTON_STARTING,
  KYBOT_SERVICE_UNAVAILABLE_COLUMN,
  KYBOT_SERVICE_UNAVAILABLE_IMAGE,
  KYBOT_SERVICE_UNAVAILABLE_SHELL,
  KYBOT_SERVICE_UNAVAILABLE_TEXT,
} from '@/components/kybotServiceUnavailableStyles';

interface KybotServiceUnavailableProps {
  starting?: boolean;
  onStart: () => void;
}

export default function KybotServiceUnavailable({ starting = false, onStart }: KybotServiceUnavailableProps) {
  return (
    <div data-testid="kybot-service-unavailable" style={KYBOT_SERVICE_UNAVAILABLE_SHELL}>
      <div style={KYBOT_SERVICE_UNAVAILABLE_COLUMN}>
        <img
          src={`${assetBasePath}/icons/noweb.svg`}
          alt=""
          aria-hidden="true"
          style={KYBOT_SERVICE_UNAVAILABLE_IMAGE}
        />
        <p style={KYBOT_SERVICE_UNAVAILABLE_TEXT}>
          {t('service.not_started_message')}
        </p>
        <button
          type="button"
          onClick={onStart}
          disabled={starting}
          style={starting ? KYBOT_SERVICE_UNAVAILABLE_BUTTON_STARTING : KYBOT_SERVICE_UNAVAILABLE_BUTTON}
        >
          {starting ? t('service.starting') : t('service.start_now')}
        </button>
      </div>
    </div>
  );
}
