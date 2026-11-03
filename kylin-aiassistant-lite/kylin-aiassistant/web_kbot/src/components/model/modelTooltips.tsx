import {
  useEffect,
  useLayoutEffect,
  useRef,
  useState,
  type CSSProperties,
} from 'react';
import { createPortal } from 'react-dom';
import { X } from 'lucide-react';
import { formatLongTextTooltip } from '@/lib/longTextTooltip';
import { t } from '@/lib/i18n';

const TOOLTIP_DELAY_MS = 500;
const MOUSE_OFFSET_X = 4;
const MOUSE_OFFSET_Y = 16;
const VIEWPORT_EDGE_PAD = 12;
/** 设计稿：中文每行约 50 字 × 14px */
const LONG_TEXT_TOOLTIP_MAX_WIDTH_PX = 700;
const DEFAULT_TOOLTIP_MAX_WIDTH_PX = 300;

/** 将 fixed tooltip 限制在视口内，靠边时翻转到鼠标另一侧。 */
export function clampFixedTooltipPosition(
  mousePos: { x: number; y: number },
  size: { width: number; height: number },
  offsetX = MOUSE_OFFSET_X,
  offsetY = MOUSE_OFFSET_Y,
): { left: number; top: number } {
  const vw = window.innerWidth;
  const vh = window.innerHeight;
  const pad = VIEWPORT_EDGE_PAD;

  let left = mousePos.x + offsetX;
  let top = mousePos.y + offsetY;

  if (left + size.width > vw - pad) {
    left = mousePos.x - size.width - offsetX;
  }
  if (left < pad) left = pad;
  if (left + size.width > vw - pad) {
    left = Math.max(pad, vw - size.width - pad);
  }

  if (top + size.height > vh - pad) {
    top = mousePos.y - size.height - offsetY;
  }
  if (top < pad) top = pad;
  if (top + size.height > vh - pad) {
    top = Math.max(pad, vh - size.height - pad);
  }

  return { left, top };
}

function tooltipMaxWidthCss(maxPx: number): string {
  return `min(${maxPx}px, calc(100vw - ${VIEWPORT_EDGE_PAD * 2}px))`;
}

const TOOLTIP_SURFACE: CSSProperties = {
  background: '#fff',
  border: '1px solid rgba(0,0,0,0.1)',
  borderRadius: 8,
  boxShadow: '0 4px 16px rgba(0,0,0,0.2)',
  padding: '8px',
  boxSizing: 'border-box',
  fontSize: 14,
  color: 'var(--pc-text-primary)',
  whiteSpace: 'nowrap',
  pointerEvents: 'none',
};

function isTextTruncated(el: HTMLElement): boolean {
  return el.scrollWidth > el.clientWidth;
}

/** 相对鼠标位置右下偏移（右 4px、下 16px）；用 fixed + portal，避免弹窗 overflow:hidden 裁切。 */
function FixedAnchorTooltip({
  visible,
  label,
  mousePos,
  zIndex = 300,
}: {
  visible: boolean;
  label: string;
  mousePos: { x: number; y: number };
  zIndex?: number;
}) {
  if (!visible || typeof document === 'undefined') return null;

  return createPortal(
    <div
      style={{
        position: 'fixed',
        top: mousePos.y + MOUSE_OFFSET_Y,
        left: mousePos.x + MOUSE_OFFSET_X,
        zIndex,
        ...TOOLTIP_SURFACE,
      }}
    >
      {label}
    </div>,
    document.body,
  );
}

function FixedTextTooltipBody({
  displayText,
  mousePos,
  zIndex,
  variant,
}: {
  displayText: string;
  mousePos: { x: number; y: number };
  zIndex: number;
  variant: 'default' | 'longText';
}) {
  const tooltipRef = useRef<HTMLDivElement>(null);
  const [position, setPosition] = useState<{ left: number; top: number } | null>(null);
  const isLongText = variant === 'longText';

  useLayoutEffect(() => {
    const el = tooltipRef.current;
    if (!el) return;
    const { width, height } = el.getBoundingClientRect();
    setPosition(clampFixedTooltipPosition(mousePos, { width, height }));
  }, [displayText, mousePos.x, mousePos.y]);

  const provisionalLeft = mousePos.x + MOUSE_OFFSET_X;
  const provisionalTop = mousePos.y + MOUSE_OFFSET_Y;

  return (
    <div
      ref={tooltipRef}
      style={{
        position: 'fixed',
        left: position?.left ?? provisionalLeft,
        top: position?.top ?? provisionalTop,
        zIndex,
        visibility: position ? 'visible' : 'hidden',
        ...TOOLTIP_SURFACE,
        fontSize: isLongText ? 14 : 12,
        whiteSpace: 'pre-wrap',
        maxWidth: tooltipMaxWidthCss(
          isLongText ? LONG_TEXT_TOOLTIP_MAX_WIDTH_PX : DEFAULT_TOOLTIP_MAX_WIDTH_PX,
        ),
        width: 'max-content',
        wordBreak: 'break-word',
        overflowWrap: 'anywhere',
        overflow: 'hidden',
        lineHeight: isLongText ? 1.5 : undefined,
      }}
    >
      {displayText}
    </div>
  );
}

