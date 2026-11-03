import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import { isModifiedZoomWheel, isMultiTouchZoom, isZoomShortcut } from './Channel';
import {
  CHANNEL_BIND_MODAL_BASE_HEIGHT,
  CHANNEL_BIND_MODAL_VIEWPORT_MARGIN,
  getChannelBindContentPaddingTop,
  getChannelBindContentStyle,
  getChannelBindModalHeight,
  getChannelBindModalStyle,
} from './channel/channelBindLayout';

const testDir = dirname(fileURLToPath(import.meta.url));

describe('resolveStatus', () => {
  function resolveStatus(configured: boolean): { status: string; statusLabel: string } {
    if (configured) return { status: 'connected', statusLabel: 'connected' };
    return { status: 'unconfigured', statusLabel: 'not_configured' };
  }

  it('returns connected when configured is true', () => {
    const result = resolveStatus(true);
    expect(result.status).toBe('connected');
    expect(result.statusLabel).toBe('connected');
  });

  it('returns unconfigured when configured is false', () => {
    const result = resolveStatus(false);
    expect(result.status).toBe('unconfigured');
    expect(result.statusLabel).toBe('not_configured');
  });
});

describe('channel QR bind layout regression', () => {
  const channelSource = readFileSync(resolve(testDir, 'Channel.tsx'), 'utf8');
  const bindUiSource = readFileSync(resolve(testDir, 'channel/ChannelBindUi.tsx'), 'utf8');
  const qrBindFiles = [
    'WeChatBind.tsx',
    'WeComBind.tsx',
    'DingTalkBind.tsx',
    'FeishuBind.tsx',
  ];
  const allBindFiles = [
    ...qrBindFiles,
    'LansengerBind.tsx',
  ];
  const manualBindFiles = [
    'WeComBind.tsx',
    'DingTalkBind.tsx',
    'FeishuBind.tsx',
    'LansengerBind.tsx',
  ];

  it.each(qrBindFiles)('%s uses the shared WebView-compatible QR panel', (file) => {
    const source = readFileSync(resolve(testDir, file), 'utf8');

    expect(source).toContain("from './channel/ChannelQrBox'");
    expect(source).toContain('ChannelQrPanel');
    expect(source).toContain("import { getChannelBindContentStyle } from './channel/channelBindLayout'");
    expect(source).toContain('<ChannelQrPanel');
    expect(source).toContain('isQrFetchError');
    expect(source).toContain('visible={!isQrFetchError}');
    expect(source).toContain('placeholderSrc={`${assetBasePath}/images/channel-qr-placeholder.png`}');
    expect(source).toContain("t('channel.qr_fetch_failed')");
    expect(source).toContain('contentPaddingTop?: number');
    expect(source).toContain('getChannelBindContentStyle(contentPaddingTop)');
    expect(source).not.toContain('<ChannelQrBox');
    expect(source).not.toContain('<ChannelQrRefreshOverlay');
    expect(source).not.toContain('<ChannelQrFetchFailedText');
    expect(source).not.toContain('absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2');
    expect(source).not.toContain('relative inline-block');
    expect(source).not.toContain('paddingTop: 48');
    expect(source).not.toContain('clamp(');
  });

  it.each(allBindFiles)('%s uses the shared bind shell UI', (file) => {
    const source = readFileSync(resolve(testDir, file), 'utf8');

    expect(source).toContain('ChannelBindHeader');
    expect(source).toContain('ChannelErrorAlert');
    expect(source).toContain('contentPaddingTop?: number');
    expect(source).toContain('getChannelBindContentStyle(contentPaddingTop)');
  });

  it.each(manualBindFiles)('%s uses the shared manual config UI', (file) => {
    const source = readFileSync(resolve(testDir, file), 'utf8');

    expect(source).toContain('ChannelManualConfigForm');
    expect(source).toContain('ChannelBindSuccess');
  });

  it('keeps QR fetch failures switchable to manual connection modes', () => {
    const wecomSource = readFileSync(resolve(testDir, 'WeComBind.tsx'), 'utf8');
    const dingtalkSource = readFileSync(resolve(testDir, 'DingTalkBind.tsx'), 'utf8');
    const feishuSource = readFileSync(resolve(testDir, 'FeishuBind.tsx'), 'utf8');

    expect(wecomSource).toContain("phase !== 'error' || mode === 'manual' || isQrFetchError");
    expect(dingtalkSource).toContain("phase !== 'error' || mode === 'stream' || isQrFetchError");
    expect(feishuSource).toContain("phase !== 'error' || mode === 'websocket' || isQrFetchError");
    expect(wecomSource).not.toContain('variant="filled"');
    expect(dingtalkSource).not.toContain('variant="filled"');
    expect(feishuSource).not.toContain('variant="filled"');
    expect(bindUiSource).toContain("border: checked ? '5px solid #3790FA' : '1.5px solid #d0d0d0'");
  });

  it('keeps WeChat polling from repainting unchanged waiting states', () => {
    const wechatSource = readFileSync(resolve(testDir, 'WeChatBind.tsx'), 'utf8');

    expect(wechatSource.match(/setStatusData\(data\)/g)).toHaveLength(1);
    expect(wechatSource).toContain("case 'confirmed':");
    expect(wechatSource).toContain('setStatusData(data);');
    expect(wechatSource).toContain("setPhase((prev) => prev === 'scanned' ? prev : 'scanned')");
    expect(wechatSource).toContain("setPhase((prev) => prev === 'scanned' || prev === 'waiting_scan' ? prev : 'waiting_scan')");
    expect(wechatSource).toContain("case 'expired':");
    expect(wechatSource).toContain('setPhase(\'expired\')');
    expect(wechatSource).toContain('onRefresh={handleFetchQrCode}');
  });

  it('keeps the bind modal layout WebView-compatible', () => {
    expect(getChannelBindModalHeight(441)).toBe(409);
    expect(getChannelBindModalHeight(900)).toBe(CHANNEL_BIND_MODAL_BASE_HEIGHT);
    expect(getChannelBindModalHeight(0)).toBe(CHANNEL_BIND_MODAL_BASE_HEIGHT);
    expect(getChannelBindModalHeight(40)).toBe(8);
    expect(getChannelBindModalHeight(500)).toBe(500 - CHANNEL_BIND_MODAL_VIEWPORT_MARGIN);

    expect(getChannelBindContentPaddingTop(441)).toBe(28.5);
    expect(getChannelBindContentPaddingTop(300)).toBe(16);
    expect(getChannelBindContentPaddingTop(900)).toBe(48);

    expect(getChannelBindModalStyle(409)).toMatchObject({
      position: 'relative',
      width: 424,
      maxWidth: 'calc(100vw - 32px)',
      height: 409,
      overflowY: 'auto',
      paddingLeft: 24,
      paddingRight: 24,
    });
    expect(getChannelBindContentStyle(28.5)).toMatchObject({
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'center',
      paddingTop: 28.5,
    });

    expect(channelSource).toContain('getChannelBindModalHeight(window.innerHeight, modalBaseHeight)');
    expect(channelSource).toContain('getChannelBindContentPaddingTop(window.innerHeight)');
    expect(channelSource).not.toContain("height: 'min(480px, calc(100vh - 32px))'");
    expect(channelSource).not.toContain('scrollbarGutter');
    expect(channelSource).not.toContain('min-h-[480px]');
    expect(channelSource).not.toContain('max-h-[calc(100vh-32px)]');
  });
});

