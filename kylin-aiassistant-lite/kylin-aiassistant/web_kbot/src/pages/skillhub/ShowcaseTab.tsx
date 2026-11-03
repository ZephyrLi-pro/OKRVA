import { useState, useEffect, useCallback } from 'react';
import { Loader2, X } from 'lucide-react';
import { getShowcase, uninstallSkill } from '@/lib/skillhubApi';
import { t } from '@/lib/i18n';
import SkillCardGrid from './SkillCardGrid';
import type { InstalledSkill, SkillHubSkill } from './types';

const SHOWCASE_CATEGORIES = ['hot', 'featured', 'newest', 'installed'] as const;
type ShowcaseCategory = typeof SHOWCASE_CATEGORIES[number];
const UNINSTALL_REMOVE_ANIMATION_MS = 1000;

interface ShowcaseTabProps {
  category: string;
  installedSlugs: Set<string>;
  installedSkills: InstalledSkill[];
  onInstall: (slug: string) => void;
  onUninstalled?: () => void;
  onLoadingChange?: (loading: boolean) => void;
}

function installedToRow(skills: InstalledSkill[]): SkillHubSkill[] {
  return skills.map((s) => ({
    name: s.name || s.slug,
    slug: s.slug,
    description: `v${s.version} · ${s.source}`,
    version: s.version,
    source: s.source,
    zip_url: s.zip_url,
    update_url: s.update_url,
    sha256: s.sha256,
    installed_at: s.installed_at,
    detail: s.detail ?? null,
  }));
}

