import { useState, useCallback, useRef, useEffect } from 'react';
import { t } from '@/lib/i18n';
import SkillRow from './SkillRow';
import type { SkillHubSkill } from './types';

const LOAD_MORE_COUNT = 10;

interface SkillCardGridProps {
  skills: SkillHubSkill[];
  installedSlugs: Set<string>;
  installingSlug: string | null;
  onInstall: (slug: string) => void;
  isInstalledMode?: boolean;
  onUninstall?: (slug: string) => void;
  uninstallingSlug?: string | null;
  removingSlug?: string | null;
  pageSize?: number;
}

export default function SkillCardGrid({
  skills,
  installedSlugs,
  installingSlug,
  onInstall,
  isInstalledMode = false,
  onUninstall,
  uninstallingSlug,
  removingSlug,
  pageSize,
}: SkillCardGridProps) {
  const [expandedSlugs, setExpandedSlugs] = useState<Set<string>>(new Set());
  const [visibleCount, setVisibleCount] = useState(pageSize ?? Infinity);
  const scrollRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    setVisibleCount(pageSize ?? Infinity);
    setExpandedSlugs(new Set());
  }, [skills, pageSize]);

  const handleScroll = useCallback(() => {
    if (!pageSize) return;
    const el = scrollRef.current;
    if (!el) return;
    if (el.scrollHeight - el.scrollTop - el.clientHeight < 50) {
      setVisibleCount((prev) => Math.min(prev + LOAD_MORE_COUNT, skills.length));
    }
  }, [pageSize, skills.length]);

  const toggleExpand = (slug: string) => {
    setExpandedSlugs((prev) => {
      const next = new Set(prev);
      if (next.has(slug)) next.delete(slug);
      else next.add(slug);
      return next;
    });
  };

  if (skills.length === 0) {
    return (
      <div
        className="p-8 text-center"
        style={{ background: '#FFFFFF', border: '1px solid rgba(0, 0, 0, 0.08)', borderRadius: 12 }}
      >
        <p style={{ color: 'rgba(0, 0, 0, 0.4)' }}>
          {isInstalledMode ? t('skillhub.empty.installed') : t('skillhub.empty.skills')}
        </p>
      </div>
    );
  }

  const visibleSkills = pageSize ? skills.slice(0, visibleCount) : skills;
  const hasMore = pageSize ? visibleCount < skills.length : false;

  return (
    <div
      style={{
        background: '#FFFFFF',
        border: '1px solid rgba(0, 0, 0, 0.08)',
        borderRadius: 12,
        overflow: 'hidden',
        display: 'flex',
        flexDirection: 'column',
        flex: 1,
        minHeight: 0,
      }}
    >
      <div
        ref={scrollRef}
        onScroll={handleScroll}
        className="flex-1 min-h-0 overflow-y-auto scrollbar-auto-hide"
        style={{ paddingLeft: 8, paddingRight: 8, paddingTop: 8, paddingBottom: 8 }}
      >
        {visibleSkills.map((skill) => (
          <SkillRow
            key={skill.slug}
            skill={skill}
            isInstalled={installedSlugs.has(skill.slug)}
            isInstalling={installingSlug === skill.slug}
            onInstall={() => onInstall(skill.slug)}
            isInstalledMode={isInstalledMode}
            expanded={expandedSlugs.has(skill.slug)}
            onToggle={() => toggleExpand(skill.slug)}
            onUninstall={onUninstall ? () => onUninstall(skill.slug) : undefined}
            isUninstalling={uninstallingSlug === skill.slug}
            isRemoving={removingSlug === skill.slug}
          />
        ))}
        {hasMore && (
          <div
            style={{
              textAlign: 'center',
              padding: '8px 0 4px',
              color: 'rgba(0, 0, 0, 0.25)',
              fontSize: 12,
            }}
          >
            {t('skillhub.scroll_more')}
          </div>
        )}
      </div>
    </div>
  );
}
