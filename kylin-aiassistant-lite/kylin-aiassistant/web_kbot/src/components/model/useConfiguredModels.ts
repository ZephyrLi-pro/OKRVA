import { useCallback, useEffect, useState } from 'react';
import {
  getDefaultModelKey,
  loadConfiguredModels,
  MODEL_CONFIG_UPDATED_EVENT,
  type ModelInfo,
} from '@/lib/models';

export interface ConfiguredModelsState {
  models: ModelInfo[];
  /** `${provider}::${model}` of the currently selected default model. */
  defaultKey: string | null;
  loading: boolean;
  /** Force a refresh; resolves when the data has been reloaded. */
  refresh: () => Promise<void>;
}

function makeKey(provider: string, model: string): string {
  return `${provider}::${model}`;
}

export function useConfiguredModels(): ConfiguredModelsState {
  const [models, setModels] = useState<ModelInfo[]>([]);
  const [defaultKey, setDefaultKey] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  const reload = useCallback(async () => {
    setLoading(true);
    try {
      const [list, def] = await Promise.all([
        loadConfiguredModels(),
        getDefaultModelKey(),
      ]);
      setModels(list);
      setDefaultKey(def ? makeKey(def.provider, def.model) : null);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void reload();
  }, [reload]);

  useEffect(() => {
    const handler = () => {
      void reload();
    };
    window.addEventListener(MODEL_CONFIG_UPDATED_EVENT, handler);
    return () => {
      window.removeEventListener(MODEL_CONFIG_UPDATED_EVENT, handler);
    };
  }, [reload]);

  return { models, defaultKey, loading, refresh: reload };
}

export function modelKey(provider: string, model: string): string {
  return makeKey(provider, model);
}
