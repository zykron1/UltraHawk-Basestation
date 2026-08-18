import { clamp } from './types';

export function mapHorizontalToValue(
  clientX: number,
  bounds: { left: number; width: number },
  min = -180,
  max = 180,
): number {
  const position = clamp((clientX - bounds.left) / bounds.width, 0, 1);
  return Math.round(min + position * (max - min));
}
