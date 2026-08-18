import { describe, expect, it } from 'vitest';
import { mapHorizontalToValue } from './controlMapping';

describe('teleop control mapping', () => {
  it('maps a centered track across arbitrary command ranges', () => {
    const bounds = { left: 10, width: 200 };

    expect(mapHorizontalToValue(10, bounds, -30, 30)).toBe(-30);
    expect(mapHorizontalToValue(110, bounds, -30, 30)).toBe(0);
    expect(mapHorizontalToValue(210, bounds, -30, 30)).toBe(30);
  });

  it('clamps pointer input to the track ends', () => {
    const bounds = { left: 10, width: 200 };

    expect(mapHorizontalToValue(-40, bounds, -100, 100)).toBe(-100);
    expect(mapHorizontalToValue(400, bounds, -100, 100)).toBe(100);
  });
});
