import { useEffect, useRef, useState, type CSSProperties } from 'react';
import { assetBasePath } from '@/lib/basePath';
import type { ModelInfo } from '@/lib/models';
import { TruncatedTextTooltip, useModelMoreTooltip } from '@/components/model/modelTooltips';
import { getProviderLabel, resolveProviderMeta } from './providerMeta';
import { t } from '@/lib/i18n';
import { ProviderIcon } from './ProviderIcon';

interface ModelRowProps {
  model: ModelInfo;
  isDefault: boolean;
  onSelect: () => void;
  onEdit: () => void;
  onDelete: () => void;
  /** Segmented corners from merged list shell (models + add row). */
  rowRadiusStyle: CSSProperties;
}

export function ModelRow({
  model,
  isDefault,
  onSelect,
  onEdit,
  onDelete,
  rowRadiusStyle,
}: ModelRowProps) {
  const [menuOpen, setMenuOpen] = useState(false);
  const [moreHover, setMoreHover] = useState(false);
  const [morePressed, setMorePressed] = useState(false);
  const menuRef = useRef<HTMLDivElement | null>(null);
  const moreBtnRef = useRef<HTMLButtonElement | null>(null);
  const actionsRef = useRef<HTMLDivElement | null>(null);
  const { onMouseEnter: onMoreMouseEnter, onMouseMove: onMoreMouseMove, onMouseLeave: onMoreMouseLeave, tooltip: moreTooltip } =
    useModelMoreTooltip(menuOpen);

  const { meta, iconAsset } = resolveProviderMeta(model.provider);

  useEffect(() => {
    if (!menuOpen) return;
    const handler = (e: MouseEvent) => {
      const target = e.target as Node | null;
      if (
        menuRef.current &&
        !menuRef.current.contains(target) &&
        moreBtnRef.current &&
        !moreBtnRef.current.contains(target)
      ) {
        setMenuOpen(false);
      }
    };
    const closeOnBlur = () => setMenuOpen(false);
    document.addEventListener('mousedown', handler);
    window.addEventListener('blur', closeOnBlur);
    return () => {
      document.removeEventListener('mousedown', handler);
      window.removeEventListener('blur', closeOnBlur);
    };
  }, [menuOpen]);

  const rowBg = '#f6f6f6';

  const moreBackground = (() => {
    if (menuOpen) return 'var(--model-alpha-hover)';
    if (morePressed) return 'var(--model-alpha-active)';
    if (moreHover) return 'var(--model-alpha-hover)';
    return 'transparent';
  })();

  const activateRow = () => {
    if (!isDefault) onSelect();
  };

  const handleRowClick = (e: React.MouseEvent) => {
    const target = e.target as Node | null;
    if (actionsRef.current?.contains(target)) return;
    activateRow();
  };

  const handleRowKeyDown = (e: React.KeyboardEvent) => {
    if (isDefault) return;
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      onSelect();
    }
  };

  return (
    <div
      className="relative flex min-w-0 box-border shrink-0 items-center"
      role={isDefault ? undefined : 'button'}
      tabIndex={isDefault ? undefined : 0}
      aria-label={isDefault ? undefined : t('model.set_default')}
      style={{
        ...rowRadiusStyle,
        height: 60,
        paddingLeft: 16,
        paddingRight: 16,
        background: rowBg,
        overflow: 'visible',
        cursor: isDefault ? 'default' : 'pointer',
      }}
      onClick={handleRowClick}
      onKeyDown={handleRowKeyDown}
    >
      {/* 图标…提供商名与中间弹性区：整块可点击以切换默认模型 */}
      <div
        className="flex min-h-0 min-w-0 flex-1 flex-row items-center overflow-hidden"
        style={{ cursor: 'inherit' }}
      >
        <div
          className="flex min-h-0 min-w-0 shrink flex-row items-center overflow-hidden"
          style={{
            width: 'fit-content',
            maxWidth: 'var(--model-row-info-width)',
          }}
        >
          <ProviderIcon meta={meta} bundledIcon={iconAsset} size={24} />

          <TruncatedTextTooltip
            className="min-w-0 shrink"
            text={model.label || model.value}
            style={{
              marginLeft: 8,
              fontFamily: 'var(--model-font-family)',
              fontSize: 'var(--model-font-size)',
              color: 'var(--model-text-primary)',
            }}
          />

          <span
            aria-hidden
            className="shrink-0"
            style={{
              width: 1,
              height: 16,
              marginLeft: 4,
              marginRight: 4,
              background: 'var(--model-divider-bg)',
              opacity: 1,
            }}
          />

          <span
            className="shrink-0 whitespace-nowrap"
            style={{
              fontFamily: 'var(--model-font-family)',
              fontSize: 'var(--model-font-size)',
              color: 'var(--model-text-secondary)',
            }}
          >
            {getProviderLabel(meta)}
          </span>
        </div>

        <div aria-hidden className="min-h-0 min-w-0 flex-1" />
      </div>

      {/* 更多、选中 — 不触发整行切换 */}
      <div
        ref={actionsRef}
        className="flex shrink-0 flex-row items-center"
        onClick={(e) => e.stopPropagation()}
        onMouseDown={(e) => e.stopPropagation()}
      >
        <div className="relative inline-flex shrink-0">
          <button
            ref={moreBtnRef}
            type="button"
            aria-expanded={menuOpen}
            aria-label={t('model.more')}
            className="inline-flex shrink-0 cursor-default items-center justify-center transition-colors border-none"
            style={{
              width: 36,
              height: 36,
              borderRadius: 6,
              background: moreBackground,
            }}
            onMouseEnter={(e) => {
              setMoreHover(true);
              onMoreMouseEnter(e);
            }}
            onMouseMove={onMoreMouseMove}
            onMouseLeave={() => {
              setMoreHover(false);
              setMorePressed(false);
              onMoreMouseLeave();
            }}
            onMouseDown={(e) => {
              e.stopPropagation();
              setMorePressed(true);
            }}
            onMouseUp={() => setMorePressed(false)}
            onClick={(e) => {
              e.stopPropagation();
              setMenuOpen((v) => !v);
            }}
          >
            <img
              src={`${assetBasePath}/icons/view-more-horizontal-symbolic.svg`}
              alt=""
              width={16}
              height={16}
              className="pointer-events-none object-contain select-none"
              draggable={false}
            />
          </button>

          {menuOpen && (
            <div
              ref={menuRef}
              className="model-more-menu-panel absolute z-[100] shrink-0"
              style={{
                top: '100%',
                right: 0,
              }}
              onMouseDown={(e) => e.stopPropagation()}
              onClick={(e) => e.stopPropagation()}
            >
              <button
                type="button"
                className="skill-uninstall-btn"
                style={{ width: 164, height: 36 }}
                onMouseDown={(e) => e.stopPropagation()}
                onClick={() => {
                  setMenuOpen(false);
                  onEdit();
                }}
              >
                {t('model.edit')}
              </button>
              <button
                type="button"
                className="skill-uninstall-btn"
                style={{ width: 164, height: 36 }}
                onMouseDown={(e) => e.stopPropagation()}
                onClick={() => {
                  setMenuOpen(false);
                  onDelete();
                }}
              >
                {t('model.delete')}
              </button>
            </div>
          )}

          {moreTooltip}
        </div>

        <DefaultModelRadioButton isDefault={isDefault} onSelect={onSelect} />
      </div>
    </div>
  );
}

function DefaultModelRadioButton({
  isDefault,
  onSelect,
}: {
  isDefault: boolean;
  onSelect: () => void;
}) {
  const tip = isDefault ? t('model.current_default') : t('model.set_default');
  const iconFile = isDefault ? 'radio-select.svg' : 'radio.svg';
  return (
    <button
      type="button"
      aria-label={tip}
      aria-pressed={isDefault}
      className="inline-flex shrink-0 cursor-default items-center justify-center bg-transparent overflow-visible border-none p-0"
      style={{
        marginLeft: 16,
        marginRight: 0,
        cursor: isDefault ? 'default' : 'pointer',
      }}
      onClick={(e) => {
        e.stopPropagation();
        if (!isDefault) onSelect();
      }}
      onMouseEnter={(e) => {
        if (!isDefault) e.currentTarget.style.opacity = '0.85';
      }}
      onMouseLeave={(e) => {
        e.currentTarget.style.opacity = '1';
      }}
    >
      <img
        src={`${assetBasePath}/icons/${iconFile}`}
        alt=""
        width={16}
        height={16}
        className="pointer-events-none object-contain select-none"
        draggable={false}
      />
    </button>
  );
}
