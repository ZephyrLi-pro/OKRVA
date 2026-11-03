import { describe, expect, it, vi, beforeEach, afterEach } from 'vitest';
import { clampFixedTooltipPosition } from './modelTooltips';

describe('clampFixedTooltipPosition', () => {
  beforeEach(() => {
    vi.stubGlobal('innerWidth', 800);
    vi.stubGlobal('innerHeight', 600);
  });

  afterEach(() => {
    vi.unstubAllGlobals();
  });

  it('keeps default offset when tooltip fits', () => {
    const pos = clampFixedTooltipPosition({ x: 100, y: 100 }, { width: 200, height: 40 });
    expect(pos).toEqual({ left: 104, top: 116 });
  });

  it('flips left when overflowing right edge', () => {
    const pos = clampFixedTooltipPosition({ x: 750, y: 100 }, { width: 300, height: 40 });
    expect(pos.left + 300).toBeLessThanOrEqual(800 - 12);
    expect(pos.left).toBe(750 - 300 - 4);
  });

  it('flips up when overflowing bottom edge', () => {
    const pos = clampFixedTooltipPosition({ x: 100, y: 580 }, { width: 200, height: 80 });
    expect(pos.top + 80).toBeLessThanOrEqual(600 - 12);
    expect(pos.top).toBe(580 - 80 - 16);
  });
});