/** 截断文本悬停提示（相对鼠标右下偏移：右 8px、下 16px）：fixed + portal，避免列表行 overflow:hidden 裁切。 */
function FixedTextTooltip({
  visible,
  text,
  mousePos,
  zIndex = 150,
  variant = 'default',
}: {
  visible: boolean;
  text: string;
  mousePos: { x: number; y: number };
  zIndex?: number;
  variant?: 'default' | 'longText';
}) {
  if (!visible || typeof document === 'undefined') return null;

  const isLongText = variant === 'longText';
  const displayText = isLongText ? formatLongTextTooltip(text) : text;

  return createPortal(
    <FixedTextTooltipBody
      displayText={displayText}
      mousePos={mousePos}
      zIndex={zIndex}
      variant={variant}
    />,
    document.body,
  );
}

function useDelayedAnchorTooltip(enabled = true) {
  const [tooltipVisible, setTooltipVisible] = useState(false);
  const [mousePos, setMousePos] = useState({ x: 0, y: 0 });
  const timerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  const onMouseEnter = (e: React.MouseEvent) => {
    if (!enabled) return;
    setMousePos({ x: e.clientX, y: e.clientY });
    timerRef.current = setTimeout(() => {
      timerRef.current = null;
      setTooltipVisible(true);
    }, TOOLTIP_DELAY_MS);
  };

  const onMouseMove = (e: React.MouseEvent) => {
    // 仅在延迟等待期间跟随鼠标，显示后固定位置
    if (timerRef.current !== null) {
      setMousePos({ x: e.clientX, y: e.clientY });
    }
  };

  const onMouseLeave = () => {
    if (timerRef.current) clearTimeout(timerRef.current);
    timerRef.current = null;
    setTooltipVisible(false);
  };

  useEffect(() => {
    return () => {
      if (timerRef.current) clearTimeout(timerRef.current);
    };
  }, []);

  return { tooltipVisible, mousePos, onMouseEnter, onMouseMove, onMouseLeave };
}

/** 弹窗关闭按钮：默认黑色无框，悬停红底白字并提示「关闭」，点击红色加深（CSS :active）。 */
export function ModelCloseButton({ onClick }: { onClick: () => void }) {
  const { tooltipVisible, mousePos, onMouseEnter, onMouseMove, onMouseLeave } = useDelayedAnchorTooltip();

  return (
    <>
      <div style={{ position: 'relative', display: 'inline-flex' }}>
        <button
          type="button"
          className="skill-dialog-close-btn"
          onClick={onClick}
          aria-label={t('common.close')}
          style={{ cursor: 'default' }}
          onMouseEnter={onMouseEnter}
          onMouseMove={onMouseMove}
          onMouseLeave={onMouseLeave}
        >
          <X size={16} />
        </button>
      </div>
      <FixedAnchorTooltip visible={tooltipVisible} label={t('common.close')} mousePos={mousePos} />
    </>
  );
}

/** 列表「更多」按钮悬停提示（菜单展开时不显示，固定在按钮右下方）。 */
export function useModelMoreTooltip(menuOpen: boolean) {
  const { tooltipVisible, mousePos, onMouseEnter, onMouseMove, onMouseLeave } = useDelayedAnchorTooltip(!menuOpen);

  const tooltip =
    tooltipVisible && !menuOpen ? (
      <FixedAnchorTooltip visible label={t('model.more')} mousePos={mousePos} zIndex={100} />
    ) : null;

  return { onMouseEnter, onMouseMove, onMouseLeave, tooltip };
}

/** 文本截断时在悬停处显示完整内容（与 Skills 卡片名称提示一致）。 */
export function TruncatedTextTooltip({
  text,
  className,
  style,
  tooltipVariant = 'default',
}: {
  text: string;
  className?: string;
  style?: CSSProperties;
  /** longText：任务指令等长文本，按设计稿截断与换行 */
  tooltipVariant?: 'default' | 'longText';
}) {
  const anchorRef = useRef<HTMLSpanElement>(null);
  const [showTooltip, setShowTooltip] = useState(false);
  const [mousePos, setMousePos] = useState({ x: 0, y: 0 });

  return (
    <>
      <span
        ref={anchorRef}
        className={`block min-w-0 truncate whitespace-nowrap ${className ?? ''}`}
        style={style}
        onMouseEnter={(e) => {
          const el = anchorRef.current;
          if (!el || !isTextTruncated(el)) return;
          setMousePos({ x: e.clientX, y: e.clientY });
          setShowTooltip(true);
        }}
        onMouseLeave={() => setShowTooltip(false)}
      >
        {text}
      </span>
      <FixedTextTooltip
        visible={showTooltip}
        text={text}
        mousePos={mousePos}
        variant={tooltipVariant}
      />
    </>
  );
}
