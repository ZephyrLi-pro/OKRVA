// Model page — 模型配置 + 用量统计
// Implements the design from `模型配置和用量统计测试用例.md`. Uses the
// schema-driven config API exposed by `models.ts` (`providers.models.<key>`
// entries plus `providers.fallback` for the active model) so the chat
// composer's model control stays in sync via `MODEL_CONFIG_UPDATED_EVENT`.

import { useCallback, useMemo, useState } from 'react';
import {
  buildConfiguredModelsAfterEdit,
  buildConfiguredModelsAfterOverwrite,
  deleteModelConfiguration,
  saveModelConfiguration,
  setDefaultModel,
  updateModelConfiguration,
  type ModelInfo,
} from '@/lib/models';
import { ModelList } from '@/components/model/ModelList';
import { ModelDialog, type ModelDialogMode } from '@/components/model/ModelDialog';
import { DeleteConfirmDialog } from '@/components/model/DeleteConfirmDialog';
import { OverwriteConfirmDialog } from '@/components/model/OverwriteConfirmDialog';
import { UsageStatsPanel } from '@/components/model/UsageStatsPanel';
import { getProviderLabel, resolveProviderMeta } from '@/components/model/providerMeta';
import { useConfiguredModels } from '@/components/model/useConfiguredModels';
import { PageLoadingState } from '@/components/ui/PageLoadingSpinner';
import { t } from '@/lib/i18n';

type ModelDialogSubmitValue = {
  storageProvider: string;
  modelName: string;
  apiKey: string;
  baseUrl: string;
  label: string;
  pricing?: import('@/components/model/ModelDialog').ModelDialogPricing | null;
};

type DialogState =
  | { kind: 'idle' }
  | { kind: 'add' }
  | { kind: 'edit'; original: ModelInfo }
  | { kind: 'delete'; target: ModelInfo };

/** Stable reference — inline `{ kind: 'add' }` would retrigger ModelDialog reset on every parent render. */
const ADD_MODEL_DIALOG_MODE = { kind: 'add' } as const satisfies ModelDialogMode;

