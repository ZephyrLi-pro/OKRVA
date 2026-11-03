import { Download, Star, Check, Loader2 } from 'lucide-react';
import type { SkillHubSkill } from './types';
import { isShowcaseSkill } from './types';

function formatDownloads(n: number): string {
  if (n >= 1_000_000) return `${(n / 1_000_000).toFixed(1)}M`;
  if (n >= 1_000) return `${(n / 1_000).toFixed(1)}K`;
  return String(n);
}

interface SkillCardProps {
  skill: SkillHubSkill;
  isInstalled: boolean;
  isInstalling: boolean;
  onInstall: () => void;
}

export default function SkillCard({ skill, isInstalled, isInstalling, onInstall }: SkillCardProps) {
  const showcase = isShowcaseSkill(skill) ? skill : null;
  const displayName = skill.name || skill.slug;
  const description = showcase?.description_zh || ('description' in skill ? skill.description : '');
  const downloads = showcase?.downloads ?? (skill as { downloads?: number | null }).downloads ?? 0;
  const stars = showcase?.stars ?? (skill as { stars?: number | null }).stars ?? 0;
  const version = skill.version;
  const tags = 'tags' in skill ? skill.tags : [];
  const author = 'author' in skill ? skill.author : null;
  const iconUrl = showcase?.icon_url ?? null;

  return (
    <div className="card card-radius-6 p-5 flex flex-col gap-3 animate-slide-in-up">
      <div className="flex items-start gap-3">
        <div
          className="h-10 w-10 rounded-lg flex items-center justify-center text-sm font-bold shrink-0"
          style={{
            background: 'var(--pc-accent-glow)',
            color: 'var(--pc-accent)',
            border: '1px solid var(--pc-accent-dim)',
          }}
        >
          {iconUrl ? (
            <img
              src={iconUrl}
              alt=""
              className="h-8 w-8 rounded object-cover"
              onError={(e) => {
                e.currentTarget.style.display = 'none';
              }}
            />
          ) : (
            displayName.charAt(0).toUpperCase()
          )}
        </div>
        <div className="min-w-0 flex-1">
          <h4
            className="text-sm font-semibold truncate"
            style={{ color: 'var(--pc-text-primary)' }}
          >
            {displayName}
          </h4>
          {author && (
            <p className="text-xs" style={{ color: 'var(--pc-text-faint)' }}>
              by {author}
            </p>
          )}
        </div>
        {stars > 0 && (
          <span
            className="flex items-center gap-1 text-xs shrink-0"
            style={{ color: 'var(--pc-text-muted)' }}
          >
            <Star className="h-3 w-3" />
            {stars}
          </span>
        )}
      </div>

      {description && (
        <p className="text-xs line-clamp-2" style={{ color: 'var(--pc-text-muted)' }}>
          {description}
        </p>
      )}

      <div className="flex items-center gap-3 text-xs" style={{ color: 'var(--pc-text-faint)' }}>
        {downloads > 0 && (
          <span className="flex items-center gap-1">
            <Download className="h-3 w-3" />
            {formatDownloads(downloads)}
          </span>
        )}
        {tags.length > 0 && (
          <span className="flex items-center gap-1 overflow-hidden">
            {tags.slice(0, 3).map((tag) => (
              <span
                key={tag}
                className="px-1.5 py-0.5 rounded text-[10px]"
                style={{ background: 'var(--pc-hover)', color: 'var(--pc-text-muted)' }}
              >
                {tag}
              </span>
            ))}
          </span>
        )}
        {version && <span className="ml-auto shrink-0">v{version}</span>}
      </div>

      <button
        onClick={onInstall}
        disabled={isInstalling || isInstalled}
        className="w-full py-2 rounded-lg text-xs font-semibold transition-all"
        style={
          isInstalled
            ? {
                background: 'rgba(0, 230, 138, 0.1)',
                color: 'var(--color-status-success)',
                border: '1px solid rgba(0, 230, 138, 0.2)',
                cursor: 'default',
              }
            : isInstalling
              ? {
                  background: 'var(--pc-accent)',
                  color: 'white',
                  opacity: 0.7,
                  cursor: 'wait',
                }
              : {
                  background: 'var(--pc-accent)',
                  color: 'white',
                  border: 'none',
                  cursor: 'pointer',
                }
        }
      >
        {isInstalling ? (
          <span className="flex items-center justify-center gap-1.5">
            <Loader2 className="h-3.5 w-3.5 animate-spin" />
            Installing...
          </span>
        ) : isInstalled ? (
          <span className="flex items-center justify-center gap-1.5">
            <Check className="h-3.5 w-3.5" />
            Installed
          </span>
        ) : (
          'Install'
        )}
      </button>
    </div>
  );
}
