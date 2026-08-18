import { Euler, MathUtils } from 'three';
import type { Attitude } from './types';

/**
 * Vehicle frame: X is right, Y is up, and -Z points through the nose.
 * Yaw is applied first, then pitch and roll in the body frame.
 */
export function attitudeToEuler(attitude: Attitude): Euler {
  return new Euler(
    MathUtils.degToRad(attitude.pitch),
    MathUtils.degToRad(attitude.yaw),
    MathUtils.degToRad(attitude.roll),
    'YXZ',
  );
}

export function dampAngle(current: number, target: number, smoothing: number, deltaSeconds: number): number {
  const difference = MathUtils.euclideanModulo(target - current + Math.PI, Math.PI * 2) - Math.PI;
  return current + difference * (1 - Math.exp(-smoothing * deltaSeconds));
}
