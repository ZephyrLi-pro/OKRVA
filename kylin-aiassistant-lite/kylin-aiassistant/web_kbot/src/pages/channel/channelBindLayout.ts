import type { CSSProperties } from 'react';

export const CHANNEL_BIND_MODAL_BASE_HEIGHT = 480;
export const CHANNEL_BIND_MODAL_VIEWPORT_MARGIN = 32;
export const CHANNEL_BIND_CONTENT_BASE_HEIGHT = 384;
export const CHANNEL_BIND_CONTENT_PADDING_TOP_MIN = 16;
export const CHANNEL_BIND_CONTENT_PADDING_TOP_MAX = 48;

function clampNumber(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

export function getChannelBindModalHeight(viewportHeight: number, baseHeight: number = CHANNEL_BIND_MODAL_BASE_HEIGHT): number {
  if (!Number.isFinite(viewportHeight) || viewportHeight <= 0) {
    return baseHeight;
  }

  return Math.max(
    0,
    Math.min(baseHeight, viewportHeight - CHANNEL_BIND_MODAL_VIEWPORT_MARGIN),
  );
}

export function getChannelBindContentPaddingTop(viewportHeight: number): number {
  if (!Number.isFinite(viewportHeight) || viewportHeight <= 0) {
    return CHANNEL_BIND_CONTENT_PADDING_TOP_MAX;
  }

  return clampNumber(
    (viewportHeight - CHANNEL_BIND_CONTENT_BASE_HEIGHT) / 2,
    CHANNEL_BIND_CONTENT_PADDING_TOP_MIN,
    CHANNEL_BIND_CONTENT_PADDING_TOP_MAX,
  );
}

export function getChannelBindOverlayStyle(): CSSProperties {
  return {
    position: 'fixed',
    top: 0,
    right: 0,
    bottom: 0,
    left: 0,
    zIndex: 50,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    padding: 16,
    background: 'transparent',
  };
}

export function getChannelBindModalStyle(height: number): CSSProperties {
  return {
    position: 'relative',
    top: -38,
    marginLeft: -28,
    width: 424,
    maxWidth: 'calc(100vw - 32px)',
    height,
    overflowY: 'auto',
    borderRadius: 8,
    border: '1px solid #e8e8e8',
    boxShadow: '0 4px 16px rgba(0,0,0,0.12)',
    paddingLeft: 24,
    paddingRight: 24,
    background: '#FFFFFF',
  };
}

export function getChannelBindContentStyle(contentPaddingTop?: number): CSSProperties {
  return {
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
    fontFamily: "'Noto Sans CJK SC', sans-serif",
    paddingTop: contentPaddingTop ?? CHANNEL_BIND_CONTENT_PADDING_TOP_MAX,
  };
}
