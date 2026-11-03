import { useState, type CSSProperties } from 'react';
import type { ModelInfo } from '@/lib/models';
import { assetBasePath } from '@/lib/basePath';
import { ModelRow } from './ModelRow';
import { modelKey } from './useConfiguredModels';
import { t } from '@/lib/i18n';

interface ModelListProps {
  models: ModelInfo[];
  defaultKey: string | null;
  onSelect: (model: ModelInfo) => void;
  onEdit: (model: ModelInfo) => void;
  onDelete: (model: ModelInfo) => void;
  onAdd: () => void;
}

export function ModelList({
  models,
  defaultKey,
  onSelect,
  onEdit,
  onDelete,
  onAdd,
}: ModelListProps) {
  const totalRows = models.length + 1;

  const rowRadiusStyle = (rowIndexInList: number): CSSProperties => {
    const isFirst = rowIndexInList === 0;
    const isLast = rowIndexInList === totalRows - 1;
    if (isFirst && isLast) {
      return { borderRadius: 8 };
    }
    if (isFirst) {
      return { borderRadius: '8px 8px 0 0' };
    }
    if (isLast) {
      return { borderRadius: '0 0 8px 8px' };
    }
    return { borderRadius: 0 };
  };

  return (
    <div style={{ background: '#ffffff', width: '100%' }}>
      {models.map((m, index) => (
        <div key={modelKey(m.provider, m.value)} style={{ marginBottom: 1 }}>
          <ModelRow
            model={m}
            isDefault={defaultKey === modelKey(m.provider, m.value)}
            onSelect={() => onSelect(m)}
            onEdit={() => onEdit(m)}
            onDelete={() => onDelete(m)}
            rowRadiusStyle={rowRadiusStyle(index)}
          />
        </div>
      ))}

      <AddRow rowRadiusStyle={rowRadiusStyle(models.length)} onClick={onAdd} />
    </div>
  );
}

interface AddRowProps {
  rowRadiusStyle: CSSProperties;
  onClick: () => void;
}

function AddRow({ rowRadiusStyle, onClick }: AddRowProps) {
  const [hover, setHover] = useState(false);
  const [pressed, setPressed] = useState(false);

  const background = pressed ? '#e0e0e0' : hover ? '#eaeaea' : '#f6f6f6';

  return (
    <button
      type="button"
      onClick={onClick}
      className="inline-flex min-w-0 w-full cursor-default border-none transition-colors flex-row flex-nowrap items-center justify-center box-border"
      style={{
        ...rowRadiusStyle,
        height: 60,
        background,
        color: 'var(--model-text-primary)',
      }}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => {
        setHover(false);
        setPressed(false);
      }}
      onMouseDown={() => setPressed(true)}
      onMouseUp={() => setPressed(false)}
    >
      <img
        src={`${assetBasePath}/icons/list-add-symbolic.svg`}
        alt=""
        width={16}
        height={16}
        className="shrink-0 object-contain pointer-events-none select-none"
        draggable={false}
      />
      <span
        className="shrink-0"
        style={{
          marginLeft: 8,
          fontFamily: 'var(--model-add-font-family)',
          fontSize: 'var(--model-font-size)',
          color: 'var(--model-text-primary)',
        }}
      >
        {t('model.add')}
      </span>
    </button>
  );
}
