import { lazy, Suspense, useEffect, useRef, useState } from 'react';
import {
  Cable,
  Check,
  ChevronDown,
  CircleStop,
  Keyboard,
  RefreshCw,
  ShieldCheck,
  TriangleAlert,
} from 'lucide-react';
import { AttitudeIndicator } from './components/AttitudeIndicator';
import { MotorPanel } from './components/MotorPanel';
import { SetpointTrack } from './components/SetpointTrack';
import { TelemetryChart } from './components/TelemetryChart';
import { parseLegacyTelemetry, formatLegacyCommand, summarizeCommand } from './core/protocol';
import { WebSerialTransport } from './core/serial';
import { SimulatorTransport } from './core/simulator';
import type { LineTransport } from './core/transport';
import {
  clamp,
  DEFAULT_COMMAND,
  formatDegrees,
  formatMissionTime,
  MODE_META,
} from './core/types';
import type {
  Command,
  TelemetryFrame,
  TransportStatus,
  VehicleMode,
} from './core/types';

const DroneAttitudeView = lazy(() => import('./components/DroneAttitudeView').then((module) => ({ default: module.DroneAttitudeView })));

const MAX_HISTORY = 120;
const INITIAL_TELEMETRY: TelemetryFrame = {
  sequence: 0,
  missionTime: 0,
  receivedAt: 0,
  mode: 'IDLE',
  attitude: { roll: 0, pitch: 0, yaw: 0 },
  motors: [0, 0, 0, 0],
};

