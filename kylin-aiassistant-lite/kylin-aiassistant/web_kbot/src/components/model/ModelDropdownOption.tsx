import type { CSSProperties, ReactNode } from 'react';
import { assetBasePath } from '@/lib/basePath';

export const MODEL_DROPDOWN_OPTION_WIDTH = 304;

const FONT =
  "'Noto Sans CJK SC', 'Source Han Sans SC', 'PingFang SC', 'Microsoft YaHei', sans-serif";

interface ModelDropdownOptionProps {
  selected: boolean;
  onPick: () => void;
  children: ReactNode;
  /** Reserve checkmark column even when unselected (provider list alignment). */
  reserveCheckmarkSlot?: boolean;
  /** 选项铺满下拉面板内容区宽度（任务弹窗等） */
  fullWidth?: boolean;
}

export function ModelDropdownOption({
  selected,
  onPick,
  children,
  reserveCheckmarkSlot = false,
  fullWidth = false,
}: ModelDropdownOptionProps) {
  return (
    <button
      type="button"
      role="option"
      aria-selected={selected}
      onClick={onPick}
      onMouseDown={(e) => e.preventDefault()}
      className={`model-dialog-dropdown-option box-border flex cursor-default flex-row flex-nowrap items-center overflow-hidden border-none outline-none ${fullWidth ? 'w-full max-w-full' : 'shrink-0'}`}
      style={{
        width: fullWidth ? '100%' : MODEL_DROPDOWN_OPTION_WIDTH,
        height: 36,
        paddingLeft: 8,
        paddingRight: 8,
        borderRadius: 4,
        color: 'rgba(0, 0, 0, 0.85)',
        fontFamily: FONT,
        fontSize: 14,
        fontWeight: 400,
        boxSizing: 'border-box',
      } as CSSProperties}
    >
      <span
        className="inline-flex shrink-0 items-center justify-center"
        style={{ width: 16, height: 16, flexShrink: 0 }}
      >
        {selected ? (
          <img
            src={`${assetBasePath}/icons/object-select-symbolic.svg`}
            alt=""
            width={16}
            height={16}
            className="block object-contain"
            draggable={false}
          />
        ) : reserveCheckmarkSlot ? (
          <span className="block" style={{ width: 16, height: 16 }} aria-hidden />
        ) : null}
      </span>
      <span
        className="flex min-w-0 flex-1 flex-row flex-nowrap items-center truncate text-left"
        style={{ marginLeft: 8 }}
      >
        {children}
      </span>
    </button>
  );
}
