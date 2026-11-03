import { ModelConfirmDialog } from './ModelConfirmDialog';
import { t } from '@/lib/i18n';

interface DeleteConfirmDialogProps {
  open: boolean;
  onCancel: () => void;
  onConfirm: () => Promise<void> | void;
}

export function DeleteConfirmDialog({
  open,
  onCancel,
  onConfirm,
}: DeleteConfirmDialogProps) {
  return (
    <ModelConfirmDialog
      open={open}
      ariaLabel={t('model.delete_confirm')}
      dialogHeight={172}
      message={t('model.delete_confirm')}
      confirmLabel={t('model.delete')}
      iconSrc="icons/dialog-question.svg"
      onCancel={onCancel}
      onConfirm={onConfirm}
    />
  );
}
