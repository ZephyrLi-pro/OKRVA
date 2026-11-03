import { useCallback, useEffect, useLayoutEffect, useRef, useState, type CSSProperties } from 'react';
import { createPortal } from 'react-dom';
import { PRESET_PROVIDERS, type PresetProviderKey, findPresetByKey, getProviderLabel } from './providerMeta';
import { ModelDropdownOption } from './ModelDropdownOption';
import { ProviderIcon } from './ProviderIcon';
import { assetBasePath } from '@/lib/basePath';

interface ProviderPickerProps {
  value: PresetProviderKey;
  onChange: (key: PresetProviderKey) => void;
  disabled?: boolean;
}

const FONT =
  "'Noto Sans CJK SC', 'Source Han Sans SC', 'PingFang SC', 'Microsoft YaHei', sans-serif";

const PROVIDER_ROW_HEIGHT = 36;
const PROVIDER_PANEL_PAD = 16;

function providerDropdownSafeModeEnabled(): boolean {
  if (typeof window === 'undefined') return false;
  return new URLSearchParams(window.location.search).get('providerDropdownSafeMode') === '1';
}

export function ProviderPicker({ value, onChange, disabled }: ProviderPickerProps) {
  const [open, setOpen] = useState(false);
  const [panelStyle, setPanelStyle] = useState<CSSProperties>({});
  const wrapperRef = useRef<HTMLDivElement | null>(null);
  const triggerRef = useRef<HTMLButtonElement | null>(null);
  const panelRef = useRef<HTMLDivElement | null>(null);
  const current = findPresetByKey(value);
  const providerDropdownSafeMode = providerDropdownSafeModeEnabled();

  const updatePanelPosition = useCallback(() => {
    if (!providerDropdownSafeMode) return;
    const trigger = triggerRef.current;
    if (!trigger) return;

    const rect = trigger.getBoundingClientRect();
    const gap = 4;
    const panelEstHeight = PRESET_PROVIDERS.length * PROVIDER_ROW_HEIGHT + PROVIDER_PANEL_PAD;
    const spaceBelow = window.innerHeight - rect.bottom - gap;
    const spaceAbove = rect.top - gap;
    const shouldDropUp = spaceBelow < panelEstHeight && spaceAbove >= spaceBelow;
    const top = shouldDropUp
      ? Math.max(8, rect.top - panelEstHeight - gap)
      : rect.bottom + gap;

    setPanelStyle({
      position: 'fixed',
      top,
      left: rect.left,
      width: rect.width,
      zIndex: 10100,
    });
  }, [providerDropdownSafeMode]);

  const toggleOpen = useCallback(() => {
    if (disabled) return;
    setOpen((wasOpen) => {
      if (!wasOpen && providerDropdownSafeMode) updatePanelPosition();
      return !wasOpen;
    });
  }, [disabled, providerDropdownSafeMode, updatePanelPosition]);

  useEffect(() => {
    if (disabled) setOpen(false);
  }, [disabled]);

  useLayoutEffect(() => {
    if (!open || !providerDropdownSafeMode) return;
    updatePanelPosition();
    const close = () => setOpen(false);
    window.addEventListener('resize', close);
    window.addEventListener('scroll', close, true);
    return () => {
      window.removeEventListener('resize', close);
      window.removeEventListener('scroll', close, true);
    };
  }, [open, providerDropdownSafeMode, updatePanelPosition]);

  useEffect(() => {
    if (!open) return;
    const handler = (e: MouseEvent) => {
      const target = e.target as Node | null;
      if (
        panelRef.current?.contains(target) ||
        triggerRef.current?.contains(target) ||
        wrapperRef.current?.contains(target)
      ) {
        return;
      }
      setOpen(false);
    };
    const closeOnBlur = () => setOpen(false);
    document.addEventListener('mousedown', handler);
    window.addEventListener('blur', closeOnBlur);
    return () => {
      document.removeEventListener('mousedown', handler);
      window.removeEventListener('blur', closeOnBlur);
    };
  }, [open]);

  const renderProviderOptions = (fullWidth: boolean) =>
    PRESET_PROVIDERS.map((p) => (
      <ModelDropdownOption
        key={p.key}
        selected={p.key === value}
        reserveCheckmarkSlot
        fullWidth={fullWidth}
        onPick={() => {
          onChange(p.key);
          setOpen(false);
        }}
      >
        <span
          style={{
            display: 'flex',
            flexShrink: 0,
            alignItems: 'center',
          }}
        >
          <ProviderIcon meta={p} bundledIcon={p.iconAsset} size={16} />
        </span>
        <span
          className="min-w-0 flex-1 truncate whitespace-nowrap text-left"
          style={{
            marginLeft: 8,
            fontFamily: FONT,
            fontSize: 14,
            fontWeight: 400,
            color: 'rgba(0,0,0,0.85)',
          }}
        >
          {getProviderLabel(p)}
        </span>
      </ModelDropdownOption>
    ));

  const safeProviderPanel = (
    <div
      ref={panelRef}
      className="model-dialog-dropdown model-dialog-provider-dropdown box-border border border-solid"
      style={{
        ...panelStyle,
        padding: '8px 6px 8px 8px',
        borderRadius: 8,
        borderColor: '#E5E7EB',
        background: '#FFFFFF',
      }}
      onMouseDown={(e) => e.stopPropagation()}
    >
      <div
        role="listbox"
        className="flex w-full flex-col"
        style={{ width: '100%' }}
        onMouseDown={(e) => e.stopPropagation()}
      >
        {renderProviderOptions(true)}
      </div>
    </div>
  );

  const scrollProviderPanel = (
    <div
      ref={panelRef}
      className="model-dialog-dropdown model-dialog-provider-dropdown absolute left-0 z-[120] box-border flex flex-col overflow-hidden border border-solid"
      style={{
        top: 'calc(100% - 36px)',
        width: 328,
        height: 274,
        padding: '8px 6px 8px 8px',
        borderRadius: 8,
        borderColor: '#E5E7EB',
        background: '#FFFFFF',
      }}
    >
      <div
        className="model-dialog-scrollbar min-h-0 flex-1 overscroll-contain"
        role="listbox"
        onMouseDown={(e) => e.stopPropagation()}
      >
        {renderProviderOptions(false)}
      </div>
    </div>
  );

  return (
    <div ref={wrapperRef} className="relative w-full" style={{ fontFamily: FONT }}>
      <button
        ref={triggerRef}
        type="button"
        disabled={disabled}
        onClick={toggleOpen}
        aria-expanded={open}
        aria-haspopup="listbox"
        className="w-full box-border cursor-default border-none outline-none transition-colors"
        style={{
          height: 36,
          padding: 8,
          borderRadius: 6,
          background: disabled ? '#E6E6E6' : '#E6E6E6',
          color: 'rgba(0, 0, 0, 0.85)',
          opacity: disabled ? 0.6 : 1,
          display: 'flex',
          flexDirection: 'row',
          flexWrap: 'nowrap',
          alignItems: 'center',
          justifyContent: 'space-between',
          columnGap: 8,
          boxSizing: 'border-box',
        }}
      >
        <span
          className="min-w-0 flex-1"
          style={{
            display: 'flex',
            flexDirection: 'row',
            flexWrap: 'nowrap',
            alignItems: 'center',
            marginRight: 'auto',
          }}
        >
          <ProviderIcon meta={current} bundledIcon={current.iconAsset} size={16} />
          <span
            className="min-w-0 truncate whitespace-nowrap"
            style={{
              fontFamily: FONT,
              fontSize: 14,
              fontWeight: 400,
              marginLeft: 8,
              flexShrink: 1,
              minWidth: 0,
            }}
          >
            {getProviderLabel(current)}
          </span>
        </span>
        <span
          style={{
            display: 'flex',
            flexShrink: 0,
            alignItems: 'center',
            justifyContent: 'center',
            height: 20,
            lineHeight: 0,
          }}
        >
          <img
            src={`${assetBasePath}/icons/${open ? 'ukui-up-symbolic.svg' : 'ukui-down-symbolic.svg'}`}
            alt=""
            width={16}
            height={16}
            className="object-contain"
            draggable={false}
            style={{
              opacity: disabled ? 0.35 : 1,
              display: 'block',
            }}
          />
        </span>
      </button>

      {open &&
        !disabled &&
        (providerDropdownSafeMode
          ? createPortal(safeProviderPanel, document.body)
          : scrollProviderPanel)}
    </div>
  );
}
