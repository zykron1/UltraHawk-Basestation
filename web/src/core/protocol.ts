import type { Command, TelemetryFrame, VehicleMode } from './types';

const VALID_MODES: VehicleMode[] = ['IDLE', 'TELEOP', 'AUTON'];

function isFiniteNumber(value: number): boolean {
  return Number.isFinite(value);
}

function parseMode(value: number): VehicleMode | null {
  const mode = VALID_MODES[value];
  return mode ?? null;
}

export function parseLegacyTelemetry(line: string, receivedAt = Date.now()): TelemetryFrame | null {
  const match = line.trim().match(
    /^DP\s+(\d+)\s+t=([^\s]+)\s+(?:roll=)?([^\s]+)\s+(?:pitch=)?([^\s]+)\s+(?:yaw=)?([^\s]+)\s+state=(\d+)\s+motor=([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)(?:\s+.*)?$/,
  );

  if (!match) return null;

  const sequence = Number(match[1]);
  const missionTime = Number(match[2]);
  const roll = Number(match[3]);
  const pitch = Number(match[4]);
  const yaw = Number(match[5]);
  const mode = parseMode(Number(match[6]));
  const motors = [Number(match[7]), Number(match[8]), Number(match[9]), Number(match[10])] as [
    number,
    number,
    number,
    number,
  ];

  if (
    !Number.isSafeInteger(sequence) ||
    !isFiniteNumber(missionTime) ||
    !isFiniteNumber(roll) ||
    !isFiniteNumber(pitch) ||
    !isFiniteNumber(yaw) ||
    !mode ||
    motors.some((motor) => !isFiniteNumber(motor))
  ) {
    return null;
  }

  return {
    sequence,
    missionTime,
    receivedAt,
    mode,
    attitude: { roll, pitch, yaw },
    motors,
  };
}

export function formatLegacyCommand(command: Command): string {
  const mode = VALID_MODES.indexOf(command.mode);
  return [
    mode,
    command.thrust,
    command.roll,
    command.pitch,
    command.yaw,
    command.xpos,
    command.ypos,
    command.zpos,
  ]
    .map((value) => Number(value).toFixed(4))
    .join(' ');
}

export function summarizeCommand(command: Command): string {
  return `${command.mode} / T ${command.thrust.toFixed(1)} / P ${command.pitch.toFixed(1)} / R ${command.roll.toFixed(1)}`;
}
