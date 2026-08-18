import { formatLegacyCommand } from './protocol';
import { TransportEvents } from './transport';
import type { LineTransport } from './transport';
import type { Command, TransportStatus } from './types';

export class SimulatorTransport implements LineTransport {
  readonly label = 'Flight simulator';
  private readonly events = new TransportEvents();
  private timer: number | null = null;
  private startedAt = 0;
  private sequence = 0;
  private command: Command = {
    mode: 'IDLE',
    thrust: 0,
    roll: 0,
    pitch: 0,
    yaw: 0,
    xpos: 0,
    ypos: 0,
    zpos: 0,
  };

  onLine(listener: (line: string) => void): () => void {
    return this.events.onLine(listener);
  }

  onStatus(listener: (status: TransportStatus, detail?: string) => void): () => void {
    return this.events.onStatus(listener);
  }

  async connect(): Promise<void> {
    if (this.timer !== null) return;
    this.startedAt = performance.now();
    this.events.emitStatus('connecting', 'Starting deterministic simulator');
    await new Promise((resolve) => window.setTimeout(resolve, 240));
    this.events.emitStatus('connected', 'Synthetic telemetry at 20 Hz');
    this.timer = window.setInterval(() => this.emitTelemetry(), 50);
  }

  async disconnect(): Promise<void> {
    if (this.timer !== null) window.clearInterval(this.timer);
    this.timer = null;
    this.events.emitStatus('disconnected');
  }

  async write(line: string): Promise<void> {
    const values = line.trim().split(/\s+/).map(Number);
    if (values.length !== 8 || values.some((value) => !Number.isFinite(value))) {
      throw new Error('Simulator rejected malformed command');
    }

    const modes = ['IDLE', 'TELEOP', 'AUTON'] as const;
    this.command = {
      mode: modes[values[0]] ?? 'IDLE',
      thrust: values[1],
      roll: values[2],
      pitch: values[3],
      yaw: values[4],
      xpos: values[5],
      ypos: values[6],
      zpos: values[7],
    };
    this.events.emitLine(`ACK command accepted ${formatLegacyCommand(this.command)}`);
  }

  private emitTelemetry(): void {
    const elapsed = (performance.now() - this.startedAt) / 1000;
    const attitudeRoll = this.command.mode === 'TELEOP' ? this.command.roll * 0.55 : Math.sin(elapsed * 0.8) * 9;
    const attitudePitch = this.command.mode === 'TELEOP' ? this.command.pitch * 0.55 : Math.cos(elapsed * 0.64) * 5;
    const attitudeYaw = (180 + elapsed * 13 + this.command.yaw * 0.2) % 360;
    const motorBias = this.command.mode === 'IDLE' ? 0.18 : 0.46 + this.command.thrust / 250;
    const rollMix = this.command.roll / 350;
    const pitchMix = this.command.pitch / 350;
    const motors = [
      motorBias + rollMix - pitchMix,
      motorBias + rollMix + pitchMix,
      motorBias - rollMix + pitchMix,
      motorBias - rollMix - pitchMix,
    ].map((value) => Math.max(0, Math.min(1, value)));

    this.sequence += 1;
    this.events.emitLine(
      `DP ${this.sequence} t=${elapsed.toFixed(3)} ${attitudeRoll.toFixed(3)} ${attitudePitch.toFixed(3)} ${attitudeYaw.toFixed(3)} state=${['IDLE', 'TELEOP', 'AUTON'].indexOf(this.command.mode)} motor=${motors.map((value) => value.toFixed(3)).join(' ')}`,
    );
  }
}
