import { useEffect, useCallback, useState } from 'react';
import { Loader2, Trash2, X } from 'lucide-react';
import { listInstalled, uninstallSkill } from '@/lib/skillhubApi';
import type { InstalledSkill } from './types';

const UNINSTALL_REMOVE_ANIMATION_MS = 1000;

export default function InstalledTab() {
  const [skills, setSkills] = useState<InstalledSkill[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [uninstallingSlug, setUninstallingSlug] = useState<string | null>(null);
  const [resultMsg, setResultMsg] = useState<string | null>(null);
  const [confirmSlug, setConfirmSlug] = useState<string | null>(null);
  const [removingSlug, setRemovingSlug] = useState<string | null>(null);

  const fetchInstalled = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const resp = await listInstalled();
      setSkills(resp.skills);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load installed skills');
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchInstalled();
  }, [fetchInstalled]);

  const handleUninstall = async (slug: string) => {
    setUninstallingSlug(slug);
    setResultMsg(null);
    try {
      await uninstallSkill(slug);
      setConfirmSlug(null);
      setRemovingSlug(slug);
      window.setTimeout(() => {
        setSkills((current) => current.filter((skill) => skill.slug !== slug));
        setRemovingSlug((current) => (current === slug ? null : current));
      }, UNINSTALL_REMOVE_ANIMATION_MS);
    } catch (err) {
      setResultMsg(err instanceof Error ? err.message : 'Uninstall failed');
    } finally {
      setUninstallingSlug(null);
    }
  };

  if (loading) {
    return (
      <div className="flex items-center justify-center py-12">
        <Loader2 className="h-8 w-8 animate-spin" style={{ color: 'var(--pc-accent)' }} />
      </div>
    );
  }

  if (error) {
    return (
      <div className="card card-radius-6 p-8 text-center">
        <p style={{ color: 'var(--color-status-error)' }}>{error}</p>
        <button
          onClick={fetchInstalled}
          className="btn-electric mt-4 px-4 py-2 text-sm font-medium"
        >
          Retry
        </button>
      </div>
    );
  }

  if (skills.length === 0) {
    return (
      <div className="card card-radius-6 p-8 text-center">
        <p style={{ color: 'var(--pc-text-muted)' }}>
          No skills installed yet. Browse the marketplace to find skills.
        </p>
      </div>
    );
  }

  return (
    <div className="space-y-2">
      {resultMsg && (
        <div
          className="p-3 rounded-lg text-xs"
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
      {skills.map((skill) => (
        <div
          key={skill.slug}
          className={`card card-radius-6 p-4 flex items-center gap-4 skill-installed-item ${removingSlug === skill.slug ? 'removing' : ''}`}
        >
          <div className="min-w-0 flex-1">
            <h4
              className="text-sm font-semibold truncate"
              style={{ color: 'var(--pc-text-primary)' }}
            >
              {skill.name || skill.slug}
            </h4>
            <div
              className="flex items-center gap-3 text-xs mt-1"
              style={{ color: 'var(--pc-text-faint)' }}
            >
              <span>{skill.slug}</span>
              {skill.version && <span>v{skill.version}</span>}
              {skill.installed_at > 0 && (
                <span>{new Date(skill.installed_at * 1000).toLocaleDateString()}</span>
              )}
            </div>
          </div>
          <button
            onClick={() => setConfirmSlug(skill.slug)}
            disabled={removingSlug === skill.slug || uninstallingSlug === skill.slug}
            className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs font-medium transition-all"
            style={{
              color: 'var(--color-status-error)',
              border: '1px solid rgba(239, 68, 68, 0.2)',
              background: 'rgba(239, 68, 68, 0.06)',
              cursor: removingSlug === skill.slug || uninstallingSlug === skill.slug ? 'wait' : 'pointer',
              opacity: removingSlug === skill.slug || uninstallingSlug === skill.slug ? 0.5 : 1,
            }}
          >
            {uninstallingSlug === skill.slug ? (
              <Loader2 className="h-3.5 w-3.5 animate-spin" />
            ) : (
              <Trash2 className="h-3.5 w-3.5" />
            )}
            卸载
          </button>
        </div>
      ))}
      {confirmSlug && (
        <UninstallConfirmDialog
          skill={skills.find((skill) => skill.slug === confirmSlug) ?? null}
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
  skill: InstalledSkill | null;
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
