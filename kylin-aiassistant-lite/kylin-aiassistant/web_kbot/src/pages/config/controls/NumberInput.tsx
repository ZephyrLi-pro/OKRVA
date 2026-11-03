/** Public icons must respect Vite `base` (dev `/`, prod `/_app/` — see vite.config.ts). */
const ICON_UP = `${import.meta.env.BASE_URL}icons/ukui-up-symbolic.svg`;
const ICON_DOWN = `${import.meta.env.BASE_URL}icons/ukui-down-symbolic.svg`;

interface NumberInputProps {
  value: number;
  onChange: (v: number) => void;
  min?: number;
  max?: number;
  step?: number;
  disabled?: boolean;
}

function clamp(n: number, min?: number, max?: number): number {
  let x = n;
  if (min !== undefined) x = Math.max(min, x);
  if (max !== undefined) x = Math.min(max, x);
  return x;
}

export default function NumberInput({ value, onChange, min, max, step = 1, disabled }: NumberInputProps) {
  const s = step ?? 1;

  const bump = (dir: 1 | -1) => {
    onChange(clamp(value + dir * s, min, max));
  };

  const atMin = min !== undefined && value <= min;
  const atMax = max !== undefined && value >= max;

  return (
    <div
      className="config-form-number-wrap box-border flex shrink-0 items-stretch overflow-hidden rounded-md pr-2"
      style={{ width: 468, height: 36, background: '#e6e6e6' }}
    >
      <input
        type="number"
        value={value}
        min={min}
        max={max}
        step={step}
        disabled={disabled}
        onChange={(e) => {
          const n = Number(e.target.value);
          if (!Number.isNaN(n)) onChange(clamp(n, min, max));
        }}
        className="config-form-number-input min-h-0 min-w-0 flex-1 border-0 bg-transparent py-0 pl-3 pr-2 font-mono text-sm outline-none disabled:opacity-50"
        style={{ color: '#1a1a1a' }}
      />
      <div className="flex min-w-[28px] shrink-0 flex-col gap-0" aria-hidden>
        <button
          type="button"
          disabled={disabled || atMax}
          className="flex flex-1 items-center justify-center border-0 bg-transparent outline-none hover:bg-black/[0.04] focus:outline-none focus-visible:outline-none disabled:cursor-not-allowed disabled:opacity-30"
          aria-label="Increase"
          onClick={() => bump(1)}
        >
          <img src={ICON_UP} alt="" width={16} height={16} className="pointer-events-none block" draggable={false} />
        </button>
        <button
          type="button"
          disabled={disabled || atMin}
          className="flex flex-1 items-center justify-center border-0 bg-transparent outline-none hover:bg-black/[0.04] focus:outline-none focus-visible:outline-none disabled:cursor-not-allowed disabled:opacity-30"
          aria-label="Decrease"
          onClick={() => bump(-1)}
        >
          <img src={ICON_DOWN} alt="" width={16} height={16} className="pointer-events-none block" draggable={false} />
        </button>
      </div>
    </div>
  );
}
