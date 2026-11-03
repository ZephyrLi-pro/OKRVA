import type { CSSProperties, ReactNode, RefObject } from 'react';

export const QR_CANVAS_SIZE = 160;
export const QR_FRAME_SIZE = 176;

interface ChannelQrBoxProps {
  canvasRef: RefObject<HTMLCanvasElement | null>;
  visible: boolean;
  dimmed: boolean;
  placeholderSrc?: string;
  clickable?: boolean;
  onClick?: () => void;
  children?: ReactNode;
}

export function getChannelQrFrameStyle(): CSSProperties {
  return {
    position: 'relative',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    background: '#fff',
    border: '1px solid rgba(0, 0, 0, 0.1)',
    borderRadius: 6,
    width: QR_FRAME_SIZE,
    height: QR_FRAME_SIZE,
    overflow: 'hidden',
    cursor: 'default',
  };
}

export function getChannelQrCanvasStyle(visible: boolean, dimmed: boolean): CSSProperties {
  return {
    position: 'static',
    width: QR_CANVAS_SIZE,
    height: QR_CANVAS_SIZE,
    display: 'block',
    visibility: visible ? 'visible' : 'hidden',
    filter: dimmed ? 'blur(4px) opacity(0.12)' : 'none',
  };
}

export function getChannelQrPlaceholderStyle(src: string): CSSProperties {
  return {
    position: 'absolute',
    top: 0,
    right: 0,
    bottom: 0,
    left: 0,
    borderRadius: 6,
    backgroundImage: `linear-gradient(0deg, rgba(255,255,255,0.9), rgba(255,255,255,0.9)), url(${src})`,
    backgroundSize: 'cover',
    backgroundPosition: 'center',
    filter: 'blur(5px)',
    zIndex: 0,
  };
}

export function getChannelQrRefreshOverlayStyle(): CSSProperties {
  return {
    position: 'absolute',
    left: 53,
    top: 67,
    zIndex: 2,
    width: 70,
    height: 41,
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
  };
}

export function getChannelQrRefreshTextStyle(): CSSProperties {
  return {
    width: 70,
    height: 21,
    fontFamily: "'Noto Sans CJK SC', sans-serif",
    fontSize: 14,
    fontWeight: 400,
    lineHeight: '21px',
    color: 'rgba(0, 0, 0, 0.85)',
    textAlign: 'center',
    marginTop: 4,
  };
}

