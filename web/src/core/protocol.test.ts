import { describe, expect, it } from 'vitest';
import { formatLegacyCommand, parseLegacyTelemetry } from './protocol';

describe('legacy telemetry protocol', () => {
  it('normalizes a valid data packet', () => {
    const frame = parseLegacyTelemetry(
      'DP 42 t=12.345000 1.2 -3.4 180.0 state=1 motor=0.4 0.5 0.6 0.7',
      9876,
    );

    expect(frame).toEqual({
      sequence: 42,
      missionTime: 12.345,
      receivedAt: 9876,
      mode: 'TELEOP',
      attitude: { roll: 1.2, pitch: -3.4, yaw: 180 },
      motors: [0.4, 0.5, 0.6, 0.7],
    });
  });

  it('parses firmware telemetry with labeled attitude fields', () => {
    const frame = parseLegacyTelemetry(
      'DP 2176 t=108.880 roll=2.80 pitch=0.19 yaw=-1.61 state=0 motor=1000.0 1000.0 1000.0 1000.0',
      9876,
    );

    expect(frame).toEqual({
      sequence: 2176,
      missionTime: 108.88,
      receivedAt: 9876,
      mode: 'IDLE',
      attitude: { roll: 2.8, pitch: 0.19, yaw: -1.61 },
      motors: [1000, 1000, 1000, 1000],
    });
  });

  it('ignores diagnostics and malformed packets', () => {
    expect(parseLegacyTelemetry('DEBUG Received: 1 0 0 0')).toBeNull();
    expect(parseLegacyTelemetry('DP 2 t=1.0 1.0 2.0 state=1 motor=0.1 0.2 0.3 0.4')).toBeNull();
    expect(parseLegacyTelemetry('DP 2 t=1.0 1.0 2.0 3.0 state=7 motor=0.1 0.2 0.3 0.4')).toBeNull();
  });

  it('formats the firmware command field order', () => {
    expect(formatLegacyCommand({
      mode: 'AUTON',
      thrust: 3,
      roll: -4.5,
      pitch: 6,
      yaw: 7.25,
      xpos: 8,
      ypos: 9,
      zpos: -10,
    })).toBe('2.0000 3.0000 -4.5000 6.0000 7.2500 8.0000 9.0000 -10.0000');
  });
});