function App() {
  const [transportKind, setTransportKind] = useState<'simulator' | 'serial'>('simulator');
  const [transportStatus, setTransportStatus] = useState<TransportStatus>('disconnected');
  const [telemetry, setTelemetry] = useState(INITIAL_TELEMETRY);
  const [history, setHistory] = useState<TelemetryFrame[]>([]);
  const [command, setCommand] = useState<Command>(DEFAULT_COMMAND);
  const [controlEnabled, setControlEnabled] = useState(false);
  const [pressedKeys, setPressedKeys] = useState<string[]>([]);
  const [commandStatus, setCommandStatus] = useState('No command sent');
  const [now, setNow] = useState(Date.now());

  const transportRef = useRef<LineTransport | null>(null);
  const commandRef = useRef(command);
  const statusRef = useRef(transportStatus);
  const pressedKeysRef = useRef(new Set<string>());

  useEffect(() => {
    commandRef.current = command;
  }, [command]);

  useEffect(() => {
    statusRef.current = transportStatus;
  }, [transportStatus]);

  useEffect(() => {
    const timer = window.setInterval(() => setNow(Date.now()), 250);
    return () => window.clearInterval(timer);
  }, []);

  useEffect(() => {
    const transport = transportKind === 'simulator' ? new SimulatorTransport() : new WebSerialTransport(115200);
    transportRef.current = transport;

    const removeLineListener = transport.onLine((line) => {
      console.info('[GroundStation] received line', JSON.stringify(line));
      const frame = parseLegacyTelemetry(line);
      if (frame) {
        console.info('[GroundStation] telemetry parsed', {
          sequence: frame.sequence,
          missionTime: frame.missionTime,
          receivedAt: frame.receivedAt,
        });
        setTelemetry(frame);
        setHistory((previous) => [...previous, frame].slice(-MAX_HISTORY));
        return;
      }

      console.warn('[GroundStation] line was not telemetry', JSON.stringify(line));

      if (line.startsWith('ACK')) {
        setCommandStatus(`ACK / ${line.slice(3).trim()}`);
      }
    });
    const removeStatusListener = transport.onStatus((status, detail) => {
      console.info('[GroundStation] transport status', { status, detail });
      setTransportStatus(status);
      if (status === 'error' && detail) setCommandStatus(`LINK ERROR / ${detail}`);
    });

    return () => {
      removeLineListener();
      removeStatusListener();
      void transport.disconnect();
      transportRef.current = null;
    };
  }, [transportKind]);

  useEffect(() => {
    if (!controlEnabled || command.mode !== 'TELEOP') {
      pressedKeysRef.current.clear();
      setPressedKeys([]);
      return;
    }

    const movementKeys = new Set(['w', 'a', 's', 'd', 'q', 'e']);
    const onKeyDown = (event: KeyboardEvent) => {
      const key = event.key.toLowerCase();
      if (!movementKeys.has(key)) return;
      event.preventDefault();

      const isRepeat = pressedKeysRef.current.has(key);
      pressedKeysRef.current.add(key);
      setPressedKeys([...pressedKeysRef.current]);
      if (isRepeat && !['q', 'e'].includes(key)) return;

      const current = commandRef.current;
      if (key === 'q' || key === 'e') {
        const delta = key === 'q' ? 5 : -5;
        issueCommand({ ...current, thrust: clamp(current.thrust + delta, -100, 100) });
        return;
      }

      const nextPitch = (pressedKeysRef.current.has('w') ? 5 : 0) + (pressedKeysRef.current.has('s') ? -5 : 0);
      const nextRoll = (pressedKeysRef.current.has('d') ? 5 : 0) + (pressedKeysRef.current.has('a') ? -5 : 0);
      issueCommand({ ...current, pitch: nextPitch, roll: nextRoll });
    };
    const onKeyUp = (event: KeyboardEvent) => {
      const key = event.key.toLowerCase();
      if (!movementKeys.has(key)) return;
      event.preventDefault();
      pressedKeysRef.current.delete(key);
      setPressedKeys([...pressedKeysRef.current]);
      if (key === 'q' || key === 'e') return;

      const current = commandRef.current;
      const nextPitch = (pressedKeysRef.current.has('w') ? 5 : 0) + (pressedKeysRef.current.has('s') ? -5 : 0);
      const nextRoll = (pressedKeysRef.current.has('d') ? 5 : 0) + (pressedKeysRef.current.has('a') ? -5 : 0);
      issueCommand({ ...current, pitch: nextPitch, roll: nextRoll });
    };

    window.addEventListener('keydown', onKeyDown);
    window.addEventListener('keyup', onKeyUp);
    return () => {
      window.removeEventListener('keydown', onKeyDown);
      window.removeEventListener('keyup', onKeyUp);
    };
  }, [controlEnabled, command.mode]);

  const telemetryAge = telemetry.receivedAt === 0 ? Number.POSITIVE_INFINITY : now - telemetry.receivedAt;
  const isLive = transportStatus === 'connected' && telemetryAge < 1200;
  const isStale = transportStatus === 'connected' && !isLive;
  const hasTelemetry = telemetry.receivedAt !== 0;
  const connectionLabel = transportStatus === 'connected' ? (isStale ? 'STALE LINK' : 'LIVE LINK') : transportStatus.toUpperCase();
  const connectionTone = transportStatus === 'error' || isStale ? 'amber' : transportStatus === 'connected' ? 'lime' : 'neutral';

  async function connectOrDisconnect(): Promise<void> {
    const transport = transportRef.current;
    if (!transport) return;

    if (transportStatus === 'connected' || transportStatus === 'connecting') {
      await transport.disconnect();
      return;
    }

    try {
      await transport.connect();
    } catch {
      // The transport has already published the useful browser/device error.
    }
  }

  function issueCommand(nextCommand: Command): void {
    commandRef.current = nextCommand;
    setCommand(nextCommand);
    const transport = transportRef.current;
    const summary = summarizeCommand(nextCommand);

    if (!transport || statusRef.current !== 'connected') {
      setCommandStatus('BLOCKED / no live link');
      return;
    }

    void transport.write(`${formatLegacyCommand(nextCommand)}\n`).then(
      () => {
        setCommandStatus(`SENT / ${summary}`);
      },
      (error: unknown) => {
        const detail = error instanceof Error ? error.message : 'write failed';
        setCommandStatus(`FAILED / ${detail}`);
      },
    );
  }

  function chooseMode(mode: VehicleMode): void {
    if (mode !== 'IDLE' && !controlEnabled) return;
    if (mode === 'IDLE') {
      setControlEnabled(false);
      issueCommand({ ...DEFAULT_COMMAND });
      return;
    }
    issueCommand({ ...commandRef.current, mode });
  }

  function toggleControl(): void {
    if (controlEnabled) {
      setControlEnabled(false);
      issueCommand({ ...DEFAULT_COMMAND });
      return;
    }
    setControlEnabled(true);
    setCommandStatus('Control enabled');
  }

  function emergencyStop(): void {
    setControlEnabled(false);
    issueCommand({ ...DEFAULT_COMMAND });
  }

  function changeSetpoints(next: Partial<Pick<Command, 'thrust' | 'roll' | 'pitch' | 'yaw'>>): void {
    issueCommand({ ...commandRef.current, ...next });
  }

  return (
    <div className="app-shell">
      <header className="topbar">
        <div className="brand-lockup">
          <div className="brand-mark"><span>U</span><i /></div>
          <div>
            <div className="brand-name">ULTRAHAWK</div>
             <div className="brand-subtitle">Ground control</div>
          </div>
        </div>

        <div className="topbar-center">
          <div className="transport-select">
             <span className="eyebrow">Link source</span>
            <select value={transportKind} onChange={(event) => setTransportKind(event.target.value as 'simulator' | 'serial')} disabled={transportStatus === 'connected'}>
               <option value="simulator">Simulator</option>
               <option value="serial">Serial</option>
            </select>
            <ChevronDown size={13} />
          </div>
          <button className={`button connect-button ${transportStatus === 'connected' ? 'connected' : ''}`} onClick={() => void connectOrDisconnect()}>
            {transportStatus === 'connected' ? <RefreshCw size={15} /> : <Cable size={15} />}
             {transportStatus === 'connected' ? 'Disconnect' : transportStatus === 'connecting' ? 'Connecting' : 'Connect'}
          </button>
        </div>

        <div className="topbar-right">
          <div className="link-summary">
            <span className={`status-orb ${connectionTone}`} />
             <div><span className="eyebrow">Connection</span><strong>{connectionLabel}</strong><small>{hasTelemetry && Number.isFinite(telemetryAge) ? `${Math.max(0, telemetryAge).toFixed(0)} ms since update` : 'No telemetry'}</small></div>
          </div>
        </div>
      </header>

      <div className="body-layout">
        <main className="workspace">
          <div className="workspace-heading">
            <div>
               <span className="eyebrow">Vehicle operations</span>
               <h1>Vehicle status</h1>
            </div>
          </div>

          <section className="orientation-panel panel-surface">
              <div className="panel-heading flight-heading">
                 <div><span className="eyebrow">Primary display</span><h2>Orientation</h2></div>
              </div>
              <div className="orientation-views">
                <div className="orientation-view pfd-view">
                   <span className="orientation-label">Attitude</span>
                  <AttitudeIndicator attitude={telemetry.attitude} stale={isStale || !hasTelemetry} />
                </div>
                <div className="orientation-view model-view">
                   <span className="orientation-label">Vehicle view</span>
                     <Suspense fallback={<div className="drone-loading">Loading vehicle view...</div>}>
                    <DroneAttitudeView attitude={telemetry.attitude} motors={telemetry.motors} stale={isStale || !hasTelemetry} />
                  </Suspense>
                </div>
              </div>
              <div className="flight-footer">
                 <div className="flight-stat"><span>Heading</span><strong>{Math.round(telemetry.attitude.yaw).toString().padStart(3, '0')}<small> deg</small></strong></div>
                 <div className="flight-stat"><span>Mission time</span><strong>{formatMissionTime(telemetry.missionTime)}</strong></div>
                 <div className="flight-stat"><span>Vehicle mode</span><strong className={`mode-text ${MODE_META[telemetry.mode].tone}`}>{telemetry.mode}</strong></div>
              </div>
          </section>

          <div className="operations-grid">
            <ControlPanel
              command={command}
              telemetryMode={telemetry.mode}
              actualAttitude={telemetry.attitude}
              controlEnabled={controlEnabled}
              transportConnected={transportStatus === 'connected'}
              pressedKeys={pressedKeys}
              commandStatus={commandStatus}
              onToggleControl={toggleControl}
              onChooseMode={chooseMode}
              onEmergencyStop={emergencyStop}
              onChangeSetpoints={changeSetpoints}
            />

            <MotorPanel telemetry={telemetry} stale={isStale || !hasTelemetry} />
          </div>

          <TelemetryChart frames={history} stale={isStale || !hasTelemetry} />
        </main>
      </div>
    </div>
  );
}