describe('channel page interaction guards', () => {
  const channelSource = readFileSync(resolve(testDir, 'Channel.tsx'), 'utf8');
  const indexCssSource = readFileSync(resolve(testDir, '../index.css'), 'utf8');
  const i18nSource = readFileSync(resolve(testDir, '../lib/i18n.ts'), 'utf8');
  const bindUiSource = readFileSync(resolve(testDir, 'channel/ChannelBindUi.tsx'), 'utf8');
  const qrBoxSource = readFileSync(resolve(testDir, 'channel/ChannelQrBox.tsx'), 'utf8');

  it('detects browser zoom shortcuts', () => {
    expect(isModifiedZoomWheel({ ctrlKey: true, metaKey: false })).toBe(true);
    expect(isModifiedZoomWheel({ ctrlKey: false, metaKey: true })).toBe(true);
    expect(isModifiedZoomWheel({ ctrlKey: false, metaKey: false })).toBe(false);

    expect(isZoomShortcut({ ctrlKey: true, metaKey: false, key: '+', code: 'Equal' })).toBe(true);
    expect(isZoomShortcut({ ctrlKey: true, metaKey: false, key: '=', code: 'Equal' })).toBe(true);
    expect(isZoomShortcut({ ctrlKey: true, metaKey: false, key: '-', code: 'Minus' })).toBe(true);
    expect(isZoomShortcut({ ctrlKey: true, metaKey: false, key: '0', code: 'Digit0' })).toBe(true);
    expect(isZoomShortcut({ ctrlKey: false, metaKey: true, key: '+', code: 'Equal' })).toBe(true);
    expect(isZoomShortcut({ ctrlKey: false, metaKey: false, key: '+', code: 'Equal' })).toBe(false);
    expect(isZoomShortcut({ ctrlKey: true, metaKey: false, key: 'r', code: 'KeyR' })).toBe(false);
  });

  it('detects multi-touch pinch gestures', () => {
    expect(isMultiTouchZoom({ touches: { length: 2 } as TouchList })).toBe(true);
    expect(isMultiTouchZoom({ touches: { length: 1 } as TouchList })).toBe(false);
  });

  it('registers channel-only right-click and zoom guards', () => {
    expect(channelSource).toContain("document.querySelector<HTMLMetaElement>('meta[name=\"viewport\"]')");
    expect(channelSource).toContain('maximum-scale=1.0, user-scalable=no');
    expect(channelSource).toContain("viewportMeta.setAttribute('content', previousViewportContent)");
    expect(channelSource).toContain("document.addEventListener('contextmenu'");
    expect(channelSource).toContain("document.addEventListener('wheel'");
    expect(channelSource).toContain("document.addEventListener('keydown'");
    expect(channelSource).toContain("document.addEventListener('touchmove'");
    expect(channelSource).toContain("document.addEventListener('gesturestart'");
    expect(channelSource).toContain("document.removeEventListener('contextmenu'");
    expect(channelSource).toContain("document.removeEventListener('wheel'");
    expect(channelSource).toContain("document.removeEventListener('keydown'");
    expect(channelSource).toContain("document.removeEventListener('touchmove'");
    expect(channelSource).toContain("document.removeEventListener('gesturestart'");
  });

  it('adds close tooltips to channel modals', () => {
    expect(channelSource).toContain('function ChannelModalCloseButton');
    expect(channelSource).toContain('function ChannelCloseTooltipLayer');
    expect(channelSource).toContain("const closeTooltip = t('common.close')");
    expect(channelSource.match(/<ChannelModalCloseButton/g)).toHaveLength(2);
    expect(channelSource).toContain('aria-label={label}');
    expect(channelSource).toContain('const CHANNEL_CLOSE_TOOLTIP_DELAY_MS = 1000');
    expect(channelSource).toContain('window.setTimeout(() => {');
    expect(channelSource).toContain('CHANNEL_CLOSE_TOOLTIP_DELAY_MS');
    expect(channelSource).toContain('const [isCloseHovered, setIsCloseHovered] = useState(false)');
    expect(channelSource).toContain('const [isClosePressed, setIsClosePressed] = useState(false)');
    expect(channelSource).toContain("background: isClosePressed ? '#D93030' : isCloseHovered ? '#F53F3F' : 'transparent'");
    expect(channelSource).toContain("color: isCloseHovered || isClosePressed ? '#FFFFFF' : '#000000'");
    expect(channelSource).toContain('ref={channelPageRef}');
    expect(channelSource).toContain('<ChannelCloseTooltipLayer tooltip={closeTooltip} />');
    expect(channelSource).toContain('clientX - (rect?.left ?? 0) + CHANNEL_CLOSE_TOOLTIP_OFFSET_X');
    expect(channelSource).toContain('clientY - (rect?.top ?? 0) + CHANNEL_CLOSE_TOOLTIP_OFFSET_Y');
    expect(channelSource).toContain('if (!tooltipVisibleRef.current && tooltipTimerRef.current !== null)');
    expect(channelSource).toContain("position: 'absolute'");
    expect(channelSource).toContain('left: tooltip.left');
    expect(channelSource).toContain('top: tooltip.top');
    expect(channelSource).not.toContain('createPortal');
    expect(channelSource).not.toContain('title={closeTooltip}');
    expect(channelSource).not.toContain("e.currentTarget.style.background = '#F53F3F'");
    expect(channelSource).not.toContain("e.currentTarget.style.background = '#D93030'");
  });

  it('keeps configure button background states explicit', () => {
    expect(channelSource).toContain("const CHANNEL_CONFIG_BUTTON_BG = '#E5E5E5'");
    expect(channelSource).toContain("const CHANNEL_CONFIG_BUTTON_HOVER_BG = '#D9D9D9'");
    expect(channelSource).toContain("const CHANNEL_CONFIG_BUTTON_ACTIVE_BG = '#CCCCCC'");
    expect(channelSource).toContain("const CHANNEL_DISCONNECT_BUTTON_BG = '#F53F3F'");
    expect(channelSource).toContain("const CHANNEL_DISCONNECT_BUTTON_HOVER_BG = '#D93030'");
    expect(channelSource).toContain("const CHANNEL_DISCONNECT_BUTTON_ACTIVE_BG = '#B82020'");
    expect(channelSource).toContain('onMouseDown={(e) => {');
    expect(channelSource).toContain('onMouseUp={(e) => {');
  });

  it('uses configure button states for the connected modal confirm button', () => {
    expect(channelSource).toContain('const [isConfirmHovered, setIsConfirmHovered] = useState(false)');
    expect(channelSource).toContain('const [isConfirmPressed, setIsConfirmPressed] = useState(false)');
    expect(channelSource).toContain('const confirmBackground = isConfirmPressed');
    expect(channelSource).toContain('? CHANNEL_CONFIG_BUTTON_ACTIVE_BG');
    expect(channelSource).toContain('? CHANNEL_CONFIG_BUTTON_HOVER_BG');
    expect(channelSource).toContain(': CHANNEL_CONFIG_BUTTON_BG');
    expect(channelSource).toContain('background: confirmBackground');
    expect(channelSource).toContain('setIsConfirmHovered(true)');
    expect(channelSource).toContain('setIsConfirmPressed(true)');
  });

  it('uses deeper red states for the connected modal disconnect button', () => {
    expect(channelSource).toContain('const [isDisconnectHovered, setIsDisconnectHovered] = useState(false)');
    expect(channelSource).toContain('const [isDisconnectPressed, setIsDisconnectPressed] = useState(false)');
    expect(channelSource).toContain('const disconnectBackground = disconnecting');
    expect(channelSource).toContain('? CHANNEL_DISCONNECT_BUTTON_ACTIVE_BG');
    expect(channelSource).toContain('? CHANNEL_DISCONNECT_BUTTON_HOVER_BG');
    expect(channelSource).toContain(': CHANNEL_DISCONNECT_BUTTON_BG');
    expect(channelSource).toContain('background: disconnectBackground');
    expect(channelSource).toContain('setIsDisconnectHovered(true)');
    expect(channelSource).toContain('setIsDisconnectPressed(true)');
  });

  it('centers success toast with Chromium 69-compatible transform', () => {
    expect(channelSource).toContain("position: 'fixed'");
    expect(channelSource).toContain("left: '50%'");
    expect(channelSource).toContain("top: '50%'");
    expect(channelSource).toContain("transform: 'translate(-50%, -50%)'");
    expect(channelSource).toContain("animation: 'channelToastFadeInScale 0.3s ease-out both'");
    expect(channelSource).not.toContain('left-1/2 top-1/2 -translate-x-1/2 -translate-y-1/2');
    expect(indexCssSource).toContain('@keyframes channelToastFadeInScale');
    expect(indexCssSource).toContain('transform: translate(-50%, -50%) scale(1)');
  });

  it('centers connected modal with the shared WebView-compatible modal styles', () => {
    expect(channelSource).toContain('const CHANNEL_CONNECTED_MODAL_HEIGHT = 298');
    expect(channelSource).toContain('style={getChannelBindOverlayStyle()}');
    expect(channelSource).toContain('...getChannelBindModalStyle(CHANNEL_CONNECTED_MODAL_HEIGHT)');
    expect(channelSource).toContain('paddingTop: 32');
    expect(channelSource).not.toContain('fixed inset-0 z-50 flex items-center justify-center p-4 animate-fade-in');
    expect(channelSource).not.toContain('pt-8');
  });

  it('keeps the original two-column channel card grid layout', () => {
    expect(channelSource).toContain('grid grid-cols-2 gap-x-2 gap-y-2 stagger-children');
    expect(channelSource).toContain('borderRadius: 8');
    expect(channelSource).not.toContain('getChannelCardBorderRadius(idx, channels.length)');
    expect(channelSource).not.toContain('const columns = 2');
    expect(channelSource).not.toContain('gridTemplateColumns');
    expect(channelSource).not.toContain("display: 'table'");
    expect(channelSource).not.toContain('auto-fit');
  });

  it('uses requested channel card typography and status visibility', () => {
    expect(channelSource).toContain('fontWeight: 500');
    expect(channelSource).toContain('fontWeight: 400');
    expect(channelSource).toContain("channel.status === 'connected' ? (");
    expect(channelSource).toContain("{t('channel.connected')}");
    expect(channelSource).not.toContain('font-semibold');
    expect(channelSource).not.toContain('{s.label}');
    expect(channelSource).not.toContain("t('channel.not_configured')");
  });

  it('keeps channel cards borderless with full rounded corners', () => {
    expect(channelSource).toContain(
      "const cornerStyle: React.CSSProperties = {\n            background: '#F6F6F6',\n            borderRadius: 8,",
    );
    expect(channelSource).not.toContain(
      "const cornerStyle: React.CSSProperties = {\n            background: '#F6F6F6',\n            border: '1px solid var(--pc-border)'",
    );
    expect(channelSource).not.toContain('getChannelCardBorderRadius');
  });

  it('uses the updated Lansenger Chinese channel description', () => {
    expect(i18nSource).toContain(
      "'channel.lansenger.desc': '关联蓝信工作账号，在安全通信环境中直接调用小K，快速处理协同办公与任务指令'",
    );
  });

  it('avoids flex gap in channel route spacing for Chromium 69', () => {
    expect(channelSource).toContain('paddingLeft: 20');
    expect(channelSource).toContain('paddingRight: 20');
    expect(channelSource).toContain('marginRight: 16');
    expect(channelSource).toContain('marginLeft: 8');
    expect(channelSource).toContain('marginRight: 12');
    expect(channelSource).not.toContain('px-5 animate-slide-in-up');
    expect(channelSource).not.toContain('flex items-center gap-4');
    expect(channelSource).not.toContain('flex items-center gap-2');
    expect(channelSource).not.toContain('justify-end gap-3');
    expect(channelSource).not.toContain('items-center gap-2 rounded-lg');
  });

  it('avoids flex gap in channel bind modal spacing for Chromium 69', () => {
    expect(bindUiSource).toContain('marginRight: index === options.length - 1 ? 0 : 24');
    expect(bindUiSource).toContain('marginRight: compact ? 8 : 12');
    expect(bindUiSource).toContain('paddingLeft: 12');
    expect(bindUiSource).toContain('paddingRight: 12');
    expect(bindUiSource).not.toContain('gap: 16');
    expect(bindUiSource).not.toContain('gap: compact ? 8 : 12');
    expect(bindUiSource).not.toContain('gap-2');
    expect(bindUiSource).not.toContain('gap-3');
    expect(bindUiSource).not.toContain('gap-4');
    expect(qrBoxSource).toContain('marginTop: 4');
    expect(qrBoxSource).toContain('marginTop: 8');
    expect(qrBoxSource).not.toContain('gap: 4');
    expect(qrBoxSource).not.toContain('gap-3');
  });
});

