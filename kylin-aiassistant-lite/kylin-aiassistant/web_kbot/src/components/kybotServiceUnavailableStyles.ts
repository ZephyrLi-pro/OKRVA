import type { CSSProperties } from 'react';

/** 与 Skills/Cron 等嵌入页空状态、弹窗共用视口偏移，保证通道/技能/任务/模型一致居中。 */
export const KYBOT_SERVICE_UNAVAILABLE_SHELL: CSSProperties = {
  position: 'fixed',
  top: 0,
  left: 0,
  width: '100vw',
  height: '100vh',
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'center',
  background: '#FFFFFF',
  transform: 'translate(-28px, -40px)',
  zIndex: 1,
};

export const KYBOT_SERVICE_UNAVAILABLE_COLUMN: CSSProperties = {
  display: 'flex',
  flexDirection: 'column',
  alignItems: 'center',
  width: 266,
  gap: 0,
};

export const KYBOT_SERVICE_UNAVAILABLE_IMAGE: CSSProperties = {
  width: 96,
  height: 96,
  display: 'block',
  flexShrink: 0,
  margin: 0,
};

export const KYBOT_SERVICE_UNAVAILABLE_TEXT: CSSProperties = {
  position: 'static',
  margin: 0,
  marginTop: -4,
  width: 266,
  height: 21,
  opacity: 1,
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'center',
  whiteSpace: 'nowrap',
  fontFamily: "'Noto Sans CJK SC', sans-serif",
  fontSize: 14,
  fontWeight: 'normal',
  lineHeight: 'normal',
  letterSpacing: '0em',
  color: 'rgba(0, 0, 0, 0.6)',
  zIndex: 1,
};

export const KYBOT_SERVICE_UNAVAILABLE_BUTTON: CSSProperties = {
  marginTop: 16,
  padding: '6px 16px',
  borderRadius: 6,
  border: 'none',
  background: '#3790FA',
  color: '#FFFFFF',
  cursor: 'pointer',
  fontWeight: 500,
  fontFamily: "'Noto Sans CJK SC', sans-serif",
};

/** KComponent-Disable — 「正在启动…」按钮态 */
export const KYBOT_SERVICE_UNAVAILABLE_BUTTON_STARTING: CSSProperties = {
  ...KYBOT_SERVICE_UNAVAILABLE_BUTTON,
  background: '#EEEEEE',
  color: 'rgba(0, 0, 0, 0.35)',
  cursor: 'not-allowed',
};
