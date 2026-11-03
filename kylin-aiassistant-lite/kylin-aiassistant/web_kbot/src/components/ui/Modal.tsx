import { useEffect, useRef, useState, type ReactNode } from 'react';
import { createPortal } from 'react-dom';
import { X } from 'lucide-react';
import { assetBasePath } from '@/lib/basePath';

interface ModalProps {
  open: boolean;
  onClose: () => void;
  /** Title shown in the header. Pass `null` for the headerless variant (delete confirmation). */
  title?: ReactNode;
  /** Hide the assistant icon next to the title. */
  hideIcon?: boolean;
  /** Path under the asset base (e.g. `icons/logo.svg`). Defaults to KylinBot icon. */
  titleIconSrc?: string;
  /** Body content */
  children: ReactNode;
  /** Footer area (right-aligned buttons, etc.) */
  footer?: ReactNode;
  /** Fixed dialog width (px). Defaults to 376 to match the design spec. */
  width?: number;
  /** Fixed outer height (px). When set, the panel does not expand with content (no drag resize implied). */
  height?: number;
  /** Optional aria-label fallback when title is omitted */
  ariaLabel?: string;
  /** When set, calling Enter triggers this callback (only fires when no input/select is focused or when shouldFireEnter returns true). */
  onEnter?: () => void;

  /** When false, do not register global Escape / Enter handlers (use when another modal is stacked above). */
  keyboardShortcuts?: boolean;
  /** Root stacking order (must beat in-root overlays e.g. z-[100] toasts under Qt WebEngine). */
  zIndex?: number;
  /** Fixed header height in px (optional). When set, overrides default padding-based height. */
  headerHeight?: number;
  /** Close button size in px (optional). Defaults to 28. */
  closeButtonSize?: number;
  /** Close button margin top/bottom in px (optional). */
  closeButtonMarginY?: number;
  /** Close button margin right in px (optional). */
  closeButtonMarginRight?: number;
}

export function Modal({
  open,
  onClose,
  title,
  hideIcon,
  titleIconSrc,
  children,
  footer,
  width = 376,
  height,
  ariaLabel,
  onEnter,
  keyboardShortcuts = true,
  zIndex = 50000,
  headerHeight,
  closeButtonSize = 28,
  closeButtonMarginY = 0,
  closeButtonMarginRight = 0,
}: ModalProps) {
  const containerRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    if (!open) return;
    const previousOverflow = document.body.style.overflow;
    document.body.style.overflow = 'hidden';
    return () => {
      document.body.style.overflow = previousOverflow;
    };
  }, [open]);

  useEffect(() => {
    if (!open || !keyboardShortcuts) return;
    const handler = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        e.stopPropagation();
        onClose();
        return;
      }
      if (e.key === 'Enter' && onEnter) {
        const target = e.target as HTMLElement | null;
        // Ignore Enter inside textareas
        if (target?.tagName === 'TEXTAREA') return;
        // Allow Enter from any other element including inputs
        e.preventDefault();
        onEnter();
      }
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [open, keyboardShortcuts, onClose, onEnter]);

  // Autofocus a sensible control when opened (prefer fields over picker buttons).
  useEffect(() => {
    if (!open) return;
    let cancelled = false;
    const id = window.setTimeout(() => {
      if (cancelled) return;
      const root = containerRef.current;
      if (!root?.isConnected) return;
      const field = root.querySelector<HTMLElement>(
        'input:not([type="hidden"]):not([disabled]),textarea:not([disabled]),select:not([disabled])',
      );
      const focusable =
        field ?? root.querySelector<HTMLElement>('button:not([disabled])');
      focusable?.focus();
    }, 30);
    return () => {
      cancelled = true;
      window.clearTimeout(id);
    };
  }, [open]);

  if (!open) return null;

  /** Portal keeps dialogs outside `<main overflow-y-auto>` so `position:fixed` is not clipped (Qt WebEngine). */
  const subtree = (
    <div
      role="dialog"
      aria-modal="true"
      aria-label={typeof title === 'string' ? title : ariaLabel ?? 'dialog'}
      className="fixed inset-0 flex items-center justify-center"
      style={{ zIndex }}
    >
      <div
        className="absolute inset-0"
        style={{ background: 'transparent' }}
      />
      <div
        ref={containerRef}
        className="relative z-[1] rounded-2xl shadow-2xl animate-fade-in flex flex-col"
        style={{
          width,
          ...(typeof height === 'number'
            ? { height, maxHeight: height, overflow: 'hidden', boxSizing: 'border-box' as const }
            : {}),
          background: '#ffffff',
          border: '1px solid #e8e8e8',
          boxShadow: '0 6px 30px rgba(0, 0, 0, 0.12)',
          pointerEvents: 'auto',
        }}
        onClick={(e) => e.stopPropagation()}
      >
        {title !== undefined && (
          <div
            className={`flex items-center justify-between${typeof height === 'number' ? ' flex-shrink-0' : ''}`}
            style={
              headerHeight
                ? { height: headerHeight, paddingLeft: 0, paddingRight: 0 }
                : undefined
            }
          >
            <div className="flex items-center">
              {!hideIcon && (
                <img
                  src={
                    titleIconSrc
                      ? `${assetBasePath}/${titleIconSrc.replace(/^\//, '')}`
                      : `${assetBasePath}/kylin-bot.svg`
                  }
                  alt=""
                  className="h-5 w-5 object-contain"
                  style={{ marginLeft: 8, marginRight: 8 }}
                  onError={(e) => {
                    e.currentTarget.style.display = 'none';
                  }}
                />
              )}
              {typeof title === 'string' ? (
                <h2
                  className="text-sm font-semibold"
                  style={{ color: 'var(--pc-text-primary)' }}
                >
                  {title}
                </h2>
              ) : (
                title
              )}
            </div>
            <CloseButton
              onClose={onClose}
              size={closeButtonSize}
              marginY={closeButtonMarginY}
              marginRight={closeButtonMarginRight}
            />
          </div>
        )}

        {/* Body — scrollable middle region when dialog has fixed height */}
        <div
          className={`px-6 flex-1 min-h-0${typeof height === 'number' ? ' overflow-y-auto overflow-x-hidden' : ''}`}
        >
          {children}
        </div>

        {footer && (
          <div
            className={`px-6 flex items-center justify-end gap-3 flex-shrink-0 ${typeof height === 'number' ? 'pt-4 pb-5' : 'pb-6'}`}
          >
            {footer}
          </div>
        )}

        {/* When no header is rendered, still show a corner close button. */}
        {title === undefined && (
          <div className="absolute top-3 right-3">
            <CloseButton onClose={onClose} />
          </div>
        )}
      </div>
    </div>
  );

  const host =
    typeof document !== 'undefined'
      ? (document.getElementById('modal-root') ?? document.body)
      : null;

  return host ? createPortal(subtree, host) : null;
}