export function ChannelQrRefreshIcon() {
  return (
    <svg width="16" height="16" viewBox="0 0 16 16" fill="none" aria-hidden="true">
      <path
        fillRule="evenodd"
        clipRule="evenodd"
        d="M11.89999962779541,6.4000000907348635L10.59999942779541,6.4000000907348635L10.09999942779541,6.4000000907348635L10.09999942779541,5.4000000907348635L10.59999942779541,5.4000000907348635L11.89999962779541,5.4000000907348635L12.69999982779541,5.4000000907348635L12.69999982779541,3.3000001907348633L12.69999982779541,2.8000001907348633L13.69999982779541,2.8000001907348633L13.69999982779541,3.3000001907348633L13.69999982779541,4.600000390734863L13.69999982779541,5.700000290734863C13.69999982779541,5.9000000907348635,13.59999942779541,6.100000390734863,13.50000002779541,6.200000290734863C13.39999962779541,6.300000190734863,13.19999982779541,6.4000000907348635,13.00000002779541,6.4000000907348635L11.89999962779541,6.4000000907348635Z"
        fill="#1A1A1A"
      />
      <path
        fillRule="evenodd"
        clipRule="evenodd"
        d="M7.499999523162842,3.00000018Q10.380733923162841,3.00000024,12.087978523162842,5.4832643999999995L12.088060523162842,5.4832076999999995Q12.158005523162842,5.5849452,12.267271523162842,5.6424224Q12.376537523162842,5.6998997,12.499999523162842,5.6998999Q12.549234523162841,5.6998997,12.597525523162842,5.6902943Q12.645814523162842,5.6806889,12.691302523162841,5.6618469000000005Q12.736789523162841,5.6430049,12.777728523162843,5.6156511Q12.818665523162842,5.5882971,12.853481523162841,5.553482300000001Q12.888296523162841,5.518667199999999,12.915649523162841,5.4777287999999995Q12.943004523162841,5.4367906999999995,12.961845523162841,5.3913025999999995Q12.980687523162842,5.3458147,12.990293523162842,5.2975252Q12.999898523162841,5.2492354,12.999899523162842,5.1999998000000005Q12.999899523162842,5.0447361,12.911938523162842,4.9167924L12.912021523162842,4.9167354Q10.906764523162842,1.99999994,7.499999523162842,2.00000018Q4.8178928231628415,2,2.908946423162842,3.9089465Q0.9999994631628418,5.8178933,0.9999995231628418,8.5Q0.9999995231628418,11.182107,2.9089463231628416,13.091053Q4.817892523162842,15,7.499999523162842,15Q9.929027123162841,15,11.752977523162842,13.414899Q13.568589523162842,11.8370476,13.895309523162842,9.4683189L13.899999523162842,9.43432L13.899999523162842,9.399999600000001Q13.899999523162842,9.349510200000001,13.940289523162841,9.148057900000001Q13.999999523162842,8.849509699999999,13.999999523162842,8.6999998L13.999899523162842,8.6999998Q13.999898523162841,8.6507635,13.990293523162842,8.602473700000001Q13.980687523162842,8.554184,13.961844523162842,8.5086961Q13.943003523162842,8.4632082,13.915648523162842,8.422269799999999Q13.888295523162842,8.3813314,13.853480523162842,8.346516600000001Q13.818665523162842,8.3117018,13.777728523162843,8.284348Q13.736789523162841,8.2569938,13.691301523162842,8.238152Q13.645814523162842,8.219310799999999,13.597525523162842,8.2097054Q13.549235523162842,8.2000999,13.499999523162842,8.2000999Q13.450763523162841,8.2000999,13.402471523162841,8.2097054Q13.354182523162843,8.219310799999999,13.308694523162842,8.238152Q13.263205523162842,8.2569938,13.222268523162843,8.284347499999999Q13.181331523162841,8.3117018,13.146516523162841,8.346516600000001Q13.111701523162841,8.3813314,13.084347523162842,8.422269799999999Q13.056992523162842,8.4632077,13.038150523162841,8.5086956Q13.019309523162843,8.554184,13.009704523162842,8.602473700000001Q13.000098523162842,8.6507635,13.000099523162842,8.6999998L12.999999523162842,8.6999998Q12.999999523162842,8.7504902,12.959708523162842,8.9519424Q12.907934523162842,9.2108116,12.901053523162842,9.357626Q12.619300523162842,11.3371658,11.097021623162842,12.6601Q9.555218223162841,14,7.499999523162842,14Q5.232107123162842,14,3.6160531231628417,12.3839455Q1.9999995231628418,10.7678928,1.9999995231628418,8.5Q1.9999995831628419,6.2321072,3.6160531231628417,4.6160533Q5.2321067231628415,3,7.499999523162842,3.00000018Z"
        fill="#1A1A1A"
      />
    </svg>
  );
}

export function ChannelQrRefreshOverlay({ label }: { label: string }) {
  return (
    <div style={getChannelQrRefreshOverlayStyle()}>
      <ChannelQrRefreshIcon />
      <span style={getChannelQrRefreshTextStyle()}>{label}</span>
    </div>
  );
}

