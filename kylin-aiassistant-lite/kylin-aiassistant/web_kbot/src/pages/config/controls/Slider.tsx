import type { CSSProperties } from 'react';

interface SliderProps {
  value: number;
  onChange: (v: number) => void;
  min: number;
  max: number;
  step?: number;
  disabled?: boolean;
}

function formatEdge(n: number, step: number): string {
  if (step < 1) {
    return Number.isInteger(n) ? n.toFixed(1) : n.toFixed(1);
  }
  return String(Math.round(n));
}

export default function Slider({ value, onChange, min, max, step = 1, disabled }: SliderProps) {
  const pct = max === min ? 0 : ((value - min) / (max - min)) * 100;

  return (
    <div
      className={`flex items-center gap-2 min-w-0 ${disabled ? 'opacity-40' : ''}`}
    >
      <span className="text-xs font-mono tabular-nums flex-shrink-0 w-9 text-left" style={{ color: '#525252' }}>
        {formatEdge(min, step)}
      </span>
      <div className="w-[410px] shrink-0">
        <input
          type="range"
          min={min}
          max={max}
          step={step}
          value={value}
          disabled={disabled}
          onChange={(e) => onChange(Number(e.target.value))}
          className="config-form-slider w-full cursor-pointer disabled:cursor-not-allowed"
          style={{ '--slider-pct': `${pct}%` } as CSSProperties}
        />
      </div>
      <span className="text-xs font-mono tabular-nums flex-shrink-0 w-9 text-right" style={{ color: '#525252' }}>
        {formatEdge(max, step)}
      </span>
    </div>
  );
}