describe('CHANNEL_KEY_MAP', () => {
  const CHANNEL_KEY_MAP: Record<string, string> = {
    WeChat: 'wechat',
    WeCom: 'wecom',
    DingTalk: 'dingtalk',
    Feishu: 'feishu',
    Lansenger: 'lansenger',
  };

  it('maps WeChat to wechat', () => {
    expect(CHANNEL_KEY_MAP['WeChat']).toBe('wechat');
  });

  it('maps WeCom to wecom', () => {
    expect(CHANNEL_KEY_MAP['WeCom']).toBe('wecom');
  });

  it('maps DingTalk to dingtalk', () => {
    expect(CHANNEL_KEY_MAP['DingTalk']).toBe('dingtalk');
  });

  it('maps Feishu to feishu', () => {
    expect(CHANNEL_KEY_MAP['Feishu']).toBe('feishu');
  });

  it('maps Lansenger to lansenger', () => {
    expect(CHANNEL_KEY_MAP['Lansenger']).toBe('lansenger');
  });

  it('returns undefined for lowercase keys', () => {
    expect(CHANNEL_KEY_MAP['wechat']).toBeUndefined();
    expect(CHANNEL_KEY_MAP['wecom']).toBeUndefined();
    expect(CHANNEL_KEY_MAP['dingtalk']).toBeUndefined();
    expect(CHANNEL_KEY_MAP['feishu']).toBeUndefined();
    expect(CHANNEL_KEY_MAP['lansenger']).toBeUndefined();
  });

  it('has exactly 5 entries', () => {
    expect(Object.keys(CHANNEL_KEY_MAP)).toHaveLength(5);
  });
});