export default function Model() {
  const { models, defaultKey, loading, refresh } = useConfiguredModels();
  const [dialog, setDialog] = useState<DialogState>({ kind: 'idle' });
  const [pendingOverwrite, setPendingOverwrite] = useState<{
    value: ModelDialogSubmitValue;
    providerLabel: string;
  } | null>(null);

  const dialogMode = useMemo((): ModelDialogMode | null => {
    if (dialog.kind === 'add') return ADD_MODEL_DIALOG_MODE;
    if (dialog.kind === 'edit') {
      return { kind: 'edit', original: dialog.original };
    }
    return null;
  }, [dialog]);

  const modelTagLookup = useMemo(() => {
    const lookup = new Map<
      string,
      { displayLabel: string; providerKey: string }
    >();
    for (const m of models) {
      lookup.set(m.value, {
        displayLabel: m.label || m.value,
        providerKey: m.provider,
      });
    }
    return lookup;
  }, [models]);

  const closeDialog = useCallback(() => {
    setPendingOverwrite(null);
    setDialog({ kind: 'idle' });
  }, []);

  const dismissOverwriteLayer = useCallback(() => {
    setPendingOverwrite(null);
  }, []);

  const handleAdd = () => setDialog({ kind: 'add' });

  const handleEdit = (m: ModelInfo) =>
    setDialog({ kind: 'edit', original: m });

  const handleDelete = (m: ModelInfo) =>
    setDialog({ kind: 'delete', target: m });

  const handleSelectDefault = async (m: ModelInfo) => {
    try {
      await setDefaultModel({ provider: m.provider, model: m.value });
    } catch (err) {
      console.error('[Model] failed to set default', err);
    }
  };

  const persistModelValue = async (
    value: ModelDialogSubmitValue,
    options?: { replacedModel?: string; configuredAfter?: ModelInfo[] },
  ) => {
    // `value.storageProvider` is already the resolved key (`deepseek`,
    // `custom:https://...` etc.) coming from the dialog. We split the
    // custom path back into ('custom', baseUrl) since the persistence
    // helper rebuilds the canonical key from those two arguments.
    const isCustomTarget = value.storageProvider.startsWith('custom:');
    const persistArgs = {
      provider: isCustomTarget ? 'custom' : value.storageProvider,
      model: value.modelName,
      apiKey: value.apiKey || undefined,
      baseUrl: value.baseUrl || undefined,
      label: value.label,
      pricing: value.pricing,
      replacedModel: options?.replacedModel,
      configuredAfter:
        options?.configuredAfter ??
        (options?.replacedModel
          ? buildConfiguredModelsAfterOverwrite(
              models,
              value.storageProvider,
              value.modelName,
            )
          : models),
    } as const;
    await saveModelConfiguration(persistArgs);
  };

  const providerLabelForOverwrite = (storageProvider: string) => {
    if (storageProvider.startsWith('custom:')) {
      return storageProvider.slice('custom:'.length);
    }
    const meta = resolveProviderMeta(storageProvider).meta;
    return getProviderLabel(meta);
  };

  const submitDialog = async (value: ModelDialogSubmitValue) => {
    if (dialog.kind === 'add') {
      const existing = models.find((m) => m.provider === value.storageProvider);
      if (existing) {
        setPendingOverwrite({
          value,
          providerLabel: providerLabelForOverwrite(value.storageProvider),
        });
        return false;
      }
      // `saveModelConfiguration` handles fallback promotion internally:
      // first-add becomes the default, subsequent adds preserve the
      // existing default.
      await persistModelValue(value);
    } else if (dialog.kind === 'edit') {
      const isCustomTarget = value.storageProvider.startsWith('custom:');
      const persistArgs = {
        provider: isCustomTarget ? 'custom' : value.storageProvider,
        model: value.modelName,
        apiKey: value.apiKey || undefined,
        baseUrl: value.baseUrl || undefined,
        label: value.label,
        pricing: value.pricing,
      } as const;
      await updateModelConfiguration(
        {
          provider: dialog.original.provider,
          model: dialog.original.value,
        },
        {
          ...persistArgs,
          configuredAfter: buildConfiguredModelsAfterEdit(
            models,
            dialog.original,
            {
              storageProvider: value.storageProvider,
              modelName: value.modelName,
              label: value.label,
            },
          ),
        },
      );
    }
    closeDialog();
    await refresh();
  };

  const confirmOverwrite = async () => {
    if (!pendingOverwrite) return;
    const existing = models.find(
      (m) => m.provider === pendingOverwrite.value.storageProvider,
    );
    await persistModelValue(pendingOverwrite.value, {
      replacedModel: existing?.value,
    });
    setPendingOverwrite(null);
    closeDialog();
    await refresh();
  };

  const confirmDelete = async () => {
    if (dialog.kind !== 'delete') return;
    const target = dialog.target;
    const remaining = models.filter(
      (m) => !(m.provider === target.provider && m.value === target.value),
    );
    await deleteModelConfiguration({
      provider: target.provider,
      model: target.value,
      remainingAfterDelete: remaining,
    });
    closeDialog();
    await refresh();
  };

  return (
    <div
      className="model-page box-border"
      style={{ marginLeft: 24, marginRight: 24, marginTop: 0 }}
    >
      <h2 className="model-section-title">{t('model.title')}</h2>

      {/* Model list + add row */}
      {loading ? (
        <PageLoadingState size={24} />
      ) : (
        <ModelList
          models={models}
          defaultKey={defaultKey}
          onSelect={(m) => void handleSelectDefault(m)}
          onEdit={handleEdit}
          onDelete={handleDelete}
          onAdd={handleAdd}
        />
      )}

      {/* Stats only when there is at least one model */}
      {!loading && models.length > 0 && (
        <div style={{ marginTop: 32 }}>
          <UsageStatsPanel modelTagLookup={modelTagLookup} />
        </div>
      )}

      {/* Add / Edit dialog */}
      <ModelDialog
        open={dialog.kind === 'add' || dialog.kind === 'edit'}
        mode={dialogMode}
        onCancel={closeDialog}
        onSubmit={submitDialog}
        keyboardShortcuts={pendingOverwrite === null}
        interactionLocked={pendingOverwrite !== null}
      />

      <OverwriteConfirmDialog
        open={pendingOverwrite !== null}
        isCustom={pendingOverwrite?.value.storageProvider.startsWith('custom:') ?? false}
        quotedInner={pendingOverwrite?.providerLabel ?? ''}
        onClose={dismissOverwriteLayer}
        onConfirm={confirmOverwrite}
      />

      {/* Delete confirmation */}
      <DeleteConfirmDialog
        open={dialog.kind === 'delete'}
        onCancel={closeDialog}
        onConfirm={confirmDelete}
      />
    </div>
  );
}
