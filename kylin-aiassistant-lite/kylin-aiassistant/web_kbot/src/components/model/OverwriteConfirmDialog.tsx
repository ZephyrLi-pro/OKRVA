import { useCallback, useEffect, useMemo, useState, type CSSProperties } from 'react';
import { assetBasePath } from '@/lib/basePath';
import { ModelCloseButton } from '@/components/model/modelTooltips';
import { SKILL_EMBEDDED_DIALOG_SHELL_ABOVE } from '@/components/model/modelEmbeddedShellStyles';
import { splitOverwriteMutedBody } from '@/components/model/overwriteConfirmCopy';
import { t } from '@/lib/i18n';

interface OverwriteConfirmDialogProps {
  open: boolean;
  /** 是否为自定义（OpenAI 兼容）镜像：文案走「请求地址」模板 */
  isCustom: boolean;
  /**
   * 模板中「引号内核」：`custom:` 为请求地址本体；预设为提供商展示名（如 MiniMax）。
   */
  quotedInner: string;
  /** 右上角 X / 取消 / Esc：仅关闭覆盖确认层 */
  onClose: () => void;
  onConfirm: () => Promise<void> | void;
}

export function OverwriteConfirmDialog({
  open,
  isCustom,
  quotedInner,
  onClose,
  onConfirm,
}: OverwriteConfirmDialogProps) {
  const [busy, setBusy] = useState(false);

  const mutedParts = useMemo(
    () => splitOverwriteMutedBody(isCustom ? 'custom' : 'preset', quotedInner),
    [isCustom, quotedInner],
  );

  const mutedLineStyle = useMemo(
    (): CSSProperties => ({
      fontSize: 14,
      color: 'var(--pc-text-muted)',
      lineHeight: '20px',
      marginLeft: 24,
      wordBreak: 'break-word',
      overflowWrap: 'anywhere',
    }),
    [],
  );

  useEffect(() => {
    if (!open) {
      setBusy(false);
      return;
    }
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
        onClose();
      }
      if (e.key === 'Enter' && !busy) {
        e.preventDefault();
        e.stopPropagation();
        void handleConfirmInner();
      }
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [open, onClose, busy, handleConfirmInner]);

  if (!open) return null;

  const panelStyle: CSSProperties = {
    width: 376,
    height: 212,
    background: '#fff',
    borderRadius: 8,
    boxSizing: 'border-box',
    display: 'flex',
    flexDirection: 'column',
    overflow: 'hidden',
    boxShadow: '0 6px 30px rgba(0, 0, 0, 0.12)',
  };

  return (
    <div
      role="dialog"
      aria-modal="true"
      aria-label={t('model.overwrite_title')}
      className="model-dialog-shell"
      style={SKILL_EMBEDDED_DIALOG_SHELL_ABOVE}
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
          <ModelCloseButton onClick={onClose} />
        </div>

        <div
          style={{
            paddingLeft: 24,
            paddingRight: 24,
            paddingBottom: 24,
            flex: 1,
            display: 'flex',
            flexDirection: 'column',
            minHeight: 0,
          }}
        >
          <div style={{ display: 'flex', alignItems: 'flex-start', marginBottom: 4 }}>
            <img
              src={`${assetBasePath}/icons/dialog-question.svg`}
              alt=""
              width={16}
              height={16}
              style={{ flexShrink: 0, marginTop: 2, display: 'block' }}
            />
            <span style={{ width: 8, flexShrink: 0 }} aria-hidden />
            <span
              style={{
                fontSize: 14,
                color: 'var(--pc-text-primary)',
                lineHeight: '20px',
                minWidth: 0,
              }}
            >
              {t('model.overwrite_title')}
            </span>
          </div>

          {/* 仅用 JS 收窄引号内核；后缀「模型提供商…/请求地址…」不被 line-clamp 截断 */}
          <span style={mutedLineStyle}>
            <span>{mutedParts.prefix}</span>
            <span style={{ wordBreak: 'break-all' }}>{mutedParts.inner}</span>
            <span>{mutedParts.suffix}</span>
          </span>

          <div style={{ flex: 1 }} />

          <div style={{ display: 'flex', justifyContent: 'flex-end', alignItems: 'center' }}>
            <button
              type="button"
              onClick={(e) => {
                e.stopPropagation();
                onClose();
              }}
              className="skill-dialog-cancel-btn"
            >
              {t('common.cancel')}
            </button>
            <span style={{ width: 8, flexShrink: 0 }} aria-hidden />
            <button
              type="button"
              disabled={busy}
              className="skill-dialog-uninstall-btn"
              onClick={() => void handleConfirmInner()}
            >
              {busy ? t('model.overwriting') : t('model.overwrite')}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
