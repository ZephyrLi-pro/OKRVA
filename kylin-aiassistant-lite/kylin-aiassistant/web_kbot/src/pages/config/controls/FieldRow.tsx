import type { ReactNode } from 'react';

interface FieldRowProps {
  label: string;
  description?: string;
  children: ReactNode;
}

export default function FieldRow({ label, description, children }: FieldRowProps) {
  return (
    <div
      className="flex items-center justify-between gap-4 px-4 min-h-[68px] w-full min-w-0 flex-shrink-0 box-border"
      style={{ background: '#F6F6F6' }}
    >
      <div className="flex-1 min-w-0 py-2">
        <div className="text-sm font-semibold" style={{ color: '#1a1a1a' }}>{label}</div>
        {description && (
          <div className="text-xs mt-0.5 leading-snug" style={{ color: '#737373' }}>{description}</div>
        )}
      </div>
      <div className="flex-shrink-0 flex items-center justify-end min-w-0">{children}</div>
    </div>
  );
}
