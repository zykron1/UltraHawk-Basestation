import type { TelemetryFrame } from '../core/types';

interface MotorPanelProps {
  telemetry: TelemetryFrame;
  stale: boolean;
}

export function MotorPanel({ telemetry, stale }: MotorPanelProps) {
  const positions = [
    { x: 82, y: 55, label: 'M1' },
    { x: 176, y: 55, label: 'M2' },
    { x: 176, y: 149, label: 'M3' },
    { x: 82, y: 149, label: 'M4' },
  ];

  return (
    <div className={`motor-panel ${stale ? 'is-stale' : ''}`}>
      <div className="panel-heading compact-heading">
        <div>
           <span className="eyebrow">Actuators</span>
          <h3>Motor outputs</h3>
        </div>
      </div>
      <div className="motor-readouts">
        {telemetry.motors.map((motor, index) => (
          <div className="motor-readout" key={index}>
            <div className="motor-readout-top"><span>Motor {index + 1}</span><strong aria-label={`Motor ${index + 1} output`}>{motor.toFixed(2)}</strong></div>
            <div className="meter" aria-hidden="true"><span style={{ width: `${Math.max(0, Math.min(100, motor * 100))}%` }} /></div>
          </div>
        ))}
      </div>
      <svg className="motor-svg" viewBox="0 0 258 210" aria-label="Quadrotor motor layout" role="img">
        <line x1="83" y1="55" x2="176" y2="149" className="motor-arm" />
        <line x1="176" y1="55" x2="83" y2="149" className="motor-arm" />
        <circle cx="129.5" cy="102" r="22" className="motor-body" />
        <path d="M129.5 87 L121 105 H138 Z" className="motor-nose" />
        {positions.map((position, index) => (
          <g key={position.label}>
            <circle cx={position.x} cy={position.y} r="22" className={`motor-ring motor-${index + 1}`} />
            <text x={position.x} y={position.y + 4} className="motor-label">{position.label}</text>
          </g>
        ))}
      </svg>
    </div>
  );
}
