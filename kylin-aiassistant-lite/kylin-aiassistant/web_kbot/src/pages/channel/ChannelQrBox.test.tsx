import { createRef } from 'react';
import { render, screen } from '@testing-library/react';
import { describe, expect, it } from 'vitest';
import ChannelQrBox, {
  ChannelQrExpiredText,
  ChannelQrFetchFailedText,
  ChannelQrLoadingSpinner,
  ChannelQrPanel,
  ChannelQrRefreshOverlay,
  ChannelQrScannedBadge,
  QR_CANVAS_SIZE,
  QR_FRAME_SIZE,
  getChannelQrCanvasStyle,
  getChannelQrFrameStyle,
  getChannelQrPlaceholderStyle,
  getChannelQrRefreshOverlayStyle,
  getChannelQrRefreshTextStyle,
} from './ChannelQrBox';

describe('ChannelQrBox layout', () => {
  it('keeps the QR canvas centered with WebView-compatible flex layout and clips overflow', () => {
    const canvasRef = createRef<HTMLCanvasElement>();

    render(
      <ChannelQrBox canvasRef={canvasRef} visible dimmed={false}>
        <div data-testid="overlay" />
      </ChannelQrBox>,
    );

    const frame = screen.getByTestId('channel-qr-frame');
    const canvas = screen.getByTestId('channel-qr-canvas');

    expect(frame).toHaveStyle({
      position: 'relative',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      width: `${QR_FRAME_SIZE}px`,
      height: `${QR_FRAME_SIZE}px`,
      overflow: 'hidden',
    });

    expect(canvas).not.toHaveClass(
      'absolute',
      'top-1/2',
      'left-1/2',
      '-translate-x-1/2',
      '-translate-y-1/2',
    );
    expect(canvas).toHaveAttribute('width', String(QR_CANVAS_SIZE));
    expect(canvas).toHaveAttribute('height', String(QR_CANVAS_SIZE));
    expect(canvas).toHaveStyle({
      position: 'static',
      width: `${QR_CANVAS_SIZE}px`,
      height: `${QR_CANVAS_SIZE}px`,
      visibility: 'visible',
      filter: 'none',
    });
  });

  it('exposes pure style helpers for white-box regression checks', () => {
    expect(getChannelQrFrameStyle()).toMatchObject({
      position: 'relative',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      width: QR_FRAME_SIZE,
      height: QR_FRAME_SIZE,
      overflow: 'hidden',
    });

    expect(getChannelQrCanvasStyle(false, true)).toMatchObject({
      position: 'static',
      width: QR_CANVAS_SIZE,
      height: QR_CANVAS_SIZE,
      display: 'block',
      visibility: 'hidden',
      filter: 'blur(4px) opacity(0.12)',
    });

    expect(getChannelQrPlaceholderStyle('/images/channel-qr-placeholder.png')).toMatchObject({
      position: 'absolute',
      borderRadius: 6,
      backgroundSize: 'cover',
      backgroundPosition: 'center',
      filter: 'blur(5px)',
    });

    expect(getChannelQrRefreshOverlayStyle()).toMatchObject({
      position: 'absolute',
      left: 53,
      top: 67,
      width: 70,
      height: 41,
    });

    expect(getChannelQrRefreshTextStyle()).toMatchObject({
      width: 70,
      height: 21,
      fontSize: 14,
      fontWeight: 400,
      lineHeight: '21px',
      color: 'rgba(0, 0, 0, 0.85)',
      textAlign: 'center',
      marginTop: 4,
    });
  });

  it('renders the design-matched QR failure placeholder and copy', () => {
    const canvasRef = createRef<HTMLCanvasElement>();

    render(
      <>
        <ChannelQrBox
          canvasRef={canvasRef}
          visible={false}
          dimmed
          placeholderSrc="/images/channel-qr-placeholder.png"
        >
          <ChannelQrRefreshOverlay label="刷新二维码" />
        </ChannelQrBox>
        <ChannelQrFetchFailedText label="获取二维码失败" />
        <ChannelQrExpiredText label="二维码已过期" />
        <ChannelQrScannedBadge label="已扫码待确认" />
      </>,
    );

    expect(screen.getByTestId('channel-qr-placeholder')).toHaveStyle({
      filter: 'blur(5px)',
      backgroundSize: 'cover',
      backgroundPosition: 'center',
    });
    expect(screen.getByText('刷新二维码')).toHaveStyle({
      width: '70px',
      height: '21px',
      fontSize: '14px',
      lineHeight: '21px',
      color: 'rgba(0, 0, 0, 0.85)',
    });
    expect(screen.getByText('获取二维码失败')).toHaveStyle({
      width: '98px',
      height: '21px',
      fontSize: '14px',
      lineHeight: '21px',
      color: '#F53F3F',
    });
    expect(screen.getByText('二维码已过期')).toHaveStyle({
      fontSize: '14px',
      color: '#F53F3F',
    });
    expect(screen.getByText('已扫码待确认')).toHaveStyle({
      background: 'rgba(59,130,246,0.08)',
      color: '#3b82f6',
      border: '1px solid rgba(59,130,246,0.2)',
      fontSize: '14px',
    });
  });

  it('uses an inline SVG animation for QR loading instead of Tailwind animate-spin', () => {
    const canvasRef = createRef<HTMLCanvasElement>();

    const { container } = render(
      <>
        <ChannelQrLoadingSpinner color="#3370FF" />
        <ChannelQrPanel
          canvasRef={canvasRef}
          phase="loading"
          qrRendered={false}
          isQrFetchError={false}
          placeholderSrc="/images/channel-qr-placeholder.png"
          brandColor="#3370FF"
          onRefresh={() => {}}
          refreshLabel="刷新二维码"
          fetchFailedLabel="获取二维码失败"
          bindSuccessLabel="绑定成功"
        />
      </>,
    );

    const spinners = screen.getAllByTestId('channel-qr-loading-spinner');
    expect(spinners[0]).not.toHaveClass('animate-spin');
    expect(spinners[0]).toHaveStyle({ width: '32px', height: '32px' });
    expect(container.querySelector('animate[attributeName="stroke-dashoffset"]')).toBeInTheDocument();
  });

  it('keeps showing loading while the QR canvas is not rendered yet', () => {
    const canvasRef = createRef<HTMLCanvasElement>();

    render(
      <ChannelQrPanel
        canvasRef={canvasRef}
        phase="waiting_scan"
        qrRendered={false}
        isQrFetchError={false}
        placeholderSrc="/images/channel-qr-placeholder.png"
        brandColor="#3370FF"
        onRefresh={() => {}}
        refreshLabel="刷新二维码"
        fetchFailedLabel="获取二维码失败"
        bindSuccessLabel="绑定成功"
      />,
    );

    expect(screen.getByTestId('channel-qr-loading-spinner')).toBeInTheDocument();
    expect(screen.queryByText('刷新二维码')).not.toBeInTheDocument();
  });

  it('renders fetch failures through the shared QR panel', () => {
    const canvasRef = createRef<HTMLCanvasElement>();

    render(
      <ChannelQrPanel
        canvasRef={canvasRef}
        phase="error"
        qrRendered={false}
        isQrFetchError
        placeholderSrc="/images/channel-qr-placeholder.png"
        brandColor="#3370FF"
        onRefresh={() => {}}
        refreshLabel="刷新二维码"
        fetchFailedLabel="获取二维码失败"
        bindSuccessLabel="绑定成功"
      />,
    );

    expect(screen.getByTestId('channel-qr-placeholder')).toBeInTheDocument();
    expect(screen.getByText('刷新二维码')).toBeInTheDocument();
    expect(screen.getByText('获取二维码失败')).toBeInTheDocument();
  });

  it('hides the real QR canvas and shows the placeholder when the QR expires', () => {
    const canvasRef = createRef<HTMLCanvasElement>();

    render(
      <ChannelQrPanel
        canvasRef={canvasRef}
        phase="expired"
        qrRendered
        isQrFetchError={false}
        placeholderSrc="/images/channel-qr-placeholder.png"
        brandColor="#3370FF"
        onRefresh={() => {}}
        refreshLabel="刷新二维码"
        fetchFailedLabel="获取二维码失败"
        bindSuccessLabel="绑定成功"
        statusContent={<ChannelQrExpiredText label="二维码已过期，请刷新" />}
      />,
    );

    expect(screen.getByTestId('channel-qr-canvas')).toHaveStyle({
      visibility: 'hidden',
    });
    expect(screen.getByTestId('channel-qr-placeholder')).toBeInTheDocument();
    expect(screen.getByText('刷新二维码')).toBeInTheDocument();
    expect(screen.getByText('二维码已过期，请刷新')).toBeInTheDocument();
  });
});
