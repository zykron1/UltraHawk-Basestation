import type { TelemetryFrame } from '../core/types';

interface TelemetryChartProps {
  frames: TelemetryFrame[];
  stale: boolean;
}

function makePath(values: number[], min: number, max: number, width: number, height: number): string {
  if (values.length === 0) return '';
  return values
    .map((value, index) => {
      const x = (index / Math.max(1, values.length - 1)) * width;
      const y = height - ((Math.max(min, Math.min(max, value)) - min) / (max - min)) * height;
      return `${index === 0 ? 'M' : 'L'} ${x.toFixed(1)} ${y.toFixed(1)}`;
    })
    .join(' ');
}

export function TelemetryChart({ frames, stale }: TelemetryChartProps) {
  const width = 760;
  const height = 170;
  const roll = frames.map((frame) => frame.attitude.roll);
  const pitch = frames.map((frame) => frame.attitude.pitch);
  const yaw = frames.map((frame) => ((frame.attitude.yaw + 180) % 360) - 180);
  const gridRows = [0, 0.25, 0.5, 0.75, 1];

  return (
    <section className={`chart-panel ${stale ? 'is-stale' : ''}`}>
      <div className="panel-heading">
        <div>
           <span className="eyebrow">Telemetry history</span>
          <h2>Attitude traces</h2>
        </div>
        <div className="chart-controls">
           <span className="chart-legend"><i className="legend-line roll" /> Roll</span>
           <span className="chart-legend"><i className="legend-line pitch" /> Pitch</span>
           <span className="chart-legend"><i className="legend-line yaw" /> Yaw</span>
        </div>
      </div>
      <div className="chart-wrap">
        <div className="chart-axis"><span>+180</span><span>+90</span><span>0</span><span>-90</span><span>-180</span></div>
        <svg viewBox={`0 0 ${width} ${height}`} preserveAspectRatio="none" role="img" aria-label="Attitude telemetry chart">
          {gridRows.map((row) => <line key={row} x1="0" y1={row * height} x2={width} y2={row * height} className="chart-grid" />)}
          <path d={makePath(roll, -180, 180, width, height)} className="chart-path roll" />
          <path d={makePath(pitch, -180, 180, width, height)} className="chart-path pitch" />
          <path d={makePath(yaw, -180, 180, width, height)} className="chart-path yaw" />
        </svg>
      </div>
       <div className="chart-footer"><span>Recent window</span><span>{frames.length} samples</span><span>Mission time / sec</span></div>
    </section>
  );
}