describe('status mapping from backend', () => {
  const CHANNEL_KEY_MAP: Record<string, string> = {
    WeChat: 'wechat',
    WeCom: 'wecom',
    DingTalk: 'dingtalk',
    Feishu: 'feishu',
    Lansenger: 'lansenger',
  };

  function mapChannels(
    backendChannels: Record<string, boolean>,
  ): Record<string, boolean> {
    const result: Record<string, boolean> = {
      wechat: false,
      wecom: false,
      dingtalk: false,
      feishu: false,
      lansenger: false,
    };
    for (const [backendKey, configured] of Object.entries(backendChannels)) {
      const mapped = CHANNEL_KEY_MAP[backendKey];
      if (mapped) result[mapped] = configured;
    }
    return result;
  }

  it('maps all five channels when all are configured', () => {
    const backend = {
      WeChat: true,
      WeCom: true,
      DingTalk: true,
      Feishu: true,
      Lansenger: true,
    };
    const result = mapChannels(backend);
    expect(result.wechat).toBe(true);
    expect(result.wecom).toBe(true);
    expect(result.dingtalk).toBe(true);
    expect(result.feishu).toBe(true);
    expect(result.lansenger).toBe(true);
  });

  it('maps correctly when some channels are unconfigured', () => {
    const backend = {
      WeChat: true,
      WeCom: false,
      DingTalk: true,
      Feishu: false,
      Lansenger: true,
    };
    const result = mapChannels(backend);
    expect(result.wechat).toBe(true);
    expect(result.wecom).toBe(false);
    expect(result.dingtalk).toBe(true);
    expect(result.feishu).toBe(false);
    expect(result.lansenger).toBe(true);
  });

  it('ignores unknown channel keys', () => {
    const backend = {
      WeChat: true,
      Telegram: true,
      Slack: false,
    };
    const result = mapChannels(backend);
    expect(result.wechat).toBe(true);
    expect(result.wecom).toBe(false);
  });

  it('handles empty backend channels', () => {
    const result = mapChannels({});
    expect(result.wechat).toBe(false);
    expect(result.wecom).toBe(false);
    expect(result.dingtalk).toBe(false);
    expect(result.feishu).toBe(false);
    expect(result.lansenger).toBe(false);
  });
});

