import { useId } from 'react';
import type { Attitude } from '../core/types';
import { formatDegrees } from '../core/types';

interface AttitudeIndicatorProps {
  attitude: Attitude;
  stale: boolean;
}

export function AttitudeIndicator({ attitude, stale }: AttitudeIndicatorProps) {
  const clipId = useId().replace(/:/g, '');
  const pitchOffset = attitude.pitch * 2.15;
  const rollTicks = [-60, -45, -30, -15, 0, 15, 30, 45, 60];
  const pitchTicks = [-30, -20, -10, 10, 20, 30];

  return (
    <div className={`attitude-wrap ${stale ? 'is-stale' : ''}`}>
      <svg className="attitude-svg" viewBox="0 0 520 440" role="img" aria-label="Primary flight display">
        <defs>
          <clipPath id={clipId}>
            <circle cx="260" cy="207" r="162" />
          </clipPath>
          <linearGradient id={`${clipId}-sky`} x1="0" x2="0" y1="0" y2="1">
             <stop offset="0" stopColor="#355a68" />
             <stop offset="1" stopColor="#527b89" />
          </linearGradient>
          <linearGradient id={`${clipId}-ground`} x1="0" x2="0" y1="0" y2="1">
             <stop offset="0" stopColor="#765a3a" />
             <stop offset="1" stopColor="#4b3928" />
          </linearGradient>
        </defs>

        <g className="attitude-bezel">
          <circle cx="260" cy="207" r="176" />
          <circle cx="260" cy="207" r="169" />
        </g>
        <g clipPath={`url(#${clipId})`}>
          <g transform={`rotate(${-attitude.roll} 260 207) translate(0 ${pitchOffset})`}>
            <rect x="0" y="-300" width="520" height="507" fill={`url(#${clipId}-sky)`} />
            <rect x="0" y="207" width="520" height="500" fill={`url(#${clipId}-ground)`} />
            <line x1="0" y1="207" x2="520" y2="207" className="horizon-line" />
            {pitchTicks.map((tick) => (
              <g key={tick}>
                <line x1={tick % 20 === 0 ? 210 : 225} y1={207 - tick * 2.15} x2={tick % 20 === 0 ? 310 : 295} y2={207 - tick * 2.15} className="pitch-tick" />
                {tick % 20 === 0 && <text x="194" y={211 - tick * 2.15} className="pitch-label">{Math.abs(tick)}</text>}
              </g>
            ))}
          </g>
        </g>

        <g className="roll-scale">
          {rollTicks.map((tick) => {
            const radians = (tick - 90) * (Math.PI / 180);
            const inner = tick === 0 ? 151 : tick % 30 === 0 ? 147 : 153;
            const outer = 163;
            return (
              <line
                key={tick}
                x1={260 + Math.cos(radians) * inner}
                y1={207 + Math.sin(radians) * inner}
                x2={260 + Math.cos(radians) * outer}
                y2={207 + Math.sin(radians) * outer}
                className={tick === 0 ? 'roll-tick roll-tick-major' : 'roll-tick'}
              />
            );
          })}
          <path d="M248 43 L260 57 L272 43" className="roll-index" />
        </g>

        <g className="aircraft-symbol">
          <path d="M206 210 H239 L250 201 L261 210 H314" />
          <circle cx="260" cy="210" r="5" />
          <line x1="260" y1="188" x2="260" y2="232" />
        </g>

        <g className="instrument-readout">
          <rect x="182" y="375" width="84" height="25" rx="4" />
          <text x="224" y="392">R {formatDegrees(attitude.roll)}</text>
          <rect x="254" y="375" width="84" height="25" rx="4" />
          <text x="296" y="392">P {formatDegrees(attitude.pitch)}</text>
        </g>
      </svg>
      <div className="attitude-caption">
         <span><i className="signal-dot cyan" /> Attitude</span>
         <span className={stale ? 'text-amber' : 'text-muted'}>{stale ? 'Stale' : `Heading ${Math.round(attitude.yaw).toString().padStart(3, '0')}`}</span>
      </div>
    </div>
  );
}
