import { useEffect, useState } from 'react';
import { Download, Star, Loader2, ChevronDown, Trash2 } from 'lucide-react';
import { t } from '@/lib/i18n';
import { getSkillDetail } from '@/lib/skillhubApi';
import type { InstalledSkill, SkillDetail, SkillHubSkill, SkillSearchResult } from './types';
import { isShowcaseSkill } from './types';
import SkillDetailPanel from './SkillDetailPanel';

function formatDownloads(n: number): string {
  if (n >= 10_000) return `${(n / 10_000).toFixed(1)}万`;
  if (n >= 1_000) return `${(n / 1_000).toFixed(1)}千`;
  return String(n);
}

const AVATAR_COLORS = [
  'rgba(0, 122, 255, 0.1)',
  'rgba(52, 199, 89, 0.1)',
  'rgba(255, 149, 0, 0.1)',
  'rgba(175, 82, 222, 0.1)',
  'rgba(255, 45, 85, 0.1)',
  'rgba(0, 199, 190, 0.1)',
  'rgba(255, 204, 0, 0.1)',
  'rgba(88, 86, 214, 0.1)',
  'rgba(255, 59, 48, 0.1)',
  'rgba(48, 209, 88, 0.1)',
];

const AVATAR_TEXT_COLORS = [
  'rgba(0, 122, 255, 0.9)',
  'rgba(52, 199, 89, 0.9)',
  'rgba(255, 149, 0, 0.9)',
  'rgba(175, 82, 222, 0.9)',
  'rgba(255, 45, 85, 0.9)',
  'rgba(0, 199, 190, 0.9)',
  'rgba(204, 163, 0, 0.9)',
  'rgba(88, 86, 214, 0.9)',
  'rgba(255, 59, 48, 0.9)',
  'rgba(48, 209, 88, 0.9)',
];

function getAvatarColor(slug: string): string {
  let hash = 0;
  for (let i = 0; i < slug.length; i++) {
    hash = slug.charCodeAt(i) + ((hash << 5) - hash);
  }
  const idx = Math.abs(hash) % AVATAR_COLORS.length;
  return AVATAR_COLORS[idx] as string;
}

function getAvatarTextColor(slug: string): string {
  let hash = 0;
  for (let i = 0; i < slug.length; i++) {
    hash = slug.charCodeAt(i) + ((hash << 5) - hash);
  }
  const idx = Math.abs(hash) % AVATAR_TEXT_COLORS.length;
  return AVATAR_TEXT_COLORS[idx] as string;
}

function isSearchResult(skill: SkillHubSkill): skill is SkillSearchResult {
  return 'author' in skill;
}

function isInstalledSkill(skill: SkillHubSkill): skill is InstalledSkill {
  return 'installed_at' in skill;
}

interface SkillRowProps {
  skill: SkillHubSkill;
  isInstalled: boolean;
  isInstalling: boolean;
  onInstall: () => void;
  isInstalledMode?: boolean;
  expanded?: boolean;
  onToggle?: () => void;
  onUninstall?: () => void;
  isUninstalling?: boolean;
  isRemoving?: boolean;
}

