interface SetpointTrackProps {
  label: string;
  value: number;
  actual?: number;
  min: number;
  max: number;
  unit: string;
  disabled: boolean;
  centered?: boolean;
  onChange: (value: number) => void;
}

export function SetpointTrack({ label, value, actual, min, max, unit, disabled, onChange }: SetpointTrackProps) {
  const step = label === 'Thrust' ? 5 : 1;

  function update(nextValue: number): void {
    if (!Number.isFinite(nextValue)) return;
    onChange(Math.min(max, Math.max(min, nextValue)));
  }

  return (
    <div className={`setpoint-row ${disabled ? 'is-disabled' : ''}`}>
      <div className="setpoint-row-head">
        <span className="setpoint-name">{label}</span>
        {actual !== undefined && <span className="setpoint-actual">Actual {actual.toFixed(1)}{unit}</span>}
      </div>
      <div className="setpoint-entry">
        <button type="button" aria-label={`Decrease ${label}`} disabled={disabled || value <= min} onClick={() => update(value - step)}>-</button>
        <input
          type="number"
          min={min}
          max={max}
          step={step}
          value={value}
          disabled={disabled}
          aria-label={`${label} target`}
          onChange={(event) => update(Number(event.target.value))}
        />
        <span className="setpoint-unit">{unit.trim()}</span>
        <button type="button" aria-label={`Increase ${label}`} disabled={disabled || value >= max} onClick={() => update(value + step)}>+</button>
      </div>
    </div>
  );
}
