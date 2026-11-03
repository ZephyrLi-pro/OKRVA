import type { CSSProperties } from 'react';
import { useCallback, useEffect, useState } from 'react';
import { assetBasePath } from '@/lib/basePath';
import { ModelCloseButton } from '@/components/model/modelTooltips';
import { SKILL_EMBEDDED_DIALOG_SHELL } from '@/components/model/modelEmbeddedShellStyles';
import { t } from '@/lib/i18n';

interface ModelConfirmDialogProps {
  open: boolean;
  /** 无障碍名称 */
  ariaLabel: string;
  message: string;
  confirmLabel: string;
  /** Path under the asset base, e.g. `icons/dialog-question.svg` */
  iconSrc: string;
  onCancel: () => void;
  onConfirm: () => Promise<void> | void;
  /** Outer dialog height in px（Skills 卸载确认为固定高度；删除模型用 172） */
  dialogHeight?: number;
}

export function ModelConfirmDialog({
  open,
  ariaLabel,
  message,
  confirmLabel,
  iconSrc,
  onCancel,
  onConfirm,
  dialogHeight,
}: ModelConfirmDialogProps) {
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!open) return;
    const prev = document.body.style.overflow;
    document.body.style.overflow = 'hidden';
    return () => {
      document.body.style.overflow = prev;
    };
  }, [open]);

  const handleConfirmInner = useCallback(async () => {
    if (busy) return;
    setBusy(true);
    try {
      await onConfirm();
    } finally {
      setBusy(false);
    }
  }, [busy, onConfirm]);

  useEffect(() => {
    if (!open) return;
    const handler = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        e.stopPropagation();
        onCancel();
      }
      if (e.key === 'Enter' && !busy) {
        e.preventDefault();
        e.stopPropagation();
        void handleConfirmInner();
      }
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [open, onCancel, busy, handleConfirmInner]);

  if (!open) return null;

  const panelStyle: CSSProperties = {
    width: 376,
    background: '#fff',
    borderRadius: 8,
    boxSizing: 'border-box',
    display: 'flex',
    flexDirection: 'column',
    overflow: 'hidden',
    boxShadow: '0 6px 30px rgba(0, 0, 0, 0.12)',
    ...(dialogHeight !== undefined ? { height: dialogHeight } : { minHeight: 172 }),
  };

  return (
    <div
      role="dialog"
      aria-modal="true"
      aria-label={ariaLabel}
      className="model-dialog-shell"
      style={SKILL_EMBEDDED_DIALOG_SHELL}
    >
      <div style={panelStyle} onClick={(e) => e.stopPropagation()}>
        <div
          style={{
            width: 376,
            height: 40,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'flex-end',
            paddingRight: 8,
            boxSizing: 'border-box',
            flexShrink: 0,
          }}
        >
          <ModelCloseButton onClick={onCancel} />
        </div>

        <div
          style={{
            paddingLeft: 24,
            paddingRight: 24,
            paddingBottom: 24,
            flex: 1,
            display: 'flex',
            flexDirection: 'column',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'flex-start', paddingTop: 2 }}>
            <img
              src={`${assetBasePath}/${iconSrc.replace(/^\//, '')}`}
              alt=""
              width={16}
              height={16}
              style={{ flexShrink: 0, marginTop: 2, display: 'block' }}
            />
            {/* 嵌入式 WebKit 对 flex gap / 部分 margin 支持不稳定，用定宽占位保证 8px */}
            <span style={{ width: 8, flexShrink: 0 }} aria-hidden />
            <p
              className="text-sm leading-relaxed min-w-0"
              style={{ color: 'rgba(0, 0, 0, 0.55)' }}
            >
              {message}
            </p>
          </div>

          <div style={{ flex: 1 }} />

          <div style={{ display: 'flex', justifyContent: 'flex-end', marginTop: 16 }}>
            <button type="button" onClick={onCancel} className="skill-dialog-cancel-btn">
              {t('common.cancel')}
            </button>
            {/* 与删「添加」弹窗footer一致：定宽占位保证按钮间距 8px */}
            <span style={{ width: 8, flexShrink: 0 }} aria-hidden />
            <button
              type="button"
              disabled={busy}
              onClick={() => void handleConfirmInner()}
              className="skill-dialog-uninstall-btn"
            >
              {confirmLabel}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