interface ControlPanelProps {
  command: Command;
  telemetryMode: VehicleMode;
  actualAttitude: TelemetryFrame['attitude'];
  controlEnabled: boolean;
  transportConnected: boolean;
  pressedKeys: string[];
  commandStatus: string;
  onToggleControl: () => void;
  onChooseMode: (mode: VehicleMode) => void;
  onEmergencyStop: () => void;
  onChangeSetpoints: (next: Partial<Pick<Command, 'thrust' | 'roll' | 'pitch' | 'yaw'>>) => void;
}

function ControlPanel({
  command,
  telemetryMode,
  actualAttitude,
  controlEnabled,
  transportConnected,
  pressedKeys,
  commandStatus,
  onToggleControl,
  onChooseMode,
  onEmergencyStop,
  onChangeSetpoints,
}: ControlPanelProps) {
  const canControl = controlEnabled && transportConnected;
  const commandSucceeded = commandStatus.startsWith('SENT') || commandStatus.startsWith('ACK');
  const commandFailed = commandStatus.startsWith('FAILED') || commandStatus.startsWith('BLOCKED') || commandStatus.startsWith('LINK ERROR');
  const commandTone = commandSucceeded ? 'success' : commandFailed ? 'warning' : '';
  const commandIcon = commandSucceeded ? <Check size={12} /> : commandFailed ? <TriangleAlert size={12} /> : null;

  return (
    <section className={`control-panel panel-surface ${controlEnabled ? 'control-active' : ''}`}>
      <div className="panel-heading control-heading">
        <div>
          <span className="eyebrow">Command console</span>
          <h2>Manual control</h2>
        </div>
        <div className={`control-state ${controlEnabled ? 'is-enabled' : ''}`}>
          <ShieldCheck size={16} />
          <strong>{controlEnabled ? 'Enabled' : 'Locked'}</strong>
          <button className={`control-toggle ${controlEnabled ? 'active' : ''}`} onClick={onToggleControl} disabled={!transportConnected}>
            {controlEnabled ? 'Disable' : 'Enable'}
          </button>
        </div>
      </div>

      <div className="mode-section">
        <div className="section-label">
          <span>Command mode</span>
          <span className="actual-mode">Actual <b>{telemetryMode}</b></span>
        </div>
        <div className="mode-buttons">
          {(['IDLE', 'TELEOP', 'AUTON'] as VehicleMode[]).map((mode) => (
            <button key={mode} className={`mode-button ${command.mode === mode ? 'selected' : ''} ${MODE_META[mode].tone}`} disabled={!transportConnected || (mode !== 'IDLE' && !controlEnabled)} onClick={() => onChooseMode(mode)}>
              {MODE_META[mode].label}
            </button>
          ))}
        </div>
      </div>

      <div className="setpoint-section">
        <div className="section-label">
          <span>Teleop setpoints</span>
          <span className={`command-state ${canControl && command.mode === 'TELEOP' ? 'armed' : ''}`}><i />{canControl && command.mode === 'TELEOP' ? 'Live input' : 'Input locked'}</span>
        </div>
        <div className="setpoint-list">
          <SetpointTrack label="Roll" value={command.roll} actual={actualAttitude.roll} min={-30} max={30} unit=" deg" centered disabled={!canControl || command.mode !== 'TELEOP'} onChange={(roll) => onChangeSetpoints({ roll })} />
          <SetpointTrack label="Pitch" value={command.pitch} actual={actualAttitude.pitch} min={-30} max={30} unit=" deg" centered disabled={!canControl || command.mode !== 'TELEOP'} onChange={(pitch) => onChangeSetpoints({ pitch })} />
          <SetpointTrack label="Thrust" value={command.thrust} min={-100} max={100} unit="%" centered disabled={!canControl || command.mode !== 'TELEOP'} onChange={(thrust) => onChangeSetpoints({ thrust })} />
          <SetpointTrack label="Yaw" value={command.yaw} actual={normalizeHeading(actualAttitude.yaw)} min={-180} max={180} unit=" deg" centered disabled={!canControl || command.mode !== 'TELEOP'} onChange={(yaw) => onChangeSetpoints({ yaw })} />
        </div>
      </div>

      {command.mode === 'TELEOP' && <div className="shortcut-row"><Keyboard size={14} /><span><b>WASD</b> attitude</span><span><b>Q / E</b> thrust</span>{pressedKeys.length > 0 && <span className="keys-live">Active: {pressedKeys.join(' ').toUpperCase()}</span>}</div>}

      <div className="control-footer">
        <div className={`command-status ${commandTone}`}>
          <span>{commandIcon}</span>
          <strong>{commandStatus}</strong>
        </div>
        <button className="stop-button" onClick={onEmergencyStop} disabled={!transportConnected}>
          <CircleStop size={17} />
          <strong>Stop / idle</strong>
        </button>
      </div>
    </section>
  );
}

function normalizeHeading(value: number): number {
  return ((value + 180) % 360 + 360) % 360 - 180;
}

export default App;