describe('saveXxxConfig API endpoint regression', () => {
  const apiSource = readFileSync(resolve(testDir, '../lib/api.ts'), 'utf8');

  it('uses dedicated channel config endpoints instead of PUT /api/config', () => {
    expect(apiSource).toContain("apiFetch<void>('/api/feishu/config'");
    expect(apiSource).toContain("apiFetch<void>('/api/dingtalk/config'");
    expect(apiSource).toContain("apiFetch<void>('/api/wecom/config'");
    expect(apiSource).not.toContain('const feishuSection =');
    expect(apiSource).not.toContain('const dingtalkSection =');
    expect(apiSource).not.toContain('const wecomSection =');
  });

  it('keeps manual bind request field names aligned with gateway handlers', () => {
    expect(apiSource).toContain('app_id: appId');
    expect(apiSource).toContain('app_secret: appSecret');
    expect(apiSource).toContain('client_id: clientId');
    expect(apiSource).toContain('client_secret: clientSecret');
    expect(apiSource).toContain('bot_id: botId');
  });
});

describe('disconnectChannel TOML deletion', () => {
  function removeChannelKey(
    config: Record<string, unknown>,
    channelKey: string,
  ): Record<string, unknown> {
    const channels = config.channels as Record<string, unknown> | undefined;
    if (channels && channelKey in channels) {
      const updated = { ...channels };
      delete updated[channelKey];
      return { ...config, channels: updated };
    }
    return config;
  }

  it('removes feishu from channels config', () => {
    const config = {
      channels: {
        feishu: { enabled: true },
        wecom: { enabled: true },
      },
    };
    const result = removeChannelKey(config, 'feishu');
    const channels = result.channels as Record<string, unknown>;
    expect('feishu' in channels).toBe(false);
    expect('wecom' in channels).toBe(true);
  });

  it('removes wecom from channels config', () => {
    const config = {
      channels: {
        feishu: { enabled: true },
        wecom: { enabled: true },
      },
    };
    const result = removeChannelKey(config, 'wecom');
    const channels = result.channels as Record<string, unknown>;
    expect('wecom' in channels).toBe(false);
    expect('feishu' in channels).toBe(true);
  });

  it('does nothing when channel key does not exist', () => {
    const config = {
      channels: {
        feishu: { enabled: true },
      },
    };
    const result = removeChannelKey(config, 'dingtalk');
    const channels = result.channels as Record<string, unknown>;
    expect('feishu' in channels).toBe(true);
  });

  it('does nothing when channels is undefined', () => {
    const config = {};
    const result = removeChannelKey(config, 'feishu');
    expect(result).toEqual({});
  });
});

