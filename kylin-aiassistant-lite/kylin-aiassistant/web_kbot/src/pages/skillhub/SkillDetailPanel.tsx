import { useEffect, useState } from 'react';
import { ShieldCheck, Tag } from 'lucide-react';
import { t } from '@/lib/i18n';
import type { SkillDetail } from './types';

function formatDownloads(n: number): string {
  if (n >= 10_000) return `${(n / 10_000).toFixed(1)} 万`;
  if (n >= 1_000) return `${(n / 1_000).toFixed(1)} 千`;
  return String(n);
}

interface SkillDetailPanelProps {
  active: boolean;
  detail: SkillDetail | null;
}

export default function SkillDetailPanel({ active, detail }: SkillDetailPanelProps) {
  const [showContent, setShowContent] = useState(false);

  useEffect(() => {
    if (!active) {
      setShowContent(false);
      return;
    }
    if (detail) {
      requestAnimationFrame(() => {
        requestAnimationFrame(() => {
          setShowContent(true);
        });
      });
    }
  }, [active, detail]);

  const hasData = detail !== null;

  return (
    <div>
      <div
        style={{
          opacity: showContent && active ? 1 : 0,
          transform: showContent && active ? 'translateY(0)' : 'translateY(4px)',
          transition: 'opacity 0.55s ease-out, transform 0.55s cubic-bezier(0.16, 1, 0.3, 1)',
          willChange: 'opacity, transform',
        }}
      >
        {hasData ? (
          <div className="pt-3 pb-4 px-4 space-y-4" style={{ borderTop: '1px solid rgba(0,0,0,0.06)' }}>
            <ContentBody detail={detail!} />
          </div>
        ) : (
          <div className="py-6" />
        )}
      </div>
    </div>
  );
}

function ContentBody({ detail }: { detail: SkillDetail }) {
  const { skill, latestVersion, securityReports } = detail;

  const isSafe = securityReports.keen.status === 'benign' || securityReports.sanbu.status === 'benign';
  const author = (detail.owner.displayName || detail.owner.handle || '').trim();

  const sourceLabel = skill.source === 'clawhub'
    ? t('skillhub.detail.source_clawhub')
    : t('skillhub.detail.source').replace('{source}', skill.source);

  return (
    <>
      {author && (
        <div
          className="inline-flex max-w-full items-center rounded-xl px-2.5 py-1 text-[12px] font-medium"
          title={t('skillhub.author').replace('{author}', author)}
          aria-label={t('skillhub.author').replace('{author}', author)}
          style={{ background: 'rgba(0, 0, 0, 0.035)', color: 'rgba(0, 0, 0, 0.55)' }}
        >
          <span className="truncate">{t('skillhub.author').replace('{author}', author)}</span>
        </div>
      )}

      <p
        className="text-[13px] leading-relaxed"
        style={{ color: 'rgba(0,0,0,0.55)', letterSpacing: '-0.01em', whiteSpace: 'pre-line' }}
      >
        {skill.summary_zh || skill.summary}
      </p>

      <div className="flex items-stretch gap-3 py-1">
        {[
          { label: sourceLabel, sub: '' },
          { label: `V ${latestVersion.version}`, sub: '' },
          { label: isSafe ? t('skillhub.detail.safe') : t('skillhub.detail.unsafe'), sub: '', safe: isSafe },
          { label: formatDownloads(skill.stats.downloads), sub: t('skillhub.detail.downloads') },
          { label: `${skill.stats.stars}`, sub: t('skillhub.detail.stars') },
        ].map((item, i) => (
          <div
            key={i}
            className="flex-1 flex flex-col items-center justify-center gap-1 py-2 rounded-lg"
            style={{ background: 'rgba(0,0,0,0.02)' }}
          >
            {i === 2 ? (
              <ShieldCheck className="h-4 w-4" style={{ color: item.safe ? '#52c41a' : 'rgba(0,0,0,0.25)' }} />
            ) : i === 1 ? (
              <Tag className="h-4 w-4" style={{ color: 'rgba(0,0,0,0.35)' }} />
            ) : i === 0 ? (
              <span className="text-[11px]" style={{ color: 'rgba(0,0,0,0.25)' }}>🏆</span>
            ) : (
              <span className="text-[11px]" style={{ color: 'rgba(0,0,0,0.25)' }}>
                {i === 3 ? '📥' : '⭐'}
              </span>
            )}
            <span
              className="text-[13px] font-medium"
              style={{ color: item.safe !== undefined ? (item.safe ? '#52c41a' : 'rgba(0,0,0,0.65)') : 'rgba(0,0,0,0.65)', letterSpacing: '-0.01em' }}
            >
              {item.label}
            </span>
            {item.sub && (
              <span className="text-[11px]" style={{ color: 'rgba(0,0,0,0.3)' }}>{item.sub}</span>
            )}
          </div>
        ))}
      </div>

      {isSafe && (
        <div
          className="rounded-xl p-3 space-y-2.5"
          style={{ background: 'rgba(82,196,26,0.04)', border: '1px solid rgba(82,196,26,0.12)' }}
        >
          <div className="flex items-center gap-1.5">
            <ShieldCheck className="h-4 w-4" style={{ color: '#52c41a' }} />
            <span className="text-[13px] font-medium" style={{ color: '#52c41a' }}>{t('skillhub.detail.security_check')}</span>
          </div>

          <div className="flex gap-3">
            {[
              { lab: t('skillhub.detail.lab_keen'), report: securityReports.keen },
              { lab: t('skillhub.detail.lab_sanbu'), report: securityReports.sanbu },
            ].map(({ lab, report }) => (
              <div
                key={lab}
                className="flex-1 flex items-center gap-2 rounded-lg px-3 py-2"
                style={{ background: 'white', boxShadow: '0 1px 3px rgba(0,0,0,0.04)' }}
              >
                <span className="text-[13px] font-medium shrink-0" style={{ color: 'rgba(0,0,0,0.75)' }}>{lab}</span>
                <span
                  className="inline-flex items-center gap-1 text-[12px] px-1.5 py-0.5 rounded-full shrink-0"
                  style={{
                    background: report.status === 'benign' ? 'rgba(82,196,26,0.1)' : 'rgba(250,173,20,0.1)',
                    color: report.status === 'benign' ? '#52c41a' : '#fa8c16',
                    fontWeight: 500,
                  }}
                >
                  ● {report.statusText}
                </span>
              </div>
            ))}
          </div>

          <p
            className="text-[11px] leading-relaxed"
            style={{ color: 'rgba(0,0,0,0.3)', whiteSpace: 'pre-line' }}
          >
            {t('skillhub.detail.security_disclaimer')}
          </p>
        </div>
      )}
    </>
  );
}
