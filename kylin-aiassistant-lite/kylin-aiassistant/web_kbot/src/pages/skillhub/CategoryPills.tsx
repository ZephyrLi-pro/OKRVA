import { t } from '@/lib/i18n';

interface CategoryPillsProps {
  active: string;
  onChange: (category: string) => void;
  installedCount?: number;
  disabled?: boolean;
}

const categoryKeys = ['hot', 'featured', 'newest', 'installed'] as const;

export default function CategoryPills({ active, onChange, installedCount, disabled }: CategoryPillsProps) {
  return (
    <div className="flex gap-2 overflow-x-auto pb-1">
      {categoryKeys.map((key) => {
        const label = key === 'installed'
          ? t('skillhub.tab.installed_count').replace('{count}', String(installedCount ?? 0))
          : t(`skillhub.tab.${key}`);
        return (
          <button
            key={key}
            onClick={() => onChange(key)}
            disabled={disabled}
            className="px-4 py-2.5 rounded-[8px] text-[13px] font-medium transition-all whitespace-nowrap"
            style={
              active === key
                ? { background: 'rgba(0, 0, 0, 0.9)', color: '#FFFFFF', opacity: disabled ? 0.5 : 1 }
                : {
                    color: 'rgba(0, 0, 0, 0.55)',
                    background: 'rgba(0, 0, 0, 0.04)',
                    border: 'none',
                    opacity: disabled ? 0.5 : 1,
                    cursor: disabled ? 'not-allowed' : 'pointer',
                  }
            }
          >
            {label}
          </button>
        );
      })}
    </div>
  );
}
