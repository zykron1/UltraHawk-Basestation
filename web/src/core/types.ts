export type VehicleMode = 'IDLE' | 'TELEOP' | 'AUTON';

export type TransportStatus =
  | 'disconnected'
  | 'connecting'
  | 'connected'
  | 'error';

export interface Attitude {
  roll: number;
  pitch: number;
  yaw: number;
}

export interface TelemetryFrame {
  sequence: number;
  missionTime: number;
  receivedAt: number;
  mode: VehicleMode;
  attitude: Attitude;
  motors: [number, number, number, number];
  linkQuality?: number;
}

export interface Command {
  mode: VehicleMode;
  thrust: number;
  roll: number;
  pitch: number;
  yaw: number;
  xpos: number;
  ypos: number;
  zpos: number;
}

export interface CommandRecord {
  id: number;
  sentAt: number;
  status: 'sent' | 'failed';
  summary: string;
}

export const DEFAULT_COMMAND: Command = {
  mode: 'IDLE',
  thrust: 0,
  roll: 0,
  pitch: 0,
  yaw: 0,
  xpos: 0,
  ypos: 0,
  zpos: 0,
};

export const MODE_META: Record<VehicleMode, { label: string; tone: string; description: string }> = {
  IDLE: { label: 'IDLE', tone: 'neutral', description: 'Outputs held at safe idle' },
  TELEOP: { label: 'TELEOP', tone: 'cyan', description: 'Manual setpoints active' },
  AUTON: { label: 'AUTON', tone: 'lime', description: 'Autonomous setpoints active' },
};

export function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

export function formatDegrees(value: number, digits = 1): string {
  return `${value.toFixed(digits)}\u00b0`;
}

export function formatMissionTime(seconds: number): string {
  return `${seconds.toFixed(3)} s`;
}