export default function SkillRow({ skill, isInstalled, isInstalling, onInstall, isInstalledMode = false, expanded = false, onToggle, onUninstall, isUninstalling = false, isRemoving = false }: SkillRowProps) {
  const showcase = isShowcaseSkill(skill) ? skill : null;
  const displayName = skill.name || skill.slug;
  const description = showcase?.description_zh
    || (isSearchResult(skill) ? skill.description : '')
    || (isInstalledSkill(skill) ? `v${skill.version} · ${skill.source}` : '');
  const downloads = showcase?.downloads ?? (skill as { downloads?: number | null }).downloads ?? 0;
  const stars = showcase?.stars ?? (skill as { stars?: number | null }).stars ?? 0;
  const iconUrl = showcase?.icon_url ?? null;
  const tags = isSearchResult(skill) ? skill.tags : [];
  const cachedDetail = isInstalledSkill(skill) ? skill.detail ?? null : null;
  const [detail, setDetail] = useState<SkillDetail | null>(cachedDetail);
  const actionLabels = [
    t('skillhub.install'),
    t('skillhub.installing'),
    t('skillhub.installed'),
    t('skillhub.uninstall'),
  ];
  const actionButtonWidth = `${Math.max(...actionLabels.map((label) => label.length)) + 4}ch`;

  const avatarBg = getAvatarColor(skill.slug);
  const avatarText = getAvatarTextColor(skill.slug);

  useEffect(() => {
    if (!expanded || detail) return;

    let cancelled = false;
    getSkillDetail(skill.slug)
      .then((nextDetail) => {
        if (!cancelled) setDetail(nextDetail);
      })
      .catch(() => { });

    return () => {
      cancelled = true;
    };
  }, [detail, expanded, skill.slug]);

  useEffect(() => {
    setDetail(cachedDetail);
  }, [cachedDetail, skill.slug]);

  return (
    <div className={isInstalledMode ? 'skill-row-clip' : ''}>
    <div
      className={`w-full rounded-xl overflow-hidden transition-[background-color,border-color,box-shadow] duration-200 ${isInstalledMode ? 'skill-installed-item' : ''} ${isRemoving ? 'removing' : ''}`}
      style={{
        background: expanded ? '#FFFFFF' : 'transparent',
        border: '1px solid',
        borderColor: expanded ? 'rgba(0, 0, 0, 0.08)' : 'transparent',
        boxShadow: expanded ? '0 1px 4px rgba(0,0,0,0.06)' : 'none',
      }}
    >
      <div
        className="flex items-center gap-3 w-full py-3 px-5 transition-all duration-200 hover:bg-[#F9F9F9] cursor-pointer"
        style={{ borderRadius: expanded ? 0 : undefined }}
        onClick={() => onToggle?.()}
      >
        <div
          className="w-10 h-10 rounded-[8px] flex items-center justify-center shrink-0 font-bold text-[14px]"
          style={{ background: avatarBg, color: avatarText }}
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

        <div className="min-w-0 flex-1 flex flex-col gap-1">
          <span
            className="text-[14px] font-medium truncate"
            style={{ color: 'rgba(0, 0, 0, 0.9)', letterSpacing: '-0.015em', lineHeight: '1.57em' }}
          >
            {displayName}
          </span>
          <div className="flex items-center gap-2">
            {tags.length > 0 && (
              <span
                className="shrink-0 text-[10px] font-normal px-2 h-5 inline-flex items-center rounded-xl leading-none"
                style={{ border: '1px solid rgba(0, 0, 0, 0.08)', color: 'rgba(0, 0, 0, 0.4)' }}
              >
                {tags[0]}
              </span>
            )}
            {!isInstalledMode && (
              <>
                {stars > 0 && (
                  <span className="inline-flex items-center gap-1 text-[12px] font-light tabular-nums" style={{ color: 'rgba(0, 0, 0, 0.4)', letterSpacing: '-0.015em', lineHeight: '1.67em' }}>
                    <Star className="h-3 w-3" style={{ color: 'rgba(0, 0, 0, 0.4)' }} />
                    {stars}
                  </span>
                )}
                <span className="inline-flex items-center gap-1 text-[12px] font-light tabular-nums" style={{ color: 'rgba(0, 0, 0, 0.4)', letterSpacing: '-0.015em', lineHeight: '1.67em' }}>
                  <Download className="h-3 w-3" style={{ color: 'rgba(0, 0, 0, 0.4)' }} />
                  {formatDownloads(downloads)}
                </span>
              </>
            )}
            {isInstalledMode && description && (
              <span className="text-[12px] font-light truncate" style={{ color: 'rgba(0, 0, 0, 0.4)' }}>
                {description}
              </span>
            )}
          </div>
        </div>

        <ChevronDown
          className="h-4 w-4 shrink-0 transition-transform duration-300 ease-out"
          style={{
            color: 'rgba(0,0,0,0.25)',
            transform: expanded ? 'rotate(180deg)' : 'rotate(0deg)',
          }}
        />

        {!isInstalledMode && (
          <button
            onClick={(e) => { e.stopPropagation(); onInstall(); }}
            disabled={isInstalling || isUninstalling || isInstalled}
            className="px-3 py-1.5 rounded-md text-xs font-semibold transition-all shrink-0 inline-flex items-center justify-center gap-1.5 whitespace-nowrap"
            style={
              isInstalled
                ? {
                  width: actionButtonWidth,
                  background: 'rgba(0, 230, 138, 0.08)',
                  color: 'var(--color-status-success)',
                  border: '1px solid rgba(0, 230, 138, 0.2)',
                  cursor: 'default',
                }
                : isInstalling
                  ? {
                    width: actionButtonWidth,
                    background: 'var(--pc-accent)',
                    color: 'white',
                    opacity: 0.7,
                    cursor: 'wait',
                  }
                  : {
                    width: actionButtonWidth,
                    background: 'var(--pc-accent)',
                    color: 'white',
                    border: 'none',
                    cursor: 'pointer',
                  }
            }
          >
            {isInstalling ? (
              <>
                <Loader2 className="h-3 w-3 animate-spin" />
                {t('skillhub.installing')}
              </>
            ) : isInstalled ? (
              t('skillhub.installed')
            ) : (
              t('skillhub.install')
            )}
          </button>
        )}
        {isInstalledMode && onUninstall && (
          <button
            onClick={(e) => { e.stopPropagation(); onUninstall(); }}
            disabled={isUninstalling || isRemoving}
            className="px-3 py-1.5 rounded-md text-xs font-semibold transition-all shrink-0 inline-flex items-center justify-center gap-1.5 whitespace-nowrap"
            style={{
              width: actionButtonWidth,
              color: 'var(--color-status-error)',
              border: '1px solid rgba(239, 68, 68, 0.2)',
              background: 'rgba(239, 68, 68, 0.06)',
              cursor: isUninstalling || isRemoving ? 'wait' : 'pointer',
              opacity: isUninstalling || isRemoving ? 0.5 : 1,
            }}
          >
            {isUninstalling ? (
              <Loader2 className="h-3 w-3 animate-spin" />
            ) : (
              <Trash2 className="h-3 w-3" />
            )}
            {t('skillhub.uninstall')}
          </button>
        )}
      </div>

      <div className={`skill-detail-wrapper ${expanded ? 'expanded' : ''}`}>
        <div>
          <SkillDetailPanel active={expanded} detail={detail} />
        </div>
      </div>
    </div>
    </div>
  );
}
