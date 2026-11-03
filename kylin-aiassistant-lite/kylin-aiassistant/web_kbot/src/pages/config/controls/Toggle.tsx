interface ToggleProps {
  value: boolean;
  onChange: (v: boolean) => void;
  disabled?: boolean;
}

/** Solid #3790FA when on — colors inline so WebKit/Tauri hover rules cannot wash out the blue track. */
export default function Toggle({ value, onChange, disabled }: ToggleProps) {
  const blue = '#3790FA';
  return (
    <button
      type="button"
      role="switch"
      aria-checked={value}
      disabled={disabled}
      onClick={() => onChange(!value)}
      className="relative inline-flex h-6 w-11 shrink-0 items-center rounded-full transition-[opacity,transform] duration-200 focus:outline-none disabled:cursor-not-allowed disabled:opacity-40"
      style={{
        backgroundColor: value ? blue : '#e6e6e6',
        border: value ? `1px solid ${blue}` : '1px solid #e5e5e5',
        opacity: disabled ? 0.4 : 1,
        cursor: disabled ? 'not-allowed' : 'pointer',
      }}
    >
      <span
        className={`inline-block h-4 w-4 rounded-full bg-white transition-transform duration-200 ${value ? '' : 'shadow-[inset_0_0_0_1px_rgba(0,0,0,0.06)]'}`}
        style={{ transform: value ? 'translateX(22px)' : 'translateX(4px)' }}
      />
    </button>
  );
}
