import type { ReactNode } from 'react';

interface SettingsSectionProps {
  title: string;
  children: ReactNode;
}

export default function SettingsSection({ title, children }: SettingsSectionProps) {
  return (
    <section className="w-full min-w-0">
      <h3
        className="mb-2 px-4 text-left text-sm font-semibold"
        style={{ color: 'var(--pc-text-primary)' }}
      >
        {title}
      </h3>
      <div
        className="rounded-[6px] overflow-hidden flex w-full min-w-0 flex-col gap-px bg-white"
      >
        {children}
      </div>
    </section>
  );
}
