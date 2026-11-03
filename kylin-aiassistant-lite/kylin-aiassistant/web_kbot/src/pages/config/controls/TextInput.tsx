import { useState } from 'react';
import { Eye, EyeOff } from 'lucide-react';

interface TextInputProps {
  value: string;
  onChange: (v: string) => void;
  placeholder?: string;
  disabled?: boolean;
  masked?: boolean;
  /** Match config form control size (#e6e6e6, 468×36) */
  variant?: 'default' | 'config';
}

export default function TextInput({ value, onChange, placeholder, disabled, masked, variant = 'default' }: TextInputProps) {
  const [revealed, setRevealed] = useState(false);
  const isMaskedValue = value === '***MASKED***';
  const showAsPassword = masked && !revealed;

  const configStyle = variant === 'config'
    ? {
        width: 'min(468px, 100%)' as const,
        maxWidth: '100%' as const,
        height: 36,
        background: '#e6e6e6',
        color: '#1a1a1a',
      }
    : undefined;

  return (
    <div
      className="relative flex items-center min-w-0"
      style={variant === 'config' ? { width: 'min(468px, 100%)', maxWidth: '100%' } : undefined}
    >
      <input
        type={showAsPassword ? 'password' : 'text'}
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        disabled={disabled}
        className={
          variant === 'config'
            ? 'rounded-md border-0 text-sm px-3 font-mono outline-none w-full disabled:opacity-50'
            : 'input-electric text-sm px-3 py-1.5 w-52 font-mono'
        }
        style={
          variant === 'config'
            ? { ...(configStyle ?? {}), ...(isMaskedValue ? { color: 'var(--pc-text-muted)' } : {}) }
            : isMaskedValue ? { color: 'var(--pc-text-muted)' } : undefined
        }
      />
      {masked && (
        <button
          type="button"
          onClick={() => setRevealed(!revealed)}
          className="absolute right-2 p-0.5"
          style={{ color: 'var(--pc-text-muted)' }}
        >
          {revealed ? <EyeOff className="h-3.5 w-3.5" /> : <Eye className="h-3.5 w-3.5" />}
        </button>
      )}
    </div>
  );
}