export default function ShowcaseTab({
  category,
  installedSlugs,
  installedSkills,
  onInstall,
  onUninstalled,
  onLoadingChange,
}: ShowcaseTabProps) {
  const activeIndex = Math.max(0, SHOWCASE_CATEGORIES.indexOf(category as ShowcaseCategory));
  const [skillsByCategory, setSkillsByCategory] = useState<Record<ShowcaseCategory, SkillHubSkill[]>>({
    hot: [],
    featured: [],
    newest: [],
    installed: installedToRow(installedSkills),
  });
  const [loading, setLoading] = useState(true);
  const [errors, setErrors] = useState<Partial<Record<ShowcaseCategory, string>>>({});
  const [uninstallingSlug, setUninstallingSlug] = useState<string | null>(null);
  const [resultMsg, setResultMsg] = useState<string | null>(null);
  const [confirmSlug, setConfirmSlug] = useState<string | null>(null);
  const [removingSlug, setRemovingSlug] = useState<string | null>(null);

  const updateLoading = useCallback((val: boolean) => {
    setLoading(val);
    onLoadingChange?.(val);
  }, [onLoadingChange]);

  const fetchShowcases = useCallback(async () => {
    updateLoading(true);
    const nextErrors: Partial<Record<ShowcaseCategory, string>> = {};
    const results = await Promise.all(
      SHOWCASE_CATEGORIES.filter((cat) => cat !== 'installed').map(async (cat) => {
        try {
          const resp = await getShowcase(cat);
          return [cat, resp.skills] as const;
        } catch (err) {
          nextErrors[cat] = err instanceof Error ? err.message : t('skillhub.load_failed');
          return [cat, []] as const;
        }
      }),
    );
    const nextSkills = results.reduce(
      (acc, [cat, skills]) => ({ ...acc, [cat]: skills }),
      {} as Partial<Record<ShowcaseCategory, SkillHubSkill[]>>,
    );

    setSkillsByCategory((current) => ({
      ...current,
      ...nextSkills,
    }));
    setErrors(nextErrors);
    updateLoading(false);
  }, [updateLoading]);

  useEffect(() => {
    fetchShowcases();
  }, [fetchShowcases]);

  useEffect(() => {
    setSkillsByCategory((current) => ({
      ...current,
      installed: installedToRow(installedSkills),
    }));
  }, [installedSkills]);

  const handleUninstall = async (slug: string) => {
    setUninstallingSlug(slug);
    setResultMsg(null);
    try {
      await uninstallSkill(slug);
      setConfirmSlug(null);
      setRemovingSlug(slug);
      window.setTimeout(() => {
        setSkillsByCategory((current) => ({
          ...current,
          installed: current.installed.filter((skill) => skill.slug !== slug),
        }));
        setRemovingSlug((current) => (current === slug ? null : current));
        onUninstalled?.();
      }, UNINSTALL_REMOVE_ANIMATION_MS);
    } catch (err) {
      setResultMsg(err instanceof Error ? err.message : t('skillhub.load_failed'));
    } finally {
      setUninstallingSlug(null);
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', flex: 1, minHeight: 0 }}>
      {resultMsg && (
        <div
          className="p-3 rounded-lg text-xs shrink-0 mb-4"
          style={{
            background: resultMsg.toLowerCase().includes('fail')
              ? 'rgba(239, 68, 68, 0.1)'
              : 'rgba(0, 230, 138, 0.1)',
            color: resultMsg.toLowerCase().includes('fail')
              ? 'var(--color-status-error)'
              : 'var(--color-status-success)',
          }}
        >
          {resultMsg}
        </div>
      )}

      {loading ? (
        <div className="flex items-center justify-center py-12">
          <Loader2 className="h-8 w-8 animate-spin" style={{ color: 'var(--pc-accent)' }} />
        </div>
      ) : (
        <div style={{ position: 'relative', flex: 1, minHeight: 0, overflow: 'hidden', contain: 'layout paint' }}>
          {SHOWCASE_CATEGORIES.map((cat, index) => {
            const offset = index - activeIndex;
            const nearby = Math.abs(offset) <= 1;

            return (
              <div
                key={cat}
                style={{
                  position: 'absolute',
                  inset: 0,
                  minHeight: 0,
                  display: 'flex',
                  flexDirection: 'column',
                  transform: `translate3d(${offset * 100}%, 0, 0)`,
                  transition: 'transform 0.32s cubic-bezier(0.16, 1, 0.3, 1)',
                  visibility: nearby ? 'visible' : 'hidden',
                  pointerEvents: offset === 0 ? 'auto' : 'none',
                  willChange: nearby ? 'transform' : undefined,
                  contain: 'layout paint',
                }}
              >
                {errors[cat] ? (
                  <div
                    className="p-8 text-center"
                    style={{ background: '#FFFFFF', border: '1px solid rgba(0, 0, 0, 0.08)', borderRadius: 12 }}
                  >
                    <p style={{ color: 'var(--color-status-error)' }}>{errors[cat]}</p>
                    <button
                      onClick={fetchShowcases}
                      className="btn-electric mt-4 px-4 py-2 text-sm font-medium"
                    >
                      {t('skillhub.retry')}
                    </button>
                  </div>
                ) : (
                  <SkillCardGrid
                    skills={skillsByCategory[cat]}
                    installedSlugs={installedSlugs}
                    installingSlug={null}
                    onInstall={onInstall}
                    isInstalledMode={cat === 'installed'}
                    onUninstall={cat === 'installed' ? setConfirmSlug : undefined}
                    uninstallingSlug={uninstallingSlug}
                    removingSlug={removingSlug}
                  />
                )}
              </div>
            );
          })}
        </div>
      )}
      {confirmSlug && (
        <UninstallConfirmDialog
          skill={skillsByCategory.installed.find((skill) => skill.slug === confirmSlug) ?? null}
          busy={uninstallingSlug === confirmSlug}
          onCancel={() => setConfirmSlug(null)}
          onConfirm={() => handleUninstall(confirmSlug)}
        />
      )}
    </div>
  );
}

function UninstallConfirmDialog({
  skill,
  busy,
  onCancel,
  onConfirm,
}: {
  skill: SkillHubSkill | null;
  busy: boolean;
  onCancel: () => void;
  onConfirm: () => void;
}) {
  const name = skill?.name || skill?.slug || 'this skill';

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center p-4"
      style={{ background: 'rgba(0, 0, 0, 0.28)' }}
      onClick={() => {
        if (!busy) onCancel();
      }}
    >
      <div
        className="w-full max-w-sm rounded-xl bg-white p-5 shadow-xl"
        onClick={(event) => event.stopPropagation()}
      >
        <div className="mb-4 flex items-start justify-between gap-3">
          <div>
            <h3 className="text-base font-semibold" style={{ color: 'var(--pc-text-primary)' }}>
              确认卸载技能？
            </h3>
            <p className="mt-2 text-sm leading-6" style={{ color: 'var(--pc-text-muted)' }}>
              将从本地移除 {name}，并清理安装记录。
            </p>
          </div>
          <button
            onClick={onCancel}
            disabled={busy}
            className="rounded-lg p-1 transition-colors hover:bg-black/5"
            style={{ color: 'var(--pc-text-faint)', cursor: busy ? 'not-allowed' : 'pointer' }}
            aria-label="关闭"
          >
            <X className="h-4 w-4" />
          </button>
        </div>

        <div className="flex justify-end gap-2">
          <button
            onClick={onCancel}
            disabled={busy}
            className="rounded-lg px-4 py-2 text-sm font-medium transition-all"
            style={{
              color: 'var(--pc-text-muted)',
              border: '1px solid rgba(0,0,0,0.1)',
              cursor: busy ? 'not-allowed' : 'pointer',
            }}
          >
            取消
          </button>
          <button
            onClick={onConfirm}
            disabled={busy}
            className="inline-flex items-center gap-2 rounded-lg px-4 py-2 text-sm font-medium transition-all"
            style={{
              background: 'rgba(239, 68, 68, 0.92)',
              color: 'white',
              cursor: busy ? 'wait' : 'pointer',
              opacity: busy ? 0.75 : 1,
            }}
          >
            {busy && <Loader2 className="h-4 w-4 animate-spin" />}
            确认卸载
          </button>
        </div>
      </div>
    </div>
  );
}
