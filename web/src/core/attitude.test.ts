import { describe, expect, it } from 'vitest';
import { attitudeToEuler, dampAngle } from './attitude';

describe('vehicle attitude mapping', () => {
  it('maps degrees into the documented body-frame Euler order', () => {
    const euler = attitudeToEuler({ roll: 10, pitch: -20, yaw: 30 });

    expect(euler.order).toBe('YXZ');
    expect(euler.x).toBeCloseTo(-20 * Math.PI / 180);
    expect(euler.y).toBeCloseTo(30 * Math.PI / 180);
    expect(euler.z).toBeCloseTo(10 * Math.PI / 180);
  });

  it('takes the shortest route across the heading wrap', () => {
    const result = dampAngle(359 * Math.PI / 180, 1 * Math.PI / 180, 10, 1);

    expect(result).toBeGreaterThan(359 * Math.PI / 180);
  });
});