function CloseButton({
  onClose,
  size = 32,
  marginY = 0,
  marginRight = 0,
}: {
  onClose: () => void;
  size?: number;
  marginY?: number;
  marginRight?: number;
}) {
  const [showTooltip, setShowTooltip] = useState(false);
  const iconSize = 16;
  return (
    <div style={{ position: 'relative', display: 'inline-block' }}>
      <button
        type="button"
        onClick={onClose}
        aria-label="关闭"
        className="flex items-center justify-center"
        style={{
          width: size,
          height: size,
          marginTop: marginY,
          marginBottom: marginY,
          marginRight,
          color: 'var(--pc-text-muted)',
          background: 'transparent',
          border: 'none',
          borderRadius: 6,
          cursor: 'default',
          padding: 0,
          transition: 'background 0.15s ease, color 0.15s ease',
        }}
        onMouseEnter={(e) => {
          e.currentTarget.style.background = '#F53F3F';
          e.currentTarget.style.color = '#ffffff';
          setShowTooltip(true);
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.background = 'transparent';
          e.currentTarget.style.color = 'var(--pc-text-muted)';
          setShowTooltip(false);
        }}
        onMouseDown={(e) => {
          if (e.button === 0) {
            e.currentTarget.style.background = '#D93030';
            e.currentTarget.style.color = '#ffffff';
          }
        }}
        onMouseUp={(e) => {
          if (e.button === 0) {
            e.currentTarget.style.background = '#F53F3F';
            e.currentTarget.style.color = '#ffffff';
          }
        }}
      >
        <X size={iconSize} />
      </button>
      {showTooltip && (
        <div
          style={{
            position: 'absolute',
            top: '50%',
            left: '100%',
            transform: 'translateY(-50%)',
            marginLeft: 8,
            padding: '4px 10px',
            background: '#ffffff',
            border: '1px solid #e8e8e8',
            borderRadius: 4,
            fontSize: 12,
            color: '#374151',
            whiteSpace: 'nowrap',
            boxShadow: '0 2px 8px rgba(0, 0, 0, 0.1)',
            zIndex: 100,
          }}
        >
          关闭
        </div>
      )}
    </div>
  );
}
