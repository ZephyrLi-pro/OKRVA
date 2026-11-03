import { ChevronDown } from 'lucide-react';

interface SelectProps {
  value: string;
  onChange: (v: string) => void;
  options: { value: string; label: string }[];
  disabled?: boolean;
}

/** Fixed 468×36 per design; arrow vertically centered in the control box */
export default function Select({ value, onChange, options, disabled }: SelectProps) {
  return (
    <div
      className="relative shrink-0 overflow-hidden rounded-md"
      style={{ width: 468, height: 36 }}
    >
      <select
        value={value}
        onChange={(e) => onChange(e.target.value)}
        disabled={disabled}
        className="absolute inset-0 w-full rounded-md border-0 text-sm outline-none cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
        style={{
          height: 36,
          paddingLeft: 12,
          paddingRight: 36,
          background: '#e6e6e6',
          color: '#1a1a1a',
          appearance: 'none',
          WebkitAppearance: 'none',
          lineHeight: '36px',
        }}
      >
        {options.map((o) => (
          <option key={o.value} value={o.value}>{o.label}</option>
        ))}
      </select>
      <div
        className="pointer-events-none absolute top-0 bottom-0 right-3 flex items-center justify-center"
        aria-hidden
      >
        <ChevronDown className="h-4 w-4 shrink-0" style={{ color: '#525252' }} />
      </div>
    </div>
  );
}