describe('getChannelIdentifier logic', () => {
  function extractChannelId(
    channelKey: string,
    channels: Record<string, Record<string, unknown>> | undefined,
  ): string {
    const section = channels?.[channelKey];
    switch (channelKey) {
      case 'wecom':
        return String(section?.bot_id ?? '—');
      case 'dingtalk':
        return String(section?.client_id ?? '—');
      case 'feishu':
      case 'lansenger':
        return String(section?.app_id ?? '—');
      default:
        return '—';
    }
  }

  it('extracts bot_id for wecom', () => {
    const channels = { wecom: { bot_id: 'bot_abc' } };
    expect(extractChannelId('wecom', channels)).toBe('bot_abc');
  });

  it('extracts client_id for dingtalk', () => {
    const channels = { dingtalk: { client_id: 'client_xyz' } };
    expect(extractChannelId('dingtalk', channels)).toBe('client_xyz');
  });

  it('extracts app_id for feishu', () => {
    const channels = { feishu: { app_id: 'app_123' } };
    expect(extractChannelId('feishu', channels)).toBe('app_123');
  });

  it('extracts app_id for lansenger', () => {
    const channels = { lansenger: { app_id: 'lanxin_app' } };
    expect(extractChannelId('lansenger', channels)).toBe('lanxin_app');
  });

  it('returns placeholder when channel not configured', () => {
    expect(extractChannelId('wecom', {})).toBe('—');
    expect(extractChannelId('dingtalk', undefined)).toBe('—');
    expect(extractChannelId('feishu', {})).toBe('—');
    expect(extractChannelId('lansenger', {})).toBe('—');
  });

  it('returns placeholder when field is missing', () => {
    const channels = { wecom: { enabled: true } };
    expect(extractChannelId('wecom', channels)).toBe('—');
  });
});
