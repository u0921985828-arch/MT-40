//==============================================================================
//  Knob.tsx — rotary control tuned to the grow-lab identity.
//
//  A 270° arc knob: dim track, glowing value arc in the control's accent
//  (chlorophyll / LED-magenta / ink), a bright tip, and a unit-bearing readout.
//  Vertical drag changes the value; double-click resets to the default.
//==============================================================================

import { useCallback, useRef } from "react";
import { usePhenotypeStore, type ParamId } from "./store/usePhenotypeStore";
import { formatParam } from "./format";
import { PALETTE } from "./three/theme";

const A0 = 135; // start angle (deg, screen clockwise) — gap at the bottom
const SWEEP = 270;

function pt(cx: number, cy: number, r: number, deg: number) {
  const a = (deg * Math.PI) / 180;
  return [cx + r * Math.cos(a), cy + r * Math.sin(a)];
}
function arc(cx: number, cy: number, r: number, t0: number, t1: number) {
  const [x0, y0] = pt(cx, cy, r, A0 + SWEEP * t0);
  const [x1, y1] = pt(cx, cy, r, A0 + SWEEP * t1);
  const large = SWEEP * (t1 - t0) > 180 ? 1 : 0;
  return `M ${x0.toFixed(2)} ${y0.toFixed(2)} A ${r} ${r} 0 ${large} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}

export interface KnobDef {
  id: ParamId;
  label: string;
  chromosome?: 0 | 1;
  def?: number;
}

export function Knob({ def }: { def: KnobDef }) {
  const value = usePhenotypeStore((s) => s.params[def.id]);
  const setParam = usePhenotypeStore((s) => s.setParam);
  const setDragging = usePhenotypeStore((s) => s.setDragging);
  const drag = useRef<{ y: number; v: number } | null>(null);

  const accent =
    def.chromosome === 0 ? PALETTE.chlorophyll : def.chromosome === 1 ? PALETTE.ledMagenta : PALETTE.ink;

  const onDown = useCallback(
    (e: React.PointerEvent) => {
      (e.target as Element).setPointerCapture(e.pointerId);
      drag.current = { y: e.clientY, v: value };
      setDragging(def.id);
    },
    [value, def.id, setDragging],
  );
  const onMove = useCallback(
    (e: React.PointerEvent) => {
      if (!drag.current) return;
      const dy = drag.current.y - e.clientY;
      const fine = e.shiftKey ? 0.25 : 1;
      setParam(def.id, drag.current.v + (dy / 180) * fine);
    },
    [def.id, setParam],
  );
  const onUp = useCallback(
    (e: React.PointerEvent) => {
      drag.current = null;
      setDragging(null);
      (e.target as Element).releasePointerCapture?.(e.pointerId);
    },
    [setDragging],
  );

  const [tipX, tipY] = pt(50, 52, 30, A0 + SWEEP * value);
  const fmt = formatParam(def.id, value);

  return (
    <div className="knob" title={def.label}>
      <svg
        viewBox="0 0 100 100"
        className="knob__dial"
        role="slider"
        aria-label={def.label}
        aria-valuenow={Math.round(value * 100)}
        tabIndex={0}
        onPointerDown={onDown}
        onPointerMove={onMove}
        onPointerUp={onUp}
        onPointerCancel={onUp}
        onDoubleClick={() => def.def !== undefined && setParam(def.id, def.def)}
        onKeyDown={(e) => {
          if (e.key === "ArrowUp" || e.key === "ArrowRight") setParam(def.id, value + 0.02);
          if (e.key === "ArrowDown" || e.key === "ArrowLeft") setParam(def.id, value - 0.02);
        }}
      >
        <circle cx="50" cy="52" r="38" className="knob__well" />
        <path d={arc(50, 52, 30, 0, 1)} className="knob__track" />
        <path d={arc(50, 52, 30, 0, Math.max(0.0001, value))} className="knob__value" style={{ stroke: accent }} />
        <line x1="50" y1="52" x2={tipX.toFixed(2)} y2={tipY.toFixed(2)} className="knob__ind" style={{ stroke: accent }} />
        <circle cx={tipX.toFixed(2)} cy={tipY.toFixed(2)} r="3.4" className="knob__tip" style={{ fill: accent }} />
      </svg>
      <div className="knob__read" style={{ color: accent }}>
        <span className="knob__val">{fmt.value}</span>
        {fmt.unit && <span className="knob__unit">{fmt.unit}</span>}
      </div>
      <div className="knob__label">{def.label}</div>
    </div>
  );
}