export function ChannelQrFetchFailedText({ label }: { label: string }) {
  return (
    <div
      style={{
        marginTop: 8,
        height: 21,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      <span
        style={{
          width: 98,
          height: 21,
          fontFamily: "'Noto Sans CJK SC', sans-serif",
          fontSize: 14,
          fontWeight: 400,
          lineHeight: '21px',
          color: '#F53F3F',
          textAlign: 'center',
        }}
      >
        {label}
      </span>
    </div>
  );
}

export function ChannelQrExpiredText({ label }: { label: string }) {
  return (
    <span style={{ fontSize: 14, color: '#F53F3F', fontWeight: 400 }}>
      {label}
    </span>
  );
}

export function ChannelQrScannedBadge({ label }: { label: string }) {
  return (
    <span
      className="inline-flex items-center rounded-full"
      style={{
        display: 'inline-flex',
        alignItems: 'center',
        background: 'rgba(59,130,246,0.08)',
        color: '#3b82f6',
        border: '1px solid rgba(59,130,246,0.2)',
        fontSize: 14,
        padding: '8px 16px',
        borderRadius: 999,
      }}
    >
      {label}
    </span>
  );
}

export function ChannelQrLoadingSpinner({ color }: { color: string }) {
  return (
    <svg
      data-testid="channel-qr-loading-spinner"
      width="32"
      height="32"
      viewBox="0 0 32 32"
      fill="none"
      aria-hidden="true"
      style={{ width: 32, height: 32, display: 'block' }}
    >
      <circle
        cx="16"
        cy="16"
        r="12"
        stroke={color}
        strokeWidth="3"
        strokeOpacity="0.18"
      />
      <circle
        cx="16"
        cy="16"
        r="12"
        pathLength="100"
        stroke={color}
        strokeWidth="3"
        strokeLinecap="round"
        strokeDasharray="28 72"
        strokeDashoffset="0"
      >
        <animate
          attributeName="stroke-dashoffset"
          values="0;-100"
          dur="0.9s"
          repeatCount="indefinite"
        />
      </circle>
    </svg>
  );
}

interface ChannelQrPanelProps {
  canvasRef: RefObject<HTMLCanvasElement | null>;
  phase: string;
  qrRendered: boolean;
  isQrFetchError: boolean;
  placeholderSrc: string;
  brandColor: string;
  onRefresh: () => void;
  refreshLabel: string;
  fetchFailedLabel: string;
  bindSuccessLabel: string;
  confirmedFontSize?: number;
  statusContent?: ReactNode;
}

export function ChannelQrPanel({
  canvasRef,
  phase,
  qrRendered,
  isQrFetchError,
  placeholderSrc,
  brandColor,
  onRefresh,
  refreshLabel,
  fetchFailedLabel,
  bindSuccessLabel,
  confirmedFontSize = 12,
  statusContent,
}: ChannelQrPanelProps) {
  const shouldShowQrPlaceholder = phase === 'expired' || isQrFetchError;

  return (
    <>
      <ChannelQrBox
        canvasRef={canvasRef}
        visible={qrRendered && phase !== 'loading' && !shouldShowQrPlaceholder}
        dimmed={phase === 'expired' || phase === 'confirmed' || isQrFetchError}
        placeholderSrc={shouldShowQrPlaceholder ? placeholderSrc : undefined}
        clickable={phase === 'loading' || phase === 'expired' || isQrFetchError}
        onClick={onRefresh}
      >
        {phase === 'expired' || isQrFetchError ? (
          <ChannelQrRefreshOverlay label={refreshLabel} />
        ) : (phase === 'loading' || !qrRendered || phase === 'confirmed') && (
          <div
            className="absolute inset-0 flex flex-col items-center justify-center"
            style={{
              position: 'absolute',
              top: 0,
              right: 0,
              bottom: 0,
              left: 0,
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
            }}
          >
            {phase === 'confirmed' ? null : (phase === 'loading' || !qrRendered) ? (
              <ChannelQrLoadingSpinner color={brandColor} />
            ) : (
              <ChannelQrRefreshIcon />
            )}
            {phase === 'confirmed' ? (
              <span
                style={{
                  fontFamily: "'Noto Sans CJK SC', sans-serif",
                  fontSize: confirmedFontSize,
                  fontWeight: 500,
                  color: '#10b981',
                  textAlign: 'center',
                  padding: '0 16px',
                }}
              >
                {bindSuccessLabel}
              </span>
            ) : null}
          </div>
        )}
      </ChannelQrBox>

      {isQrFetchError ? (
        <ChannelQrFetchFailedText label={fetchFailedLabel} />
      ) : (
        <div
          className="flex items-center justify-center"
          style={{
            marginTop: 8,
            height: 28,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
          }}
        >
          {statusContent}
        </div>
      )}
    </>
  );
}

export default function ChannelQrBox({
  canvasRef,
  visible,
  dimmed,
  placeholderSrc,
  clickable = false,
  onClick,
  children,
}: ChannelQrBoxProps) {
  return (
    <div
      data-testid="channel-qr-frame"
      style={{
        ...getChannelQrFrameStyle(),
        cursor: clickable ? 'pointer' : 'default',
      }}
      onClick={clickable ? onClick : undefined}
    >
      {placeholderSrc ? <div data-testid="channel-qr-placeholder" style={getChannelQrPlaceholderStyle(placeholderSrc)} /> : null}
      <canvas
        ref={canvasRef}
        data-testid="channel-qr-canvas"
        width={QR_CANVAS_SIZE}
        height={QR_CANVAS_SIZE}
        style={getChannelQrCanvasStyle(visible, dimmed)}
      />
      {children}
    </div>
  );
}
